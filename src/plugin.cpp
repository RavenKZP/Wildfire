#include "logger.h"

#include "Wildfire/Wildfire.h"
#include "MCP.h"
#include "Hooks.h"
#include "Settings.h"

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        Hooks::InstallHooks();
        MCP::Register();
        WildfireMgr::GetSingleton();  // Init
        Settings::GetSingleton();     // Init

    } else if (message->type == SKSE::MessagingInterface::kNewGame ||
               message->type == SKSE::MessagingInterface::kPostLoadGame) {
        logger::info("kPostLoadGame or kNewGame");
        WildfireMgr::GetSingleton()->OnGameLoad();
    } else if (message->type == SKSE::MessagingInterface::kSaveGame) {
        logger::info("kSaveGame");
        WildfireMgr::GetSingleton()->OnGameSave();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}
