#pragma once

#include "config/icharacterconfiguration.h"

#include <cip_gen.h>

#include <QString>

#include <array>

namespace tibia {
namespace analysers {
class TItemPriceStorage;
class TGainWasteStorage;
class TImpactStorage;
class TDamageInputStorage;
class TProgressStorage;
class THuntingSessionStorage;
}
namespace cyclopedia {
class TItemTrackingStorage;
class TCyclopediaMapCharacterConfigurationStorage;
}
namespace questlog {
class TQuestLogStorage;
}
namespace quickloot {
class TQuickLootBlackWhitelistStorage;
}
namespace game {
class TStatusbarStorage;
class TOutfitDialogStorage;
}
namespace skillwheel {
class TSkillWheelStorage;
}
namespace config {

class TSidebarOptions;
class TActionBarOptions;

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

    /// The session's character configuration storages; a null one is left out.
    struct TStorages
    {
        analysers::TItemPriceStorage* itemPrice = nullptr;
        cyclopedia::TItemTrackingStorage* itemTracking = nullptr;
        questlog::TQuestLogStorage* questLog = nullptr;
        analysers::TGainWasteStorage* gainWaste = nullptr;
        analysers::TImpactStorage* impact = nullptr;
        analysers::TDamageInputStorage* damageInput = nullptr;
        analysers::TProgressStorage* progress = nullptr;
        analysers::THuntingSessionStorage* huntingSession = nullptr;
        quickloot::TQuickLootBlackWhitelistStorage* quickLootBlackWhitelist = nullptr;
        TSidebarOptions* sidebar = nullptr;
        TActionBarOptions* actionBar = nullptr;
        game::TStatusbarStorage* statusbar = nullptr;
        cyclopedia::TCyclopediaMapCharacterConfigurationStorage* cyclopediaMap = nullptr;
        game::TOutfitDialogStorage* outfitDialog = nullptr;
        skillwheel::TSkillWheelStorage* skillWheel = nullptr;
    };

    explicit TCharacterConfigurationManager(const TStorages& storages, QObject* parent = nullptr);
    ~TCharacterConfigurationManager() override;

    /// Writes every storage when one changed since the last load or save.
    void saveConfiguration();
    /// Reads every storage of the character; 0 loads nothing.
    void loadConfigurationForCharacter(quint32 characterId);
    bool isConfigurationLoaded() const { return m_loaded; }
    /// Whether the character had a sidebars file.
    bool isSidebarConfigurationLoadedFromFile() const { return m_sidebarLoadedFromFile; }

    analysers::TItemPriceStorage* itemPriceStorage() const { return m_storages.itemPrice; }
    cyclopedia::TItemTrackingStorage* itemTrackingStorage() const { return m_storages.itemTracking; }
    analysers::TGainWasteStorage* gainWasteStorage() const { return m_storages.gainWaste; }
    analysers::TImpactStorage* impactStorage() const { return m_storages.impact; }
    analysers::TDamageInputStorage* damageInputStorage() const { return m_storages.damageInput; }
    analysers::TProgressStorage* progressStorage() const { return m_storages.progress; }
    analysers::THuntingSessionStorage* huntingSessionStorage() const { return m_storages.huntingSession; }
    quickloot::TQuickLootBlackWhitelistStorage* quickLootBlackWhitelistStorage() const { return m_storages.quickLootBlackWhitelist; }
    TSidebarOptions* sidebarOptions() const { return m_storages.sidebar; }
    TActionBarOptions* actionBarOptions() const { return m_storages.actionBar; }
    game::TStatusbarStorage* statusbarStorage() const { return m_storages.statusbar; }
    cyclopedia::TCyclopediaMapCharacterConfigurationStorage* cyclopediaMapCharacterConfigurationStorage() const
    {
        return m_storages.cyclopediaMap;
    }
    skillwheel::TSkillWheelStorage* skillWheelStorage() const { return m_storages.skillWheel; }

public Q_SLOTS:
    /// Saves, then loads the player's character if it is another one.
    void onPlayerCreatureLoaded(quint32 characterId);
    void onConfigurationChanged();

private:
    static constexpr std::size_t ConfigurationCount = 15;

    void save(const TConfiguration& configuration) const;
    bool load(const TConfiguration& configuration) const;

    TStorages m_storages;
    /// The storages in CIP's member order (item prices first, skill wheel last).
    std::array<TConfiguration, ConfigurationCount> m_configurations;
    quint32 m_characterId = 0;
    bool m_loaded = false;
    bool m_dirty = false;
    bool m_sidebarLoadedFromFile = false;
};

}
}
