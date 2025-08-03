#include "MCP.h"

#include "SKSEMCP/SKSEMenuFramework.hpp"
#include "Utils.h"
#include "WildfireMgr.h"
#include "HazardMgr.h"

namespace MCP {

    void Register() {
        if (!SKSEMenuFramework::IsInstalled()) {
            logger::warn("SKSE Menu Framework is not installed. Cannot register menu.");
            return;
        }
        SKSEMenuFramework::SetSection("Wildfire");
        SKSEMenuFramework::AddSectionItem("Settings", RenderSettings);
        SKSEMenuFramework::AddSectionItem("Loaded Grass Config", RenderGrassConfig);
        SKSEMenuFramework::AddSectionItem("Loaded Fire Config", RenderFireConfig);

#ifndef NDEBUG
        SKSEMenuFramework::AddSectionItem("WildfireMgr", RenderWildfireMgr);
        SKSEMenuFramework::AddSectionItem("GrassMgr", RenderGrassMgr);
        SKSEMenuFramework::AddSectionItem("Log", RenderLog);
#endif

        logger::info("SKSE Menu Framework registered.");
    }

    void __stdcall RenderSettings() {
        static auto* set = Settings::GetSingleton();
        static auto* player = RE::PlayerCharacter::GetSingleton();
        static auto* WildfireMgr = WildfireMgr::GetSingleton();
        static auto* hazardMgr = HazardMgr::GetSingleton();
        static auto* GrassMgr = RE::BGSGrassManager::GetSingleton();

        ImGui::Checkbox("Mod Active", &set->ModActive);
        ImGui::SameLine();
        ImGui::Checkbox("Debug Mode", &set->DebugMode);
        ImGui::SliderFloat("Periodic Update Time (s)", &set->PeriodicUpdateTime, 0.1f, 10.0f, "%.1f");
        ImGui::SliderFloat("Heat Distribution", &set->HeatDistributionFactor, 1.0f, 100.0f, "%.1f");
        ImGui::SliderFloat("Fuel Consumption Rate", &set->FuelConsumptionRate, 0.1f, 10.0f, "%.2f");
        ImGui::SliderFloat("Fuel To Heat Rate", &set->FuelToHeatRate, 0.01f, 1.0f, "%.2f");
        ImGui::SliderFloat("Self Heat Loss", &set->SelfHeatLoss, 0.01f, 1.0f, "%.2f");
        ImGui::SliderFloat("Damage Multiplayer", &set->DamageMultiplayer, 0.1f, 10.0f, "%.1f");
        ImGui::SliderFloat("Raining Factor", &set->RainingFactor, 0.1f, 1.0f, "%.2f");
    }

