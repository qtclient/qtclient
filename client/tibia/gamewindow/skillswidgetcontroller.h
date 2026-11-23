#pragma once

#include "config/sidebaroptions.h"
#include "game/skillswidgetentrydata.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <QPointer>

#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace tibia::game {
class IGameSession;
class TPlayerSkillStats;
class TPremiumTimeStorage;
}

namespace tibia {
namespace gamewindow {

/// The skills widget, skillswidget.qml (CIP: TSkillsWidgetController). One
/// entry per (skill type, separator) pair; the plain entries are kept by type,
/// the separators (the XP boost button row, the line before the magic level)
/// in order. The session's player data and premium time refresh the entries,
/// its skill stats the combat stats. The widget's options are a copy of the
/// character's skillsWidgetOptions: the hidden progress bars, the stat blocks
/// and the size. The XP boost button publishes OPEN_STORE and "more details"
/// OPEN_CYCLOPEDIA for their owners.
class TSkillsWidgetController final : public TSidebarWidgetController<cip::tibia::gamewindow::TSkillsWidgetController>
{
    Q_OBJECT

public:
    TSkillsWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TSkillsWidgetController() override;

    /// Takes the session's player data, skill stats and sidebar options (null:
    /// none), then refreshes the combat stats and the entries.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// A premium time change refreshes the entries.
    void setPremiumTimeStorage(game::TPremiumTimeStorage* premiumTimeStorage);

    /// CIP moc property getters.
    QList<QObject*> skillsModel() const override;
    QObject* offenceStats() const override;
    QObject* defenceStats() const override;
    QObject* miscStats() const override;
    /// The stat blocks the widget options show.
    bool showOffenceStats() const override;
    bool showDefenceStats() const override;
    bool showMiscStats() const override;

    QQuickItem* gameWindowQuickItem() const override;

public Q_SLOTS:
    /// Stores the widget's size in the options copy and hands the copy to the
    /// sidebar options.
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

private Q_SLOTS:
    /// Brings the player's speed over from the own creature, refreshes every
    /// plain entry and the separators' XP boost button.
    void onPlayerDataChanged();
    /// Offence / defence / misc from TPlayerSkillStats.
    void onPlayerSkillStatsChanged();
    /// Refreshes the entries before the QML tooltip opens.
    void onBeforeTooltipShow();
    void onRequestContextMenu();
    void onStoreXpBoostButtonClicked();
    void onMoreStatDetailsClicked();
    /// Copies the sidebar options' skillsWidgetOptions, applies the size and
    /// the hidden progress bars.
    void onWidgetOptionsChanged();

private:
    using Separator = game::SkillsWidgetEntry::Separator;

    /// One entry data per (type, separator) pair; plain entries show their
    /// progress bar unless the options hide it.
    void createEntries();
    /// Applies the options' hidden progress bars to every plain entry that has
    /// a bar.
    void applyProgressBarVisibilityFromOptions();
    /// Shows or hides `type`'s progress bar; a changed set of hidden bars is
    /// saved.
    void setProgressBarVisible(game::SkillType type, bool visible);
    /// Fills the plain entry of `type` from the player.
    void refreshEntry(game::SkillType type);
    /// The plain entry of `type` (null without one).
    game::TSkillsWidgetEntryData* entry(game::SkillType type) const;

    /// The session's player data, skill stats and options.
    shared::TQtConnectionManager m_gameSessionConnections;
    std::shared_ptr<game::IGameSession> m_gameSession;
    /// The QML item's requests.
    shared::TQtConnectionManager m_connections;
    /// The plain entries by type.
    std::map<game::SkillType, game::TSkillsWidgetEntryData*> m_entriesByType;
    /// The separator entries with their type, in order.
    std::vector<std::pair<game::SkillType, game::TSkillsWidgetEntryData*>> m_separatorEntries;
    config::TSkillsWidgetOptions m_widgetOptions;
    QPointer<config::TSidebarOptions> m_options;
    QPointer<game::TPremiumTimeStorage> m_premiumTimeStorage;
    /// The widget's rows: skill type and separator.
    std::vector<std::pair<game::SkillType, Separator>> m_entries;
    QPointer<game::TPlayerSkillStats> m_skillStats;
};

}
}
