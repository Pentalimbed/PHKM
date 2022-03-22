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
    void update();

    std::vector<std::pair<double, std::function<void()>>> delayed_funcs;
    std::mutex                                            delayed_funcs_mutex;

private:
    void bugFixes(RE::Actor* attacker, RE::Actor* victim);
    bool checkActors(RE::Actor* attacker, RE::Actor* victim);
    bool isValid(RE::Actor* actor);
    bool canExecute(RE::Actor* victim);
    bool canTrigger(RE::Actor* attacker, RE::Actor* victim, bool do_exec, float total_damage);
    void filterEntries(std::unordered_map<std::string, AnimEntry>& entries, RE::Actor* attacker, RE::Actor* victim, RE::HitData& hit_data, bool do_exec);
    void prepareForKillmove(RE::Actor* actor);
};
} // namespace phkm