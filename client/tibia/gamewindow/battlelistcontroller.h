#pragma once

#include "client/gamecore.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "input/gameaction.h"
#include "input/gameactionmapper.h"

#include <cip_gen.h>

#include <client/declarations.h>

#include <QColor>
#include <QPointer>
#include <QTimer>

#include <array>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace tibia::config {
class TSidebarOptions;
}

namespace tibia::qmlcomponents {
class TWorldMapRenderProvider;
}

namespace tibia::gamewindow {

class TSidebarManager;
class TBattleListControllerStorage;
class TBattleListDataModelSortFilterProxy;
class TBattleListGameActionHandler;
class TGenericEnterTextDialogController;

/// One entry of sidebars.json's battleListsOptions, keyed by battle list id
/// (CIP: TBattleListWidgetOptions; ids 0..19, 253 the party list).
struct BattleListOptions
{
    using Filter = cip::tibia::gamewindow::TBattleListController::EBattleListWidgetFilterKey;
    /// CIP 0x100d3ea90's sort keys: by age, distance, hit points and name,
    /// each ascending then descending.
    enum Sort { ByAgeAscending = 0, ByAgeDescending, ByDistanceAscending, ByDistanceDescending, ByHitpointsAscending,
                ByHitpointsDescending, ByNameAscending, ByNameDescending };

    std::set<Filter> filters;
    std::array<Sort, 2> sortOrder{ ByAgeAscending, ByAgeAscending };
    int contentHeight = 38;
    bool contentMaximized = true;
    bool isPartyView = false;
    bool isPrimary = false;
    QString name;
    bool showFilters = false;

    bool operator==(const BattleListOptions&) const = default;
};

/// One row of a battle list (CIP: TBattleListDrawCommand, read by
/// TBattleListDataModel::data 0x100d3b500).
struct BattleListEntry
{
    uint32_t creatureId = 0;
    QString name;
    float healthPercent = 0;
    QColor healthColor;
    float manaPercent = 0;
    bool showMana = false;
    QVariantList frameColors;
    QStringList statusIcons;
    uint outfitId = 0;
    bool outfitIsObject = false;
    QColor headColor;
    QColor torsoColor;
    QColor legsColor;
    QColor detailColor;
    bool firstAddOn = false;
    bool secondAddOn = false;
    uint visibleOrder = 0;
    uint distance = 0;
    bool temporarySelected = false;
    bool inViewport = false;
    bool inSharedXpDistance = false;
    bool fiendish = false;
    bool leader = false;
    bool leaderMinion = false;
    int vocation = 0;

    bool operator==(const BattleListEntry&) const = default;
};

/// battleListModel (CIP: TBattleListDataModel, roles 0x100d3db40).
class TBattleListDataModel final : public cip::tibia::gamewindow::TBattleListDataModel
{
    Q_OBJECT
public:
    using cip::tibia::gamewindow::TBattleListDataModel::TBattleListDataModel;

    void setEntries(std::vector<BattleListEntry> entries);
    const std::vector<BattleListEntry>& entries() const { return m_entries; }

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<BattleListEntry> m_entries;
};

/// One battle list widget, battlelist.qml (CIP: TBattleListController,
/// static meta object 0x100805f80).
class TBattleListController final : public TSidebarWidgetController<cip::tibia::gamewindow::TBattleListController>
{
    Q_OBJECT
public:
    TBattleListController(int battleListId, TBattleListControllerStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine,
                          QObject* parent = nullptr);
    ~TBattleListController() override;

    int battleListId() const { return m_id; }
    void setControlScheme(int controlScheme) { m_mapper.setControlScheme(controlScheme); }
    /// The list's options (name, filters, sort order, view) and size.
    void applyOptions(const BattleListOptions& options);
    void setEntries(std::vector<BattleListEntry> entries) { m_model.setEntries(std::move(entries)); }
    void setPremium(bool premium);

    bool isFilterActive(EBattleListWidgetFilterKey filterKey) override;

public Q_SLOTS:
    /// CIP 0x100ba8230: rename (the rename dialog with the current name,
    /// FUN_100ba5e80), secondary list, make primary and sort order.
    void onShowConfigurationMenu() override;
    void onRequestClose() override;
    void onFilterButtonsVisibleClicked() override;
    void saveWidgetOptions() override;
    void toggleFilter(EBattleListWidgetFilterKey filterKey) override;
    /// CIP 0x100ba5b50: premium publishes
    /// OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR (0xff, 0xff), otherwise
    /// OPEN_STORE on the premium page (open action 1).
    void requestOpenSecondaryBattleList() override;
    void requestMakePrimary() override;
    void removeThisBattleListOptionsIfNotSpecialBattleList() override;

Q_SIGNALS:
    void battleListClosed(int battleListId);

private Q_SLOTS:
    /// CIP 0x100ba7120.
    void onCurrentlyHoveredCreatureChanged(int creatureId);
    /// CIP 0x100ba7700.
    void onCreatureClicked(int creatureId, int mouseButton, int keyboardModifiers);
    void onCreatureTargetSelected(int creatureId);
    void onContextMenuClosed();
    /// CIP 0x100ba4e00: the HTML-escaped text becomes the name (empty: the
    /// default caption), then the options are saved.
    void onRenameTextEntered(const QString& text);

private:
    int mapperContext() const;
    void setOptions(const BattleListOptions& options);
    void temporarySelect(uint32_t creatureId);
    /// CIP 0x100ba6060 (USE_AT_MOUSECURSOR 0x100ba6190, SPELL_AT_MOUSE_CURSOR
    /// 0x100ba67f0): the hovered entry's TARGET_SELECTED creature takes the
    /// object (USE_ON_CREATURE) or the spell (Talk aimed, mode 2, at its field).
    bool onAtMouseCursor(const input::TGameAction& action);

