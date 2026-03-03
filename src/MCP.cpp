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

        SKSEMenuFramework::AddSectionItem("Burning Gird", RenderBurningGird);

#ifndef NDEBUG
        SKSEMenuFramework::AddSectionItem("Debug Options", RenderDebugTools);
        SKSEMenuFramework::AddSectionItem("Log", RenderLog);
#endif

        logger::info("SKSE Menu Framework registered.");
    }

    void __stdcall RenderSettings() {
        static auto* set = Settings::GetSingleton();

        ImGuiMCP::Checkbox("Mod Active", &set->ModActive);
        ImGuiMCP::SameLine();
        ImGuiMCP::Checkbox("Debug Mode", &set->DebugMode);
        ImGuiMCP::SameLine();
        ImGuiMCP::TextDisabled("(?)");
        if (ImGuiMCP::IsItemHovered()) {
            ImGuiMCP::SetTooltip("");
        }
        ImGuiMCP::SliderFloat("Debug Draw Duration", &set->DebugDrawDuration, 100.0f, 10000.0f, "%.0f");
        ImGuiMCP::TextDisabled("(?)");
        if (ImGuiMCP::IsItemHovered()) {
            ImGuiMCP::SetTooltip("");
        }

        if (ImGuiMCP::CollapsingHeader("Grass Generation settings",
                                       ImGuiMCP::ImGuiTreeNodeFlags_::ImGuiTreeNodeFlags_DefaultOpen)) {
            int grassColorValue = static_cast<int>(set->MinGrassColorValue);
            if (ImGuiMCP::SliderInt("Min Grass Color Value", &grassColorValue, 0, 255)) {
                set->MinGrassColorValue = static_cast<uint8_t>(grassColorValue);
            }
        }

        if (ImGuiMCP::CollapsingHeader("Update Intervals",
                                       ImGuiMCP::ImGuiTreeNodeFlags_::ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGuiMCP::SliderFloat("Grass Periodic Update Time (s)", &set->GrassPeriodicUpdateTime, 0.1f, 10.0f, "%.1f");
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("(?)");
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("");
            }
            ImGuiMCP::SliderFloat("Hazard Periodic Update Time (s)", &set->HazardPeriodicUpdateTime, 0.1f, 10.0f,
                                  "%.1f");
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("(?)");
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("");
            }
            ImGuiMCP::SliderFloat("Ref Periodic Update Time (s)", &set->RefPeriodicUpdateTime, 5.0f, 20.0f, "%.1f");
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("(?)");
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("");
            }
        }

        if (ImGuiMCP::CollapsingHeader("Grass and Tree settings",
                                       ImGuiMCP::ImGuiTreeNodeFlags_::ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGuiMCP::SliderFloat("Heat Distribution", &set->HeatDistributionFactor, 0.0f, 100.0f, "%.2f");
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("(?)");
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("");
            }
            ImGuiMCP::SliderFloat("Fuel Consumption Rate", &set->FuelConsumptionRate, 0.1f, 10.0f, "%.2f");
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("(?)");
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("");
            }
            ImGuiMCP::SliderFloat("Fuel To Heat Rate", &set->FuelToHeatRate, 0.01f, 1.0f, "%.2f");
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("(?)");
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("");
            }
            ImGuiMCP::SliderFloat("Self Heat Loss", &set->SelfHeatLoss, 0.01f, 1.0f, "%.2f");
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("(?)");
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("");
            }
            ImGuiMCP::SliderFloat("Ref Damage Per Burning Vertex", &set->RefDamagePerBurningVertex, 1.0f, 20.0f,
                                  "%.1f");
        }

        if (ImGuiMCP::CollapsingHeader("Default Values",
                                       ImGuiMCP::ImGuiTreeNodeFlags_::ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGuiMCP::SliderFloat("Default Min Heat To Burn", &set->DefaultMinHeatToBurn, 1.0f, 100.0f, "%.1f");
            ImGuiMCP::SliderFloat("Default Initial Fuel Amount", &set->DefaultInitialFuelAmount, 10.0f, 200.0f, "%.1f");
        }

        if (ImGuiMCP::CollapsingHeader("Damage Multipliers",
                                       ImGuiMCP::ImGuiTreeNodeFlags_::ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGuiMCP::SliderFloat("Fire Damage Multiplayer", &set->FireDamageMultiplayer, 0.1f, 10.0f, "%.1f");
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("(?)");
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("");
            }
            ImGuiMCP::SliderFloat("Shock Damage Multiplayer", &set->ShockDamageMultiplayer, 0.1f, 10.0f, "%.1f");
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("(?)");
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("");
            }
            ImGuiMCP::SliderFloat("Cold Damage Multiplayer", &set->ColdDamageMultiplayer, 0.1f, 10.0f, "%.1f");
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("(?)");
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("");
            }
            ImGuiMCP::SliderFloat("Water Damage Multiplayer", &set->WaterDamageMultiplayer, 0.1f, 10.0f, "%.1f");
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("(?)");
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("");
            }
        }

        if (ImGuiMCP::CollapsingHeader("Environmental Settings",
                                       ImGuiMCP::ImGuiTreeNodeFlags_::ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGuiMCP::SliderFloat("Raining Factor", &set->RainingFactor, 0.1f, 1.0f, "%.2f");
            ImGuiMCP::SliderFloat("Snowing Factor", &set->SnowingFactor, 0.1f, 1.0f, "%.2f");
            ImGuiMCP::SliderFloat("Wind Speed Factor", &set->WindSpeedFactor, 0.1f, 10.0f, "%.1f");
        }

        if (ImGuiMCP::CollapsingHeader("Hazard Settings",
                                       ImGuiMCP::ImGuiTreeNodeFlags_::ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGuiMCP::SliderInt("Max Hazard Spawn At Once", &set->MaxHazardSpawn, 1, 60);
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("(?)");
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip(
                    "30 is the engine's visible hazard limit. If you spawn more, only the 30 closest to the camera "
                    "will be visible; the rest will be invisible.");
            }
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
                    std::string quadLabel = std::format("Quadrant {}", q);
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
                                } else if (state.heat[q][idx] != 0.0f) {
                                    color = ImGuiMCP::ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow for heated
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

                // Bottom row: Quadrant 2 (left), Quadrant 3 (right)
                for (int q = 2; q <= 3; ++q) {
                    std::string quadLabel = std::format("Quadrant {}", q);
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

    static const char* GetCompassLabel(uint8_t windDir) {
        // Skyrim: 0=E, 64=N, 128=W, 192=S (full circle = 256)
        static const char* labels[8] = {"E", "NE", "N", "NW", "W", "SW", "S", "SE"};

        // Convert to degrees in [0, 360)
        float degrees = (windDir / 256.0f) * 360.0f;

        constexpr float sliceSize = 45.0f;
        constexpr float tolerance = 16.0f * 360.0f / 256.0f;

        // Shift degrees so bins line up (0* = East)
        float shifted = std::fmod(degrees + tolerance, 360.0f);

        int idx = static_cast<int>(shifted / sliceSize) % 8;
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

        ImGuiMCP::SliderFloat("Hazard Lifetime", &lifetime, 1.0f, 60.0f, "%.1f");
        if (ImGuiMCP::Button("SmallFireHazard")) {
            auto player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                auto pos = player->GetPosition();
                SpawnHazardDebug(pos, HazardType::SmallFireHazard, lifetime);
            }
        }
        if (ImGuiMCP::Button("MediumFireHazard")) {
            auto player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                auto pos = player->GetPosition();
                SpawnHazardDebug(pos, HazardType::MediumFireHazard, lifetime);
            }
        }
        if (ImGuiMCP::Button("BigFireHazard")) {
            auto player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                auto pos = player->GetPosition();
                SpawnHazardDebug(pos, HazardType::BigFireHazard, lifetime);
            }
        }
        if (ImGuiMCP::Button("LargeFireHazard")) {
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