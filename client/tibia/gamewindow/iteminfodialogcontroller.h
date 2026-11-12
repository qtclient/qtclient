#pragma once

#include "client/gamecore.h"
#include "gamewindow/appearancetypelistmodel.h"

#include <cip_gen.h>

#include <QTimer>

#include <string>
#include <utility>
#include <vector>

namespace tibia {
namespace cyclopedia {
class TItemInfoStorage;
}
}

namespace tibia {
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
    /// The page left for another tab (CIP).
    struct Context
    {
        int category = -1;
        QString name;
        bool buttons[4] = {};
        uint16_t typeId = 0;
        bool operator==(const Context& other) const;
    };

    explicit TItemInfoDialogController(QObject* parent = nullptr);
    ~TItemInfoDialogController() override;

    Context captureContext() const;
    /// CIP ; the page must be shown.
    void restoreContext(const Context& context);
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
    /// CIP .
    bool categorySupportsTiers(int category) const;
    /// CIP .
    TAppearanceTypeFilter currentFilter() const;
    /// CIP .
    void updateAvailableClassifications();
    void setEnableTier(bool enable);
    void setClassificationIndex(int index);
    void setButtons(bool level, bool vocation, bool oneHand, bool twoHand);
    /// CIP : the "loading" details, no NPC data, no type.
    void resetInspect();
    void setInspectData(TItemInspectData* data);
    /// CIP .
    void updateNpcData();
    /// CIP .
    void sendInspect();

    cyclopedia::TItemInfoStorage* m_storage;
    TAppearanceTypeListModel m_model;
    TAppearanceTypeListModelSortFilterProxy m_proxy;
    bool m_entriesBuilt = false;
    bool m_shown = false;
    uint16_t m_typeId = 0;
    int m_category = -1;
    QString m_name;
    bool m_buttons[4] = {};
    QTimer m_inspectTimer;
    qint64 m_lastInspect = 0;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
