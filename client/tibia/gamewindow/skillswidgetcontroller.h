#pragma once

#include "client/gamecore.h"
#include "game/skillswidgetentrydata.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QPointer>
#include <QSet>
#include <QTimer>

#include <vector>

namespace tibia {
namespace config {
class TSidebarOptions;
}
namespace game {
class TPlayerSkillStats;
}
}

namespace tibia {
namespace gamewindow {

/// The skills widget, skillswidget.qml (CIP: TSkillsWidgetController, ctor
/// ). The XP boost button publishes OPEN_STORE and "more details"
/// OPEN_CYCLOPEDIA for their owners. The context menu's experience counter
/// reset stays inert: it clears cyclopedia::TProgressStorage's experience per
/// hour samples (slot 16,), which nothing in this client keeps.
class TSkillsWidgetController final : public TSidebarWidgetController<cip::tibia::gamewindow::TSkillsWidgetController>
{
    Q_OBJECT

public:
    TSkillsWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TSkillsWidgetController() override;

    /// skillsWidgetOptions: size, hidden bars and the stat blocks.
    void setSidebarOptions(config::TSidebarOptions* options);

    /// CIP moc property getters (live members filled by row / combat updates).
    QList<QObject*> skillsModel() const override;
    QObject* offenceStats() const override;
    QObject* defenceStats() const override;
    QObject* miscStats() const override;
    bool showOffenceStats() const override;
    bool showDefenceStats() const override;
    bool showMiscStats() const override;

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
    /// CIP sidebar chrome (drag/move) — qtclient redeclare; forwarded from chrome slots.
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, tibia::gamewindow::TSidebarWidget* widget);
    /// CIP moc signal (shared_ptr arg skipped from cip_gen); qtclient name parity.
    void publishGameAction();
    /// CIP skillsModel NOTIFY — qtclient emits after skill rows rebuild.

private Q_SLOTS:
    /// CIP onPlayerDataChanged — rebuild skill rows from the local player.
    void onPlayerDataChanged();
    /// CIP onPlayerSkillStatsChanged — offence / defence / misc from TPlayerSkillStats.
    void onPlayerSkillStatsChanged();
    /// CIP onBeforeTooltipShow — refresh rows before the QML tooltip opens.
    void onBeforeTooltipShow();
    /// CIP onRequestContextMenu.
    void onRequestContextMenu();
    /// CIP onStoreXpBoostButtonClicked.
    void onStoreXpBoostButtonClicked();
    /// CIP onMoreStatDetailsClicked.
    void onMoreStatDetailsClicked();
    /// CIP onWidgetOptionsChanged — persist contentHeight / contentMaximized / bars.
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    struct Row
    {
        game::SkillType type;
        game::SkillsWidgetEntry::Separator separator;
        game::TSkillsWidgetEntryData* data;
    };

    void readOptions();
    void writeOptions();
    /// CIP .
    void setProgressBarVisible(game::SkillType type, bool visible);
    void setStatsVisible(bool& member, bool visible);

    std::vector<Row> m_rows;
    std::vector<client::gamecore::Hook> m_hooks;
    QPointer<config::TSidebarOptions> m_options;
    QPointer<game::TPlayerSkillStats> m_skillStats;
    QSet<QString> m_invisibleProgressBars;
    QTimer m_updateTimer;
};

}
}
