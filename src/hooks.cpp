#include "hooks.h"

namespace phkm
{
void ProcessHitHook::thunk(RE::Actor* a_victim, RE::HitData& a_hitData)
{
    static std::default_random_engine random_engine;
    static auto                       config    = ConfigParser::getSingleton();
    auto                              orig_func = [&]() {
        return func(a_victim, a_hitData);
    };

    // Check mod
    if (!config->isEnabled())
        return orig_func();

    // Check actors are ok
    auto a_attacker = a_hitData.aggressor.get().get();
    if (!checkActors(a_attacker, a_victim))
        return orig_func();

    logger::debug("Receive hit from: {} to: {}", a_attacker->GetName(), a_victim->GetName());

    // Check execution and other conditions
    bool do_exec = canExecute(a_victim);
    if (!canTrigger(a_attacker, a_victim, do_exec, a_hitData.totalDamage))
        return orig_func();

    logger::debug("total damage: {}, blocked percent: {}, health: {}",
                  a_hitData.totalDamage, a_hitData.percentBlocked, a_victim->GetActorValue(RE::ActorValue::kHealth));

    // Non-animated ragdoll execution
    if (!config->animated_ragdoll_exec && isInRagdoll(a_victim))
    {
        a_hitData.totalDamage = 1e8;
        return orig_func();
    }

    // Killmove chance
    static std::uniform_real_distribution<float> km_chance_distro(0, 1.0f);
    float                                        km_chance = a_attacker->IsPlayerRef() ? config->km_chance : config->npc_km_chance;
    if (!do_exec && (km_chance_distro(random_engine) > km_chance))
        return orig_func();

    // Filtering entries
    auto entries_copy(AnimEntryParser::getSingleton()->entries);
    filterEntries(entries_copy, a_attacker, a_victim, do_exec);
    if (entries_copy.empty())
        return orig_func();
    for (auto& item : entries_copy)
    {
        logger::debug("{} selected!", item.second.name);
    }

    // Alter damage and cancel stagger;
    a_hitData.totalDamage = 0;
    a_hitData.stagger     = 0;
    if (isInRagdoll(a_victim)) // Not too consistent
    {
        a_victim->NotifyAnimationGraph("GetUpStart");
        a_victim->actorState1.knockState = RE::KNOCK_STATE_ENUM::kNormal;
        if (const auto charController = a_victim->GetCharController(); charController)
        {
            charController->flags.reset(RE::CHARACTER_FLAGS::kNotPushable);
            charController->flags.set(RE::CHARACTER_FLAGS::kRecordHits);
            charController->flags.set(RE::CHARACTER_FLAGS::kHitFlags);
        }
        a_victim->EnableAI(true);
        a_victim->SetLifeState(RE::ACTOR_LIFE_STATE::kAlive);
    }
    orig_func();

    // Lottery and play
    std::uniform_int_distribution<size_t> distro(0, entries_copy.size() - 1);
    auto&                                 entry = std::next(entries_copy.begin(), distro(random_engine))->second;

    entry.play(a_attacker, a_victim);
}

bool ProcessHitHook::checkActors(RE::Actor* attacker, RE::Actor* victim)
{
    return attacker && victim &&
        isValid(attacker) && isValid(victim) &&
        !attacker->IsBleedingOut() && !isInRagdoll(attacker);
}

bool ProcessHitHook::isValid(RE::Actor* actor)
{
    return actor->Is3DLoaded() && !actor->IsDead() && !actor->IsInKillMove();
}

bool ProcessHitHook::canExecute(RE::Actor* victim)
{
    const auto config = ConfigParser::getSingleton();
    return (victim->IsBleedingOut() && config->enable_bleedout_exec) ||
        (isInRagdoll(victim) && config->enable_ragdoll_exec);
}

bool ProcessHitHook::canTrigger(RE::Actor* attacker, RE::Actor* victim, bool do_exec, float total_damage)
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
        if (!victim->IsPlayerRef())
        {
            RE::TESConditionItem isLastHostileActor;
            isLastHostileActor.data.functionData.function = RE::FUNCTION_DATA::FunctionID::kIsLastHostileActor;
            isLastHostileActor.data.comparisonValue.f     = 1.0f;
            isLastHostileActor.data.flags.opCode          = RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
            isLastHostileActor.data.object                = RE::CONDITIONITEMOBJECT::kTarget;
            RE::ConditionCheckParams params(attacker->As<RE::TESObjectREFR>(), victim->As<RE::TESObjectREFR>());
            if (!isLastHostileActor(params))
                return false;
        }
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

void ProcessHitHook::filterEntries(std::unordered_map<std::string, AnimEntry>& entries, RE::Actor* attacker, RE::Actor* victim, bool do_exec)
{
    auto edid_maps = EditorIdMaps::getSingleton();
    auto config    = ConfigParser::getSingleton();

    // Killmove v. Execution
    std::erase_if(entries, [&](const auto& item) { return do_exec != item.second.misc_conds.is_execution; });

    // Paired check
    if (!config->enable_unpaired)
        std::erase_if(entries, [&](const auto& item) { return !item.second.is_paired; });

    // Sneak check
    bool is_sneaking = attacker->IsSneaking();
    std::erase_if(entries, [&](const auto& item) { return is_sneaking != item.second.misc_conds.is_sneak; });

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
} // namespace phkm