#include "phkm.h"

#include "utils.h"

namespace phkm
{
// return true if original func are to be called
bool PostHitModule::process(RE::Actor* victim, RE::HitData& hit_data)
{
    static std::default_random_engine random_engine;
    static auto                       config = ConfigParser::getSingleton();

    // Check mod
    if (!config->isEnabled())
        return true;

    auto attacker = hit_data.aggressor.get().get();
    if (!attacker)
        return true;
    bugFixes(attacker, victim);

    // Check if actors are good for animation
    if (!checkActors(attacker, victim))
        return true;
    logger::debug("Receive hit from: {} to: {}", attacker->GetName(), victim->GetName());

    // Bypass for testing
    if (false)
    {
        // Single paired test
        // AnimEntryParser::getSingleton()->entries["execground01"].play(a_attacker, a_victim);
        // a_hitData.totalDamage = 0;
        // return ;
    }

    // Check execution and other conditions
    bool do_exec = canExecute(victim);
    if (!canTrigger(attacker, victim, do_exec, hit_data.totalDamage))
        return true;

    logger::debug("total damage: {}, blocked percent: {}, health: {}",
                  hit_data.totalDamage, hit_data.percentBlocked, victim->GetActorValue(RE::ActorValue::kHealth));

    // Non-animated ragdoll execution
    if (!config->animated_ragdoll_exec && isInRagdoll(victim))
    {
        logger::debug("Non animated execution!");
        victim->KillImpl(attacker, victim->GetActorValue(RE::ActorValue::kHealth), true, true);
        if (!victim->IsDead())
        {
            hit_data.totalDamage = 1e9;
        }
        return true;
    }

    // Killmove chance
    static std::uniform_real_distribution<float> km_chance_distro(0, 1.0f);
    float                                        km_chance = attacker->IsPlayerRef() ? config->km_chance : config->npc_km_chance;
    if (!do_exec && (km_chance_distro(random_engine) > km_chance))
        return true;

    // Filtering entries
    auto entries_copy(AnimEntryParser::getSingleton()->entries);
    filterEntries(entries_copy, attacker, victim, hit_data, do_exec);
    if (entries_copy.empty())
        return true;
    for (auto& item : entries_copy)
    {
        logger::debug("{} selected!", item.second.name);
    }

    // Alter damage and cancel stagger;
    hit_data.totalDamage = 0;
    hit_data.stagger     = 0;

    // Wash your hand
    prepareForKillmove(attacker);
    prepareForKillmove(victim);

    // Lottery and play
    std::uniform_int_distribution<size_t>
          distro(0, entries_copy.size() - 1);
    auto& entry = std::next(entries_copy.begin(), distro(random_engine))->second;
    entry.play(attacker, victim);

    return false;
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
    return isValid(attacker) && isValid(victim) &&
        !attacker->IsBleedingOut() && !isInRagdoll(attacker);
}

bool PostHitModule::isValid(RE::Actor* actor)
{
    return actor->Is3DLoaded() && !actor->IsDead() && !isInPairedAnimation(actor) && !actor->IsOnMount();
}

bool PostHitModule::canExecute(RE::Actor* victim)
{
    const auto config = ConfigParser::getSingleton();
    return (victim->IsBleedingOut() && config->enable_bleedout_exec) ||
        (isInRagdoll(victim) && config->enable_ragdoll_exec);
}

bool PostHitModule::canTrigger(RE::Actor* attacker, RE::Actor* victim, bool do_exec, float total_damage)
{
    const auto config = ConfigParser::getSingleton();

    // Check enabled
    if (!(do_exec ? config->enable_exec : config->enable_km))
        return false;
    logger::debug("1");

    // Check essential
    if (victim->IsEssential() && config->essential_protect)
        return false;
    if ((victim->boolFlags & RE::Actor::BOOL_FLAGS::kProtected) && !attacker->IsPlayerRef() && config->essential_protect)
        return false;
    logger::debug("2");

    // Check player
    if (victim->IsPlayerRef())
    {
        if (!(do_exec ? config->enable_player_exec : config->enable_player_km))
            return false;
        total_damage *= getDamageMult(true);
    }
    else
    {
        // Check npc
        if (!(attacker->IsPlayerRef() || (do_exec ? config->enable_npc_exec : config->enable_npc_km)))
            return false;
        total_damage *= getDamageMult(false);
    }
    logger::debug("3");

    // Check damage for killmoves
    //      totalDamage in HitData are positive
    //      whilst arg of HandleHeathDamage is negative and multiplied by difficulty setting (which is actual damage)
    if (!do_exec && (victim->GetActorValue(RE::ActorValue::kHealth) - total_damage > 0))
        return false;
    logger::debug("4");

    // Check last enemy
    if (do_exec ? config->last_enemy_exec : config->last_enemy_km)
    {
        // Seems not working on npcs killing player
        if (!victim->IsPlayerRef() && !isLastHostileActor(attacker, victim))
            return false;
    }
    logger::debug("5");
    return true;
}

inline void logEntries(const std::unordered_map<std::string, AnimEntry>& entries)
{
    std::string list;
    for (const auto& item : entries) list += item.first + " ";
    logger::debug("Remaining: {}", list);
}

void PostHitModule::filterEntries(std::unordered_map<std::string, AnimEntry>& entries, RE::Actor* attacker, RE::Actor* victim, RE::HitData& hit_data, bool do_exec)
{
    auto edid_maps = EditorIdMaps::getSingleton();
    auto config    = ConfigParser::getSingleton();

    // Killmove v. Execution
    std::erase_if(entries, [&](const auto& item) { return do_exec != item.second.misc_conds.is_execution; });

    // Paired check
    if (!config->enable_unpaired)
        std::erase_if(entries, [&](const auto& item) { return !item.second.is_paired; });

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

    // Race check
    std::erase_if(entries, [&](const auto& item) {
        auto& entry = item.second;
        return (entry.attacker_conds.contains("race") && !edid_maps->checkRace(attacker, entry.attacker_conds["race"])) ||
            (entry.victim_conds.contains("race") && !edid_maps->checkRace(victim, entry.victim_conds["race"]));
    });

    // WeapType check
    std::erase_if(entries, [&](const auto& item) {
        auto& entry = item.second;
        return (entry.attacker_conds.contains("weaptype_l") && !edid_maps->checkWeapType(getEquippedWeapon(attacker, true), entry.attacker_conds["weaptype_l"])) ||
            (entry.victim_conds.contains("weaptype_l") && !edid_maps->checkWeapType(getEquippedWeapon(victim, true), entry.victim_conds["weaptype_l"])) ||
            (entry.attacker_conds.contains("weaptype_r") && !edid_maps->checkWeapType(getEquippedWeapon(attacker, false), entry.attacker_conds["weaptype_r"])) ||
            (entry.victim_conds.contains("weaptype_r") && !edid_maps->checkWeapType(getEquippedWeapon(victim, false), entry.victim_conds["weaptype_r"]));
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
    actor->EnableAI(true);
    if (isInRagdoll(actor)) // Not too consistent
    {
        actor->NotifyAnimationGraph("GetUpStart");
        actor->actorState1.knockState = RE::KNOCK_STATE_ENUM::kNormal;
        if (const auto charController = actor->GetCharController(); charController)
        {
            charController->flags.reset(RE::CHARACTER_FLAGS::kNotPushable);
            charController->flags.set(RE::CHARACTER_FLAGS::kRecordHits);
            charController->flags.set(RE::CHARACTER_FLAGS::kHitFlags);
        }
    };
    // actor->As<RE::BGSKeywordForm>()->AddKeyword(RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ImmuneParalysis"));
}
} // namespace phkm