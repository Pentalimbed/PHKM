#pragma once
#include <random>

#include "files.h"
#include "utils.h"

namespace phkm
{
inline bool playPairedIdle(RE::AIProcess* proc, RE::Actor* attacker, RE::DEFAULT_OBJECT smth, RE::TESIdleForm* idle, bool a5, bool a6, RE::TESObjectREFR* target)
{
    using func_t = decltype(&playPairedIdle);
#ifdef BUILD_SE
    REL::Relocation<func_t> func{REL::ID(38290)};
#else
    REL::Relocation<func_t> func{REL::ID(39256)};
#endif
    return func(proc, attacker, smth, idle, a5, a6, target);
}

struct ProcessHitHook
{
    static void                                    thunk(RE::Actor* a_victim, RE::HitData& a_hitData);
    static inline REL::Relocation<decltype(thunk)> func;

#ifdef BUILD_SE
    static inline uint64_t id     = 37673;
    static inline size_t   offset = 0x3c0;
#else
    static inline uint64_t  id     = 38627;
    static inline size_t    offset = 0x4a8;
#endif

    static bool checkActors(RE::Actor* attacker, RE::Actor* victim);
    static bool isValid(RE::Actor* actor);
    static bool canExecute(RE::Actor* victim);
    static bool canTrigger(RE::Actor* attacker, RE::Actor* victim, bool do_exec, float total_damage);
    static void filterEntries(std::unordered_map<std::string, AnimEntry>& entries, RE::Actor* attacker, RE::Actor* victim, bool do_exec);
};

/*
struct HandleDamageVfunc
{
    // hooking RE::Actor::HandleHeathDamage(RE::Actor* a_attacker, float a_damage)
    static void thunk(RE::Actor* a_this, RE::Actor* a_attacker, float a_damage)
    {
        auto orig_func = [&]() {
            return func(a_this, a_attacker, a_damage);
        };

        logger::debug("Calculate health damage!");
        if (!a_attacker)
        {
            logger::debug("No Attacker!");
            return orig_func();
        }

        logger::debug("Attacker: {}; Target: {}", a_attacker->GetName(), a_this->GetName());

        auto health = a_this->GetActorValue(RE::ActorValue::kHealth);
        logger::debug("Damage: {}; Health: {}", a_damage, health);

        return orig_func();
    }
    static inline REL::Relocation<decltype(thunk)> func;
    static inline size_t                           size = 0x104;
};
*/
} // namespace phkm
