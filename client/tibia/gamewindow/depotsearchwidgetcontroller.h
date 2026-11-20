#pragma once

#include "client/gamecore.h"
#include "gamewindow/appearanceinstancelistmodel.h"
#include "gamewindow/appearancetypelistmodel.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "gamewindow/uifocusmanager.h"
#include "input/gameaction.h"
#include "input/gameactionmapper.h"

#include <cip_gen.h>

#include <QPointer>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace tibia {
namespace config {
class TSidebarOptions;
}
namespace container {
class TDepotSearchDetailListStorage;
}
namespace game {
class TGameSessionBase;
}
}

namespace tibia {
namespace gamewindow {

class TSidebarManager;

/// The depot search sidebar widget (CIP: TDepotSearchWidgetController,
/// DepotSearchWidget.qml). The search list holds every cyclopedia type (one
/// row per tier of a classified type) with the locker's counts from the
/// session's object counts; picking a type asks the server for its detail
/// list, which the session's TDepotSearchDetailListStorage holds and the
/// stash list and the depot and inbox grids show.
class TDepotSearchWidgetController final : public TSidebarWidgetController<cip::tibia::gamewindow::TDepotSearchWidgetController>
{
    Q_OBJECT

public:
    TDepotSearchWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TDepotSearchWidgetController() override;

    /// CIP v12: binds the lists to the session's storages (null unbinds).
    void setGameSession(game::TGameSessionBase* session);
    /// The options the widget keeps (CIP: the session's sidebar options).
    void setSidebarOptions(config::TSidebarOptions* options);

    bool showDetailList() const override;
    QString detailListObjectName() const override;
    uint totalCountDepot() const override;
    uint totalCountStash() const override;
    uint totalCountInbox() const override;
    QString totalCountDepotString() const override;
    QString totalCountStashString() const override;
    QString totalCountInboxString() const override;
    bool isAppearanceAllowedToRetrieve() const override;
    bool showLockerOnly() const override;
    bool enableTierAndClassificationSelection() const override { return m_enableTierAndClassificationSelection; }
    bool enableTierSelection() const override { return m_enableTierSelection; }
    void setEnableTierSelection(bool enable) override;
    int indexOfSelectedTier() const override { return m_indexOfSelectedTier; }
    void setIndexOfSelectedTier(int index) override;
    int indexOfSelectedClassification() const override { return m_indexOfSelectedClassification; }
    void setIndexOfSelectedClassification(int index) override;
    int indexOfSelectedNPC() const override { return m_indexOfSelectedNPC; }
    void setIndexOfSelectedNPC(int index) override;
    QStringList availableTiers() const override;
    QStringList availableClassifications() const override;
    QStringList availableNPCs() const override { return m_availableNPCs; }

    bool hasFocus() const override { return m_focus.hasFocus(); }

public Q_SLOTS:
    void takeFocus() override;
    void releaseFocus() override;

    /// CIP v32: the server's search closes with the widget.
    void onRequestClose() override;
    /// CIP v36: back from the detail list to the search.
    void onLevelUpClicked() override;
    /// CIP v37.
    void saveWidgetOptions() override;

