#pragma once

#include "input/gameactiondispatch.h"
#include "client/gamecore.h"
#include "creatures/battlelistrenderprovider.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "input/gameaction.h"
#include "input/gameactionmapper.h"

#include <cip_gen.h>
#include <qtconnectionmanager.h>

#include <client/declarations.h>

#include <QColor>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QTimer>

#include <array>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace tibia {
namespace creatures {
class TCreature;
}
}

namespace tibia {
namespace config {
class TSidebarOptions;
}
namespace game {
class IGameSession;
class TPremiumTimeStorage;
}
}

namespace tibia {
namespace qmlcomponents {
class TWorldMapRenderProvider;
}
}

namespace tibia {
namespace gamewindow {

class TSidebarManager;
class TBattleListControllerStorage;
class TGenericEnterTextDialogController;

/// One entry of sidebars.json's battleListsOptions, keyed by battle list id
/// (CIP: TBattleListWidgetOptions; ids 0..19, 253 the party list).
struct BattleListOptions
{
    using Filter = cip::tibia::gamewindow::TBattleListController::EBattleListWidgetFilterKey;
    /// CIP 's sort keys: by age, distance, hit points and name,
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
/// TBattleListDataModel::data).
using BattleListEntry = creatures::TBattleListDrawCommand;

/// battleListModel (CIP: TBattleListDataModel, roles).
class TBattleListDataModel final : public cip::tibia::gamewindow::TBattleListDataModel
{
    Q_OBJECT
public:
    using cip::tibia::gamewindow::TBattleListDataModel::TBattleListDataModel;

    /// CIP's update from TBattleListControllerStorage::triggerUpdate: rows
    /// change in place, ordered by creature id.
    void setEntries(std::vector<BattleListEntry> entries);
    const std::vector<BattleListEntry>& entries() const { return m_entries; }
    /// The roles whose data differs between two entries of one creature.
    static QList<int> changedRoles(const BattleListEntry& current, const BattleListEntry& incoming);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<BattleListEntry> m_entries;
};

/// One battle list's view of the rows (CIP: TBattleListDataModelSortFilterProxy):
/// the list's options decide which rows show and their order.
class TBattleListDataModelSortFilterProxy final : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    using Filter = BattleListOptions::Filter;

    explicit TBattleListDataModelSortFilterProxy(QObject* parent = nullptr);

    /// Takes the list's options and re-filters and re-sorts when they changed.
    void setOptions(const BattleListOptions& options);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    BattleListOptions m_options;
};

/// One battle list widget, battlelist.qml (CIP: TBattleListController,
/// static meta object).
class TBattleListController final : public TSidebarWidgetController<cip::tibia::gamewindow::TBattleListController>
{
    Q_OBJECT

public:
    TBattleListController(int battleListId, TBattleListControllerStorage* storage, TBattleListDataModel* model, QQuickItem* gameWindowItem,
                          QQmlEngine* engine, QObject* parent = nullptr);
    ~TBattleListController() override;

    int battleListId() const { return m_id; }
    void setControlScheme(int controlScheme) { m_mapper.setControlScheme(controlScheme); }
    /// The list follows its entry in `options`: it reads it now and on every
    /// change of its id (nullptr detaches).
    void attachSidebarOptions(config::TSidebarOptions* options);
    /// The list's premium state follows `storage` (premiumTimeChanged on
    /// every change and once now).
    void setPremiumTimeStorage(game::TPremiumTimeStorage* storage);
    /// USE_AT_MOUSECURSOR, forwarded by the storage: the hovered entry's
    /// TARGET_SELECTED creature takes the object (USE_ON_CREATURE).
    bool handleGameActionAtMouseCursor(const input::TGameActionUseAtMouseCursor& action);
    /// SPELL_AT_MOUSE_CURSOR, forwarded by the storage: the spell is said aimed
    /// (mode 2) at the field of the hovered entry's TARGET_SELECTED creature.
    bool handleGameActionCastSpellAtMouseCursor(const input::TGameActionSpellAtMouseCursor& action);

    /// The QML properties, read from the list's options.
    bool filterButtonsVisible() const override;
    bool isPartyView() const override;
    QString name() const override;
    bool isPrimary() const override;
    bool hasPremiumStatus() const override;
    QVariant battleListModel() const override;
    bool isFilterActive(EBattleListWidgetFilterKey filterKey) override;