    void __stdcall RenderWildfireMgr() {
        static std::unordered_map<RE::TESObjectCELL*, FireCellState> fireCellCache;
        static bool FetchData = false;
        static auto WildfireMgr = WildfireMgr::GetSingleton();
        static auto player = RE::PlayerCharacter::GetSingleton();
        static auto GrassMgr = RE::BGSGrassManager::GetSingleton();

        if (ImGui::Button("Reset Current Wildfire Cell")) {
            WildfireMgr->ResetFireCellState(player->GetParentCell());
            GrassMgr->RemoveGrassInCell(player->GetParentCell());
            GrassMgr->CreateGrassInCell(player->GetParentCell());
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset All Wildfire Cells")) {
            WildfireMgr->ResetAllFireCells();
            GrassMgr->RemoveAllGrass();
            GrassMgr->CreateAllGrass();
        }

        if (ImGui::Button("Fetch FireCell Snapshot")) {
            const auto& currentMap = WildfireMgr->GetFireCellMap();
            fireCellCache = currentMap;
        }
        ImGui::SameLine();
        ImGui::Checkbox("Fetch FireCell Continously", &FetchData);
        if (FetchData) {
            const auto& currentMap = WildfireMgr->GetFireCellMap();
            fireCellCache = currentMap;
        }
        ImGui::Text("Wind Data - Force: %d, Direction: %d", WildfireMgr->GetCurrentWind().first,
                    WildfireMgr->GetCurrentWind().second);
        ImGui::Text("Raining: %s", WildfireMgr->IsCurrentWeatherRaining() ? "Yes" : "No");

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
                                ImGui::Text("H%.0f", state.heat[q][idx] / state.minBurnHeat[q][idx]);
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

    
    void __stdcall RenderGrassMgr() {
        auto* GrassMgr = RE::BGSGrassManager::GetSingleton();
        auto* player = RE::PlayerCharacter::GetSingleton();

        REL::Relocation<std::uint8_t*> byteFlag{REL::ID(359446)};
        ImGui::Checkbox("Grass Fade Enabled", reinterpret_cast<bool*>(byteFlag.address()));

        if (ImGui::Button("Generate Grass Fast false")) {
            using GrassFunc_t = __int64(*)(RE::BGSGrassManager* mgr, RE::TESObjectCELL* cell, std::uint8_t* bytePtr);
            REL::Relocation<GrassFunc_t> func{RELOCATION_ID(15204, 15372)};
            std::uint8_t flag = 0;
            std::uint8_t* pFlag = &flag;
            func(GrassMgr, player->GetParentCell(), pFlag);
        }
        if (ImGui::Button("Generate Grass Fast true")) {
            using GrassFunc_t = __int64 (*)(RE::BGSGrassManager* mgr, RE::TESObjectCELL* cell, std::uint8_t* bytePtr);
            REL::Relocation<GrassFunc_t> func{RELOCATION_ID(15204, 15372)};
            std::uint8_t flag = 1;
            std::uint8_t* pFlag = &flag;
            func(GrassMgr, player->GetParentCell(), pFlag);
        }
        if (ImGui::Button("Execute Grass Tasks")) {
            std::int64_t flag = 0;  // low byte = 1, matches LOBYTE(a3)=1 in original
            GrassMgr->ExecuteAllGrassTasks(player->GetParentCell(), flag);
        }

        if (ImGui::Button("Toggle Grass")) {
            GrassMgr->ToggleGrass();
        }
        if (ImGui::Button("Remove All Grass")) {
            GrassMgr->RemoveAllGrass();
        }
        ImGui::SameLine();
        if (ImGui::Button("Create All Grass")) {
            GrassMgr->CreateAllGrass();
        }

        if (ImGui::Button("Remove Grass in Current Cell")) {
            GrassMgr->RemoveGrassInCell(player->GetParentCell());
        }
        ImGui::SameLine();
        if (ImGui::Button("Create Grass in Current Cell")) {
            GrassMgr->CreateGrassInCell(player->GetParentCell());
        }
        ImGui::Checkbox("Generate Grass Data Files", &GrassMgr->generateGrassDataFiles);

        ImGui::Checkbox("Enable Grass", &GrassMgr->enableGrass);
        ImGui::SliderFloat("Grass Visibility?", &GrassMgr->grassFadeDistance, 0.0f, 10000.0f);
        ImGui::Text("Eval Size: %u", GrassMgr->grassEvalSize);
        ImGui::Text("Eval Size²: %u", GrassMgr->grassEvalSizeSquared);
        ImGui::Text("unk78: %u", GrassMgr->unk78);
        ImGui::Text("unk7C: %u", GrassMgr->unk7C);

        ImGui::Separator();
        ImGui::Text("unk48 Array (likely grass tasks):");
        const auto& arr = GrassMgr->unk48;
        for (size_t i = 0; i < arr.size(); i++) {
            ImGui::Text("  [%zu] %p", i, arr[i]);
        }


        ImGui::Separator();
        ImGui::Text("=== Raw unk fields ===");

        // Raw memory dump for the block starting at unk02 up through unk30
        uint8_t* base = reinterpret_cast<uint8_t*>(&GrassMgr->unk02);
        for (int i = 0; i <= 0x30; i += 8) {
            // print in 8-byte chunks, even if they span multiple declared members
            uint64_t chunk = 0;
            memcpy(&chunk, base + i, sizeof(chunk));  // safe copy to avoid strict-aliasing
            ImGui::Text(" +0x%02X: 0x%016llX", i + 2 /* offset from start of class? adjust if needed */,
                        static_cast<unsigned long long>(chunk));
        }

        // Heuristic interpretation
        ImGui::Spacing();
        ImGui::Text("=== Heuristic views ===");

        // Interpret small fields
        ImGui::Text("unk02 (uint8_t): %u", GrassMgr->unk02);
        ImGui::Text("unk04 (uint16_t): %u", GrassMgr->unk04);
        ImGui::Text("unk08 (uint32_t): %u", GrassMgr->unk08);
        ImGui::Text("unk0C (uint32_t): %u", GrassMgr->unk0C);

        // Interpret larger fields as possible pointers / IDs
        auto tryDescribePointer = [&](const char* name, std::uint64_t raw) {
            ImGui::Text("%s raw: 0x%016llX", name, static_cast<unsigned long long>(raw));
            bool looksLikePtr = raw && (raw > 0x1000) && (raw < 0x00007FFFFFFF0000ULL);  // naive user-space range
            ImGui::SameLine();
            ImGui::TextUnformatted(looksLikePtr ? "(plausible ptr)" : "(unlikely ptr)");
        };

        tryDescribePointer("unk10", GrassMgr->unk10);
        tryDescribePointer("unk18", GrassMgr->unk18);
        tryDescribePointer("unk20", GrassMgr->unk20);
        tryDescribePointer("unk28", GrassMgr->unk28);
        tryDescribePointer("unk30", GrassMgr->unk30);

        // Live experimentation sliders (with caps) – wrap changes so user knows risk
        ImGui::Spacing();
        ImGui::TextWrapped(
            "Modify/experiment with fields. Changing them may destabilize the game; original values are preserved for "
            "revert.");

        // Backup original values static to allow revert
        static std::uint8_t original_unk02 = GrassMgr->unk02;
        static std::uint16_t original_unk04 = GrassMgr->unk04;
        static std::uint32_t original_unk08 = GrassMgr->unk08;
        static std::uint32_t original_unk0C = GrassMgr->unk0C;
        static std::uint64_t original_unk10 = GrassMgr->unk10;

        if (ImGui::Button("Revert small fields")) {
            GrassMgr->unk02 = original_unk02;
            GrassMgr->unk04 = original_unk04;
            GrassMgr->unk08 = original_unk08;
            GrassMgr->unk0C = original_unk0C;
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert unk10")) {
            GrassMgr->unk10 = original_unk10;
        }

        // Sliders for small integers (safe)
        int u8_val = GrassMgr->unk02;
        if (ImGui::SliderInt("unk02 (byte)", &u8_val, 0, 255)) {
            GrassMgr->unk02 = static_cast<std::uint8_t>(u8_val);
        }
        int u16_val = GrassMgr->unk04;
        if (ImGui::SliderInt("unk04 (word)", &u16_val, 0, 65535)) {
            GrassMgr->unk04 = static_cast<std::uint16_t>(u16_val);
        }
        int u32_val = GrassMgr->unk08;
        if (ImGui::InputScalar("unk08 (dword)", ImGuiDataType_U32, &u32_val)) {
            GrassMgr->unk08 = static_cast<std::uint32_t>(u32_val);
        }
        int u32_val2 = GrassMgr->unk0C;
        if (ImGui::InputScalar("unk0C (dword)", ImGuiDataType_U32, &u32_val2)) {
            GrassMgr->unk0C = static_cast<std::uint32_t>(u32_val2);
        }

        // For large 64-bit fields, just show editable hex (risky to change blindly)
        std::uint64_t val10 = GrassMgr->unk10;
        if (ImGui::InputScalar("unk10 (possibly ptr/ID)", ImGuiDataType_U64, &val10, nullptr, nullptr, "%llX")) {
            GrassMgr->unk10 = val10;
        }

    }

    void __stdcall RenderGrassConfig() {
        // optional: static filter if you want to search by name
        static char filterBuf[128] = "";
        ImGui::InputTextWithHint("##grassFilter", "Filter grass by name...", filterBuf, sizeof(filterBuf));
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            filterBuf[0] = '\0';
        }

        static auto grassConfigs = Settings::GetSingleton()->grassConfigs;
        if (grassConfigs.empty()) {
            ImGui::TextUnformatted("No grass configs loaded.");
            return;
        }

        ImGui::Spacing();
        for (const auto& config : grassConfigs) {
            if (filterBuf[0] != '\0') {
                std::string nameLower = config.name;
                std::string filterLower = filterBuf;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
                if (nameLower.find(filterLower) == std::string::npos) continue;
            }

            // Collapsible per-grass entry
            ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (ImGui::TreeNodeEx(config.name.c_str(), nodeFlags)) {
                // Two-column layout for labels + values
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 140);

                ImGui::TextUnformatted("Can Burn:");
                ImGui::NextColumn();
                ImGui::TextUnformatted(config.canBurn ? "Yes" : "No");
                ImGui::NextColumn();

                ImGui::TextUnformatted("Fuel:");
                ImGui::NextColumn();
                ImGui::Text("%d", config.fuel);
                ImGui::NextColumn();

                ImGui::TextUnformatted("Min Burn Heat:");
                ImGui::NextColumn();
                ImGui::Text("%d", config.minBurnHeat);
                ImGui::NextColumn();

                ImGui::Columns(1);
                ImGui::TreePop();
            }

            ImGui::Separator();
        }

    }

    
    void __stdcall RenderFireConfig() {
        static auto fireSources = Settings::GetSingleton()->fireSources;
        if (fireSources.empty()) {
            ImGui::TextUnformatted("No fire configs loaded.");
            return;
        }
        for (const auto& config : fireSources) {
            ImGui::Text("%s", config.c_str());
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