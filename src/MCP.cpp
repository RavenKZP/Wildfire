#include "MCP.h"

#include "SKSEMCP/SKSEMenuFramework.hpp"

#include "Settings.h"
#include "Utils.h"
#include "Wildfire/Wildfire.h"
#include "Wildfire/Weather.h"

namespace MCP {

    void Register() {
        if (!SKSEMenuFramework::IsInstalled()) {
            logger::warn("SKSE Menu Framework is not installed. Cannot register menu.");
            return;
        }
        SKSEMenuFramework::SetSection("Wildfire");
        SKSEMenuFramework::AddSectionItem("Settings", RenderSettings);


#ifndef NDEBUG
        SKSEMenuFramework::AddSectionItem("Burning Cells Gird", RenderBurningGird);
        SKSEMenuFramework::AddSectionItem("Burning Refs", RenderBurningRefs);
        SKSEMenuFramework::AddSectionItem("Debug Options", RenderDebugTools);
        SKSEMenuFramework::AddSectionItem("Log", RenderLog);
#endif

        logger::info("SKSE Menu Framework registered.");
    }

    static void HelpMarker(const char* desc) {
        ImGuiMCP::TextDisabled("(?)");
        if (ImGuiMCP::IsItemHovered()) ImGuiMCP::SetTooltip("%s", desc);
    }

