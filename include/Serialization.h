#pragma once

#include "Wildfire/Wildfire.h"

// Based on Containerize Serialiozation, Credits to Quantumyilmaz
// https://github.com/QTR-Modding/Containerize/tree/95c82e55cc9a153a000bbb92581a2caadaabb84f

namespace Serialization {

    enum : std::uint32_t { kCellStateRecord = 'CELL', kRefStateRecord = 'REFS' };

    enum : std::uint32_t { kSerializationVersion = 1 };

    void SaveCellStates(SKSE::SerializationInterface* a_intfc) {
        logger::info("[Serialization] SaveCellStates started");
        auto cellStateMap = WildfireMgr::GetSingleton()->GetCellStateMapCopy();

        std::uint32_t count = 0;

        for (const auto& [cell, state] : cellStateMap) {
            if (!cell || !state.valid) {
                continue;
            }
            ++count;
        }

        if (count == 0) {
            return;
        }

        if (!a_intfc->OpenRecord(kCellStateRecord, kSerializationVersion)) {
            logger::error("[Serialization] failed to open CELL serialization record");
            return;
        }

        if (!a_intfc->WriteRecordData(count)) {
            logger::error("[Serialization] failed to write CELL count");
            return;
        }

        for (const auto& [cell, state] : cellStateMap) {
            if (!cell || !state.valid) {
                continue;
            }

            const auto cellFormID = cell->GetFormID();

            if (!a_intfc->WriteRecordData(cellFormID)) {
                logger::error("[Serialization] failed to write cell FormID {:08X}", cellFormID);
            }

            if (!a_intfc->WriteRecordData(&state.heat[0][0],
                                          sizeof(state.heat[0][0]) * CELL_QUAD_COUNT * VERTS_PER_QUAD)) {
                logger::error("[Serialization] failed to write cell heat");
            }

            if (!a_intfc->WriteRecordData(&state.usedHeat[0][0],
                                          sizeof(state.usedHeat[0][0]) * CELL_QUAD_COUNT * VERTS_PER_QUAD)) {
                logger::error("[Serialization] failed to write cell usedHeat");
            }

            if (!a_intfc->WriteRecordData(&state.fuel[0][0],
                                          sizeof(state.fuel[0][0]) * CELL_QUAD_COUNT * VERTS_PER_QUAD)) {
                logger::error("[Serialization] failed to write cell fuel");
            }

            if (!a_intfc->WriteRecordData(&state.isBurning[0][0],
                                          sizeof(state.isBurning[0][0]) * CELL_QUAD_COUNT * VERTS_PER_QUAD)) {
                logger::error("[Serialization] failed to write cell isBurning");
            }

            if (!a_intfc->WriteRecordData(&state.isCharred[0][0],
                                          sizeof(state.isCharred[0][0]) * CELL_QUAD_COUNT * VERTS_PER_QUAD)) {
                logger::error("[Serialization] failed to write cell isCharred");
            }
        }

        logger::info("[Serialization] saved {} cell states", count);
    }

