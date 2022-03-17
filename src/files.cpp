#include "files.h"

#include <fstream>

#include "hooks.h"
#include "utils.h"

namespace phkm
{
	namespace fs = std::filesystem;

	const fs::path plugin_dir = "Data\\SKSE\\Plugins";
	const fs::path config_path = "PHKM.json";
	const fs::path entries_dir = "PHKM";
	const fs::path entries_ext = ".json";
	const std::vector kwd_types = { "weapon_l", "weapon_r", "actor", "race" };

	const std::string attacker_suffix = "_a";
	const std::string victim_suffix = "_v";

#define GET_TO(_j, _key, _target, _return)     \
	try {                                      \
		_j.at(_key).get_to(_target);           \
	} catch (nlohmann::json::out_of_range e) { \
		return _return;                        \
	}

	////////////////////////ConfigParser

	bool ConfigParser::readConfig()
	{
		fs::path complete_config_path = plugin_dir / config_path;
		std::ifstream config_file(complete_config_path);
		if (!config_file.is_open()) {
			logger::error("Failed to open PHKM.json!");
			return false;
		}

		nlohmann::json config_json;
		config_file >> config_json;

		if (!loadParams(config_json)) {
			enable_km = enable_exec = false;
			return false;
		}

		config_file.close();
		return true;
	}

	bool ConfigParser::loadParams(const nlohmann::json& j)
	{
		try {
			auto km_json = j.at("killmove");
			km_json.at("enabled").get_to(enable_km);
			km_json.at("killmove_chance").get_to(km_chance);
			km_json.at("killmove_on_player").get_to(enable_player_km);
			km_json.at("npc_killmove").get_to(enable_npc_km);
			km_json.at("npc_killmove_chance").get_to(npc_km_chance);
			km_json.at("last_enemy_restriction").get_to(last_enemy_km);

			auto exec_json = j.at("execution");
			exec_json.at("enabled").get_to(enable_exec);
			exec_json.at("bleedout_execution").get_to(enable_bleedout_exec);
			exec_json.at("ragdoll_execution").get_to(enable_ragdoll_exec);
			exec_json.at("execution_on_player").get_to(enable_player_exec);
			exec_json.at("npc_execution").get_to(enable_npc_exec);
			exec_json.at("last_enemy_restriction").get_to(last_enemy_exec);

			auto unpaired_json = j.at("unpaired");
			unpaired_json.at("enabled").get_to(enable_unpaired);

			auto misc_json = j.at("misc");
			misc_json.at("essential_protection").get_to(essential_protect);
			misc_json.at("decap_perk_requirement").get_to(decap_perk_req);
		} catch (std::exception e) {
			logger::error("Failed to parse PHKM.json! Error: {}", e.what());
			return false;
		}

		return true;
	}

	////////////////////////AnimEntry

	void AnimEntry::play(RE::Actor* attacker, RE::Actor* victim) const
	{
		logger::debug("Now playing {}", name);
		if (is_paired) {
			const auto& paired_info = std::get<PairedInfo>(anim_info);
			playPairedIdle(attacker->currentProcess, attacker, RE::DEFAULT_OBJECT::kActionIdle, paired_info.idle_form, true, false, victim);
		} else {
			const auto& unpaired_info = std::get<UnpairedInfo>(anim_info);
			reposition(victim, attacker, unpaired_info.dist, unpaired_info.victim_offset_angle, unpaired_info.attacker_offset_angle);
			attacker->NotifyAnimationGraph(name + attacker_suffix);
			victim->NotifyAnimationGraph(name + victim_suffix);
		}
	}

	////////////////////////AnimEntryParser

