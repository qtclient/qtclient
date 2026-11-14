#include "questlog/questlogciptypes.h"

#include "protocol/protocolciptypes.h"
#include <tibia/game/gamesession.h>

#include <string>
#include <tuple>
#include <vector>

#include <tibia/game/gamenotify.h>

#include <protocol.pb.h>

namespace tibia::questlog {

namespace {
} // namespace

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

void TQuestLogProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/, const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow)
        return;
    if (opcode == 208 && shadow->HasExtension(GameserverMessageTrackQuestFlags::gameserver_message_track_quest_flags)) {
        const auto& m = shadow->GetExtension(GameserverMessageTrackQuestFlags::gameserver_message_track_quest_flags);
        if (m.full_list()) {
            std::vector<std::tuple<uint16_t, uint16_t, std::string, std::string, std::string>> missions;
            for (const auto& mission : m.missions())
                missions.emplace_back(static_cast<uint16_t>(mission.quest_id()), static_cast<uint16_t>(mission.mission_id()),
                                      mission.quest_name(), mission.mission_name(), mission.description());
            tibia::game::notify("onQuestTracker", static_cast<uint8_t>(m.remaining()), missions);
        } else if (m.missions_size() > 0) {
            const auto& mission = m.missions(0);
            tibia::game::notify("onUpdateQuestTracker", static_cast<uint16_t>(mission.quest_id()),
                                static_cast<uint16_t>(mission.mission_id()), mission.quest_name(), mission.mission_name(),
                                mission.description());
        }
        return;
    }
    if (opcode == 241 && shadow->HasExtension(GameserverMessageQuestLine::gameserver_message_quest_line)) {
        const auto& m = shadow->GetExtension(GameserverMessageQuestLine::gameserver_message_quest_line);
        std::vector<std::tuple<std::string, std::string, uint16_t>> missions;
        for (const auto& mission : m.missions())
            missions.emplace_back(mission.name(), mission.description(), static_cast<uint16_t>(mission.mission_id()));
        tibia::game::processQuestLine(static_cast<uint16_t>(m.quest_id()), missions);
        return;
    }
    if (opcode != 240 || !shadow->HasExtension(GameserverMessageQuestLog::gameserver_message_quest_log))
        return;
    std::vector<std::tuple<uint16_t, std::string, bool>> quests;
    for (const auto& q : shadow->GetExtension(GameserverMessageQuestLog::gameserver_message_quest_log).quests())
        quests.emplace_back(static_cast<uint16_t>(q.quest_id()), q.name(), q.completed());
    tibia::game::processQuestLog(quests);
}

} // namespace tibia::questlog
