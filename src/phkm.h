#pragma once

#include "files.h"

namespace phkm
{
class PostHitModule
{
public:
    static PostHitModule* getSingleton()
    {
        static PostHitModule module;
        return std::addressof(module);
    }

    bool process(RE::Actor* victim, RE::HitData& hit_data);

private:
    void bugFixes(RE::Actor* attacker, RE::Actor* victim);
    bool canTrigger(RE::Actor* attacker, RE::Actor* victim, float actual_damage);
    void prepareForKillmove(RE::Actor* actor);
};
} // namespace phkm