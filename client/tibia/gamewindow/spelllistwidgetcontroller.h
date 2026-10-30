#pragma once

#include "client/gamecore.h"
#include "gamewindow/magicalarchivedialogcontroller.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "gamewindow/uifocusmanager.h"

#include <cip_gen.h>

#include <QPointer>

#include <vector>

namespace tibia::config {
class TSidebarOptions;
}

namespace tibia::gamewindow {

/// spelllistwidget.qml (CIP: TSpellListWidgetController). Reuses the Magical
/// Archive spell model + filter proxy. Archive rows refresh on
/// onUnlockedSpells / onSpellDataModification.
class TSpellListWidgetController final
    : public TSidebarWidgetController<cip::tibia::gamewindow::TSpellListWidgetController>
{
    Q_OBJECT
public:
    TSpellListWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);
    void reloadArchive();

public Q_SLOTS:
    void onShowContextMenu() override;
    void onShowMoreForSpell(int SpellID) override;
    void saveWidgetOptions() override;
    void takeFocus() override;
    void releaseFocus() override;

private:
    TMagicalArchiveSpellListModel* m_model;
    TSpellListDataModelSortFilterProxy* m_proxy;
    TUiFocusManager m_focus;
    QPointer<config::TSidebarOptions> m_options;
};

class TSpellListWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TSpellListWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TSpellListWidgetManager() override;

    TSidebarWidget* open(config::TSidebarOptions* options);
    void close();
    TSidebarWidget* current() const;

private:
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<TSpellListWidgetController> m_controller;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
