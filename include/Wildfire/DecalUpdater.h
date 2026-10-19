#pragma once

class DecalUpdater {
public:
    DecalUpdater();

    void AddDecal(const RE::BGSDecalManager::CreationData& creationData, bool forceDecal,
                  RE::BGSDecalGroup* decalGroup);
    bool SpawnBurnDecalAtPosition(const RE::NiPoint3& worldPosition);

private:
    RE::BGSImpactDataSet* defaultImpactDataSet{nullptr};
};
