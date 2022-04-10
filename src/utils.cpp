#include "utils.h"
#include "files.h"

namespace phkm
{
std::default_random_engine random_engine;

union ConditionParam
{
    char         c;
    std::int32_t i;
    float        f;
    RE::TESForm* form;
};

bool orCheck(nlohmann::json j_array, check_func_t func)
{
    for (const auto& item : j_array)
        if (item.is_array() ? andCheck(item, func) : func(item))
            return true;
    return j_array.empty();
}

bool andCheck(nlohmann::json j_array, check_func_t func)
{
    for (const auto& item : j_array)
        if (!(item.is_array() ? andCheck(item, func) : func(item)))
            return false;
    return j_array.empty();
}

bool areActorsReady(RE::Actor* attacker, RE::Actor* victim)
{
    const auto config = PhkmConfig::getSingleton();
    return isValid(attacker) && isValid(victim) &&
        !attacker->IsBleedingOut() && !isInRagdoll(attacker) &&
        !(config->essential_protect &&
          (victim->IsEssential() || ((victim->boolFlags & RE::Actor::BOOL_FLAGS::kProtected) && !attacker->IsPlayerRef())));
}

bool isValid(RE::Actor* actor)
{
    return actor->Is3DLoaded() && !actor->IsDead() && !isInPairedAnimation(actor) && !actor->IsOnMount();
}

void filterEntries(std::unordered_map<std::string, AnimEntry>& entries, RE::Actor* attacker, RE::Actor* victim, bool is_sneak, bool play_exec_anims)
{
    auto edid_maps = EditorIdMaps::getSingleton();
    auto config    = PhkmConfig::getSingleton();

    // Killmove v. Execution
    std::erase_if(entries, [&](const auto& item) { return play_exec_anims != item.second.misc_conds.is_execution; });

    // Sneak check
    std::erase_if(entries, [&](const auto& item) { return is_sneak != item.second.misc_conds.is_sneak; });

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

bool isDetectedBy(RE::Actor* subject, RE::Actor* target)
{
    static bool                 is_initialized = false;
    static RE::TESConditionItem isDetectedCond;
    if (!is_initialized)
    {
        is_initialized = true;

        isDetectedCond.data.functionData.function  = RE::FUNCTION_DATA::FunctionID::kGetDetected;
        isDetectedCond.data.functionData.params[0] = nullptr;
        isDetectedCond.data.comparisonValue.f      = 1.0f;
        isDetectedCond.data.flags.opCode           = RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
        isDetectedCond.data.object                 = RE::CONDITIONITEMOBJECT::kTarget;
    }

    RE::ConditionCheckParams params(subject->As<RE::TESObjectREFR>(), target->As<RE::TESObjectREFR>());
    return isDetectedCond(params);
}

bool isLastHostileActor(RE::Actor* subject, RE::Actor* target)
{
    static bool                 is_initialized = false;
    static RE::TESConditionItem isLastHostileActorCond;
    if (!is_initialized)
    {
        is_initialized = true;

        isLastHostileActorCond.data.functionData.function = RE::FUNCTION_DATA::FunctionID::kIsLastHostileActor;
        isLastHostileActorCond.data.comparisonValue.f     = 1.0f;
        isLastHostileActorCond.data.flags.opCode          = RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
        isLastHostileActorCond.data.object                = RE::CONDITIONITEMOBJECT::kTarget;
    }

    RE::ConditionCheckParams params(subject->As<RE::TESObjectREFR>(), target->As<RE::TESObjectREFR>());
    return isLastHostileActorCond(params);
}

bool isInPairedAnimation(RE::Actor* actor)
{
    static bool                 is_initialized = false;
    static RE::TESConditionItem isLastHostileActorCond;
    if (!is_initialized)
    {
        is_initialized = true;

        isLastHostileActorCond.data.functionData.function = RE::FUNCTION_DATA::FunctionID::kGetPairedAnimation;
        isLastHostileActorCond.data.comparisonValue.f     = 0.0f;
        isLastHostileActorCond.data.flags.opCode          = RE::CONDITION_ITEM_DATA::OpCode::kNotEqualTo;
        isLastHostileActorCond.data.object                = RE::CONDITIONITEMOBJECT::kTarget;
    }

    RE::ConditionCheckParams params(nullptr, actor->As<RE::TESObjectREFR>());
    return isLastHostileActorCond(params);
}

bool hasWeaponType(RE::Actor* actor, bool right_hand, int weap_type)
{
    static bool                 is_initialized = false;
    static RE::TESConditionItem hasWeaponTypeCond;
    if (!is_initialized)
    {
        is_initialized = true;

        hasWeaponTypeCond.data.functionData.function = RE::FUNCTION_DATA::FunctionID::kGetEquippedItemType;
        hasWeaponTypeCond.data.flags.opCode          = RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
        hasWeaponTypeCond.data.object                = RE::CONDITIONITEMOBJECT::kSelf;
    }

    ConditionParam cond_param;
    cond_param.i = static_cast<char>(right_hand); // RE::MagicSystem::CastingSource

    hasWeaponTypeCond.data.functionData.params[0] = std::bit_cast<void*>(cond_param);
    hasWeaponTypeCond.data.comparisonValue.f      = static_cast<float>(weap_type);

    RE::ConditionCheckParams params(actor->As<RE::TESObjectREFR>(), nullptr);
    return hasWeaponTypeCond(params);
}
} // namespace phkm