    QQuickItem* gameWindowQuickItem() const override;

public Q_SLOTS:
    /// CIP : rename (the rename dialog with the current name,
    /// ), secondary list, make primary and sort order.
    void onShowConfigurationMenu() override;
    void onRequestClose() override;
    void onCloseClicked() override;
    void onFilterButtonsVisibleClicked() override;
    void saveWidgetOptions() override;
    void toggleFilter(EBattleListWidgetFilterKey filterKey) override;
    /// CIP : premium publishes
    /// OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR (0xff, 0xff), otherwise
    /// OPEN_STORE on the premium page (open action 1).
    void requestOpenSecondaryBattleList() override;
    /// CIP: opens the rename dialog (also used from the configuration menu).
    void requestEditName() override;
    /// CIP : HTML-escaped text becomes the name (empty: default
    /// caption), then the options are saved.
    void onNewNameEntered(const QString& EnteredName) override;
    void requestMakePrimary() override;
    void removeThisBattleListOptionsIfNotSpecialBattleList() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void onLevelUpClicked() override;
    void setCursorToResizeVertical(bool Enabled) override;
    void startContentSizeChangedTimer();
    /// CIP moc handleGameAction(shared_ptr<IGameAction>) — qtclient live path is
    /// the mapper + action listeners; shared_ptr arg skipped (auth pattern).
    void handleGameAction();

Q_SIGNALS:
    /// CIP sidebar chrome (TSidebarWidgetControllerBase) — qtclient NOTIFY deepen.
    void contentSizeChanged();
    void positionInSidebarChanged();
    void sidebarIdChanged();
    void widgetClosed();
    /// CIP sidebar chrome (drag/move) — qtclient redeclare; forwarded from chrome slots.
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, tibia::gamewindow::TSidebarWidget* widget);
    /// CIP moc signal (shared_ptr arg skipped from cip_gen); qtclient name parity.
    /// Tip-owned non-chrome signals.
    void battleListClosed(int battleListId);

private Q_SLOTS:
    /// CIP .
    void onCurrentlyHoveredCreatureChanged(int creatureId);
    /// CIP .
    void onCreatureClicked(int creatureId, int mouseButton, int keyboardModifiers);
    void onCreatureTargetSelected(int creatureId);
    void onContextMenuControllerMenuClosed();
    void onWidgetOptionsChanged();
    void onThisWidgetOptionsChanged(int BattleListID);
    void onWidgetQuickItemDestroyed();

private:
    int mapperContext() const;
    /// Stores `options` with the widget's current height and maximized state.
    void storeOptions(BattleListOptions options);
    /// The entry's context menu: its actions in the battle list's order, the
    /// modifier hints of a left click, and the creature stays selected until
    /// the menu closes.
    void showCreatureContextMenu(const input::TGameActionList& actions);

    int m_id;
    TBattleListControllerStorage* m_storage;
    TBattleListDataModelSortFilterProxy* m_proxy = nullptr;
    /// CIP +0xc0: "battlelist_rename" / "battlelist_rename_text", 30
    /// characters, empty text accepted (+0xbc).
    TGenericEnterTextDialogController* m_renameDialog;
    input::TGameActionMapper m_mapper;
    BattleListOptions m_options;
    uint32_t m_hoveredCreature = 0;
    uint32_t m_lockedCreature = 0;
    QMetaObject::Connection m_contextMenuClosed;
    /// CIP +0xa0: the widget item's three signals and the rename dialog's
    /// textEntered; the destructor disconnects them.
    shared::TQtConnectionManager m_connections;
    QPointer<game::TPremiumTimeStorage> m_premiumTimeStorage;
    QPointer<config::TSidebarOptions> m_sidebarOptions;
    QMetaObject::Connection m_sidebarOptionsChanged;
};

/// The open battle lists (CIP: TBattleListControllerStorage). Battle list ids
/// are 0..19; one list may be the primary list, one the party list. All lists
/// share one data model; each list's proxy filters and sorts it by the list's
/// options, which live in the character's sidebar options.
class TBattleListControllerStorage final : public cip::tibia::gamewindow::TBattleListControllerStorage
{
    Q_OBJECT
public:
    static constexpr int MaxBattleLists = 20;
    /// A requested id or sidebar of 0xff: the first free one.
    static constexpr uint8_t AnyId = 0xff;
    /// CIP's EWidgetType of the primary and the party list.
    static constexpr int PrimaryWidgetType = 1;
    static constexpr int PartyWidgetType = 2;

