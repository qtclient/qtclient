#pragma once

#include "client/gamecore.h"
#include <client/staticdata.h>
#include "gamewindow/appearancetypelistmodel.h"
#include "gamewindow/cyclopediadialogcontext.h"

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

#include <string>
#include <utility>
#include <vector>

namespace tibia {
namespace analysers {
class TAnalyserStorages;
class TItemPriceStorage;
}
namespace cyclopedia {
class TItemTrackingStorage;
}
namespace quickloot {
class TQuickLootBlackWhitelistStorage;
}
}

namespace tibia {
namespace game {
class TPremiumTimeStorage;
}
namespace gamewindow {

/// The "Basic Item Details" of an inspected type (CIP: TInspectObjectQMLData
/// as built by the item page,).
class TItemInspectData final : public cip::tibia::gamewindow::TInspectObjectQMLData
{
    Q_OBJECT
public:
    TItemInspectData(const QString& name, uint16_t typeId, const std::vector<std::pair<std::string, std::string>>& descriptions,
                     QObject* parent = nullptr);
};

/// The Cyclopedia's item page, ItemInfoDialog.qml (CIP:
/// TItemInfoDialogController, metacall).
class TItemInfoDialogController final : public cip::tibia::gamewindow::TItemInfoDialogController
{
    Q_OBJECT
public:
    explicit TItemInfoDialogController(QObject* parent = nullptr);
    ~TItemInfoDialogController() override;

    /// The session's premium state (none: a free account).
    void setPremiumTimeStorage(game::TPremiumTimeStorage* storage);
    /// The page left for another tab: category, name search, filter buttons, type.
    TCyclopediaDialogContext captureContext() const;
    /// The page must be shown; another tab's context throws.
    void restoreContext(const TCyclopediaDialogContext& CyclopediaContext);
    /// CIP : the list narrowed to `typeId`, which is selected.
    void showType(uint16_t typeId);

    bool hasItemSelected() const override { return m_typeId != 0; }
    /// CIP .
    bool hasWeaponProficiency() const override;
    /// CIP .
    bool isPremium() const override;
    int primaryLootValueSource() const override;
    /// CIP / /.
    QString userDefinedValueString() const override;
    QString marketValueString() const override;
    QString lootValueString() const override;
    QString lootValueBackgroundUrlSource() const override;
    /// CIP.
    bool isTracked() const override;
    bool canTrackAdditionalItems() const override;
    /// CIP /.
    bool isLootBlackWhitelisted() const override;
    bool canBlackWhitelistAdditionalItems() const override;
    int blackWhiteListType() const override;

public Q_SLOTS:
    /// CIP.
    void showDialogTab() override;
    void hideDialogTab() override;
    /// CIP.
    void requestCategorySelect(int CategoryIdentifier, bool FilterByLevel, bool FilterByVocation, bool OnlyOneHanWeapons,
                               bool OnlyTwoHandWeapons) override;
    void requestNameSelect(const QString& NameFilter, bool FilterByLevel, bool FilterByVocation, bool OnlyOneHanWeapons,
                           bool OnlyTwoHandWeapons) override;
    /// CIP .
    void requestItemSelect(int AppearanceId) override;
    /// CIP /.
    void requestPrimaryLootValueSelect(int PrimaryLootValueSource) override;
    void requestCustomSaleValue(const QString& CustomSaleValueText) override;
    void requestOpenStoreWithPremiumTime() override;
    /// CIP.
    void requestItemTracking(bool TrackItem) override;
    void requestItemBlackWhitelisting(bool BlackWhitelistItem) override;
    /// CIP .
    void onClassificationFilterWasSelected(int SelectedIndex, const QString& NameFilter, bool FilterByLevel, bool FilterByVocation,
                                           bool OnlyOneHandWeapons, bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    /// CIP .
    void onWeaponProficiencyDialogButtonClicked() override;

private:
    /// CIP : one entry per type that is its own cyclopedia type.
    void rebuildEntries();
    /// CIP .
    void buildCategories();

public:
    /// The category identifiers of the item lists, in the order the
    /// drop-downs show them (gold and unsorted sorted in by name).
    static std::vector<int> categoryIdentifiers();
    /// A cyclopedia type's list entry, untiered; its NPC filter names are
    /// the NPCs that buy it.
    static TAppearanceTypeEntry cyclopediaEntry(const TAppearanceTypePtr& type);

private:
    /// CIP .
    bool categorySupportsTiers(int category) const;
    /// CIP .
    TAppearanceTypeFilter currentFilter() const;
    /// CIP .
    void updateAvailableClassifications();
    void setEnableTier(bool enable);
    void setClassificationIndex(int index);
    void setButtons(bool level, bool vocation, bool oneHand, bool twoHand);
    /// The "loading" details, no NPC data, no type; the inspect request is
    /// cancelled.
    void resetInspect();
    /// Emits the type's value, tracking, selection and list signals on a change.
    void setSelectedTypeId(uint16_t typeId);
    void setPrimaryLootValueSource(int source);
    /// The character's item settings changed: the type's loot value source
    /// again, and its value, tracking and list state.
    void onCharacterConfigurationChanged();
    void onInspectObjectDataReceived(const ItemInspectionData& data);
    void setInspectData(TItemInspectData* data);
    /// CIP .
    void updateNpcData();
    /// CIP .
    void onInspectRequestTimeoutReached();

    analysers::TAnalyserStorages* storages() const;
    analysers::TItemPriceStorage* prices() const;
    cyclopedia::TItemTrackingStorage* itemTracking() const;
    quickloot::TQuickLootBlackWhitelistStorage* lootBlackWhitelist() const;
    bool hasCharacter() const;
    /// itemTracking arrives with the game session; the page is built earlier.
    void ensureItemTrackingConnection();

    /// The item classes of the forge data, sorted.
    std::vector<int> m_classifications;
    TAppearanceTypeListModel m_model;
    TAppearanceTypeListModelSortFilterProxy m_proxy;
    bool m_entriesBuilt = false;
    bool m_shown = false;
    bool m_itemTrackingConnected = false;
    const quickloot::TQuickLootBlackWhitelistStorage* m_connectedLootBlackWhitelist = nullptr;
    QMetaObject::Connection m_lootBlackWhitelistConnection;
    uint16_t m_typeId = 0;
    QPointer<game::TPremiumTimeStorage> m_premiumTimeStorage;
    int m_primaryLootValueSource = 0;
    int m_category = -1;
    QString m_name;
    bool m_buttons[4] = {};
    QTimer m_inspectTimer;
    qint64 m_lastInspect = 0;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