	std::optional<AnimEntry> AnimEntryParser::parseEntry(const nlohmann::json& j, const std::string name)
	{
		AnimEntry entry;

		entry.name = name;
		if (!j.contains(name)) {
			logger::critical("Entry {} doesn't exist. Why is this called? Please report to author.", name);
			return std::nullopt;
		}

		auto entry_json = j[name];

		// Get bases
		if (entry_json.contains("base")) {
			if (!j.contains("bases")) {
				logger::warn("Entry {} requires base but there's none!", name);
				return std::nullopt;
			}

			nlohmann::json temp_json = {};
			for (const auto base_name : entry_json["base"]) {
				try {
					temp_json.merge_patch(j["bases"].at(base_name.get<std::string>()));
				} catch (nlohmann::json::out_of_range e) {
					logger::warn("Entry {} requires base {} which doesn't exist!", name, base_name.get<std::string>());
					return std::nullopt;
				}
			}
			temp_json.merge_patch(entry_json);
			entry_json = temp_json;
		}

		// Parse anim info
		if (entry_json.contains("unpaired_anim_info")) {
			AnimEntry::UnpairedInfo anim_info;
			auto anim_info_json = entry_json["unpaired_anim_info"];

			entry.is_paired = false;

			try {
				anim_info_json.at("dist").get_to(anim_info.dist);
				anim_info_json.at("attacker_offset_angle").get_to(anim_info.attacker_offset_angle);
				anim_info_json.at("victim_offset_angle").get_to(anim_info.victim_offset_angle);
			} catch (nlohmann::json::out_of_range e) {
				logger::warn("Entry {} has incomplete anim info! Error: {}", name, e.what());
				return std::nullopt;
			}

			entry.anim_info = anim_info;
		} else {
			AnimEntry::PairedInfo anim_info;
			auto anim_info_json = entry_json["paired_anim_info"];

			entry.is_paired = true;

			auto result = RE::TESForm::LookupByEditorID<RE::TESIdleForm>(name);
			if (!result) {
				logger::warn("Entry {} has no corresponding IdleForm!", name);
				return std::nullopt;
			}
			anim_info.idle_form = result;

			entry.anim_info = anim_info;
		}

		// Store conditions
		if (entry_json.contains("conditions")) {
			auto cond_json = entry_json["conditions"];
			if (cond_json.contains("attacker"))
				entry.attacker_conds = cond_json["attacker"];
			if (cond_json.contains("victim"))
				entry.victim_conds = cond_json["victim"];

			// Misc conditions
			if (cond_json.contains("misc")) {
				auto misc_cond_json = cond_json["misc"];
				try {
					misc_cond_json.at("is_execution").get_to(entry.misc_conds.is_execution);
					misc_cond_json.at("decap").get_to(entry.misc_conds.is_decap);
					misc_cond_json.at("sneak").get_to(entry.misc_conds.is_sneak);
					misc_cond_json.at("min_angle").get_to(entry.misc_conds.min_angle);
					misc_cond_json.at("max_angle").get_to(entry.misc_conds.max_angle);
				} catch (nlohmann::json::out_of_range e) {
					logger::warn("Entry {} has incomplete misc info! Error: {}", name, e.what());
					return std::nullopt;
				}
			}
		}

		logger::info("Entry {} successfully parsed.", name);
		return entry;
	}

	std::unordered_map<std::string, AnimEntry> AnimEntryParser::readSingleFile(fs::path file_path)
	{
		std::unordered_map<std::string, AnimEntry> local_entries;

		auto filename = file_path.filename();
		std::ifstream entry_file(file_path);
		if (!entry_file.is_open()) {
			logger::warn("Failed to open enrty file {}!", filename.string());
			return local_entries;
		}

		logger::info("Reading {}:", filename.string());
		nlohmann::json j;
		entry_file >> j;

		// Parse entries
		for (nlohmann::json::iterator it = j.begin(); it != j.end(); ++it) {
			if (it.key() != "bases") {
				auto parsed_entry = parseEntry(j, it.key());
				if (parsed_entry.has_value()) {
					local_entries[it.key()] = parsed_entry.value();
				}
			}
		}

		logger::info("{} parsed. Entries count: {}", filename.string(), local_entries.size());
		return local_entries;
	}

	void AnimEntryParser::readEntries()
	{
		const auto complete_entries_dir = plugin_dir / entries_dir;
		if (!fs::exists(complete_entries_dir)) {
			logger::error("Entries folder \"PHKM\" doesn't exist! Mod disabled.");
			ConfigParser::getSingleton()->disable();
			return;
		}

		for (auto const& dir_entry : fs::directory_iterator{ complete_entries_dir }) {
			if (dir_entry.is_regular_file() && dir_entry.path().extension() == entries_ext) {
				try {
					auto local_entries = readSingleFile(dir_entry);
					entries.merge(local_entries);
				} catch (nlohmann::json::out_of_range e) {
					logger::warn("Required items not found reading {}. Error: {}", dir_entry.path().filename().string(), e.what());
				} catch (nlohmann::json::type_error e) {
					logger::warn("Entry {} has incorrect format and cannot be parsed.", dir_entry.path().filename().string());
				}
			}
		}

		if (entries.empty()) {
			logger::warn("No entries imported. Mod disabled.");
			ConfigParser::getSingleton()->disable();
			return;
		}

		cacheForms();

		logger::info("Entry processing completed.");
	}

	void AnimEntryParser::cacheForms()
	{
		logger::info("{} entries read. Now caching forms.", entries.size());

		auto edid_maps = EditorIdMaps::getSingleton();
		// extracting all required editorids
		for (const auto& [name, entry] : entries) {
			for (const auto& actor_conds : { entry.attacker_conds, entry.victim_conds }) {
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

		// remove entries that uses invalid editorid;
		// std::erase_if(entries, [&](const auto& item) {
		//     const auto& entry = item.second;
		//     for (const auto& actor_conds : {entry.attacker_conds, entry.victim_conds})
		//     {
		//         if (actor_conds.contains("race") && !actor_conds["race"].empty())
		//             for (auto& it : actor_conds["race"].flatten())
		//                 if (!edid_maps->race_map[it.get<std::string>()]) return true;
		//         if (actor_conds.contains("faction") && !actor_conds["faction"].empty())
		//             for (auto& it : actor_conds["faction"].flatten())
		//                 if (!edid_maps->faction_map[it.get<std::string>()]) return true;
		//         if (actor_conds.contains("keyword") && !actor_conds["keyword"].empty())
		//             for (auto& it : actor_conds["keyword"].flatten())
		//                 if (!edid_maps->keyword_map[it.get<std::string>()]) return true;
		//     }
		//     return false;
		// });
		// **** On second thought, better not 'cause there're mods that adds variant races etc.
	}

}  // namespace phkm
