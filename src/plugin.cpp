#include "logger.h"
#include "Events.h"
#include "Hooks.h"
#include "MCP.h"
#include "Utils.h"
#include "Settings.h"
#include "HazardMgr.h"
#include "WildfireMgr.h"

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        Hooks::InstallHooks();
        HazardMgr::GetSingleton()->InitializeHazards();
        Settings::GetSingleton()->LoadSettings();
        MCP::Register();

    }
    if (message->type == SKSE::MessagingInterface::kPreLoadGame) {
        WildfireMgr::GetSingleton()->ResetAllFireCells();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}
