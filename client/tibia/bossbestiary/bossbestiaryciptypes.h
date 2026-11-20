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

    /// Host: the client's handler, made on first use.
    static TBossBestiaryProtocolMessageHandler* instance();
    /// The bosstiary storage the bosstiary messages fill (CipSoft: the session's,
    /// handed to the constructor); none drops them. Its owner clears it.
    static void setBosstiaryStorage(TBosstiaryStorage* pBosstiaryStorage);

    /// The kills and boss points of the three stages of each rarity.
    static void handleBosstiaryStaticDataMessage(const ::tibia::protobuf::protocol::GameserverMessageBosstiaryStaticData& BosstiaryStaticDataMessage);
    static void handleBossSlotsMessage(const ::tibia::protobuf::protocol::GameserverMessageBossSlots& BossSlotsMessage);
    /// The bosses with their rarity, kills and tracking; the raw rows also go
    /// out as "onParseSendBosstiary" for the boss tracker storage.
    static void handleBosstiaryMessage(const ::tibia::protobuf::protocol::GameserverMessageBosstiary& BosstiaryMessage);
    /// The bosstiary button lights up for the boss (requestBosstiaryButtonHighlight).
    static void handleBosstiaryHighlightMessage(const ::tibia::protobuf::protocol::GameserverMessageBosstiaryHighlight& BosstiaryHighlightMessage);
    static void handleBossTrackingMessage(const ::tibia::protobuf::protocol::GameserverMessageBossTracking& BossTrackingMessage);

Q_SIGNALS:
    void requestBosstiaryButtonHighlight(quint16 BossRaceID);

private:
    TBosstiaryStorage* m_pBosstiaryStorage = nullptr;
};

// TBossTrackerStorage — bosstrackerstorage.h
// TBosstiaryStorage — bosstiarystorage.h

}
}
