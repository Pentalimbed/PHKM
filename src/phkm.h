#pragma once

#include "files.h"

namespace phkm
{
class PostHitModule
{
public:
    static bool process(RE::Actor* victim, RE::HitData& hit_data);

private:
    static void bugFixes(RE::Actor* attacker, RE::Actor* victim);
    static bool checkActors(RE::Actor* attacker, RE::Actor* victim);
    static bool isValid(RE::Actor* actor);
    static bool canExecute(RE::Actor* victim);
    static bool canTrigger(RE::Actor* attacker, RE::Actor* victim, bool do_exec, float total_damage);
    static void filterEntries(std::unordered_map<std::string, AnimEntry>& entries, RE::Actor* attacker, RE::Actor* victim, RE::HitData& hit_data, bool do_exec);
    static void prepareForKillmove(RE::Actor* actor);
};
} // namespace phkm