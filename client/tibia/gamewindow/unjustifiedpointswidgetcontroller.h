#pragma once

#include "client/gamecore.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QPointer>

#include <memory>
#include <vector>

namespace tibia::game {
class IGameSession;
}

namespace tibia {
namespace config {
class TSidebarOptions;
}
}

namespace tibia {
namespace gamewindow {

/// unjustifiedpointswidget.qml (CIP: TUnjustifiedPointsWidgetController).
class TUnjustifiedPointsWidgetController final
    : public TSidebarWidgetController<cip::tibia::gamewindow::TUnjustifiedPointsWidgetController>
{
    Q_OBJECT
public:
    TUnjustifiedPointsWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);
    /// The session's own creature binds the skull display (null: none).
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    float progressDay() const override;
    uint fullKillsToNextSkullDay() const override;
    float progressWeek() const override;
    uint fullKillsToNextSkullWeek() const override;
    float progressMonth() const override;
    uint fullKillsToNextSkullMonth() const override;
    uint skullDuration() const override;
    uint openPvpSituationCount() const override;
    QString currentSkullSource() const override;
    QString nextSkullSource() const override;

    QQuickItem* gameWindowQuickItem() const override;

public Q_SLOTS:
    void saveWidgetOptions() override;
    void onCloseClicked() override;
    void onRequestClose() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void onLevelUpClicked() override;
    void setCursorToResizeVertical(bool Enabled) override;
    void startContentSizeChangedTimer();

Q_SIGNALS:
    /// CIP sidebar chrome (TSidebarWidgetControllerBase) — qtclient NOTIFY deepen.
    void contentSizeChanged();
    void positionInSidebarChanged();
    void sidebarIdChanged();
    void widgetClosed();
    /// CIP chrome (shared_ptr / TSidebarID skipped) — qtclient NOTIFY deepen; drag/move
    /// forwarded via TSidebarWidget dynamic emit.
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, TSidebarWidget* widget);
    /// CIP class NOTIFY — qtclient already emits.
    void displayDataChanged();

private Q_SLOTS:
    /// CIP onWidgetOptionsChanged — persist contentHeight / contentMaximized.
    void onWidgetOptionsChanged();
    /// CIP: connect local-player skull (player-killer mode) changes to display refresh.
    void connectPlayerKillerModeChangedToDisplayDataChanged();
    /// CIP: local player entered creature storage — (re)bind skull hook + refresh.
    void onPlayerCreatureAddedToCreatureStorage();
    void onWidgetQuickItemDestroyed();

private:
    /// The game session's creature storage connection.
    QMetaObject::Connection m_playerAdded;
    void refresh();

    std::vector<client::gamecore::Hook> m_hooks;
    QPointer<config::TSidebarOptions> m_options;
    bool m_playerKillerConnected = false;
};

class TUnjustifiedPointsWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TUnjustifiedPointsWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TUnjustifiedPointsWidgetManager() override;

    TSidebarWidget* open(config::TSidebarOptions* options);
    void close(bool saveWidgetOptions = true);
    TSidebarWidget* current() const;
    /// The widget follows `GameSession` (null: none).
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

private:
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<TUnjustifiedPointsWidgetController> m_controller;
    std::shared_ptr<game::IGameSession> m_gameSession;
};

}
}
