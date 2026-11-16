#pragma once

// CIP-named bossbestiary surfaces (promoted from cipwire).

#include "bossbestiary/bosstiarystorage.h"
#include "bossbestiary/bosstrackerstorage.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::protobuf::protocol {
class GameserverMessageBossSlots;
class GameserverMessageBossTracking;
class GameserverMessageBosstiary;
class GameserverMessageBosstiaryHighlight;
class GameserverMessageBosstiaryStaticData;
}

namespace tibia {
namespace bossbestiary {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TBossBestiaryProtocolMessageHandler final : public cip::tibia::bossbestiary::TBossBestiaryProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::bossbestiary::TBossBestiaryProtocolMessageHandler::TBossBestiaryProtocolMessageHandler;
    static void handleBosstiaryStaticDataMessage(const ::tibia::protobuf::protocol::GameserverMessageBosstiaryStaticData& BosstiaryStaticDataMessage);
    static void handleBossSlotsMessage(const ::tibia::protobuf::protocol::GameserverMessageBossSlots& BossSlotsMessage);
    static void handleBosstiaryMessage(const ::tibia::protobuf::protocol::GameserverMessageBosstiary& BosstiaryMessage);
    static void handleBosstiaryHighlightMessage(const ::tibia::protobuf::protocol::GameserverMessageBosstiaryHighlight& BosstiaryHighlightMessage);
    static void handleBossTrackingMessage(const ::tibia::protobuf::protocol::GameserverMessageBossTracking& BossTrackingMessage);
public:
};

// TBossTrackerStorage — bosstrackerstorage.h
// TBosstiaryStorage — bosstiarystorage.h

}
}

