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

namespace tibia::config {
class TSidebarOptions;
}

namespace tibia::gamewindow {

class TSidebarManager;

/// CipSoft DepotSearchWidget.qml — CIP TDepotSearchWidgetController with real
/// appearance models + g_game depot-search hooks.
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
    void requestRetrieveFromStash(int TypeID) override;
    void requestRetrieveFromDepotInbox(int StorageLocation) override;
    void onRequestClose() override;
    void onCloseClicked() override;

private:
    void persistOptions();
    void placeInSidebar();
    void applyResults(const std::vector<DepotSearchResultItem>& items);
    void applyDetail(const DepotSearchDetail& detail);
    void clearDetail();
    int countOf(uint16_t typeId, uint8_t tier) const;

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
