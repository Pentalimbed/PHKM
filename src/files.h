#pragma once

#include <filesystem>
#include <variant>
#include <thread>

#include "nlohmann/json.hpp"

namespace phkm
{
struct PhkmConfig
{
    static PhkmConfig* getSingleton()
    {
        static PhkmConfig parser;
        return &parser;
    }

    // OPTIONS
    float p_km_player2npc = 1.0;  // chance of player km npc
    float p_km_npc2player = 1.0;  // chance of npc km player
    float p_km_npc2npc    = 1.0;  //chance of npc km npc
    bool  last_enemy_km   = true; // only killmoves if it's the last enemy

    bool exec_player2npc       = true;  // enable prone execution
    bool exec_npc2player       = true;  // allow player to be executed
    bool exec_npc2npc          = false; // allow npc to execute each other
    bool enable_bleedout_exec  = true;  // allow executing bleedout target
    bool enable_ragdoll_exec   = true;  // allow executing ragdolled target
    bool animated_ragdoll_exec = true;  // play animation for ragdoll execution
    bool last_enemy_exec       = false; // only executes if it's the last enemy

    bool essential_protect = true; // prevent essentials from getting km'd (and maybe lost their head in the process)
    bool decap_perk_req    = true; // decap animation needs perk to trigger

    void readConfig();
    void saveConfig();
};

void to_json(nlohmann::json& j, const PhkmConfig& config);
void from_json(const nlohmann::json& j, PhkmConfig& config);

struct AnimEntry
{
    std::string      name;
    RE::TESIdleForm* idle_form;

    // Actor conditions: keyword of (weapon/actor/race), faction, race, sneaking, weaptype
    nlohmann::json attacker_conds = {};
    nlohmann::json victim_conds   = {};

    // Misc conditions: is_execution, angles
    struct MiscCond
    {
        bool  is_execution = false;
        bool  is_decap     = false;
        bool  is_sneak     = false;
        float min_angle    = 90.0;
        float max_angle    = 90.0;
    };
    MiscCond misc_conds;

    // Play the entry
    void play(RE::Actor* attacker, RE::Actor* victim) const;
};

class AnimEntryParser
{
public:
    static AnimEntryParser* getSingleton()
    {
        static AnimEntryParser parser;
        return &parser;
    }

    std::unordered_map<std::string, AnimEntry> entries;

    void readEntries();

private:
    std::optional<AnimEntry>                   parseEntry(const nlohmann::json& j, const std::string name);
    std::unordered_map<std::string, AnimEntry> readSingleFile(std::filesystem::path file_path);
    void                                       cacheForms();
};

} // namespace phkm