    TBattleListControllerStorage(TSidebarManager* sidebars, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TBattleListControllerStorage() override;

    static TBattleListControllerStorage* instance();

    void setSidebarOptions(config::TSidebarOptions* options);
    void setControlScheme(int controlScheme);
    /// Keeps the premium time storage and hands it to every list (and to the
    /// lists opened later).
    void setPremiumTimeStorage(game::TPremiumTimeStorage* premiumTimeStorage);
    /// Where TEMPORARY_SELECT_CREATURE puts the hovered creature.
    void setWorldMapRenderProvider(qmlcomponents::TWorldMapRenderProvider* provider) { m_worldMapRenderProvider = provider; }

    TBattleListController* primaryBattleList() const { return m_primary; }
    TBattleListController* partyBattleList() const { return m_party; }
    /// CIP: a new battle list with `requestedId` (taken or 0xff: the first free
    /// id) in `sidebarId`. A primary or party list takes its options to the new
    /// id. A list that is neither needs premium; without it the list closes and
    /// the status line says so. With 20 lists open the status line says that no
    /// more lists can be opened.
    void openBattleList(uint8_t sidebarId, uint8_t requestedId, bool primary, bool party);
    /// CIP: TOGGLE_WIDGET of the primary (1) or the party list (2): an open
    /// list closes, otherwise it opens in `sidebarId`.
    void togglePrimaryBattleList(uint8_t sidebarId);
    void togglePartyBattleList(uint8_t sidebarId);
    /// CIP: OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR, MAKE_BATTLE_LIST_PRIMARY,
    /// TOGGLE_WIDGET of a battle list, and the at-mouse-cursor actions every
    /// list sees.
    bool handleGameAction(const input::IGameAction& action);
    /// CIP: the list leaves the storage; `saveOptions` first saves its options.
    void removeBattleList(uint8_t battleListId, bool saveOptions);
    /// Every list leaves the storage, saving its options.
    void closeAll();
    std::vector<std::pair<int, TSidebarWidget*>> widgets() const;

    /// The list's options from the sidebar options (defaults when it has none).
    BattleListOptions options(int battleListId) const;
    void setOptions(int battleListId, const BattleListOptions& options);
    /// Whether the sidebar options hold an entry for the list.
    bool hasOptions(int battleListId) const;
    config::TSidebarOptions* sidebarOptions() const;
    void removeOptions(int battleListId);
    /// CIP: the list becomes the primary list in the sidebar options.
    void makePrimary(uint8_t battleListId);
    bool hasPremium() const;
    TBattleListDataModel* dataModel() { return &m_dataModel; }

    /// The hovered creature (the creature storage holds it and its frame).
    void setTemporarySelectedCreature(uint32_t creatureId);
    /// The session whose battle list render provider builds the rows.
    void setGameSession(std::shared_ptr<game::IGameSession> GameSession);

public Q_SLOTS:
    /// CIP: the shared model takes the draw commands of every creature.
    void triggerUpdate() override;
    /// CIP: options of lists that are not open and neither primary nor party
    /// are dropped.
    void onLoginRampUpFinished() override;
    /// CIP: the list closed itself (it already moved its options).
    void onBattleListClosed(int battleListId);

Q_SIGNALS:
    /// A list took a USE_AT_MOUSECURSOR / SPELL_AT_MOUSE_CURSOR (the map window
    /// then does not report it unhandled).
    void atMouseCursorGameActionTaken();
    /// EWidgetType 1 (primary list) and 2 (party list).
    void widgetVisible(int widgetType);
    void widgetHidden(int widgetType);

private:
    /// CIP: `requestedId` if it is free, else the lowest free id; false when
    /// 20 lists are open.
    bool acquireBattleListId(uint8_t requestedId, uint8_t* battleListId) const;
    /// The requested id if free, else the lowest free one (0..19); false when
    /// every id is taken.
    bool findFreeBattleListId(uint8_t requestedId, uint8_t* battleListId) const;
    /// CIP: a list opened after login starts maximized.
    void maximizeIfLoggedIn(TBattleListController* controller) const;
    void publishStatusLine(const QString& text, const QColor& color);
    void onUpdateTimer();

    QPointer<TSidebarManager> m_sidebars;
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    TBattleListDataModel m_dataModel;
    QPointer<config::TSidebarOptions> m_sidebarOptions;
    QPointer<game::TPremiumTimeStorage> m_premiumTimeStorage;
    qmlcomponents::TWorldMapRenderProvider* m_worldMapRenderProvider = nullptr;
    TBattleListController* m_primary = nullptr;
    TBattleListController* m_party = nullptr;
    std::map<uint8_t, TBattleListController*> m_controllers;
    std::shared_ptr<game::IGameSession> m_gameSession;
    int m_controlScheme = 0;
    bool m_loginRampUpFinished = false;
    QTimer m_updateTimer;
    std::vector<client::gamecore::Hook> m_hooks;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
