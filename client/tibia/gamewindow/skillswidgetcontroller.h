#pragma once

#include "client/gamecore.h"
#include "game/skillswidgetentrydata.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QPointer>
#include <QSet>
#include <QTimer>

#include <vector>

namespace tibia::config {
class TSidebarOptions;
}

namespace tibia::gamewindow {

/// The skills widget, skillswidget.qml (CIP: TSkillsWidgetController, ctor
/// 0x100bfcb90). The XP boost button publishes OPEN_STORE and "more details"
/// OPEN_CYCLOPEDIA for their owners. The context menu's experience counter
/// reset stays inert: it clears cyclopedia::TProgressStorage's experience per
/// hour samples (slot 16, 0x100a6e410), which nothing in this client keeps.
class TSkillsWidgetController final : public TSidebarWidgetController<cip::tibia::gamewindow::TSkillsWidgetController>
{
    Q_OBJECT
public:
    TSkillsWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TSkillsWidgetController() override;

    /// skillsWidgetOptions: size, hidden bars and the stat blocks.
    void setSidebarOptions(config::TSidebarOptions* options);

public Q_SLOTS:
    void saveWidgetOptions() override;

private Q_SLOTS:
    /// CIP 0x100bfdd30: every entry from the local player.
    void updateEntries();
    /// CIP 0x100bfe770.
    void onShowContextMenu();
    /// CIP 0x100bfe010: the offence, defence and misc blocks from the
    /// PlayerSkills combat stats.
    void updateCombatStats();
    /// CIP 0x100bff1d0: the store's premium / boost page at the XP boost.
    void onOpenStoreWithXpBoost();
    /// CIP 0x100bff430: the cyclopedia's character tab at its battle stats.
    void onOpenMoreStatDetails();

private:
    struct Row
    {
        game::SkillType type;
        game::SkillsWidgetEntry::Separator separator;
        game::TSkillsWidgetEntryData* data;
    };

    void readOptions();
    void writeOptions();
    /// CIP 0x100bfe370.
    void setProgressBarVisible(game::SkillType type, bool visible);
    void setStatsVisible(bool& member, bool visible);

    std::vector<Row> m_rows;
    std::vector<client::gamecore::Hook> m_hooks;
    QPointer<config::TSidebarOptions> m_options;
    QSet<QString> m_invisibleProgressBars;
    QTimer m_updateTimer;
    QTimer m_combatStatsTimer;
};

}
