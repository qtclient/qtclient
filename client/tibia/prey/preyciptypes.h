#pragma once

// CIP-named prey surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::protobuf::protocol {
class GameserverMessagePreyData;
class GameserverMessagePreyPrices;
class GameserverMessagePreyTimeLeft;
class GameserverMessageShowMessageDialog;
}

namespace tibia {
namespace prey {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TPreyGameActionHandler final : public cip::tibia::prey::TPreyGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::prey::TPreyGameActionHandler::TPreyGameActionHandler;
};

class TPreyProtocolMessageHandler final : public cip::tibia::prey::TPreyProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::prey::TPreyProtocolMessageHandler::TPreyProtocolMessageHandler;
public:

    static void handlePreyTimeLeftMessage(const ::tibia::protobuf::protocol::GameserverMessagePreyTimeLeft& message);
    /// One slot's state; every state also carries the next free reroll and the option.
    static void handlePreyDataMessage(const ::tibia::protobuf::protocol::GameserverMessagePreyData& message);
    static void handlePreyPricesMessage(const ::tibia::protobuf::protocol::GameserverMessagePreyPrices& message);
    /// Message dialog types 20 and 21 are the prey dialog's messages.
    void handleShowMessageDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& message);

    /// The PreyAction requests of one slot (action 0 list reroll, 1 bonus
    /// reroll, 2 list choice, 3 bestiary list, 4 bestiary choice, 5 automatic
    /// extension option 0..2).
    static void requestListReroll(int preyId);
    static void requestBonusReroll(int preyId);
    static void selectMonsterFromList(int preyId, int index);
    static void requestFreezeAndShowBestiaryMonsters(int preyId);
    static void selectMonsterFromBestiary(int preyId, int raceId);
    static void setAutomaticExtendType(int preyId, int option);
};

}
}

