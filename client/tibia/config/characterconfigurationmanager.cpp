#include "characterconfigurationmanager.h"

#include "analysers/analyserstorages.h"
#include "analysers/itempricestorage.h"
#include "config/actionbaroptions.h"
#include "config/sidebaroptions.h"
#include "cyclopedia/cyclopediamapcharacterconfigurationstorage.h"
#include "cyclopedia/itemtrackingstorage.h"
#include "quickloot/quicklootblackwhiteliststorage.h"
#include "game/gameciptypes.h"
#include "game/statusbarstorage.h"
#include "questlog/questlogciptypes.h"
#include "skillwheel/skillwheelstorage.h"
#include "shared/tibiafilesystemhelper.h"

#include <QJsonDocument>

namespace tibia::config {

namespace {

constexpr std::size_t HuntingSessionIndex = 7;
constexpr std::size_t SidebarIndex = 9;
/// CIP loads and saves the hunting session before the progress storage.
constexpr std::array<std::size_t, 15> StorageOrder{ 0, 1, 2, 3, 4, 5, 7, 6, 8, 9, 10, 11, 12, 13, 14 };

/// CIP's sidebar widgets for a character without a sidebars file: the
/// primary battle list and widgets 24 and 10 in the first sidebar.
TSidebarWidgetsManagerOptions defaultSidebarWidgets()
{
    TSidebarWidgetsManagerOptions options;
    options.openWidgetsOrderPerSidebar = { { { 1, TSidebarOptions::PrimaryBattleListId }, { 24, 0 }, { 10, 0 } } };
    options.leftSidebarCount = 0;
    return options;
}

}

namespace {

template<typename Storage>
TCharacterConfigurationManager::TConfiguration configuration(Storage* storage)
{
    return { storage, storage };
}

}

TCharacterConfigurationManager::TCharacterConfigurationManager(const TStorages& storages, QObject* parent)
    : cip::tibia::config::TCharacterConfigurationManager(parent)
    , m_storages(storages)
    , m_configurations{ configuration(storages.itemPrice),     configuration(storages.itemTracking),
                        configuration(storages.questLog),      configuration(storages.gainWaste),
                        configuration(storages.impact),        configuration(storages.damageInput),
                        configuration(storages.progress),      configuration(storages.huntingSession),
                        configuration(storages.quickLootBlackWhitelist), configuration(storages.sidebar.get()),
                        configuration(storages.actionBar.get()), configuration(storages.statusbar.get()),
                        configuration(storages.cyclopediaMap), configuration(storages.outfitDialog),
                        configuration(storages.skillWheel) }
{
    // Every storage but the hunting session marks the configuration dirty (CIP connects the
    // progress storage twice instead).
    for (std::size_t index = 0; index < m_configurations.size(); ++index) {
        const TConfiguration& entry = m_configurations[index];
        if (entry.object && index != HuntingSessionIndex)
            connect(entry.object, &cip::tibia::config::ICharacterConfigurationBase::configurationChangedFromUserInput, this,
                    &TCharacterConfigurationManager::onConfigurationChanged);
    }
}

TCharacterConfigurationManager::~TCharacterConfigurationManager()
{
    saveConfiguration();
}

void TCharacterConfigurationManager::save(const TConfiguration& configuration) const
{
    const shared::TTibiaFileSystemHelper fileSystem;
    const QDir directory = fileSystem.getCharacterDataDirectory(QString::number(m_characterId));
    const QString path = fileSystem.getFilePathInDirectory(directory, configuration.configuration->configurationName() + QStringLiteral(".json"));
    fileSystem.writeJsonFile(path, QJsonDocument(configuration.configuration->configurationToJson()));
}

bool TCharacterConfigurationManager::load(const TConfiguration& configuration) const
{
    const shared::TTibiaFileSystemHelper fileSystem;
    const QDir directory = fileSystem.getCharacterDataDirectory(QString::number(m_characterId));
    const QString path = fileSystem.getFilePathInDirectory(directory, configuration.configuration->configurationName() + QStringLiteral(".json"));
    const QJsonDocument document = fileSystem.readJsonFile(path);
    if (!document.isObject())
        return false;
    configuration.configuration->configurationFromJson(document.object());
    return true;
}

void TCharacterConfigurationManager::saveConfiguration()
{
    if (!m_dirty || m_characterId == 0)
        return;
    for (const std::size_t index : StorageOrder)
        if (m_configurations[index].configuration)
            save(m_configurations[index]);
    m_dirty = false;
}

void TCharacterConfigurationManager::loadConfigurationForCharacter(const quint32 characterId)
{
    m_characterId = characterId;
    if (characterId != 0) {
        for (const TConfiguration& entry : m_configurations)
            if (entry.configuration)
                entry.configuration->resetConfiguration();
        bool sidebarLoaded = true;
        for (const std::size_t index : StorageOrder) {
            const TConfiguration& entry = m_configurations[index];
            if (!entry.configuration)
                continue;
            const bool loaded = load(entry);
            if (index == SidebarIndex) {
                sidebarLoaded = loaded;
                if (!loaded)
                    m_storages.sidebar->setWidgetsManagerOptions(defaultSidebarWidgets());
            }
        }
        m_sidebarLoadedFromFile = sidebarLoaded;
    }
    m_loaded = true;
    Q_EMIT configurationLoadedForCharacter();
}

void TCharacterConfigurationManager::onPlayerCreatureLoaded(const quint32 characterId)
{
    saveConfiguration();
    if (characterId != 0 && characterId != m_characterId)
        loadConfigurationForCharacter(characterId);
}

void TCharacterConfigurationManager::onConfigurationChanged()
{
    m_dirty = true;
    Q_EMIT configurationChangedFromUserInput();
}

}
