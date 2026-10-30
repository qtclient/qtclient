#pragma once

#include "client/gamecore.h"
#include "gamewindow/appearancetypelistmodel.h"

#include <cip_gen.h>

#include <QTimer>

#include <string>
#include <utility>
#include <vector>

namespace tibia::cyclopedia {
class TItemInfoStorage;
}

namespace tibia::gamewindow {

/// The "Basic Item Details" of an inspected type (CIP: TInspectObjectQMLData
/// as built by the item page, 0x100cb1630).
class TItemInspectData final : public cip::tibia::gamewindow::TInspectObjectQMLData
{
    Q_OBJECT
public:
    TItemInspectData(const QString& name, uint16_t typeId, const std::vector<std::pair<std::string, std::string>>& descriptions,
                     QObject* parent = nullptr);
};

/// The Cyclopedia's item page, ItemInfoDialog.qml (CIP:
/// TItemInfoDialogController, metacall 0x100827f90).
class TItemInfoDialogController final : public cip::tibia::gamewindow::TItemInfoDialogController
{
    Q_OBJECT
public:
    /// The page left for another tab (CIP 0x100cad740 / 0x100cb18e0).
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
    /// CIP 0x100cb18e0; the page must be shown.
    void restoreContext(const Context& context);
    /// CIP 0x100cada00: the list narrowed to `typeId`, which is selected.
    void showType(uint16_t typeId);

    bool hasItemSelected() const override { return m_typeId != 0; }
    /// CIP 0x100cadd30.
    bool hasWeaponProficiency() const override;
    /// CIP 0x100cadd60.
    bool isPremium() const override;
    int primaryLootValueSource() const override;
    /// CIP 0x100cadee0 / 0x100cadd80 / 0x100cae010 / 0x100cae600.
    QString userDefinedValueString() const override;
    QString marketValueString() const override;
    QString lootValueString() const override;
    QString lootValueBackgroundUrlSource() const override;
    /// CIP 0x100cae170 / 0x100cae260.
    bool isTracked() const override;
    bool canTrackAdditionalItems() const override;
    /// CIP 0x100cae430 / 0x100cae520 / 0x100cae340.
    bool isLootBlackWhitelisted() const override;
    bool canBlackWhitelistAdditionalItems() const override;
    int blackWhiteListType() const override;

public Q_SLOTS:
    /// CIP 0x100cb0f40 / 0x100cb1880.
    void showDialogTab() override;
    void hideDialogTab() override;
    /// CIP 0x100cae7f0 / 0x100caea70.
    void requestCategorySelect(int CategoryIdentifier, bool FilterByLevel, bool FilterByVocation, bool OnlyOneHanWeapons,
                               bool OnlyTwoHandWeapons) override;
    void requestNameSelect(const QString& NameFilter, bool FilterByLevel, bool FilterByVocation, bool OnlyOneHanWeapons,
                           bool OnlyTwoHandWeapons) override;
    /// CIP 0x100cada30.
    void requestItemSelect(int AppearanceId) override;
    /// CIP 0x100cb0120 / 0x100cb04a0 / 0x100cb06a0.
    void requestPrimaryLootValueSelect(int PrimaryLootValueSource) override;
    void requestCustomSaleValue(const QString& CustomSaleValueText) override;
    void requestOpenStoreWithPremiumTime() override;
    /// CIP 0x100cb0840 / 0x100cb0970.
    void requestItemTracking(bool TrackItem) override;
    void requestItemBlackWhitelisting(bool BlackWhitelistItem) override;
    /// CIP 0x100cb0a70.
    void onClassificationFilterWasSelected(int SelectedIndex, const QString& NameFilter, bool FilterByLevel, bool FilterByVocation,
                                           bool OnlyOneHandWeapons, bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    /// CIP 0x100cb0e40.
    void onWeaponProficiencyDialogButtonClicked() override;

private:
    /// CIP 0x100cad470: one entry per type that is its own cyclopedia type.
    void rebuildEntries();
    /// CIP 0x100d59210.
    void buildCategories();
    /// CIP 0x100d6d980.
    bool categorySupportsTiers(int category) const;
    /// CIP 0x100caa910.
    TAppearanceTypeFilter currentFilter() const;
    /// CIP 0x100caa330.
    void updateAvailableClassifications();
    void setEnableTier(bool enable);
    void setClassificationIndex(int index);
    void setButtons(bool level, bool vocation, bool oneHand, bool twoHand);
    /// CIP 0x100caeca0: the "loading" details, no NPC data, no type.
    void resetInspect();
    void setInspectData(TItemInspectData* data);
    /// CIP 0x100caf2a0.
    void updateNpcData();
    /// CIP 0x100cad0a0.
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
