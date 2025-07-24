#include "MCP.h"

#include "SKSEMCP/SKSEMenuFramework.hpp"
#include "Utils.h"
#include "WildfireMgr.h"

namespace MCP {

    void Register() {
        if (!SKSEMenuFramework::IsInstalled()) {
            logger::warn("SKSE Menu Framework is not installed. Cannot register menu.");
            return;
        }
        SKSEMenuFramework::SetSection("Wildfire");
        SKSEMenuFramework::AddSectionItem("Settings", RenderSettings);

#ifndef NDEBUG
        SKSEMenuFramework::AddSectionItem("WildfireMgr", RenderWildfireMgr);
        SKSEMenuFramework::AddSectionItem("Log", RenderLog);
#endif

        logger::info("SKSE Menu Framework registered.");
    }

    void __stdcall RenderSettings() {

        auto* set = Settings::GetSingleton();

        ImGui::Checkbox("Mod Active", &set->ModActive);
        ImGui::SliderFloat("Min Heat To Burn", &set->MinHeatToBurn, 0.0f, 100.0f);
        ImGui::SliderFloat("Periodic Update Time (s)", &set->PeriodicUpdateTime, 0.1f, 10.0f);
        ImGui::SliderFloat("Heat Distribution", &set->HeatDistributionFactor, 1.0f, 100.0f);
        ImGui::SliderFloat("Fuel Consumption Rate", &set->FuelConsumptionRate, 0.1f, 10.0f);
        ImGui::SliderFloat("Initial Fuel Amount", &set->InitialFuelAmount, 1.0f, 100.0f);


        if (ImGui::Button("Create Grass")) {
            using CreateGrassInCell = void (*)(RE::TESObjectCELL*);
            REL::Relocation<CreateGrassInCell> CreateGrassInCellfunc{REL::RelocationID(13137, 13277)};
            CreateGrassInCellfunc(RE::PlayerCharacter::GetSingleton()->GetParentCell());
        }
        if (ImGui::Button("Remove Grass")) {
            using RemoveGrassInCell = void (*)(RE::BGSGrassManager*, RE::TESObjectCELL*);
            REL::Relocation<RemoveGrassInCell> RemoveGrassInCellfunc{REL::RelocationID(15207, 15375)};
            RemoveGrassInCellfunc(RE::BGSGrassManager::GetSingleton(),
                                  RE::PlayerCharacter::GetSingleton()->GetParentCell());

        }
        if (ImGui::Button("Detach Grass From Cell")) {
            auto toCull =
                RE::PlayerCharacter::GetSingleton()->GetParentCell()->extraList.GetByType<RE::ExtraCellGrassData>();

            if (toCull) {
                for (auto grass : toCull->grassHandles) {
                    grass->triShape->CullGeometry(true);
                }
                toCull->grassHandles.clear();
            }
            RE::PlayerCharacter::GetSingleton()->GetParentCell()->extraList.RemoveByType(RE::ExtraDataType::kCellGrassData);
        }
    }

