#include "ui.h"
#include "files.h"

#include <imgui.h>

namespace phkm
{
void drawMenu()
{
    auto config = PhkmConfig::getSingleton();

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
}
} // namespace phkm