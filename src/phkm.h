#pragma once

#include "files.h"

namespace phkm
{
class DelayedFuncModule
{
public:
    static DelayedFuncModule* getSingleton()
    {
        static DelayedFuncModule module;
        return std::addressof(module);
    }

    inline void addFunc(double countdown, std::function<void()> func)
    {
        funcs_mutex.lock();
        funcs.push_back(std::make_pair(countdown, func));
        funcs_mutex.unlock();
    }
    void update();
    void flush();

private:
    std::vector<std::pair<double, std::function<void()>>> funcs;
    std::mutex                                            funcs_mutex;
};

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
    bool checkActors(RE::Actor* attacker, RE::Actor* victim);
    bool isValid(RE::Actor* actor);
    bool canTrigger(RE::Actor* attacker, RE::Actor* victim, float actual_damage);
    void filterEntries(std::unordered_map<std::string, AnimEntry>& entries, RE::Actor* attacker, RE::Actor* victim, RE::HitData& hit_data, bool play_exec_anims);
    void prepareForKillmove(RE::Actor* actor);
};
} // namespace phkm