    void __stdcall RenderSettings() {
        static auto* set = Settings::GetSingleton();

        // =========================
        // General
        // =========================
        if (ImGuiMCP::CollapsingHeader("General", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGuiMCP::Checkbox("Mod Active", &set->ModActive);
            ImGuiMCP::SameLine();
            HelpMarker("Master kill switch for the wildfire system.");

            ImGuiMCP::Checkbox("Debug Mode", &set->DebugMode);
            ImGuiMCP::SameLine();
            HelpMarker("Enables debug visualization");
        }

        // =========================
        // Debug Visuals
        // =========================
        if (ImGuiMCP::CollapsingHeader("Debug Visuals", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGuiMCP::Checkbox("Draw Impacts", &set->DrawImpacts);
            ImGuiMCP::SameLine();
            HelpMarker("Draws markers where projectiles impact and transfer heat.");
            
            ImGuiMCP::Checkbox("Draw Landscape", &set->DrawLandscape);
            ImGuiMCP::SameLine();
            HelpMarker("Visualizes grass vertices that participate in the fire simulation.");

            ImGuiMCP::Checkbox("Draw Bounding Boxes", &set->DrawBoundBoxes);
            ImGuiMCP::SameLine();
            HelpMarker("Draws bounding boxes around simulated objects (trees, refs).");
            

            ImGuiMCP::Checkbox("Draw Hazards", &set->DrawHazards);
            ImGuiMCP::SameLine();
            HelpMarker("Shows spawned fire hazards in the world.");

            ImGuiMCP::SliderInt("Debug Draw Duration (ms)", &set->DebugDrawDuration, 1, 10000);
            ImGuiMCP::SameLine();
            HelpMarker("How long debug shapes remain visible.");
        }

        // =========================
        // Update Timers
        // =========================
        if (ImGuiMCP::CollapsingHeader("Simulation Update Timers", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGuiMCP::SliderFloat("Grass Update Interval (s)", &set->GrassPeriodicUpdateTime, 0.1f, 10.0f, "%.2f");
            ImGuiMCP::SameLine();
            HelpMarker("How often grass fire simulation updates. - 0 means as fast as possible");

            if (ImGuiMCP::SliderFloat("Hazard Update Interval (s)", &set->HazardPeriodicUpdateTime, 0.1f, 10.0f,
                                      "%.2f")) {
                WildfireMgr::GetSingleton()->SetHazardsLifetine(set->HazardPeriodicUpdateTime * 1.1f);
            }
            ImGuiMCP::SameLine();
            HelpMarker("How often hazards are updated.");

            ImGuiMCP::SliderFloat("Reference Update Interval (s)", &set->RefPeriodicUpdateTime, 0.1f, 20.0f, "%.2f");
            ImGuiMCP::SameLine();
            HelpMarker("Update frequency for trees and other world references.");
        }

        // =========================
        // Grass Settings
        // =========================
        if (ImGuiMCP::CollapsingHeader("Grass Simulation", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGuiMCP::SliderFloat("Grass Ignition Heat", &set->DefaultGrassIgnitionHeat, 0.0f, 100.0f, "%.1f");
            ImGuiMCP::SetTooltip("Minimum heat required for grass to ignite.");

            ImGuiMCP::SliderFloat("Grass Initial Fuel", &set->DefaultGrassFuel, 0.0f, 200.0f, "%.1f");
            ImGuiMCP::SetTooltip("Amount of fuel each grass vertex starts with.");

            ImGuiMCP::SliderFloat("Grass Max Heat", &set->DefaultMaxGrassHeat, 0.0f, 500.0f, "%.1f");
            ImGuiMCP::SetTooltip("Maximum heat grass can accumulate.");

            ImGuiMCP::SliderFloat("Grass Min Heat", &set->DefaultMinGrassHeat, -200.0f, 0.0f, "%.1f");
            ImGuiMCP::SetTooltip("Minimum heat (cold) grass can reach.");

            int grassColorValue = static_cast<int>(set->MinGrassColorValue);
            if (ImGuiMCP::SliderInt("Min Grass Color Value", &grassColorValue, 0, 255)) {
                set->MinGrassColorValue = static_cast<uint8_t>(grassColorValue);
            }
            ImGuiMCP::SetTooltip("Minimum vertex color brightness");
        }

        // =========================
        // Tree / Ref Settings
        // =========================
        if (ImGuiMCP::CollapsingHeader("Reference Objects (Trees etc.)", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGuiMCP::SliderFloat("Ref Ignition Heat", &set->DefaultRefIgnitionHeat, 0.0f, 200.0f, "%.1f");
            ImGuiMCP::SetTooltip("Heat required for trees or references to ignite.");

            ImGuiMCP::SliderFloat("Ref Initial Fuel", &set->DefaultRefFuel, 0.0f, 200.0f, "%.1f");
            ImGuiMCP::SetTooltip("Fuel amount available for references.");

            ImGuiMCP::SliderFloat("Ref Max Heat", &set->DefaultMaxRefHeat, 0.0f, 500.0f, "%.1f");
            ImGuiMCP::SetTooltip("Maximum heat references can accumulate.");

            ImGuiMCP::SliderFloat("Ref Min Heat", &set->DefaultMinRefHeat, -200.0f, 0.0f, "%.1f");
            ImGuiMCP::SetTooltip("Minimum temperature references can reach.");
        }

        // =========================
        // Fire Spread
        // =========================
        if (ImGuiMCP::CollapsingHeader("Fire Spread Simulation", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGuiMCP::SliderFloat("Heat Distribution Factor", &set->HeatDistributionFactor, 0.1f, 100.0f, "%.2f");
            ImGuiMCP::SetTooltip("Controls how quickly heat spreads between cells. Higher = slower spread.");

            ImGuiMCP::SliderFloat("Max Heat Distribution", &set->MaxHeatDistribution, 0.0f, 50.0f, "%.2f");
            ImGuiMCP::SetTooltip("Maximum heat that can transfer in a single step.");

            ImGuiMCP::SliderFloat("Fuel Consumption Rate", &set->FuelConsumptionRate, 0.01f, 10.0f, "%.2f");
            ImGuiMCP::SetTooltip("How fast burning cells consume fuel.");

            ImGuiMCP::SliderFloat("Fuel to Heat Rate", &set->FuelToHeatRate, 0.01f, 2.0f, "%.2f");
            ImGuiMCP::SetTooltip("How efficiently burning fuel generates heat.");

            ImGuiMCP::SliderFloat("Self Heat Loss", &set->SelfHeatLoss, 0.0f, 2.0f, "%.2f");
            ImGuiMCP::SetTooltip("Heat lost per second by non-burning cells.");

            ImGuiMCP::SliderFloat("Self Cold Loss", &set->SelfColdLoss, 0.0f, 2.0f, "%.2f");
            ImGuiMCP::SetTooltip("Cold decay over time for very cold cells.");
        }

        // =========================
        // Damage
        // =========================
        if (ImGuiMCP::CollapsingHeader("Damage Multipliers", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGuiMCP::SliderFloat("Fire Damage Multiplier", &set->FireDamageMultiplayer, 0.0f, 10.0f, "%.2f");
            ImGuiMCP::SetTooltip("Multiplier applied to fire damage.");

            ImGuiMCP::SliderFloat("Shock Damage Multiplier", &set->ShockDamageMultiplayer, 0.0f, 10.0f, "%.2f");
            ImGuiMCP::SetTooltip("Shock spells partially disrupt fire.");

            ImGuiMCP::SliderFloat("Cold Damage Multiplier", &set->ColdDamageMultiplayer, 0.0f, 10.0f, "%.2f");
            ImGuiMCP::SetTooltip("Cold damage cools burning cells.");

            ImGuiMCP::SliderFloat("Water Damage Multiplier", &set->WaterDamageMultiplayer, 0.0f, 10.0f, "%.2f");
            ImGuiMCP::SetTooltip("Water-based effects reduce heat.");
        }

        // =========================
        // Weather
        // =========================
        if (ImGuiMCP::CollapsingHeader("Weather Influence", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGuiMCP::SliderFloat("Rain Spread Factor", &set->RainingFactor, 0.0f, 1.0f, "%.2f");
            ImGuiMCP::SetTooltip("Reduces fire spread when raining.");

            ImGuiMCP::SliderFloat("Snow Spread Factor", &set->SnowingFactor, 0.0f, 1.0f, "%.2f");
            ImGuiMCP::SetTooltip("Reduces fire spread when snowing.");

            ImGuiMCP::SliderFloat("Wind Speed Factor", &set->WindSpeedFactor, 0.0f, 10.0f, "%.2f");
            ImGuiMCP::SetTooltip("Higher values make wind influence fire spread more.");
        }

        // =========================
        // Hazards
        // =========================
        if (ImGuiMCP::CollapsingHeader("Hazards", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGuiMCP::SliderInt("Max Hazard Spawn", &set->MaxHazardSpawn, 1, 60);
            ImGuiMCP::SameLine();
            HelpMarker(
                "The engine only renders about 30 hazards at once.\n"
                "Additional hazards may exist but remain invisible.");

            if (ImGuiMCP::Checkbox("Hazards Affect Player Only", &set->HazardAffectPCOnly)) {
                WildfireMgr::GetSingleton()->SetHazardsPCOnly(set->HazardAffectPCOnly);
            }
            ImGuiMCP::SameLine();
            HelpMarker("If enabled, hazards will only damage the player character.");
        }
    }

    void __stdcall RenderBurningGird() {
        auto wildfireMgr = WildfireMgr::GetSingleton();
        auto FireCellMap = wildfireMgr->GetCellStateMapCopy();

        ImGuiMCP::Text("Total Cells: %d", (int)FireCellMap.size());

        std::vector<std::pair<RE::TESObjectCELL*, CellState>> sortedCells(FireCellMap.begin(), FireCellMap.end());
        std::sort(sortedCells.begin(), sortedCells.end(),
                  [](const auto& a, const auto& b) { return a.first->GetFormID() < b.first->GetFormID(); });

        for (const auto& [cell, state] : sortedCells) {
            ImGuiMCP::Separator();
            std::string cellLabel = std::format("Cell: {:X}", cell->GetFormID());
            if (ImGuiMCP::CollapsingHeader(cellLabel.c_str())) {
                ImGuiMCP::Columns(2, nullptr, false);  // 2 columns for quadrants

                // Top row: Quadrant 0 (left), Quadrant 1 (right)
                for (int q = 0; q <= 1; ++q) {
                    std::string quadLabel = std::format("Quadrant {}", q+1);
                    ImGuiMCP::Text("%s", quadLabel.c_str());
                    if (ImGuiMCP::BeginTable(std::format("HeatTable{}_{}", (void*)cell, q).c_str(), 17,
                                             ImGuiMCP::ImGuiTableFlags_Borders)) {
                        ImGuiMCP::SetWindowFontScale(0.5f);
                        // Table rows
                        for (int row = 0; row < 17; ++row) {
                            ImGuiMCP::TableNextRow();
                            for (int col = 0; col < 17; ++col) {
                                ImGuiMCP::TableSetColumnIndex(col);
                                int idx = row * 17 + col;
                                ImGuiMCP::ImVec4 color = ImGuiMCP::ImVec4(0.75f, 0.75f, 0.75f, 1.0f);  // Gray by default

                                if (state.isCharred[q][idx]) {
                                    color = ImGuiMCP::ImVec4(0.0f, 0.0f, 0.0f, 1.0f);  // Black for charred
                                } else if (state.isBurning[q][idx]) {
                                    color = ImGuiMCP::ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Red for burning
                                } else if (state.heat[q][idx] > 0.0f && state.canBurn[q][idx]) {
                                    color = ImGuiMCP::ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow for heated
                                } else if (state.heat[q][idx] < 0.0f && state.canBurn[q][idx]) {
                                    color = ImGuiMCP::ImVec4(0.0f, 1.0f, 1.0f, 1.0f);  // Cyan for cold (negative heat)
                                } else if (state.canBurn[q][idx]) {
                                    color = ImGuiMCP::ImVec4(0.0f, 1.0f, 0.0f, 1.0f);  // Green for can burn
                                } else if (state.isUnderwater[q][idx]) {
                                    color = ImGuiMCP::ImVec4(0.0f, 0.0f, 1.0f, 1.0f);  // Blue for underwater
                                }
                                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, color);
                                ImGuiMCP::Text("H%.0f-%.0f", state.heat[q][idx], state.usedHeat[q][idx]);
                                ImGuiMCP::Text("F%.0f", state.fuel[q][idx]);
                                ImGuiMCP::PopStyleColor();
                            }
                        }
                        ImGuiMCP::SetWindowFontScale(1.0f);
                        ImGuiMCP::EndTable();
                    }
                    ImGuiMCP::NextColumn();
                }

                // Bottom row: Quadrant 2 (left), Quadrant 3 (right)
                for (int q = 2; q <= 3; ++q) {
                    std::string quadLabel = std::format("Quadrant {}", q+1);
                    ImGuiMCP::Text("%s", quadLabel.c_str());
                    if (ImGuiMCP::BeginTable(std::format("HeatTable{}_{}", (void*)cell, q).c_str(), 17,
                                             ImGuiMCP::ImGuiTableFlags_Borders)) {
                        ImGuiMCP::SetWindowFontScale(0.5f);
                        for (int row = 0; row < 17; ++row) {
                            ImGuiMCP::TableNextRow();
                            for (int col = 0; col < 17; ++col) {
                                ImGuiMCP::TableSetColumnIndex(col);
                                int idx = row * 17 + col;
                                ImGuiMCP::ImVec4 color = ImGuiMCP::ImVec4(0.75f, 0.75f, 0.75f, 1.0f);  // Gray by default

                                if (state.isCharred[q][idx]) {
                                    color = ImGuiMCP::ImVec4(0.0f, 0.0f, 0.0f, 1.0f);  // Black for charred
                                } else if (state.isBurning[q][idx]) {
                                    color = ImGuiMCP::ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Red for burning
                                } else if (state.heat[q][idx] > 0.0f && state.canBurn[q][idx]) {
                                    color = ImGuiMCP::ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow for heated
                                } else if (state.heat[q][idx] < 0.0f && state.canBurn[q][idx]) {
                                    color = ImGuiMCP::ImVec4(0.0f, 1.0f, 1.0f, 1.0f);  // Cyan for cold (negative heat)
                                } else if (state.canBurn[q][idx]) {
                                    color = ImGuiMCP::ImVec4(0.0f, 1.0f, 0.0f, 1.0f);  // Green for can burn
                                } else if (state.isUnderwater[q][idx]) {
                                    color = ImGuiMCP::ImVec4(0.0f, 0.0f, 1.0f, 1.0f);  // Blue for underwater
                                }
                                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, color);
                                ImGuiMCP::Text("H%.0f", state.heat[q][idx]);
                                ImGuiMCP::Text("F%.0f", state.fuel[q][idx]);

                                ImGuiMCP::PopStyleColor();
                            }
                        }
                        ImGuiMCP::SetWindowFontScale(1.0f);
                        ImGuiMCP::EndTable();
                    }
                    ImGuiMCP::NextColumn();
                }

                ImGuiMCP::Columns(1);  // Reset columns
            }
        }
    }

    void __stdcall RenderBurningRefs() {
        auto wildfireMgr = WildfireMgr::GetSingleton();
        auto refMap = wildfireMgr->GetRefStateMapCopy();

        ImGuiMCP::Text("Total Refs: %d", (int)refMap.size());

        std::vector<std::pair<RE::ObjectRefHandle, RefState>> sortedRefs(refMap.begin(), refMap.end());

        std::sort(sortedRefs.begin(), sortedRefs.end(),
                  [](const auto& a, const auto& b) { return a.first.native_handle() < b.first.native_handle(); });

        if (ImGuiMCP::BeginTable("RefStatesTable", 7,
                                 ImGuiMCP::ImGuiTableFlags_Borders | ImGuiMCP::ImGuiTableFlags_RowBg |
                                     ImGuiMCP::ImGuiTableFlags_ScrollY)) {
            ImGuiMCP::TableSetupColumn("Handle");
            ImGuiMCP::TableSetupColumn("RefID");
            ImGuiMCP::TableSetupColumn("Heat");
            ImGuiMCP::TableSetupColumn("Fuel");
            ImGuiMCP::TableSetupColumn("State");
            ImGuiMCP::TableSetupColumn("Flags");
            ImGuiMCP::TableSetupColumn("Position");

            ImGuiMCP::TableHeadersRow();

            for (auto& [handle, state] : sortedRefs) {
                ImGuiMCP::TableNextRow();

                RE::TESObjectREFR* ref = handle.get().get();

                ImGuiMCP::TableSetColumnIndex(0);
                ImGuiMCP::Text("%X", handle.native_handle());

                ImGuiMCP::TableSetColumnIndex(1);
                if (ref)
                    ImGuiMCP::Text("%X", ref->GetFormID());
                else
                    ImGuiMCP::TextDisabled("Dead");

                ImGuiMCP::TableSetColumnIndex(2);

                ImGuiMCP::ImVec4 heatColor = ImGuiMCP::ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
                if (state.isBurning)
                    heatColor = ImGuiMCP::ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                else if (state.isFrozen)
                    heatColor = ImGuiMCP::ImVec4(0.3f, 0.6f, 1.0f, 1.0f);
                else if (state.heat > 0.0f)
                    heatColor = ImGuiMCP::ImVec4(1.0f, 1.0f, 0.3f, 1.0f);

                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, heatColor);
                ImGuiMCP::Text("%.1f", state.heat);
                ImGuiMCP::PopStyleColor();

                ImGuiMCP::TableSetColumnIndex(3);
                ImGuiMCP::Text("%.1f / %.1f", state.fuel, state.initialFuel);

                ImGuiMCP::TableSetColumnIndex(4);

                if (state.isCharred)
                    ImGuiMCP::TextColored({0, 0, 0, 1}, "CHARRED");
                else if (state.isBurning)
                    ImGuiMCP::TextColored({1, 0, 0, 1}, "BURNING");
                else if (state.isFrozen)
                    ImGuiMCP::TextColored({0.4f, 0.7f, 1, 1}, "FROZEN");
                else
                    ImGuiMCP::Text("Idle");

                ImGuiMCP::TableSetColumnIndex(5);

                std::string flags;
                if (state.canBurn) flags += "CanBurn ";
                if (state.halfEvent) flags += "Half ";

                ImGuiMCP::Text("%s", flags.c_str());

                ImGuiMCP::TableSetColumnIndex(6);
                ImGuiMCP::Text("(%.0f %.0f %.0f)", state.position.x, state.position.y, state.position.z);

                // Expandable debug block
                if (ImGuiMCP::IsItemClicked()) {
                    ImGuiMCP::OpenPopup("RefDetails");
                }

                if (ImGuiMCP::BeginPopup("RefDetails")) {
                    ImGuiMCP::Text("Angle: %.2f %.2f %.2f", state.angle.x, state.angle.y, state.angle.z);

                    ImGuiMCP::Separator();
                    ImGuiMCP::Text("BBox:");

                    for (int i = 0; i < 8; i++) {
                        ImGuiMCP::Text("V%d: %.0f %.0f %.0f", i, state.BBox[i].x, state.BBox[i].y, state.BBox[i].z);
                    }

                    ImGuiMCP::EndPopup();
                }
            }

            ImGuiMCP::EndTable();
        }
    }

    static const char* GetCompassLabel(float windAngle) {
        static const char* labels[8] = {"E", "NE", "N", "NW", "W", "SW", "S", "SE"};

        float degrees = windAngle * (180.0f / std::numbers::pi_v<float>);
        if (degrees < 0) degrees += 360.0f;

        int idx = static_cast<int>((degrees + 22.5f) / 45.0f) % 8;
        return labels[idx];
    }

     static void SpawnHazardDebug(const RE::NiPoint3& pos, HazardType size, float HazardLifetime) {
        RE::BGSHazard* hazardToSpawn = nullptr;
        switch (size) {
            case HazardType::SmallFireHazard:
                hazardToSpawn = RE::TESForm::LookupByEditorID("Wildfire_SmallHazard")->As<RE::BGSHazard>();
                break;
            case HazardType::MediumFireHazard:
                hazardToSpawn = RE::TESForm::LookupByEditorID("Wildfire_MediumHazard")->As<RE::BGSHazard>();
                break;
            case HazardType::BigFireHazard:
                hazardToSpawn = RE::TESForm::LookupByEditorID("Wildfire_BigHazard")->As<RE::BGSHazard>();
                break;
            case HazardType::LargeFireHazard:
                hazardToSpawn = RE::TESForm::LookupByEditorID("Wildfire_LargeHazard")->As<RE::BGSHazard>();
                break;
            default:
                logger::error("Invalid hazard size: {}", size);
                return;
        }
        if (!hazardToSpawn) {
            logger::error("Hazard form not found for size: {}", size);
            return;
        }

        hazardToSpawn->data.lifetime = HazardLifetime;

        auto player = RE::PlayerCharacter::GetSingleton();
        auto hazardRef = player->PlaceObjectAtMe(hazardToSpawn, false);
        if (!hazardRef) {
            logger::error("Failed to place hazard");
            return;
        }
        hazardRef->SetPosition(pos);
    }

    void __stdcall RenderDebugTools() {

        if (auto player = RE::PlayerCharacter::GetSingleton()) {
            auto cell = player->parentCell;  // Player's current cell
            if (cell) {
                ImGuiMCP::Text("Current Cell Info:");
                ImGuiMCP::Text("Cell Ptr: %p", cell);

                // Access ExtraCellGrassData
                auto extraData = cell->extraList.GetByType(RE::ExtraDataType::kCellGrassData);
                auto* extraGrass = extraData ? static_cast<RE::ExtraCellGrassData*>(extraData) : nullptr;

                if (extraGrass) {
                    ImGuiMCP::Text("ExtraCellGrassData Ptr: %p", extraGrass);
                    ImGuiMCP::Text("AddGrassTask Ptr: %p", extraGrass->addGrassTask.get());
                    ImGuiMCP::Text("Grass Handles Count: %zu", extraGrass->grassHandles.size());

                    // Loop through all CellGrassData
                    for (std::size_t i = 0; i < extraGrass->grassHandles.size(); i++) {
                        auto* grass = extraGrass->grassHandles[i];
                        if (!grass) continue;

                        ImGuiMCP::Separator();
                        ImGuiMCP::Text("Grass Index: %zu", i);
                        ImGuiMCP::Text("Grass Ptr: %p", grass);
                        ImGuiMCP::Text("Grass Type Key: 0x%llX", grass->grassTypeKey);
                        ImGuiMCP::Text("Grass Model Key: %s", grass->grassModelKey.c_str());
                        ImGuiMCP::Text("TriShape Ptr: %p", grass->triShape.get());
                        ImGuiMCP::Text("Unk18 Size: %zu", grass->unk18.size());
                        ImGuiMCP::Text("Unk30 Size: %zu", grass->unk30.size());

                        // Optional: iterate through unk18 and unk30 if you want full detail
                        for (std::size_t j = 0; j < grass->unk18.size(); j++) {
                            ImGuiMCP::Text("  unk18[%zu] Ptr: %p", j, grass->unk18[j]);
                        }
                        for (std::size_t j = 0; j < grass->unk30.size(); j++) {
                            ImGuiMCP::Text("  unk30[%zu] Ptr: %p", j, grass->unk30[j]);
                        }
                    }
                } else {
                    ImGuiMCP::Text("No ExtraCellGrassData found in this cell.");
                }
            } else {
                ImGuiMCP::Text("Player is not in a valid cell.");
            }
        }

        static float lifetime = 10.0f;

        ImGuiMCP::Text("Wind Data");
        auto isRaining = Weather::IsRaining();
        auto isSnowing = Weather::IsSnowing();
        auto windData = Weather::GetCurrentWind();

        ImGuiMCP::Text("Is Raining: %s", isRaining ? "Yes" : "No");
        ImGuiMCP::Text("Is Snowing: %s", isSnowing ? "Yes" : "No");
        ImGuiMCP::Text("Wind Speed: %.1f", windData.speed);
        ImGuiMCP::Text("Wind Direction: %.1f", windData.direction);
        ImGuiMCP::Text("Wind Direction: %s", GetCompassLabel(windData.direction));
        auto* sky = RE::Sky::GetSingleton();
        if (sky) {
            ImGuiMCP::SliderFloat("Direction", &sky->windAngle, 0.0f, 6.28318f);
            ImGuiMCP::SliderFloat("Wind Speed", &sky->windSpeed, 0.0f, 10.0f);
        }

        ImGuiMCP::SliderFloat("Hazard Lifetime", &lifetime, 1.0f, 60.0f, "%.1f");
        if (ImGuiMCP::Button("Spawn SmallFireHazard")) {
            auto player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                auto pos = player->GetPosition();
                SpawnHazardDebug(pos, HazardType::SmallFireHazard, lifetime);
            }
        }
        if (ImGuiMCP::Button("Spawn MediumFireHazard")) {
            auto player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                auto pos = player->GetPosition();
                SpawnHazardDebug(pos, HazardType::MediumFireHazard, lifetime);
            }
        }
        if (ImGuiMCP::Button("Spawn BigFireHazard")) {
            auto player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                auto pos = player->GetPosition();
                SpawnHazardDebug(pos, HazardType::BigFireHazard, lifetime);
            }
        }
        if (ImGuiMCP::Button("Spawn LargeFireHazard")) {
            auto player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                auto pos = player->GetPosition();
                SpawnHazardDebug(pos, HazardType::LargeFireHazard, lifetime);
            }
        }
        // Hazard Limit Test
        static int hazardCount = 10;
        ImGuiMCP::Text("Hazard Limit Test");
        ImGuiMCP::SliderInt("Hazard Count", &hazardCount, 1, 100);
        ImGuiMCP::Text("Spawns hazards in a circle around the player to test the max hazard limit. Spoiler: it's 30");
        if (ImGuiMCP::Button("Spawn Multiple Hazards")) {
            auto player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                auto pos = player->GetPosition();
                for (int i = 0; i < hazardCount; ++i) {
                    // spawn hazards in a circle around the player
                    float angle = (i / static_cast<float>(hazardCount)) * 2.0f * 3.14159f;
                    float radius = 100.0f;  // 100 units away from the player
                    pos.x += radius * std::cos(angle);
                    pos.y += radius * std::sin(angle);
                    SpawnHazardDebug(pos, HazardType::MediumFireHazard, lifetime);
                }
            }
        }
    }

    void __stdcall MCP::RenderLog() {
        ImGuiMCP::Checkbox("Trace", &MCPLog::log_trace);
        ImGuiMCP::SameLine();
        ImGuiMCP::Checkbox("Info", &MCPLog::log_info);
        ImGuiMCP::SameLine();
        ImGuiMCP::Checkbox("Warning", &MCPLog::log_warning);
        ImGuiMCP::SameLine();
        ImGuiMCP::Checkbox("Error", &MCPLog::log_error);
        ImGuiMCP::InputText("Custom Filter", MCPLog::custom, 255);

        // if"Generate Log" button is pressed, read the log file
        if (ImGuiMCP::Button("Generate Log")) {
            logLines = MCPLog::ReadLogFile();
        }

        // Display each line in a new ImGuiMCP::Text() element
        for (const auto& line : logLines) {
            if (line.find("trace") != std::string::npos && !MCPLog::log_trace) continue;
            if (line.find("info") != std::string::npos && !MCPLog::log_info) continue;
            if (line.find("warning") != std::string::npos && !MCPLog::log_warning) continue;
            if (line.find("error") != std::string::npos && !MCPLog::log_error) continue;
            if (line.find(MCPLog::custom) == std::string::npos && MCPLog::custom != "") continue;
            ImGuiMCP::Text(line.c_str());
        }
    }
}

namespace MCPLog {
    std::filesystem::path GetLogPath() {
        const auto logsFolder = SKSE::log::log_directory();
        if (!logsFolder) SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
        auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
        auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
        return logFilePath;
    }

    std::vector<std::string> ReadLogFile() {
        std::vector<std::string> logLines;

        // Open the log file
        std::ifstream file(GetLogPath().c_str());
        if (!file.is_open()) {
            // Handle error
            return logLines;
        }

        // Read and store each line from the file
        std::string line;
        while (std::getline(file, line)) {
            logLines.push_back(line);
        }

        file.close();

        return logLines;
    }

}