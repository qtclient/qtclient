#include "characterconfigurationmanager.h"

#include "config/sidebaroptions.h"
#include "shared/tibiafilesystemhelper.h"

#include <QJsonDocument>

namespace tibia::config {

namespace {

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

TCharacterConfigurationManager::TCharacterConfigurationManager(std::vector<TConfiguration> configurations,
                                                               TSidebarOptions* sidebarOptions, QObject* parent)
    : cip::tibia::config::TCharacterConfigurationManager(parent)
    , m_configurations(std::move(configurations))
    , m_sidebarOptions(sidebarOptions)
{
    for (const TConfiguration& configuration : m_configurations) {
        if (configuration.object)
            connect(configuration.object, &cip::tibia::config::ICharacterConfigurationBase::configurationChangedFromUserInput,
                    this, &TCharacterConfigurationManager::onConfigurationChanged);
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
    for (const TConfiguration& configuration : m_configurations)
        save(configuration);
    m_dirty = false;
}

void TCharacterConfigurationManager::loadConfigurationForCharacter(const quint32 characterId)
{
    m_characterId = characterId;
    if (characterId != 0) {
        for (const TConfiguration& configuration : m_configurations)
            configuration.configuration->resetConfiguration();
        bool sidebarLoaded = true;
        for (const TConfiguration& configuration : m_configurations) {
            const bool loaded = load(configuration);
            if (m_sidebarOptions && configuration.configuration == static_cast<ICharacterConfiguration*>(m_sidebarOptions)) {
                sidebarLoaded = loaded;
                if (!loaded)
                    m_sidebarOptions->setWidgetsManagerOptions(defaultSidebarWidgets());
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
