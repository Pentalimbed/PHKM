#pragma once

#include "nlohmann/json.hpp"

#ifndef M_PI
#    define M_PI 3.141592653589793238462643383279
#endif

namespace phkm
{
using check_func_t = std::function<bool(nlohmann::json)>;
bool orCheck(nlohmann::json j_array, check_func_t func);
bool andCheck(nlohmann::json j_array, check_func_t func);

inline RE::TESObjectWEAP* getEquippedWeapon(RE::Actor* actor, bool is_left)
{
    auto weapon = actor->GetEquippedObject(is_left);
    if (weapon && (weapon->GetFormType() == RE::FormType::Weapon))
        return weapon->As<RE::TESObjectWEAP>();
    return nullptr;
}

inline bool is2h(RE::TESObjectWEAP* weapon)
{
    if (!weapon)
        return false;
    auto weap_type = weapon->GetWeaponType();
    return (weap_type == RE::WEAPON_TYPE::kTwoHandSword) || (weap_type == RE::WEAPON_TYPE::kTwoHandAxe);
}

inline bool is2h(RE::Actor* actor)
{
    return is2h(getEquippedWeapon(actor, true)) || is2h(getEquippedWeapon(actor, false));
}

struct EditorIdMaps
{
    static EditorIdMaps* getSingleton()
    {
        static EditorIdMaps edid_maps;
        return &edid_maps;
    }

    std::unordered_map<std::string, RE::TESRace*>    race_map;
    std::unordered_map<std::string, RE::TESFaction*> faction_map;
    std::unordered_map<std::string, RE::BGSKeyword*> keyword_map;

    inline bool checkRace(RE::Actor* actor, nlohmann::json cond_json)
    {
        return orCheck(cond_json, [&](std::string race_edid) {
            return actor->GetRace() && (actor->GetRace() == race_map[race_edid]);
        });
    }

    inline bool checkWeapType(RE::TESObjectWEAP* weapon, nlohmann::json cond_json)
    {
        return orCheck(cond_json, [&](int req_weap_type) {
            return weapon ?
                (req_weap_type < 0) != (static_cast<int>(weapon->GetWeaponType()) == std::abs(req_weap_type)) :
                req_weap_type == 0;
        });
    }

    inline bool checkFaction(RE::Actor* actor, nlohmann::json cond_json)
    {
        return orCheck(cond_json, [&](std::string faction_edid) {
            return actor->IsInFaction(faction_map[faction_edid]);
        });
    }

    inline bool checkKeywords(RE::Actor* actor, nlohmann::json cond_json)
    {
        return (!cond_json.contains("actor") || checkKeyword(actor, cond_json["actor"])) &&
            (!cond_json.contains("race") || checkKeyword(actor->GetRace(), cond_json["race"])) &&
            (!cond_json.contains("weapon_l") || checkKeyword(getEquippedWeapon(actor, true), cond_json["weapon_l"])) &&
            (!cond_json.contains("weapon_r") || checkKeyword(getEquippedWeapon(actor, false), cond_json["weapon_r"]));
    }

    template <class T>
    inline bool checkKeyword(T* subject, nlohmann::json kwd_json)
    {
        return orCheck(kwd_json, [&](std::string keyword_edid) {
            return subject->HasKeyword(keyword_map[keyword_edid]);
        });
    }
};

inline float deg2rad(float deg) { return static_cast<float>(deg / 180.0 * M_PI); }
inline float rad2deg(float rad) { return static_cast<float>(rad * 180.0 / M_PI); }
inline bool  isBetweenAngle(float deg, const float deg_min, const float deg_max) // not actually but enough
{
    while (deg <= deg_min)
        deg += 360;
    return deg <= deg_max;
}

inline float magnitude(RE::NiPoint2 vec) { return std::sqrt(vec.x * vec.x + vec.y * vec.y); }
inline float theta(RE::NiPoint2 vec) { return std::atan2(vec.y, vec.x); }

inline void reposition(RE::Actor* victim, RE::Actor* attacker, float dist, float victim_angle_offset, float attacker_angle_offset)
{
    RE::NiPoint3 curr_offset = attacker->GetPosition() - victim->GetPosition();
    RE::NiPoint3 new_offset  = curr_offset * dist / magnitude(RE::NiPoint2(curr_offset.x, curr_offset.y));
    attacker->SetPosition(victim->GetPosition() + new_offset, true);

    float facing_angle = rad2deg(theta(RE::NiPoint2(-curr_offset.x, -curr_offset.y)));
    attacker->SetRotationZ(facing_angle + attacker_angle_offset);
    victim->SetRotationZ(facing_angle + victim_angle_offset);
}

template <class Form>
void getForms(std::unordered_map<std::string, Form*>& id_map)
{
    for ([[maybe_unused]] const auto& [editor_id, value] : id_map)
    {
        auto form = RE::TESForm::LookupByEditorID<Form>(editor_id);
        if (!form)
            logger::warn("EditorID {} doesn't exist", editor_id);
        else
            id_map[editor_id] = form;
    }
}

inline float getDamageMult(bool is_player)
{
    auto              difficulty   = RE::PlayerCharacter::GetSingleton()->difficulty;
    const std::vector diff_str     = {"VE", "E", "N", "H", "VH", "L"};
    auto              setting_name = fmt::format("fDiffMultHP{}PC{}", is_player ? "To" : "By", diff_str[difficulty]);
    auto              setting      = RE::GameSettingCollection::GetSingleton()->GetSetting(setting_name.c_str());
    return setting->data.f;
}

inline bool isProtected(RE::Actor* actor)
{
    return actor->boolFlags.all(RE::Actor::BOOL_FLAGS::kProtected);
}

inline bool isParalyzed(RE::Actor* actor)
{
    return actor->HasEffectWithArchetype(RE::MagicTarget::Archetype::kParalysis);
}

inline bool isInRagdoll(RE::Actor* actor)
{
    return actor->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal;
}

// Lazy condition workarounds
bool isDetectedBy(RE::Actor* subject, RE::Actor* target);
bool isLastHostileActor(RE::Actor* subject, RE::Actor* target);
bool isInPairedAnimation(RE::Actor* actor);
} // namespace phkm