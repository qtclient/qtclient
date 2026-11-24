#pragma once

#include "input/gameaction.h"
#include "questlog/questlogciptypes.h"

#include <cip_gen.h>

#include <protocol.pb.h>

#include <QObject>

#include <memory>
#include <set>

namespace tibia::questlog {

/// CIP: TQuestLogGameActionHandler. Changes the tracked quest flags and the
/// quest tracker's options, then sends the whole tracked list with both
/// options (the server answers with the tracked flags).
class TQuestLogGameActionHandler final : public cip::tibia::questlog::TQuestLogGameActionHandler
{
    Q_OBJECT
public:
    explicit TQuestLogGameActionHandler(std::shared_ptr<TQuestLogStorage> questLogStorage);

public Q_SLOTS:
    /// The router's game actions (connected to publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
    /// Option 0 auto-tracks new quests, option 1 auto-untracks completed ones.
    void handleChangeQuesttrackerOptionsGameAction(const ::tibia::input::TGameActionChangeQuesttrackerOptions& pGameAction);
    /// Adds (track) or removes the action's quest flags from the tracked ones.
    void handleChangeTrackedQuestFlagsGameAction(const ::tibia::input::TGameActionChangeTrackedQuestFlags& pGameAction);

Q_SIGNALS:
    void sendTrackQuestFlags(const ::tibia::protobuf::protocol::GameclientMessageTrackQuestFlags& TrackQuestFlagsMessage);

private:
    /// The ids of the storage's tracked quest flags.
    std::set<TQuestFlagID> trackedQuestFlagIds() const;
    /// CIP: the TrackQuestFlags message with the ids and the storage's options.
    void trackQuestFlags(const std::set<TQuestFlagID>& questFlagIds);

    std::shared_ptr<TQuestLogStorage> m_questLogStorage;
};

}