    int m_id;
    TBattleListControllerStorage* m_storage;
    TBattleListDataModel m_model;
    TBattleListDataModelSortFilterProxy* m_proxy = nullptr;
    /// CIP +0xc0: "battlelist_rename" / "battlelist_rename_text", 30
    /// characters, empty text accepted (+0xbc).
    TGenericEnterTextDialogController* m_renameDialog;
    input::TGameActionMapper m_mapper;
    BattleListOptions m_options;
    uint32_t m_hoveredCreature = 0;
    uint32_t m_lockedCreature = 0;
    std::vector<int> m_actionListeners;
};

/// The open battle lists and their options (CIP: TBattleListControllerStorage,
/// static meta object 0x1008067b0, with TBattleListGameActionHandler's attack
/// first / next / previous target).
class TBattleListControllerStorage final : public cip::tibia::gamewindow::TBattleListControllerStorage
{
    Q_OBJECT
public:
    static constexpr int PartyListId = 253;
    static constexpr int MaxBattleLists = 20;

    TBattleListControllerStorage(TSidebarManager* sidebars, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TBattleListControllerStorage() override;

    void setSidebarOptions(config::TSidebarOptions* options);
    void setControlScheme(int controlScheme);
    /// Where TEMPORARY_SELECT_CREATURE puts the hovered creature.
    void setWorldMapRenderProvider(qmlcomponents::TWorldMapRenderProvider* provider) { m_worldMapRenderProvider = provider; }

    /// The primary list's id (isPrimary in the options, else 0).
    int primaryId() const;
    bool isOpen(int battleListId) const { return m_controllers.contains(battleListId); }
    /// Opens the list in `sidebarId` (0xff or a missing sidebar: the first
    /// with room), creating its options if it has none.
    TBattleListController* open(int battleListId, uint sidebarId);
    void close(int battleListId);
    void closeAll();
    std::vector<std::pair<int, TSidebarWidget*>> widgets() const;

    const BattleListOptions& options(int battleListId) const;
    void setOptions(int battleListId, const BattleListOptions& options);
    void removeOptions(int battleListId);
    /// CIP: OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR with a new id.
    void openSecondary(uint sidebarId);
    void makePrimary(int battleListId);
    bool hasPremium() const;

    /// The list's creatures, filtered and sorted (CIP: filterDrawCommands and
    /// 0x100d3ea90).
    std::vector<CreaturePtr> creatures(const BattleListOptions& options) const;
    uint32_t temporarySelectedCreature() const { return m_temporarySelected; }
    void setTemporarySelectedCreature(uint32_t creatureId);

    /// CIP 0x100baabb0 / 0x100baae20 (TBattleListGameActionHandler slots).
    bool attackFirstTarget();
    bool attackNextOrPreviousTarget(bool previous);

public Q_SLOTS:
    void triggerUpdate() override;
    void onLoginRampUpFinished() override;

Q_SIGNALS:
    /// EWidgetType 1 (primary list) and 2 (party list).
    void widgetVisible(int widgetType);
    void widgetHidden(int widgetType);

private:
    void readOptions();
    void writeOptions();
    void refresh();
    void emitVisibility(int battleListId, bool visible);
    BattleListEntry entryOf(const CreaturePtr& creature, const LocalPlayerPtr& player) const;

    QPointer<TSidebarManager> m_sidebars;
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<config::TSidebarOptions> m_sidebarOptions;
    qmlcomponents::TWorldMapRenderProvider* m_worldMapRenderProvider = nullptr;
    std::map<int, BattleListOptions> m_options;
    std::map<int, TBattleListController*> m_controllers;
    std::map<uint32_t, uint> m_visibleOrder;
    uint m_nextVisibleOrder = 1;
    uint32_t m_temporarySelected = 0;
    int m_controlScheme = 0;
    QTimer m_updateTimer;
    TBattleListGameActionHandler* m_gameActionHandler = nullptr;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
