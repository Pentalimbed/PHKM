#include "phkm.h"

#include <thread>
#include <chrono>

#include "utils.h"
#include "events.h"
#include "hooks.h"

namespace phkm
{
std::default_random_engine random_engine;

// Called every frame
void DelayedFuncModule::update()
{
    auto delta_time = *g_delta_real_time;
    funcs_mutex.lock();
    std::erase_if(
        funcs, [=](auto& pair) {
            auto& [delay_time, func] = pair;
            delay_time -= delta_time;
            if (delay_time < 0)
            {
                logger::debug("Executing delayed func");
                func();
                return true;
            }
            return false;
        });
    funcs_mutex.unlock();
}

void DelayedFuncModule::flush()
{
    funcs_mutex.lock();
    funcs.clear();
    funcs_mutex.unlock();
}

// return true if original func are to be called
bool PostHitModule::process(RE::Actor* victim, RE::HitData& hit_data)
{
    static auto config = PhkmConfig::getSingleton();

    auto attacker = hit_data.aggressor.get().get();
    if (!attacker)
        return true;
    bugFixes(attacker, victim);

    // Bypass for testing
    if (false)
    {
    }

    // Check if actors are good for animation
    if (!checkActors(attacker, victim))
        return true;

    // Check if exec/km conditions are met
    float dmg_mult = getDamageMult(victim->IsPlayerRef());
    if (!canTrigger(attacker, victim, hit_data.totalDamage * dmg_mult))
        return true;

    // Non-animated
    bool play_exec_anims = victim->IsBleedingOut() || isInRagdoll(victim);
    if (!config->animated_ragdoll_exec && play_exec_anims)
    {
        hit_data.totalDamage = victim->GetActorValue(RE::ActorValue::kHealth) / dmg_mult * 2;
        return true;
    }

    // Filtering entries
    auto entries_copy(AnimEntryParser::getSingleton()->entries);
    filterEntries(entries_copy, attacker, victim, hit_data, play_exec_anims);
    if (entries_copy.empty())
    {
        hit_data.totalDamage = victim->GetActorValue(RE::ActorValue::kHealth) / dmg_mult * 2;
        return true;
    }

    // Alter damage and cancel stagger
    hit_data.totalDamage = 0;
    hit_data.stagger     = 0;
    attacker->NotifyAnimationGraph("staggerStop");
    victim->NotifyAnimationGraph("staggerStop");

    // Choose random anim and play
    std::uniform_int_distribution<size_t> distro(0, entries_copy.size() - 1);
    auto                                  entry = std::next(entries_copy.begin(), distro(random_engine))->second;

    if (isInRagdoll(victim))
    {
        victim->SetPosition(RE::NiPoint3(victim->data.location.x, victim->data.location.y, attacker->GetPositionZ()), true);
        victim->SetActorValue(RE::ActorValue::kParalysis, 0);
        victim->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
        victim->NotifyAnimationGraph("GetUpStart");

        DelayedFuncModule::getSingleton()->addFunc(0.2f, [=]() {
            victim->actorState1.knockState = RE::KNOCK_STATE_ENUM::kNormal;
            victim->NotifyAnimationGraph("GetUpExit");
            entry.play(attacker, victim);
        });
    }
    else
        entry.play(attacker, victim);

    return true;
}

void PostHitModule::bugFixes(RE::Actor* attacker, RE::Actor* victim)
{
    // Killmove status fix
    if (victim->IsInKillMove() && !isInPairedAnimation(victim))
    {
        logger::debug("Fixing {} killmove status", victim->GetName());
        victim->boolFlags.reset(RE::Actor::BOOL_FLAGS::kIsInKillMove);
    }
    // Prolonged paralysis fix
    if ((victim->boolBits.underlying() & (uint32_t)RE::Actor::BOOL_BITS::kParalyzed) && !victim->HasEffectWithArchetype(RE::MagicTarget::Archetype::kParalysis))
    {
        logger::debug("Fixing {} paralysis status", victim->GetName());
        victim->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
    }
    //Negative health fix (NOT WORKING)
    if ((victim->GetActorValue(RE::ActorValue::kHealth) < 0) && !victim->IsDead() &&
        !victim->IsEssential() && (!attacker->IsPlayerRef() || !isProtected(victim)))
    {
        logger::debug("Fixing {} negative health status", victim->GetName());
        victim->NotifyAnimationGraph("KillActor");
        victim->KillImpl(attacker, 0, true, true);
    }
}

bool PostHitModule::checkActors(RE::Actor* attacker, RE::Actor* victim)
{
    const auto config = PhkmConfig::getSingleton();
    return isValid(attacker) && isValid(victim) &&
        !attacker->IsBleedingOut() && !isInRagdoll(attacker) &&
        !(config->essential_protect &&
          (victim->IsEssential() || ((victim->boolFlags & RE::Actor::BOOL_FLAGS::kProtected) && !attacker->IsPlayerRef())));
}

bool PostHitModule::isValid(RE::Actor* actor)
{
    return actor->Is3DLoaded() && !actor->IsDead() && !isInPairedAnimation(actor) && !actor->IsOnMount();
}

bool PostHitModule::canTrigger(RE::Actor* attacker, RE::Actor* victim, float actual_damage)
{
    const auto config = PhkmConfig::getSingleton();

    // Check execution
    bool can_trigger_exec = (config->enable_bleedout_exec && victim->IsBleedingOut()) || (config->enable_ragdoll_exec && isInRagdoll(victim));
    if (can_trigger_exec)
        if ((attacker->IsPlayerRef() ? config->exec_player2npc : (victim->IsPlayerRef() ? config->exec_npc2player : config->exec_npc2npc)) &&
            (config->last_enemy_exec ? (!victim->IsPlayerRef() && !isLastHostileActor(attacker, victim)) : true))
            return true;

    // Check killmove
    bool can_trigger_km = victim->GetActorValue(RE::ActorValue::kHealth) <= actual_damage;
    if (can_trigger_km)
        if (config->last_enemy_exec ? (!victim->IsPlayerRef() && !isLastHostileActor(attacker, victim)) : true)
        {
            // lottery
            std::uniform_real_distribution<float> km_chance_distro(0, 1.0f);
            float                                 km_chance =
                attacker->IsPlayerRef() ? config->p_km_player2npc : (victim->IsPlayerRef() ? config->p_km_npc2player : config->p_km_npc2npc);
            if (km_chance_distro(random_engine) < km_chance)
                return true;
        }

    return false;
}

inline void logEntries(const std::unordered_map<std::string, AnimEntry>& entries)
{
    std::string list;
    for (const auto& item : entries) list += item.first + " ";
    logger::debug("Remaining: {}", list);
}

void PostHitModule::filterEntries(std::unordered_map<std::string, AnimEntry>& entries, RE::Actor* attacker, RE::Actor* victim, RE::HitData& hit_data, bool play_exec_anims)
{
    auto edid_maps = EditorIdMaps::getSingleton();
    auto config    = PhkmConfig::getSingleton();

    // Killmove v. Execution
    std::erase_if(entries, [&](const auto& item) { return play_exec_anims != item.second.misc_conds.is_execution; });

    // Sneak check
    bool is_sneak_attack = static_cast<bool>(static_cast<uint32_t>(hit_data.flags) & static_cast<uint32_t>(RE::HitData::Flag::kSneakAttack));
    std::erase_if(entries, [&](const auto& item) { return is_sneak_attack != item.second.misc_conds.is_sneak; });

    // Angle check
    float relative_angle = victim->GetHeadingAngle(attacker->GetPosition(), false);
    std::erase_if(entries, [&](const auto& item) { return !isBetweenAngle(relative_angle, item.second.misc_conds.min_angle, item.second.misc_conds.max_angle); });

    // Decap check
    static const auto decap_1h_perk = RE::TESForm::LookupByID<RE::BGSPerk>(0x3af81); // Savage Strike
    static const auto decap_2h_perk = RE::TESForm::LookupByID<RE::BGSPerk>(0x52d52); // Devastating Blow
    assert(decap_1h_perk && decap_2h_perk);
    if (config->decap_perk_req && !attacker->HasPerk(is2h(attacker) ? decap_2h_perk : decap_1h_perk))
        std::erase_if(entries, [&](const auto& item) { return item.second.misc_conds.is_decap; });

    // Skel check
    auto att_skel  = attacker->GetRace()->skeletonModels[attacker->GetActorBase()->IsFemale()].model;
    auto vic_skel  = victim->GetRace()->skeletonModels[victim->GetActorBase()->IsFemale()].model;
    auto att_check = [&](std::string skel_name) {
        return skel_name == att_skel.c_str();
    };
    auto vic_check = [&](std::string skel_name) {
        return skel_name == vic_skel.c_str();
    };
    std::erase_if(entries, [&](const auto& item) {
        auto& entry = item.second;
        return (entry.attacker_conds.contains("skeleton") && !orCheck(entry.attacker_conds["skeleton"], att_check)) ||
            (entry.victim_conds.contains("skeleton") && !orCheck(entry.victim_conds["skeleton"], vic_check));
    });

    // Race check
    std::erase_if(entries, [&](const auto& item) {
        auto& entry = item.second;
        return (entry.attacker_conds.contains("race") && !edid_maps->checkRace(attacker, entry.attacker_conds["race"])) ||
            (entry.victim_conds.contains("race") && !edid_maps->checkRace(victim, entry.victim_conds["race"]));
    });

// WeapType check
#define WEAPTYPECHECK(actor, json, key) orCheck(json[key],                                                                                              \
                                                [&](int weaptype) {                                                                                     \
                                                    return (weaptype < 0) != hasWeaponType(actor, stringsEqual(key, "weaptype_r"), std::abs(weaptype)); \
                                                })

