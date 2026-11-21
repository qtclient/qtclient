#pragma once

#include "client/gamecore.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QPointer>
#include <QTimer>

#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace tibia::game {
class IGameSession;
}

namespace tibia {
namespace config {
class TSidebarOptions;
}
namespace news {
class TNewsStorage;
}
}

namespace tibia {
namespace gamewindow {

/// The spells a level up brings (CIP: the player guide's anonymous
/// TSpellListModel): the roles spellName and iconID.
class TPlayerGuideSpellListModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    struct TEntry
    {
        int iconIndex = 0;
        QString name;
    };

    using QAbstractListModel::QAbstractListModel;

    void setEntries(std::vector<TEntry> entries);
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<TEntry> m_entries;
};

/// The older unlocked player guide news (CIP: the anonymous
/// TPlayerGuideEntryListModel): the roles playerGuideHeader and
/// newsEntryID.
class TPlayerGuideEntryListModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    struct TEntry
    {
        QString header;
        int newsEntryId = 0;
    };

    using QAbstractListModel::QAbstractListModel;

    void setEntries(std::vector<TEntry> entries);
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<TEntry> m_entries;
};

/// PlayerGuideWidget.qml (CIP: TPlayerGuideWidgetController). On a level up
/// it shows the level and the spells of that level for the player's
/// vocation (unlocked ones and ones to learn) for 15 seconds; otherwise the
/// latest unlocked player guide news, up to two older ones, or the special
/// popup a reached level (9, 10, 20) still has to show.
class TPlayerGuideWidgetController final
    : public TSidebarWidgetController<cip::tibia::gamewindow::TPlayerGuideWidgetController>
{
    Q_OBJECT
public:
    TPlayerGuideWidgetController(news::TNewsStorage* newsStorage, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                 QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);

    /// Queues the special popup of a reached level unless the player saw it
    /// already (the option sets pendingLevelSpecialPopups /
    /// confirmedLevelSpecialPopups of playerGuideWidgetOptions).
    static void queueLevelSpecialPopup(int level, config::TSidebarOptions* options);

    bool isShowLevelUp() const override;
    bool isShowAutoSpellList() const override;
    bool isShowManualSpellList() const override;
    int playerLevelToShow() const override;
    QObject* autoSpellList() const override;
    QObject* manualSpellList() const override;
    bool isShowPlayerGuideEntries() const override;
    bool hasLatestUnlockedPlayerGuideEntry() const override;
    QString latestUnlockedPlayerGuideEntry() const override;
    bool hasPreviousUnlockedPlayerGuideEntries() const override;
    QObject* previousUnlockedPlayerGuideEntries() const override;
    bool isShowLevelSpecialPopup() const override;
    QString levelSpecialPopupName() const override;

    QQuickItem* gameWindowQuickItem() const override;

public Q_SLOTS:
    /// Stores the size, keeping the popup sets.
    void saveWidgetOptions() override;
    void onCloseClicked() override;
    void onRequestClose() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void onLevelUpClicked() override;
    void setCursorToResizeVertical(bool Enabled) override;
    void startContentSizeChangedTimer();

public:
    /// Opens the latest unlocked guide entry in the news dialog.
    Q_INVOKABLE void showLatestUnlockedPlayerGuide() override;
    Q_INVOKABLE void showPreviousUnlockedPlayerGuide(int newsEntryId) override;
    /// Shows the pending special popup and marks it seen.
    Q_INVOKABLE void showLevelSpecialPopup() override;

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

private Q_SLOTS:
    /// Applies the widget options' size.
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();
    void onNewsStorageContentsChanged();

public:
    /// Follows the new session's player: level ups, then the guide entries.
    void onGameSessionChanged();

private:
    /// The level up view: the level and its spells, shown until the hide timer runs out.
    void onPlayerLevelUp();
    /// Hides the level up view once the hide timer is not running.
    void hideLevelUpUnlessTimerRuns();
    /// The latest unlocked player guide news and up to two older ones.
    void refreshPlayerGuideEntries();
    /// The lowest pending special popup level once the player reached it.
    std::optional<int> levelSpecialPopupLevel() const;
    void setFlag(bool& flag, bool value, void (TPlayerGuideWidgetController::*changed)());

    QPointer<news::TNewsStorage> m_newsStorage;
    QTimer m_hideTimer;
    TPlayerGuideSpellListModel m_autoSpells;
    TPlayerGuideSpellListModel m_manualSpells;
    TPlayerGuideEntryListModel m_previousEntries;
    int m_latestUnlockedNewsEntryId = 0;
    QPointer<config::TSidebarOptions> m_options;
};

class TPlayerGuideWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TPlayerGuideWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TPlayerGuideWidgetManager() override;

    TSidebarWidget* open(config::TSidebarOptions* options);
    void close(bool saveWidgetOptions = true);
    TSidebarWidget* current() const;
    /// The guide follows the session's player (its level and the news it
    /// unlocks), now and when it enters the world.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

Q_SIGNALS:
    /// The controller's publishGameAction, forwarded.
    void publishGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    void onGameStarted();
    void onPlayerLevelUp();

    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<TPlayerGuideWidgetController> m_controller;
    QPointer<config::TSidebarOptions> m_options;
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
};

}
}
