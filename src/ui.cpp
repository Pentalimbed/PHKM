#include "ui.h"
#include "files.h"
#include "utils.h"

#include <imgui.h>
#include "extern/imgui_stdlib.h"

#ifndef UNICODE
#    define UNICODE 1
#endif
#include <dinput.h>

namespace phkm
{
std::string scanCode2String(uint32_t scancode)
{
    if (scancode >= kGamepadOffset)
    {
        // not implemented
        return "";
    }
    else if (scancode >= kMouseOffset)
    {
        auto key = scancode - kMouseOffset;
        switch (key)
        {
            case 0:
                return "Left Mouse Button";
            case 1:
                return "Right Mouse Button";
            case 2:
                return "Middle Mouse Button";
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
                return fmt::format("Mouse Button {}", key);
            case 8:
                return "Wheel Up";
            case 9:
                return "Wheel Down";
            default:
                return "";
        }
    }
    else
    {
        TCHAR lpszName[256];
        if (GetKeyNameText(scancode << 16, lpszName, sizeof(lpszName)))
        {
            int         size_needed = WideCharToMultiByte(CP_UTF8, 0, lpszName, wcslen(lpszName), NULL, 0, NULL, NULL);
            std::string key_str(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, lpszName, wcslen(lpszName), &key_str[0], size_needed, NULL, NULL);
            return key_str;
        }
        else
        {
            return "";
        }
    }
}

void drawMenu()
{
    auto config = PhkmConfig::getSingleton();

    if (ImGui::BeginTabBar("PHKM", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("General"))
        {
            if (ImGui::Button("Save Config"))
                config->saveConfig();

            if (ImGui::TreeNode("Killmove"))
            {
                ImGui::SliderFloat("Player -> NPC", &config->p_km_player2npc, 0, 1, "%.2f");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Chance of player killmoving NPCs.");
                ImGui::SliderFloat("NPC -> Player", &config->p_km_npc2player, 0, 1, "%.2f");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Chance of NPCs killmoving player.");
                ImGui::SliderFloat("NPC -> NPC", &config->p_km_npc2npc, 0, 1, "%.2f");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Chance of NPCs killmoving NPCs.");
                ImGui::Checkbox("Last Enemy Only", &config->last_enemy_km);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Only triggers killmove when target is the last hostile actor.");

                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Execution"))
            {
                ImGui::Checkbox("Player -> NPC", &config->exec_player2npc);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Enable player executing NPCs.");
                ImGui::SameLine();
                ImGui::Checkbox("NPC -> Player", &config->exec_npc2player);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Enable NPCs executing player.");
                ImGui::SameLine();
                ImGui::Checkbox("NPC -> NPC", &config->exec_npc2npc);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Enable NPCs executing NPCs.");
                ImGui::Checkbox("Bleedout Execution", &config->enable_bleedout_exec);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Enable executing bleedout target.");
                ImGui::SameLine();
                ImGui::Checkbox("Ragdoll Execution", &config->enable_ragdoll_exec);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Enable executing ragdolled target.");
                ImGui::SameLine();
                ImGui::Checkbox("Animated Ragdoll Execution", &config->animated_ragdoll_exec);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Plays animation when executing ragdolled target. Slight chance of glitching.");
                ImGui::Checkbox("Last Enemy Only", &config->last_enemy_exec);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Only triggers execution when target is the last hostile actor.");

                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Misc"))
            {
                ImGui::Checkbox("Essential Protection", &config->essential_protect);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Prevent essetial npcs from getting executed/killmoved.");

                ImGui::Checkbox("Decap Perks", &config->decap_perk_req);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Decapitation requires corresponding perks.");

                ImGui::TreePop();
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Anim Entry List"))
        {
            std::string all_anim_entry = "";
            for (const auto& [name, entry] : EntryParser::getSingleton()->entries)
                all_anim_entry += name + '\n';
            ImGui::InputTextMultiline("", &all_anim_entry);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Key Trigger"))
        {
            auto entry_parser = EntryParser::getSingleton();
            if (ImGui::Button("Save Key Entries"))
                entry_parser->writeKeyEntries();
            if (ImGui::Button("+"))
                entry_parser->keys.push_back(KeyEntry());

            auto del_entry_iter = entry_parser->keys.end();
            for (auto key_entry_iter = entry_parser->keys.begin(); key_entry_iter != entry_parser->keys.end(); ++key_entry_iter)
            {
                auto& key_entry = *key_entry_iter;
                if (ImGui::TreeNode(fmt::format("{}", key_entry_iter - entry_parser->keys.begin()).c_str()))
                {
                    ImGui::Separator();

                    if (ImGui::Button("-"))
                        del_entry_iter = key_entry_iter;

                    ImGui::InputText("Name", &key_entry.name);
                    ImGui::InputInt("Key - ", &key_entry.key), ImGui::SameLine();
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("DX Scancode of the key");
                    ImGui::Text(scanCode2String(key_entry.key).c_str());

                    ImGui::Separator();

                    ImGui::InputFloat("Stamina Cost", &key_entry.stamina_cost, 0.01f, 10, "%.2f");
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip(">1 - Amount, <=1 - Proportion");
                    ImGui::InputFloat("Max Target Health", &key_entry.enemy_hp, 0.01f, 10, "%.2f");
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip(">1 - Amount, <=1 - Proportion");
                    ImGui::SliderInt("Min Level Difference", &key_entry.level_diff, 0, 100);
                    ImGui::InputText("Trigger Sound", &key_entry.trigger_sound);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Sound played when a target was selected");

                    ImGui::Separator();

                    if (ImGui::TreeNode("Anim List"))
                    {
                        if (ImGui::Button("+"))
                            key_entry.anims.push_back("");
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Leaving this empty includes ALL animations.");
                        ImGui::SameLine();
                        if (ImGui::Button("-"))
                            key_entry.anims.pop_back();
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Leaving this empty includes ALL animations.");
                        for (auto anim_iter = key_entry.anims.begin(); anim_iter != key_entry.anims.end(); ++anim_iter)
                        {
                            auto& anim = *anim_iter;
                            ImGui::InputText(fmt::format("{}", anim_iter - key_entry.anims.begin()).c_str(), &anim);
                        }

                        ImGui::TreePop();
                    }

                    ImGui::Separator();
                    ImGui::TreePop();
                }
            }

            if (del_entry_iter != entry_parser->keys.end())
                entry_parser->keys.erase(del_entry_iter);

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}
} // namespace phkm