    void __stdcall RenderWildfireMgr() {
        static std::unordered_map<RE::TESObjectCELL*, FireCellState> fireCellCache;
        static bool ShowData = false;

        if (ImGui::Button("Fetch FireCell Snapshot")) {
            const auto& currentMap = WildfireMgr::GetSingleton()->GetFireCellMap();
            fireCellCache = currentMap;
        }

        ImGui::Text("Total Cells: %d", (int)fireCellCache.size());

        for (const auto& [cell, state] : fireCellCache) {
            ImGui::Separator();
            std::string cellLabel = std::format("Cell: {:X}", cell->GetFormID());
            if (ImGui::CollapsingHeader(cellLabel.c_str())) {
                ImGui::Columns(2, nullptr, false);  // 2 columns for quadrants

                // Top row: Quadrant 0 (left), Quadrant 1 (right)
                for (int q = 0; q <= 1; ++q) {
                    std::string quadLabel = std::format("Quadrant {}", q);
                    ImGui::Text("%s", quadLabel.c_str());
                    if (ImGui::BeginTable(std::format("HeatTable{}_{}", (void*)cell, q).c_str(), 17,
                                          ImGuiTableFlags_Borders)) {
                        // Table rows
                        for (int row = 0; row < 17; ++row) {
                            ImGui::TableNextRow();
                            for (int col = 0; col < 17; ++col) {
                                ImGui::TableSetColumnIndex(col);
                                int idx = row * 17 + col;
                                ImVec4 color = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);  // Gray by default

                                if (state.isCharred[q][idx]) {
                                    color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);  // Black for charred
                                } else if (state.isBurning[q][idx]) {
                                    color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Red for burning
                                } else if (state.heat[q][idx] != 0.0f) {
                                    color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow for heated
                                } else if (state.canBurn[q][idx]) {
                                    color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);  // Green for can burn
                                }
                                ImGui::PushStyleColor(ImGuiCol_Text, color);
                                ImGui::Text("H%.0f", state.heat[q][idx]);
                                ImGui::Text("F%.0f", state.fuel[q][idx]);
                                ImGui::PopStyleColor();
                            }
                        }
                        ImGui::EndTable();
                    }
                    ImGui::NextColumn();
                }

                // Bottom row: Quadrant 2 (left), Quadrant 3 (right)
                for (int q = 2; q <= 3; ++q) {
                    std::string quadLabel = std::format("Quadrant {}", q);
                    ImGui::Text("%s", quadLabel.c_str());
                    if (ImGui::BeginTable(std::format("HeatTable{}_{}", (void*)cell, q).c_str(), 17,
                                          ImGuiTableFlags_Borders)) {
                        for (int row = 0; row < 17; ++row) {
                            ImGui::TableNextRow();
                            for (int col = 0; col < 17; ++col) {
                                ImGui::TableSetColumnIndex(col);
                                int idx = row * 17 + col;
                                ImVec4 color = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);  // Gray by default

                                if (state.isCharred[q][idx]) {
                                    color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);  // Black for charred
                                } else if (state.isBurning[q][idx]) {
                                    color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Red for burning
                                } else if (state.heat[q][idx] != 0.0f) {
                                    color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow for heated
                                } else if (state.canBurn[q][idx]) {
                                    color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);  // Green for can burn
                                }
                                ImGui::PushStyleColor(ImGuiCol_Text, color);
                                ImGui::Text("H%.0f", state.heat[q][idx]);
                                ImGui::Text("F%.0f", state.fuel[q][idx]);

                                ImGui::PopStyleColor();
                            }
                        }
                        ImGui::EndTable();
                    }
                    ImGui::NextColumn();
                }

                ImGui::Columns(1);  // Reset columns
            }
        }
    }

    void __stdcall MCP::RenderLog() {
        ImGui::Checkbox("Trace", &MCPLog::log_trace);
        ImGui::SameLine();
        ImGui::Checkbox("Info", &MCPLog::log_info);
        ImGui::SameLine();
        ImGui::Checkbox("Warning", &MCPLog::log_warning);
        ImGui::SameLine();
        ImGui::Checkbox("Error", &MCPLog::log_error);
        ImGui::InputText("Custom Filter", MCPLog::custom, 255);

        // if"Generate Log" button is pressed, read the log file
        if (ImGui::Button("Generate Log")) {
            logLines = MCPLog::ReadLogFile();
        }

        // Display each line in a new ImGui::Text() element
        for (const auto& line : logLines) {
            if (line.find("trace") != std::string::npos && !MCPLog::log_trace) continue;
            if (line.find("info") != std::string::npos && !MCPLog::log_info) continue;
            if (line.find("warning") != std::string::npos && !MCPLog::log_warning) continue;
            if (line.find("error") != std::string::npos && !MCPLog::log_error) continue;
            if (line.find(MCPLog::custom) == std::string::npos && MCPLog::custom != "") continue;
            ImGui::Text(line.c_str());
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