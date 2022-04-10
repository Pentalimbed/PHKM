#include "files.h"

#include <fstream>

#include "hooks.h"
#include "utils.h"
#include "events.h"

namespace fs = std::filesystem;

const fs::path plugin_dir  = "Data\\SKSE\\Plugins";
const fs::path config_path = "PHKM.json";
const fs::path key_path    = "PHKM\\keys.json";
const fs::path entries_dir = "PHKM\\entries";
const fs::path entries_ext = ".json";

namespace phkm
{
////////////////////////Config

void PhkmConfig::readConfig()
{
    fs::path      complete_config_path = plugin_dir / config_path;
    std::ifstream config_file(complete_config_path);
    if (!config_file.is_open())
    {
        logger::error("Failed to open PHKM.json!");
        return;
    }

    nlohmann::json config_json;
    try
    {
        config_file >> config_json;
        config_json.get_to(*this);
    }
    catch (nlohmann::detail::exception e)
    {
        logger::error("Faile to parse PHKM.json.\n\tError: {}", e.what());
    }
}

void PhkmConfig::saveConfig()
{
    fs::path      complete_config_path = plugin_dir / config_path;
    std::ofstream config_file(complete_config_path);
    if (!config_file.is_open())
    {
        logger::error("Failed to open PHKM.json!");
        return;
    }

    config_file << nlohmann::json(*this).dump(4);
}

void to_json(nlohmann::json& j, const PhkmConfig& config)
{
    j = nlohmann::json{{"killmove", {
                                        {"player_to_npc", config.p_km_player2npc},
                                        {"npc_to_player", config.p_km_npc2player},
                                        {"npc_to_npc", config.p_km_npc2npc},
                                        {"last_enemy", config.last_enemy_km},
                                    }},
                       {"execution", {
                                         {"player_to_npc", config.exec_player2npc},
                                         {"npc_to_player", config.exec_npc2player},
                                         {"npc_to_npc", config.exec_npc2npc},
                                         {"bleedout", config.enable_bleedout_exec},
                                         {"ragdoll", config.enable_ragdoll_exec},
                                         {"ragdoll_animated", config.animated_ragdoll_exec},
                                         {"last_enemy", config.last_enemy_exec},
                                     }},
                       {"misc", {
                                    {"essential_protection", config.essential_protect},
                                    {"decap_perk_req", config.decap_perk_req},
                                }}};
}

void from_json(const nlohmann::json& j, PhkmConfig& config)
{
    auto km_json = j.at("killmove");
    tryGet(config.p_km_player2npc, km_json, "player_to_npc");
    tryGet(config.p_km_npc2player, km_json, "npc_to_player");
    tryGet(config.p_km_npc2npc, km_json, "npc_to_npc");
    tryGet(config.last_enemy_km, km_json, "last_enemy");

    auto exec_json = j.at("execution");
    tryGet(config.exec_player2npc, exec_json, "player_to_npc");
    tryGet(config.exec_npc2player, exec_json, "npc_to_player");
    tryGet(config.exec_npc2npc, exec_json, "npc_to_npc");
    tryGet(config.enable_bleedout_exec, exec_json, "bleedout");
    tryGet(config.enable_ragdoll_exec, exec_json, "ragdoll");
    tryGet(config.animated_ragdoll_exec, exec_json, "ragdoll_animated");
    tryGet(config.last_enemy_exec, exec_json, "last_enemy");

    auto misc_json = j.at("misc");
    tryGet(config.essential_protect, misc_json, "essential_protection");
    tryGet(config.decap_perk_req, misc_json, "decap_perk_req");
}

////////////////////////AnimEntry

void AnimEntry::play(RE::Actor* attacker, RE::Actor* victim) const
{
    logger::debug("Now playing {}", name);
    playPairedIdle(attacker->currentProcess, attacker, RE::DEFAULT_OBJECT::kActionIdle, idle_form, true, false, victim);
}

////////////////////////KeyEntry

void to_json(nlohmann::json& j, const KeyEntry& entry)
{
    j = nlohmann::json{
        {"name", entry.name},
        {"key", entry.key},
        {"anims", entry.anims},
        {"level_diff", entry.level_diff},
        {"stamina_cost", entry.stamina_cost},
        {"enemy_hp", entry.enemy_hp},
        {"sound", entry.trigger_sound},
    };
}
void from_json(const nlohmann::json& j, KeyEntry& entry)
{
    tryGet(entry.name, j, "name");
    tryGet(entry.key, j, "key");
    tryGet(entry.anims, j, "anims");
    tryGet(entry.level_diff, j, "level_diff");
    tryGet(entry.stamina_cost, j, "stamina_cost");
    tryGet(entry.enemy_hp, j, "enemy_hp");
    tryGet(entry.trigger_sound, j, "sound");
}

////////////////////////AnimEntryParser

std::optional<AnimEntry> EntryParser::parseAnimEntry(const nlohmann::json& j, const std::string name)
{
    AnimEntry entry;

    entry.name = name;
    if (!j.contains(name))
    {
        logger::critical("Entry {} doesn't exist. Why is this called? Please report to author.", name);
        return std::nullopt;
    }

    auto entry_json = j[name];

    // Get bases
    if (entry_json.contains("base"))
    {
        if (!j.contains("bases"))
        {
            logger::warn("Entry {} requires base but there's none!", name);
            return std::nullopt;
        }

        nlohmann::json temp_json = {};
        for (const auto base_name : entry_json["base"])
        {
            try
            {
                temp_json.merge_patch(j["bases"].at(base_name.get<std::string>()));
            }
            catch (nlohmann::json::out_of_range e)
            {
                logger::warn("Entry {} requires base {} which doesn't exist!", name, base_name.get<std::string>());
                return std::nullopt;
            }
        }
        temp_json.merge_patch(entry_json);
        entry_json = temp_json;
    }

    // Get idleform
    auto result = RE::TESForm::LookupByEditorID<RE::TESIdleForm>(name);
    if (!result)
    {
        logger::warn("Entry {} has no corresponding IdleForm!", name);
        return std::nullopt;
    }
    entry.idle_form = result;

    // Store conditions
    if (entry_json.contains("conditions"))
    {
        auto cond_json = entry_json["conditions"];
        if (cond_json.contains("attacker"))
            entry.attacker_conds = cond_json["attacker"];
        if (cond_json.contains("victim"))
            entry.victim_conds = cond_json["victim"];

        // Misc conditions
        if (cond_json.contains("misc"))
        {
            auto misc_cond_json = cond_json["misc"];
            try
            {
                misc_cond_json.at("is_execution").get_to(entry.misc_conds.is_execution);
                misc_cond_json.at("decap").get_to(entry.misc_conds.is_decap);
                misc_cond_json.at("sneak").get_to(entry.misc_conds.is_sneak);
                misc_cond_json.at("min_angle").get_to(entry.misc_conds.min_angle);
                misc_cond_json.at("max_angle").get_to(entry.misc_conds.max_angle);
                while (entry.misc_conds.min_angle < -90)
                {
                    entry.misc_conds.min_angle += 360;
                    entry.misc_conds.max_angle += 360;
                }
            }
            catch (nlohmann::json::out_of_range e)
            {
                logger::warn("Entry {} has incomplete misc info! Error: {}", name, e.what());
                return std::nullopt;
            }
        }
    }

    logger::info("Entry {} successfully parsed.", name);
    return entry;
}

std::unordered_map<std::string, AnimEntry> EntryParser::readSingleFile(fs::path file_path)
{
    std::unordered_map<std::string, AnimEntry> local_entries;

    auto          filename = file_path.filename();
    std::ifstream entry_file(file_path);
    if (!entry_file.is_open())
    {
        logger::warn("Failed to open enrty file {}!", filename.string());
        return local_entries;
    }

    logger::info("Reading {}:", filename.string());
    nlohmann::json j;
    try
    {
        entry_file >> j;
    }
    catch (nlohmann::detail::exception e)
    {
        logger::error("Failed to parse {}.\n\tError: {}", file_path.string(), e.what());
        return local_entries;
    }

    // Parse entries
    for (nlohmann::json::iterator it = j.begin(); it != j.end(); ++it)
    {
        if (it.key() != "bases")
        {
            auto parsed_entry = parseAnimEntry(j, it.key());
            if (parsed_entry.has_value())
            {
                local_entries[it.key()] = parsed_entry.value();
            }
        }
    }

    logger::info("{} parsed. Entries count: {}", filename.string(), local_entries.size());
    return local_entries;
}

void EntryParser::readAnimEntries()
{
    const auto complete_entries_dir = plugin_dir / entries_dir;
    if (!fs::exists(complete_entries_dir))
    {
        logger::error("Entries folder \"PHKM\" doesn't exist!");
        return;
    }

    for (auto const& dir_entry : fs::directory_iterator{complete_entries_dir})
    {
        if (dir_entry.is_regular_file() && dir_entry.path().extension() == entries_ext)
        {
            try
            {
                auto local_entries = readSingleFile(dir_entry);
                entries.merge(local_entries);
            }
            catch (nlohmann::detail::exception e)
            {
                logger::error("Failed to parse {}.\n\tError: {}", dir_entry.path().filename().string(), e.what());
            }
        }
    }

    if (entries.empty())
    {
        logger::warn("No entries imported.");
        return;
    }

    cacheForms();

    logger::info("Entry processing completed.");
}

void EntryParser::readKeyEntries()
{
    fs::path      complete_path = plugin_dir / key_path;
    std::ifstream entry_file(complete_path);
    if (!entry_file.is_open())
    {
        logger::error("Failed to open {}!", complete_path.string());
        return;
    }

    nlohmann::json j;
    try
    {
        entry_file >> j;
    }
    catch (nlohmann::detail::exception e)
    {
        logger::error("Failed to parse {}.\n\tError: {}", complete_path.string(), e.what());
        return;
    }
    tryGet(keys, j, "key_entries");
}

void EntryParser::writeKeyEntries()
{
    fs::path      complete_path = plugin_dir / key_path;
    std::ofstream entry_file(complete_path);
    if (!entry_file.is_open())
    {
        logger::error("Failed to open {}!", complete_path.string());
        return;
    }

    entry_file << nlohmann::json{{"key_entries", keys}}.dump(4);
}

void EntryParser::cacheForms()
{
    logger::info("{} entries read. Now caching forms.", entries.size());

    auto edid_maps = EditorIdMaps::getSingleton();
    // extracting all required editorids
    for (const auto& [name, entry] : entries)
    {
        for (const auto& actor_conds : {entry.attacker_conds, entry.victim_conds})
        {
            // races
            if (actor_conds.contains("race") && !actor_conds["race"].empty())
                for (auto& it : actor_conds["race"].flatten())
                    edid_maps->race_map[it.get<std::string>()] = nullptr;
            // faction
            if (actor_conds.contains("faction") && !actor_conds["faction"].empty())
                for (auto& it : actor_conds["faction"].flatten())
                    edid_maps->faction_map[it.get<std::string>()] = nullptr;
            // keywords
            if (actor_conds.contains("keyword") && !actor_conds["keyword"].empty())
                for (auto& it : actor_conds["keyword"].flatten())
                    edid_maps->keyword_map[it.get<std::string>()] = nullptr;
        }
    }

    std::string keywords;
    for ([[maybe_unused]] const auto& [key, val] : edid_maps->keyword_map)
        keywords += key + " ";
    logger::debug("All keywords: {}", keywords);

    getForms(edid_maps->race_map);
    getForms(edid_maps->faction_map);
    getForms(edid_maps->keyword_map);
}

} // namespace phkm