    void requestCategorySelect(int ClassificationIdentifier, bool FilterByLevel, bool FilterByVocation, bool OnlyOneHanWeapons,
                               bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    void requestNameSelect(const QString& NameFilter, bool FilterByLevel, bool FilterByVocation, bool OnlyOneHanWeapons,
                           bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    void requestItemSelect(int AppearanceId, int UpgradeTier) override;
    void requestItemChosen(int AppearanceId, int UpgradeTier) override;
    void onClassificationFilterWasSelected(int SelectedIndex, const QString& NameFilter, bool FilterByLevel, bool FilterByVocation,
                                           bool OnlyOneHandWeapons, bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    void onTierFilterWasSelected(int SelectedIndex, const QString& NameFilter, bool FilterByLevel, bool FilterByVocation,
                                 bool OnlyOneHandWeapons, bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    void onNpcFilterWasSelected(int SelectedIndex, const QString& NameFilter, bool FilterByLevel, bool FilterByVocation,
                                bool OnlyOneHandWeapons, bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    void onSearchedItemClicked(int AppearanceId, int UpgradeTier, int MouseButtonRaw, int KeyboardModifiersRaw) override;
    void requestSearch() override;
    void requestRefresh() override;
    void requestRetrieveFromStash(int TypeID) override;
    void requestRetrieveFromDepotInbox(int StorageLocation) override;
    void slotClicked(int StorageLocation, int SlotIDRaw, int MouseButtonRaw, int KeyboardModifiersRaw) override;
    void slotEntered(int StorageLocation, int SlotIDRaw) override;
    void slotExited(int StorageLocation, int SlotIDRaw) override;
    void emptySlotAreaEntered() override;
    void emptySlotAreaExited() override;
    void slotStartDrag(int StorageLocation, int SlotIDRaw, QObject* pSource) override;
    void slotTargetSelected(int StorageLocation, int SlotIDRaw) override;

private Q_SLOTS:
    void onWidgetOptionsChanged();
    /// CIP: the stash list and the depot and inbox grids follow the
    /// session's detail list; the search field has the focus while no
    /// detail list is shown.
    void onDetailListChanged();

private:
    /// CIP TSidebarWidgetControllerWithFocusHandling +0xa0.
    TUiFocusManager m_focus;
    /// QML storage locations.
    enum EStorageLocation
    {
        Depot = 0,
        Stash = 1,
        Inbox = 2,
    };

    /// CIP v38: the widget options to the widget and the search list.
    void applyWidgetOptions();
    /// A category picked: the classification and tier drop-downs follow
    /// whether the category has classified types.
    void setCategory(int category);
    /// Whether any type of `category` is classified.
    bool categorySupportsTiers(int category) const;
    /// The row of the category in `categories`, or -1.
    int categoryIndex(int identifier) const;
    /// The drop-downs' selections with the buttons.
    TAppearanceTypeFilter currentFilter(bool filterByLevel, bool filterByVocation, bool oneHand, bool twoHand) const;
    /// The filter to the search list: the name if there is one, else the
    /// category.
    void applyFilter(const QString& nameFilter, bool filterByLevel, bool filterByVocation, bool oneHand, bool twoHand, bool showLockerOnly);
    void publishSimpleAction(input::EGameAction type);
    /// The mouse cursor over the hovered slot.
    void updateHoverCursor();
    /// The actions of a depot or inbox slot, or of the stash entry.
    input::TGameActionList slotActions(int storageLocation, int slotId) const;
    /// The actions of a search list row.
    input::TGameActionList typeActions(uint16_t typeId) const;

    game::TGameSessionBase* m_session = nullptr;
    QPointer<config::TSidebarOptions> m_options;
    TAppearanceTypeListModel m_searchModel;
    TAppearanceTypeListModelSortFilterProxy m_searchProxy;
    TAppearanceTypeListModel m_stashModel;
    TAppearanceTypeListModelSortFilterProxy m_stashProxy;
    TAppearanceInstanceListModel m_depotModel;
    TAppearanceInstanceListModel m_inboxModel;
    uint16_t m_selectedTypeId = 0;
    std::optional<uint8_t> m_selectedTier;
    int m_hoveredSlot = -1;
    int m_hoveredStorageLocation = -1;
    int m_lastCategory = -1;
    QString m_nameFilter;
    std::shared_ptr<container::TDepotSearchDetailListStorage> m_detailList;
    input::TGameActionMapper m_mapper;
    std::vector<QMetaObject::Connection> m_sessionConnections;
};

/// Host: the controller the client keeps for its sidebar.
class TDepotSearchWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TDepotSearchWidgetManager(TSidebarManager* sidebars, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TDepotSearchWidgetManager() override;

    void setSidebarOptions(config::TSidebarOptions* options);
    TSidebarWidget* current() const;


Q_SIGNALS:
    /// The controller's publishGameAction, forwarded.
    void publishGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    /// Results or a detail list arrived: the widget shows in a sidebar.
    void show();

    QPointer<TSidebarManager> m_sidebars;
    TDepotSearchWidgetController* m_controller = nullptr;
};

}
}
