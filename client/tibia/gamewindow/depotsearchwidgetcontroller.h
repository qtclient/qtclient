#pragma once

#include "client/gamecore.h"
#include "gamewindow/appearancetypelistmodel.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <client/staticdata.h>
#include <cip_gen.h>

#include <QHash>
#include <QPair>
#include <QPointer>
#include <QString>
#include <QVariant>

#include <cstdint>
#include <vector>

namespace tibia {
namespace config {
class TSidebarOptions;
}
}

namespace tibia {
namespace gamewindow {

class TSidebarManager;

/// CipSoft DepotSearchWidget.qml — CIP TDepotSearchWidgetController with real
/// appearance models + tibia::game:: depot-search peels / notify hooks.
class TDepotSearchWidgetController final : public TSidebarWidgetController<cip::tibia::gamewindow::TDepotSearchWidgetController>
{
    Q_OBJECT

public:
    TDepotSearchWidgetController(TSidebarManager* sidebars, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);

    QVariant appearanceTypeListModel() const override;
    QVariant stashAppearanceTypeListModel() const override;
    QVariant depotAppearanceTypeListModel() const override;
    QVariant inboxAppearanceTypeListModel() const override;
    QList<QObject*> categories() const override;

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
    bool enableTierAndClassificationSelection() const override;
    bool enableTierSelection() const override;
    int indexOfSelectedTier() const override;
    int indexOfSelectedClassification() const override;
    QStringList availableTiers() const override;
    QStringList availableClassifications() const override;
    int indexOfSelectedNPC() const override;
    QStringList availableNPCs() const override;

    /// CIP TSidebarWidgetControllerBase chrome.
    QQuickItem* gameWindowQuickItem() const override;
    /// CIP TSidebarWidgetControllerWithFocusHandling.
    bool hasFocus() const override;

public Q_SLOTS:
    void saveWidgetOptions() override;
    void requestSearch() override;
    void requestRefresh() override;
    void requestCategorySelect(int ClassificationIdentifier, bool FilterByLevel, bool FilterByVocation, bool OnlyOneHanWeapons,
                               bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    void requestNameSelect(const QString& NameFilter, bool FilterByLevel, bool FilterByVocation, bool OnlyOneHanWeapons,
                           bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    void requestItemSelect(int AppearanceId, int UpgradeTier) override;
    void requestItemChosen(int AppearanceId, int UpgradeTier) override;
    void onSearchedItemClicked(int AppearanceId, int UpgradeTier, int MouseButtonRaw, int KeyboardModifiersRaw) override;
    void onClassificationFilterWasSelected(int SelectedIndex, const QString& NameFilter, bool FilterByLevel, bool FilterByVocation,
                                           bool OnlyOneHandWeapons, bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    void onTierFilterWasSelected(int SelectedIndex, const QString& NameFilter, bool FilterByLevel, bool FilterByVocation,
                                  bool OnlyOneHandWeapons, bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    void onNpcFilterWasSelected(int SelectedIndex, const QString& NameFilter, bool FilterByLevel, bool FilterByVocation,
                                 bool OnlyOneHandWeapons, bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    void requestRetrieveFromStash(int TypeID) override;
    void requestRetrieveFromDepotInbox(int StorageLocation) override;
    void slotClicked(int StorageLocation, int SlotIDRaw, int MouseButtonRaw, int KeyboardModifiersRaw) override;
    void slotEntered(int StorageLocation, int SlotIDRaw) override;
    void slotExited(int StorageLocation, int SlotIDRaw) override;
    void emptySlotAreaEntered() override;
    void emptySlotAreaExited() override;
    void slotStartDrag(int StorageLocation, int SlotIDRaw, QObject* pSource) override;
    void slotTargetSelected(int StorageLocation, int SlotIDRaw) override;
    void onRequestClose() override;
    void onCloseClicked() override;
    /// CIP sidebar chrome (battlelist/container pattern).
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    /// CIP: level-up from detail list returns to search results (QML levelUpButtonVisible).
    void onLevelUpClicked() override;
    void setCursorToResizeVertical(bool Enabled) override;
    void startContentSizeChangedTimer();
    void takeFocus() override;
    void releaseFocus() override;

Q_SIGNALS:
    /// CIP sidebar chrome (TSidebarWidgetControllerBase) — tip NOTIFY deepen.
    void contentSizeChanged();
    void positionInSidebarChanged();
    void sidebarIdChanged();
    void widgetClosed();
    /// CIP sidebar chrome (drag/move) — tip redeclare; forwarded from chrome slots.
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, tibia::gamewindow::TSidebarWidget* widget);
    /// CIP moc signal (shared_ptr arg skipped from cip_gen); tip name parity.
    void publishGameAction();
    void focusChanged();
    void requestWidgetControlToTakeFocus();
    /// CIP index NOTIFYs — tip forwards after cip:: setters in filter slots.
    /// CIP filter/list NOTIFYs — tip redeclare (members filled; no dynamic filter source yet).
    void categoryListSelectionRequest();

private Q_SLOTS:
    /// CIP private slots — options / detail refresh hooks from QML.
    void onWidgetOptionsChanged();
    void onDetailListChanged();
    void onWidgetQuickItemDestroyed();

private:
    void persistOptions();
    void placeInSidebar();
    void applyResults(const std::vector<DepotSearchResultItem>& items);
    void applyDetail(const DepotSearchDetail& detail);
    void clearDetail();
    int countOf(uint16_t typeId, uint8_t tier) const;
    TAppearanceTypeListModel* modelForStorage(int storageLocation) const;
    uint8_t retrieveTypeForStorage(int storageLocation) const;
    bool entryAt(int storageLocation, int slotId, uint16_t& typeId, uint8_t& tier) const;

    QPointer<TSidebarManager> m_sidebars;
    QPointer<config::TSidebarOptions> m_options;
    TAppearanceTypeListModel* m_resultsModel = nullptr;
    TAppearanceTypeListModelSortFilterProxy* m_resultsProxy = nullptr;
    TAppearanceTypeListModel* m_stashModel = nullptr;
    TAppearanceTypeListModel* m_depotModel = nullptr;
    TAppearanceTypeListModel* m_inboxModel = nullptr;
    QHash<QPair<uint16_t, uint8_t>, int> m_counts;
    uint16_t m_detailTypeId = 0;
    uint8_t m_detailTier = 0;
    std::vector<client::gamecore::Hook> m_hooks;
};

class TDepotSearchWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TDepotSearchWidgetManager(TSidebarManager* sidebars, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TDepotSearchWidgetManager() override;

    void setSidebarOptions(config::TSidebarOptions* options);
    TSidebarWidget* open(config::TSidebarOptions* options);
    void close();
    TSidebarWidget* current() const;

private:
    QPointer<TSidebarManager> m_sidebars;
    QQuickItem* m_gameWindowItem = nullptr;
    QQmlEngine* m_engine = nullptr;
    TDepotSearchWidgetController* m_controller = nullptr;
};

}
}
