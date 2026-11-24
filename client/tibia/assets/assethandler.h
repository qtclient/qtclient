#pragma once

#include "assets/asset.h"
#include "client/staticdata.h"

#include <QObject>

#include <functional>
#include <memory>
#include <vector>

namespace tibia::shared {
class TReadOnlyMemoryBlob;
}
namespace tibia::sprites {
class TSpritesheetManager;
class TSpritesheetsMetadataManager;
}
namespace tibia::appearances {
class TAppearancesManager;
}
namespace tibia::sound {
class TSoundManager;
class TSoundMetadataManager;
}
namespace tibia::game {
class TTraderStorage;
}
namespace tibia::proficiency {
class TProficiencyStorage;
}
namespace tibia::cyclopedia {
class TCyclopediaMapStaticDataStorage;
}

namespace tibia {
namespace assets {

/// Hands every loaded asset to the object that uses it (CIP assets::TAssetHandler):
/// spritesheets and map images to the spritesheet manager, the appearances to the
/// appearances manager (then the NPC sale objects to the trader storage), the
/// static data and static map data to the client's callbacks, the proficiency
/// tables, the Cyclopedia map data, and the sound bank. Asset requests of the map
/// storage come out again as newAssetDataToLoad.
class TAssetHandler : public QObject
{
    Q_OBJECT
public:
    using TStaticInformationHandler = std::function<void(TStaticInformation)>;
    using TStaticMapDataHandler = std::function<void(std::vector<THouseMapArea>)>;

    TAssetHandler(std::shared_ptr<sprites::TSpritesheetManager> SpritesheetManager,
                  std::shared_ptr<appearances::TAppearancesManager> AppearancesManager,
                  std::shared_ptr<sound::TSoundMetadataManager> SoundMetadataManager,
                  std::shared_ptr<sound::TSoundManager> SoundManager,
                  std::shared_ptr<game::TTraderStorage> TraderStorage,
                  proficiency::TProficiencyStorage* ProficiencyStorage,
                  TStaticInformationHandler StaticInformationHandler,
                  TStaticMapDataHandler StaticMapDataHandler,
                  std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> CyclopediaMapStaticDataStorage,
                  QObject* parent = nullptr);

Q_SIGNALS:
    void newAssetDataToLoad(tibia::assets::TAsset AssetToLoad, bool Prioritized);

public Q_SLOTS:
    /// Dispatches the asset's data by its type. An appearances asset before the
    /// spritesheet metadata throws std::runtime_error.
    virtual void handleAssetData(const tibia::assets::TAsset& Asset, const std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>& pData);
    virtual void handleSpritesheetMetadata(std::shared_ptr<tibia::sprites::TSpritesheetsMetadataManager> pSpritesheetsMetadataManager);

private:
    std::shared_ptr<sprites::TSpritesheetManager> m_pSpritesheetManager;
    std::shared_ptr<appearances::TAppearancesManager> m_pAppearancesManager;
    std::shared_ptr<sound::TSoundMetadataManager> m_pSoundMetadataManager;
    std::shared_ptr<sound::TSoundManager> m_pSoundManager;
    std::shared_ptr<game::TTraderStorage> m_pTraderStorage;
    proficiency::TProficiencyStorage* m_pProficiencyStorage = nullptr;
    TStaticInformationHandler m_StaticInformationHandler;
    TStaticMapDataHandler m_StaticMapDataHandler;
    std::shared_ptr<sprites::TSpritesheetsMetadataManager> m_pSpritesheetsMetadataManager;
    std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> m_pCyclopediaMapStaticDataStorage;
};

}
}