    std::erase_if(entries, [&](const auto& item) {
        auto& entry = item.second;
        return (entry.attacker_conds.contains("weaptype_l") && !WEAPTYPECHECK(attacker, entry.attacker_conds, "weaptype_l")) ||
            (entry.victim_conds.contains("weaptype_l") && !WEAPTYPECHECK(victim, entry.victim_conds, "weaptype_l")) ||
            (entry.attacker_conds.contains("weaptype_r") && !WEAPTYPECHECK(attacker, entry.attacker_conds, "weaptype_r")) ||
            (entry.victim_conds.contains("weaptype_r") && !WEAPTYPECHECK(victim, entry.victim_conds, "weaptype_r"));
    });

    // Faction check
    std::erase_if(entries, [&](const auto& item) {
        auto& entry = item.second;
        return (entry.attacker_conds.contains("faction") && !edid_maps->checkFaction(attacker, entry.attacker_conds["faction"])) ||
            (entry.victim_conds.contains("faction") && !edid_maps->checkFaction(victim, entry.victim_conds["faction"]));
    });

    // Keyword check
    std::erase_if(entries, [&](const auto& item) {
        auto& entry = item.second;
        return (entry.attacker_conds.contains("keyword") && !edid_maps->checkKeywords(attacker, entry.attacker_conds["keyword"])) ||
            (entry.victim_conds.contains("keyword") && !edid_maps->checkKeywords(victim, entry.victim_conds["keyword"]));
    });
}

void PostHitModule::prepareForKillmove(RE::Actor* actor)
{
    auto para_kwd = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ImmuneParalysis");
    if (para_kwd && !actor->HasKeyword(para_kwd))
    {
        actor->GetObjectReference()->As<RE::BGSKeywordForm>()->AddKeyword(para_kwd);
        DelayedFuncModule::getSingleton()->addFunc(1.0f, [=]() {
            actor->GetObjectReference()->As<RE::BGSKeywordForm>()->RemoveKeyword(para_kwd);
        });
    }
    auto push_kwd = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ImmuneStrongUnrelentingForce");
    if (push_kwd && !actor->HasKeyword(push_kwd))
    {
        actor->GetObjectReference()->As<RE::BGSKeywordForm>()->AddKeyword(push_kwd);
        DelayedFuncModule::getSingleton()->addFunc(1.0f, [=]() {
            actor->GetObjectReference()->As<RE::BGSKeywordForm>()->RemoveKeyword(push_kwd);
        });
    }
}
} // namespace phkm