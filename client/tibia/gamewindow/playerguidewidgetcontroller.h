#pragma once

#include "client/gamecore.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QPointer>

#include <vector>

namespace tibia::config {
class TSidebarOptions;
}

namespace tibia::gamewindow {

/// PlayerGuideWidget.qml CipSoft shell (CIP: TPlayerGuideWidgetController).
/// Newhaven / tutorial player-guide entries are not published on CipSoft today —
/// the widget stays an empty entries pane. The manager still listens for
/// onUnlockedSpells so CipSoft can later surface level-up auto-spell rows.
class TPlayerGuideWidgetController final
    : public TSidebarWidgetController<cip::tibia::gamewindow::TPlayerGuideWidgetController>
{
    Q_OBJECT
public:
    TPlayerGuideWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);
    void onUnlockedSpells(const std::vector<uint16_t>& spellIds);

public Q_SLOTS:
    void saveWidgetOptions() override;

public:
    Q_INVOKABLE void showLatestUnlockedPlayerGuide() override {}
    Q_INVOKABLE void showPreviousUnlockedPlayerGuide(int) override {}
    Q_INVOKABLE void showLevelSpecialPopup() override {}

private:
    QPointer<config::TSidebarOptions> m_options;
};

class TPlayerGuideWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TPlayerGuideWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TPlayerGuideWidgetManager() override;

    TSidebarWidget* open(config::TSidebarOptions* options);
    void close();
    TSidebarWidget* current() const;

private:
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<TPlayerGuideWidgetController> m_controller;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
