#pragma once

// CIP-named bossbestiary surfaces (promoted from cipwire).

#include "bossbestiary/bosstiarystorage.h"
#include "bossbestiary/bosstrackerstorage.h"
#include "input/gameactionptr.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

#include <cstdint>
#include <memory>

namespace tibia::protobuf::protocol {
class GameserverMessageBossSlots;
class GameserverMessageShowMessageDialog;
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

/// The protocol's boss rarity (0 bane, 1 archfoe, 2 nemesis); other values throw.
int bossCategory(uint32_t protocolValue);
/// The protocol's race repeat value of a boss slot (0..3); other values throw.
int bossRaceRepeat(uint32_t protocolValue);

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
    /// The free podium message (dialog type 60) opens the bosstiary's free podium dialog with its text.
    void handleShowMessageDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& ShowMessageDialogMessage);

Q_SIGNALS:
    void requestBosstiaryButtonHighlight(quint16 BossRaceID);
    void publishGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    std::shared_ptr<cyclopedia::TMonsterRaceStorage> m_pMonsterRaceStorage;
    std::shared_ptr<TBossTrackerStorage> m_pBossTrackerStorage;
    std::shared_ptr<TBosstiaryStorage> m_pBosstiaryStorage;
};

// TBossTrackerStorage — bosstrackerstorage.h
// TBosstiaryStorage — bosstiarystorage.h

}
}
