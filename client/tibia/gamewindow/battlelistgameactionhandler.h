#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

#include <QObject>
#include <QPointer>

#include <memory>
#include <vector>

namespace tibia {
namespace config {
class TSidebarOptions;
struct TBattleListOptions;
}
namespace creatures {
class TBattleListRenderProvider;
class TCreatureStorage;
struct TBattleListDrawCommand;
}
}

namespace tibia {
namespace gamewindow {

/// Attacks the first, next or previous creature of the battle list and keeps
/// the battle list's tutorial markers (CIP: TBattleListGameActionHandler, made
/// by the game session). The list is the primary battle list's: its filters
/// and sort order applied to the battle list's rows.
class TBattleListGameActionHandler final : public cip::tibia::gamewindow::TBattleListGameActionHandler
{
    Q_OBJECT
public:
    TBattleListGameActionHandler(config::TSidebarOptions* sidebarOptions, std::shared_ptr<creatures::TCreatureStorage> creatureStorage,
                                 std::shared_ptr<creatures::TBattleListRenderProvider> battleListRenderProvider,
                                 QObject* parent = nullptr);

    /// Drops the rows the options' filters hide (and those of creatures the
    /// storage no longer has).
    static void filterDrawCommands(std::vector<creatures::TBattleListDrawCommand>& DrawCommands,
                                   const config::TBattleListOptions& BattleListWidgetOptions,
                                   const creatures::TCreatureStorage& CreatureStorage);
    /// Sorts the rows by each of the options' sort keys, the last key first.
    static void sortDrawCommands(std::vector<creatures::TBattleListDrawCommand>& DrawCommands,
                                 const config::TBattleListOptions& BattleListWidgetOptions);

public Q_SLOTS:
    /// The router's game actions (connected to publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
    void handleAttackFirstTargetGameAction();
    void handleAttackNextTargetGameAction();
    void handleAttackPreviousTargetGameAction();
    /// The first row's creature is attacked, unless it is an NPC, already the
    /// attack target or not on a field.
    void attackFirstTarget() override;
    /// The creature after (or before) the attack target in the list is
    /// attacked: the first one from there that is not an NPC, not the attack
    /// target and on a field.
    void attackNextOrPreviousTarget(bool Previous) override;
    /// A marker on a battle list creature becomes its row's tutorial marker.
    void handleAddGUITutorialMarkerAction(const ::tibia::input::TGameActionAddGUITutorialMarker& pGameAction);
    void handleRemoveAllTutorialMarkersGameAction() override;

private:
    /// The primary battle list's options (defaults without one).
    config::TBattleListOptions battleListWidgetOptions() const;
    /// The row's creature is attacked when it is not an NPC, not
    /// `attackTargetId` and on a field; false when it is not.
    bool attackIfPossible(const creatures::TBattleListDrawCommand& command, uint32_t attackTargetId);

    QPointer<config::TSidebarOptions> m_sidebarOptions;
    std::shared_ptr<creatures::TCreatureStorage> m_creatureStorage;
    std::shared_ptr<creatures::TBattleListRenderProvider> m_battleListRenderProvider;
};

}
}
