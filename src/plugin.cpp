#include "logger.h"

#include "Wildfire/Wildfire.h"
#include "Events.h"
#include "MCP.h"
#include "Hooks.h"
#include "Settings.h"
#include "Serialization.h"

void RegisterSerialization() {
    auto* serialization = SKSE::GetSerializationInterface();

    serialization->SetUniqueID('FIRE');

    serialization->SetSaveCallback(Serialization::Save);
    serialization->SetLoadCallback(Serialization::Load);
    //serialization->SetRevertCallback(Serialization::Revert);
}

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        Hooks::InstallHooks();
        // Events::RegisterForEvents(); //Used just for testing
        MCP::Register();
        WildfireMgr::GetSingleton();  // Init
        Settings::GetSingleton();     // Init

    } else if (message->type == SKSE::MessagingInterface::kNewGame ||
               message->type == SKSE::MessagingInterface::kPreLoadGame) {
        WildfireMgr::GetSingleton()->OnGameLoad();
    } else if (message->type == SKSE::MessagingInterface::kSaveGame) {
        WildfireMgr::GetSingleton()->OnGameSave();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    SKSE::Init(skse);
    logger::info("Game version: {}", skse->RuntimeVersion().string());
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    RegisterSerialization();
    return true;
}
