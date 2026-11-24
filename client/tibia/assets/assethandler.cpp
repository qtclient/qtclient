#include "assets/assethandler.h"

#include "assets/things.h"
#include "cyclopedia/cyclopediamapstaticdatastorage.h"
#include "game/traderstorage.h"
#include "proficiency/proficiencystorage.h"
#include "readonlymemoryblob.h"
#include "sound/soundmanager.h"
#include "sound/soundmetadatamanager.h"
#include "sprites/spritesheetmanager.h"
#include "sprites/spritesheetsmetadatamanager.h"

#include <stdexcept>

namespace tibia::assets {

TAssetHandler::TAssetHandler(std::shared_ptr<sprites::TSpritesheetManager> SpritesheetManager,
                             std::shared_ptr<appearances::TAppearancesManager> AppearancesManager,
                             std::shared_ptr<sound::TSoundMetadataManager> SoundMetadataManager,
                             std::shared_ptr<sound::TSoundManager> SoundManager,
                             std::shared_ptr<game::TTraderStorage> TraderStorage,
                             proficiency::TProficiencyStorage* ProficiencyStorage,
                             TStaticInformationHandler StaticInformationHandler,
                             TStaticMapDataHandler StaticMapDataHandler,
                             std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> CyclopediaMapStaticDataStorage,
                             QObject* parent)
    : QObject(parent)
    , m_pSpritesheetManager(std::move(SpritesheetManager))
    , m_pAppearancesManager(std::move(AppearancesManager))
    , m_pSoundMetadataManager(std::move(SoundMetadataManager))
    , m_pSoundManager(std::move(SoundManager))
    , m_pTraderStorage(std::move(TraderStorage))
    , m_pProficiencyStorage(ProficiencyStorage)
    , m_StaticInformationHandler(std::move(StaticInformationHandler))
    , m_StaticMapDataHandler(std::move(StaticMapDataHandler))
    , m_pCyclopediaMapStaticDataStorage(std::move(CyclopediaMapStaticDataStorage))
{
    // The map images the Cyclopedia map asks for are loaded like every other asset.
    if (m_pCyclopediaMapStaticDataStorage) {
        connect(m_pCyclopediaMapStaticDataStorage.get(), &cyclopedia::TCyclopediaMapStaticDataStorage::newAssetDataToLoad, this,
                &TAssetHandler::newAssetDataToLoad);
    }
    // The sound bank's in-memory and streamed sound files are loaded like
    // every other asset (their data goes to the sound manager).
    if (m_pSoundMetadataManager) {
        connect(m_pSoundMetadataManager.get(), &sound::TSoundMetadataManager::newAssetDataToLoad, this,
                &TAssetHandler::newAssetDataToLoad);
    }
}

void TAssetHandler::handleAssetData(const TAsset& Asset, const std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>& pData)
{
    switch (Asset.type()) {
    case TAsset::Sprite:
    case TAsset::MapImage:
        m_pSpritesheetManager->addCompressedSpritesheet(Asset.file(), pData);
        break;
    case TAsset::Appearances: {
        if (!m_pSpritesheetsMetadataManager)
            throw std::runtime_error("m_pSpritesheetsMetadataManager == nullptr");
        m_pAppearancesManager->initialize(pData->getData().toStdString(), m_pSpritesheetsMetadataManager);
        m_pAppearancesManager->postprocessAppearances();
        const TAppearanceTypeList objectTypes = m_pAppearancesManager->getNpcSaleObjectTypes();
        m_pTraderStorage->addAppearanceTypes(std::vector<const appearances::TAppearanceType*>(objectTypes.begin(), objectTypes.end()));
        break;
    }
    case TAsset::StaticData: {
        TStaticInformation information = appearances::TAppearancesManager::parseStaticInformation(pData->getData().toStdString());
        if (!m_StaticInformationHandler)
            throw std::bad_function_call();
        m_StaticInformationHandler(std::move(information));
        break;
    }
    case TAsset::StaticMapData: {
        std::vector<THouseMapArea> houseMapAreas = appearances::TAppearancesManager::parseStaticMapData(pData->getData().toStdString());
        if (!m_StaticMapDataHandler)
            throw std::bad_function_call();
        m_StaticMapDataHandler(std::move(houseMapAreas));
        break;
    }
    case TAsset::Proficiencies:
        m_pProficiencyStorage->setTables(proficiency::parseProficiencies(pData->getData()));
        break;
    case TAsset::Map:
        m_pCyclopediaMapStaticDataStorage->initialize(pData->getData());
        break;
    case TAsset::Sounds:
        if (m_pSoundMetadataManager)
            m_pSoundMetadataManager->initialize(pData->getData());
        break;
    case TAsset::Sound:
        m_pSoundManager->addSoundAssetData(Asset.file(), pData);
        break;
    case TAsset::FullMap:
        m_pCyclopediaMapStaticDataStorage->setFullMapFileName(pData->getFilePath());
        break;
    default:
        break;
    }
}

void TAssetHandler::handleSpritesheetMetadata(std::shared_ptr<tibia::sprites::TSpritesheetsMetadataManager> pSpritesheetsMetadataManager)
{
    m_pSpritesheetsMetadataManager = std::move(pSpritesheetsMetadataManager);
}

}