    void SaveRefStates(SKSE::SerializationInterface* a_intfc) {
        logger::info("[Serialization] SaveRefStates started");
        auto refStateMap = WildfireMgr::GetSingleton()->GetRefStateMapCopy();

        std::uint32_t count = 0;

        for (const auto& [refHandle, state] : refStateMap) {
            if (auto ref = refHandle.get().get()) {
                if (ref->IsDisabled() || ref->IsDeleted()) {
                    continue;
                }
                ++count;
            } else {
                continue;
            }
        }

        if (count == 0) {
            return;
        }

        if (!a_intfc->OpenRecord(kRefStateRecord, kSerializationVersion)) {
            logger::error("[Serialization] failed to open REFS serialization record");
            return;
        }

        if (!a_intfc->WriteRecordData(count)) {
            logger::error("[Serialization] failed to write REFS count");
            return;
        }

        for (const auto& [refHandle, state] : refStateMap) {
            if (auto ref = refHandle.get().get()) {
                if (ref->IsDisabled() || ref->IsDeleted()) {
                    continue;
                }

                const auto refFormID = ref->GetFormID();

                if (!a_intfc->WriteRecordData(refFormID)) {
                    logger::error("[Serialization] failed to write ref FormID {:08X}", refFormID);
                }

                if (!a_intfc->WriteRecordData(&state.heat, sizeof(state.heat))) {
                    logger::error("[Serialization] failed to write ref heat");
                }

                if (!a_intfc->WriteRecordData(&state.fuel, sizeof(state.fuel))) {
                    logger::error("[Serialization] failed to write ref fuel");
                }

                if (!a_intfc->WriteRecordData(&state.isBurning, sizeof(state.isBurning))) {
                    logger::error("[Serialization] failed to write ref isBurning");
                }

                if (!a_intfc->WriteRecordData(&state.isCharred, sizeof(state.isCharred))) {
                    logger::error("[Serialization] failed to write ref isCharred");
                }

                if (!a_intfc->WriteRecordData(&state.isFrozen, sizeof(state.isFrozen))) {
                    logger::error("[Serialization] failed to write ref isFrozen");
                }

                if (!a_intfc->WriteRecordData(&state.halfEvent, sizeof(state.halfEvent))) {
                    logger::error("[Serialization] failed to write ref halfEvent");
                }

                if (state.originalBaseObj) {
                    const auto originalBaseObjFormID = state.originalBaseObj->GetFormID();
                    if (!a_intfc->WriteRecordData(originalBaseObjFormID)) {
                        logger::error("[Serialization] failed to write ref originalBaseObjFormID {:08X}", refFormID);
                    }
                } else {
                    RE::FormID emptyFormID;
                    a_intfc->WriteRecordData(emptyFormID);
                    logger::error("[Serialization] failed to find ref originalBaseObj");
                }

            } else {
                continue;
            }

        }

        logger::info("[Serialization] saved {} ref states", count);
    }

    void LoadCellStates(SKSE::SerializationInterface* a_intfc, std::uint32_t a_version, std::uint32_t a_length) {
        if (a_version != kSerializationVersion) {
            logger::warn("[Serialization] unsupported CELL serialization version {}", a_version);
            return;
        }

        std::uint32_t count = 0;

        if (!a_intfc->ReadRecordData(count)) {
            logger::error("[Serialization] failed to read CELL count");
            return;
        }

        auto* manager = WildfireMgr::GetSingleton();

        for (std::uint32_t i = 0; i < count; ++i) {
            RE::FormID savedFormID;
            bool errored = false;

            if (!a_intfc->ReadRecordData(savedFormID)) {
                logger::error("[Serialization] failed to read cell FormID");
                errored = true;
            }

            RE::FormID resolvedFormID;

            if (!a_intfc->ResolveFormID(savedFormID, resolvedFormID)) {
                logger::warn("[Serialization] failed to resolve cell FormID {:08X}", savedFormID);
                errored = true;
            }

            auto* cell = RE::TESForm::LookupByID<RE::TESObjectCELL>(resolvedFormID);

            if (!cell) {
                logger::warn("[Serialization] failed to find cell {:08X}", resolvedFormID);
                errored = true;
            }

            auto state = CellState(cell);

            if (!a_intfc->ReadRecordData(state.heat, sizeof(state.heat))) {
                logger::error("[Serialization] failed to read cell heat");
                errored = true;
            }

            if (!a_intfc->ReadRecordData(state.usedHeat, sizeof(state.usedHeat))) {
                logger::error("[Serialization] failed to read cell usedHeat");
                errored = true;
            }

            if (!a_intfc->ReadRecordData(state.fuel, sizeof(state.fuel))) {
                logger::error("[Serialization] failed to read cell fuel");
                errored = true;
            }

            if (!a_intfc->ReadRecordData(state.isBurning, sizeof(state.isBurning))) {
                logger::error("[Serialization] failed to read cell isBurning");
                errored = true;
            }

            if (!a_intfc->ReadRecordData(state.isCharred, sizeof(state.isCharred))) {
                logger::error("[Serialization] failed to read cell isCharred");
                errored = true;
            }

            if (errored) {
                logger::error("[Serialization] failed to load CELL data from SKSE co-save");
            } else {
                state.valid = true;

                manager->AddCellState(cell, state);
            }
        }

        logger::info("[Serialization] loaded {} cell states", count);
    }

