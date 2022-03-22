#pragma once
#include <random>

#include "files.h"
#include "utils.h"
#include "phkm.h"

namespace phkm
{
#ifdef BUILD_SE
inline static float* g_delta_time      = (float*)REL::ID(523660).address(); // 2F6B948
inline static float* g_delta_real_time = (float*)REL::ID(523661).address(); // 2F6B94C
#else
inline static float* g_delta_time      = (float*)REL::ID(410199).address(); // 30064C8
inline static float* g_delta_real_time = (float*)REL::ID(410200).address(); // 30064CC
#endif

struct ProcessHitHook
{
    inline static void thunk(RE::Actor* a_victim, RE::HitData& a_hitData)
    {
        if (PostHitModule::getSingleton()->process(a_victim, a_hitData))
            return func(a_victim, a_hitData);
    }
    static inline REL::Relocation<decltype(thunk)> func;

#ifdef BUILD_SE
    static inline uint64_t id     = 37673;
    static inline size_t   offset = 0x3c0;
#else
    static inline uint64_t id     = 38627;
    static inline size_t   offset = 0x4a8;
#endif
};

struct UpdateHook
{
    inline static void thunk()
    {
        PostHitModule::getSingleton()->update();
        func();
    }
    static inline REL::Relocation<decltype(thunk)> func;

#ifdef BUILD_SE
    static inline uint64_t id     = 35551; // 5AF3D0
    static inline size_t   offset = 0x11F;
#else
    static inline uint64_t id     = 36544; // 5D29F0
    static inline size_t   offset = 0x160;
#endif
};

typedef void(_fastcall* _setIsGhost)(RE::Actor* actor, bool isGhost);
#ifdef BUILD_SE
inline static REL::Relocation<_setIsGhost> setIsGhost{REL::ID(36287)}; // 5D25E0
#else
inline static REL::Relocation<_setIsGhost>     setIsGhost{REL::ID(37276)}; // 5F6C60
#endif

typedef void(_fastcall* _playPairedIdle)(RE::AIProcess* proc, RE::Actor* attacker, RE::DEFAULT_OBJECT smth, RE::TESIdleForm* idle, bool a5, bool a6, RE::TESObjectREFR* target);
#ifdef BUILD_SE
inline static REL::Relocation<_playPairedIdle> playPairedIdle{REL::ID(38290)};
#else
inline static REL::Relocation<_playPairedIdle> playPairedIdle{REL::ID(39256)};
#endif

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
