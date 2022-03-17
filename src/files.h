#pragma once

#include <filesystem>
#include <variant>

#include "../extern/json.hpp"

namespace phkm
{
	class ConfigParser
	{
	public:
		static ConfigParser* getSingleton()
		{
			static ConfigParser parser;
			return &parser;
		}

		// OPTIONS
		// No, I'm not writing getters
		bool enable_km;         // enable post-hit killmoves
		float km_chance;        // killmove chance
		bool enable_player_km;  // allow player to be killmove'd
		bool enable_npc_km;     // allow npc to killmove each other
		float npc_km_chance;    // chance of npc doing killmoves
		bool last_enemy_km;     // only killmoves if it's the last enemy

		bool enable_exec;            // enable prone execution
		bool enable_bleedout_exec;   // allow executing bleedout target
		bool enable_ragdoll_exec;    // allow executing ragdolled target
		bool enable_paralysis_exec;  // allow executing paralyzed target
		bool enable_player_exec;     // allow player to be executed
		bool enable_npc_exec;        // allow npc to execute each other
		bool last_enemy_exec;        // only executes if it's the last enemy

		bool enable_unpaired;  // allow unpaired animations
		bool enable_repos;     // repositioning actors when
		bool force_3rd;        // force switching to 3rd person camera when km happens

		bool essential_protect;  // prevent essentials from getting km'd (and maybe lost their head in the process)
		bool decap_perk_req;     // decap animation needs perk to trigger

		bool readConfig();
		inline void disable() { enable_km = enable_exec = false; }
		inline bool isEnabled() { return enable_km || enable_exec; }

	private:
		bool loadParams(const nlohmann::json& j);
	};

	struct AnimEntry
	{
		std::string name;

		bool is_paired;
		struct PairedInfo
		{
			RE::TESIdleForm* idle_form;
		};
		struct UnpairedInfo
		{
			float dist = 10.0;
			float attacker_offset_angle = 0.0;
			float victim_offset_angle = 0.0;
		};
		std::variant<PairedInfo, UnpairedInfo> anim_info;

		// These are flexible
		// Actor conditions: keyword of (weapon/actor/race), faction, race, sneaking, weaptype
		nlohmann::json attacker_conds = {};
		nlohmann::json victim_conds = {};

		// Misc conditions: is_execution, angles
		struct MiscCond
		{
			bool is_execution = false;
			bool is_decap = false;
			bool is_sneak = false;
			float min_angle = 90.0;
			float max_angle = 90.0;
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
		std::optional<AnimEntry> parseEntry(const nlohmann::json& j, const std::string name);
		std::unordered_map<std::string, AnimEntry> readSingleFile(std::filesystem::path file_path);
		void cacheForms();
	};

}  // namespace phkm