    void LoadRefStates(SKSE::SerializationInterface* a_intfc, std::uint32_t a_version, std::uint32_t a_length) {
        if (a_version != kSerializationVersion) {
            logger::warn("[Serialization] unsupported REFS serialization version {}", a_version);
            return;
        }

        std::uint32_t count = 0;

        if (!a_intfc->ReadRecordData(count)) {
            logger::error("[Serialization] failed to read REFS count");
            return;
        }

        auto* manager = WildfireMgr::GetSingleton();

        for (std::uint32_t i = 0; i < count; ++i) {
            RE::FormID savedFormID;
            bool errored = false;

            if (!a_intfc->ReadRecordData(savedFormID)) {
                logger::error("[Serialization] failed to read ref FormID");
                errored = true;
            }

            RE::FormID resolvedFormID;

            if (!a_intfc->ResolveFormID(savedFormID, resolvedFormID)) {
                logger::warn("[Serialization] failed to resolve ref FormID {:08X}", savedFormID);
                errored = true;
            }

            auto* ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(resolvedFormID);

            if (!ref) {
                logger::warn("[Serialization] failed to find ref {:08X}", resolvedFormID);
                errored = true;
            }

            auto state = RefState(ref);

            if (!a_intfc->ReadRecordData(state.heat)) {
                logger::error("[Serialization] failed to read ref heat");
                errored = true;
            }

            if (!a_intfc->ReadRecordData(state.fuel)) {
                logger::error("[Serialization] failed to read ref fuel");
                errored = true;
            }

            if (!a_intfc->ReadRecordData(state.isBurning)) {
                logger::error("[Serialization] failed to read ref isBurning");
                errored = true;
            }

            if (!a_intfc->ReadRecordData(state.isCharred)) {
                logger::error("[Serialization] failed to read ref isCharred");
                errored = true;
            }

            if (!a_intfc->ReadRecordData(state.isFrozen)) {
                logger::error("[Serialization] failed to read ref isFrozen");
                errored = true;
            }

            if (!a_intfc->ReadRecordData(state.halfEvent)) {
                logger::error("[Serialization] failed to read ref halfEvent");
                errored = true;
            }

            RE::FormID originalBaseFormID;
            if (!a_intfc->ReadRecordData(originalBaseFormID)) {
                logger::error("[Serialization] failed to read ref base FormID");
                errored = true;
            }

            RE::FormID resolvedOriginalBaseFormID;

            if (!a_intfc->ResolveFormID(originalBaseFormID, resolvedOriginalBaseFormID)) {
                logger::warn("[Serialization] failed to resolve ref base FormID {:08X}", originalBaseFormID);
                errored = true;
            }

            auto* refBaseObj = RE::TESForm::LookupByID<RE::TESBoundObject>(resolvedOriginalBaseFormID);

            if (!refBaseObj) {
                logger::warn("[Serialization] failed to find ref base {:08X}", resolvedOriginalBaseFormID);
                errored = true;
            }

            state.originalBaseObj = refBaseObj;

            if (errored) {
                logger::error("[Serialization] failed to load REFS data from SKSE co-save");
            } else {
                manager->AddRefState(ref, state);
            }
        }

        logger::info("[Serialization] loaded {} ref states", count);
    }

    void Save(SKSE::SerializationInterface* intfc) {
        SaveCellStates(intfc);
        SaveRefStates(intfc);
    }

    void Load(SKSE::SerializationInterface* a_intfc) {
        std::uint32_t type;
        std::uint32_t version;
        std::uint32_t length;

        while (a_intfc->GetNextRecordInfo(type, version, length)) {
            switch (type) {
                case kCellStateRecord:
                    LoadCellStates(a_intfc, version, length);
                    break;

                case kRefStateRecord:
                    LoadRefStates(a_intfc, version, length);
                    break;

                default:
                    logger::warn("[Serialization] unknown serialization record {:08X}", type);
                    break;
            }
        }
    }

}  // Serialization