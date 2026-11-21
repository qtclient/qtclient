#pragma once

// CIP-named bossbestiary surfaces (promoted from cipwire).

#include "bossbestiary/bosstiarystorage.h"
#include "bossbestiary/bosstrackerstorage.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

#include <memory>

namespace tibia::protobuf::protocol {
class GameserverMessageBossSlots;
class GameserverMessageBossTracking;
class GameserverMessageBosstiary;
class GameserverMessageBosstiaryHighlight;
class GameserverMessageBosstiaryStaticData;
}

namespace tibia {
namespace cyclopedia {
class TMonsterRaceStorage;
}
namespace bossbestiary {

class TBossBestiaryProtocolMessageHandler final : public cip::tibia::bossbestiary::TBossBestiaryProtocolMessageHandler
{
    Q_OBJECT
public:
    /// The session's monster race, boss tracker and bosstiary storages.
    TBossBestiaryProtocolMessageHandler(std::shared_ptr<cyclopedia::TMonsterRaceStorage> pMonsterRaceStorage,
                                        std::shared_ptr<TBossTrackerStorage> pBossTrackerStorage,
                                        std::shared_ptr<TBosstiaryStorage> pBosstiaryStorage, QObject* parent = nullptr);

    /// The kills and boss points of the three stages of each rarity.
    void handleBosstiaryStaticDataMessage(const ::tibia::protobuf::protocol::GameserverMessageBosstiaryStaticData& BosstiaryStaticDataMessage);
    void handleBossSlotsMessage(const ::tibia::protobuf::protocol::GameserverMessageBossSlots& BossSlotsMessage);
    /// The bosses with their rarity, kills and tracking; the raw rows also go
    /// out as "onParseSendBosstiary" for the boss tracker storage.
    void handleBosstiaryMessage(const ::tibia::protobuf::protocol::GameserverMessageBosstiary& BosstiaryMessage);
    /// The bosstiary button lights up for the boss (requestBosstiaryButtonHighlight).
    void handleBosstiaryHighlightMessage(const ::tibia::protobuf::protocol::GameserverMessageBosstiaryHighlight& BosstiaryHighlightMessage);
    static void handleBossTrackingMessage(const ::tibia::protobuf::protocol::GameserverMessageBossTracking& BossTrackingMessage);

Q_SIGNALS:
    void requestBosstiaryButtonHighlight(quint16 BossRaceID);

private:
    std::shared_ptr<cyclopedia::TMonsterRaceStorage> m_pMonsterRaceStorage;
    std::shared_ptr<TBossTrackerStorage> m_pBossTrackerStorage;
    std::shared_ptr<TBosstiaryStorage> m_pBosstiaryStorage;
};

// TBossTrackerStorage — bosstrackerstorage.h
// TBosstiaryStorage — bosstiarystorage.h

}
}
