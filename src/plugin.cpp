#include "logger.h"
#include "Hooks.h"
#include "MCP.h"
#include "Utils.h"
#include "Settings.h"
#include "HazardManager.h"

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        Hooks::InstallAddImpactHooks();
        Hooks::UpdateHook::Install();
        HazardMgr::GetSingleton()->InitializeHazards();
        MCP::Register();
    }
    if (message->type == SKSE::MessagingInterface::kNewGame || message->type == SKSE::MessagingInterface::kPostLoadGame) {
        // Post-load
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}
