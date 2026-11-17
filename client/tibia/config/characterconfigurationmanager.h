#pragma once

#include <cip_gen.h>

#include <QJsonObject>
#include <QString>

#include <vector>

namespace tibia {
namespace config {

class TSidebarOptions;

/// What a per-character configuration storage gives the manager (CIP:
/// ICharacterConfigurationBase's name / toJson / fromJson). The storage's
/// QObject side is cip::tibia::config::ICharacterConfigurationBase, whose
/// configurationChangedFromUserInput marks the configuration dirty.
class ICharacterConfiguration
{
public:
    virtual ~ICharacterConfiguration() = default;
    /// The file's base name in the character's data directory.
    virtual QString configurationName() const = 0;
    virtual QJsonObject configurationToJson() const = 0;
    virtual void configurationFromJson(const QJsonObject& json) = 0;
    /// Host: qtclient's storages outlive a session (CIP builds them per
    /// session), so a new character starts them from their defaults.
    virtual void resetConfiguration() = 0;
};

/// Loads and saves every per-character configuration storage in
/// <characterdata>/<character id>/<name>.json: on the player's creature
/// loading (saving the previous character's first), and on save while any
/// storage changed. A missing sidebars file starts the sidebar widgets from
/// CIP's default layout.
class TCharacterConfigurationManager final : public cip::tibia::config::TCharacterConfigurationManager
{
    Q_OBJECT
public:
    struct TConfiguration
    {
        ICharacterConfiguration* configuration = nullptr;
        cip::tibia::config::ICharacterConfigurationBase* object = nullptr;
    };

    /// The storages in load order; `sidebarOptions` is one of them.
    TCharacterConfigurationManager(std::vector<TConfiguration> configurations, TSidebarOptions* sidebarOptions,
                                   QObject* parent = nullptr);
    ~TCharacterConfigurationManager() override;

    /// Writes every storage when one changed since the last load or save.
    void saveConfiguration();
    /// Reads every storage of the character; 0 loads nothing.
    void loadConfigurationForCharacter(quint32 characterId);
    bool isConfigurationLoaded() const { return m_loaded; }
    /// Whether the character had a sidebars file.
    bool isSidebarConfigurationLoadedFromFile() const { return m_sidebarLoadedFromFile; }
    TSidebarOptions* sidebarOptions() const { return m_sidebarOptions; }

public Q_SLOTS:
    /// Saves, then loads the player's character if it is another one.
    void onPlayerCreatureLoaded(quint32 characterId);
    void onConfigurationChanged();

private:
    void save(const TConfiguration& configuration) const;
    bool load(const TConfiguration& configuration) const;

    std::vector<TConfiguration> m_configurations;
    TSidebarOptions* m_sidebarOptions = nullptr;
    quint32 m_characterId = 0;
    bool m_loaded = false;
    bool m_dirty = false;
    bool m_sidebarLoadedFromFile = false;
};

}
}
