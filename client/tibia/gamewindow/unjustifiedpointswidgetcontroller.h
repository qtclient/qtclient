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

/// unjustifiedpointswidget.qml (CIP: TUnjustifiedPointsWidgetController).
class TUnjustifiedPointsWidgetController final
    : public TSidebarWidgetController<cip::tibia::gamewindow::TUnjustifiedPointsWidgetController>
{
    Q_OBJECT
public:
    TUnjustifiedPointsWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);

public Q_SLOTS:
    void saveWidgetOptions() override;

private:
    void refresh();

    std::vector<client::gamecore::Hook> m_hooks;
    QPointer<config::TSidebarOptions> m_options;
};

class TUnjustifiedPointsWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TUnjustifiedPointsWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TUnjustifiedPointsWidgetManager() override;

    TSidebarWidget* open(config::TSidebarOptions* options);
    void close();
    TSidebarWidget* current() const;

private:
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<TUnjustifiedPointsWidgetController> m_controller;
};

}
