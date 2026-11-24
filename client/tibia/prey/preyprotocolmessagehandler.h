#pragma once

#include <cip_gen.h>

#include <protocol.pb.h>

#include <memory>

namespace tibia::protobuf::protocol {
class GameserverMessagePreyData;
class GameserverMessagePreyPrices;
class GameserverMessagePreyTimeLeft;
class GameserverMessageResourceBalance;
class GameserverMessageShowMessageDialog;
}

namespace tibia {
namespace prey {

class TPreyStorage;

/// Fills the session's prey storage from the prey messages, and sends the
/// prey dialog's PreyAction requests.
class TPreyProtocolMessageHandler final : public cip::tibia::prey::TPreyProtocolMessageHandler
{
    Q_OBJECT
public:
    /// The handler keeps the session's prey storage. Host: the appearance
    /// instance factory is static in qtclient.
    explicit TPreyProtocolMessageHandler(std::shared_ptr<TPreyStorage> preyStorage);

    void handlePreyTimeLeftMessage(const ::tibia::protobuf::protocol::GameserverMessagePreyTimeLeft& message);
    /// One slot's state; every state also carries the next free reroll and the option.
    void handlePreyDataMessage(const ::tibia::protobuf::protocol::GameserverMessagePreyData& message);
    void handlePreyPricesMessage(const ::tibia::protobuf::protocol::GameserverMessagePreyPrices& message);
    /// Message dialog types 20 and 21 are the prey dialog's messages.
    void handleShowMessageDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& message);
    /// The prey wildcards balance goes to the prey storage.
    void handleResourceBalanceMessage(const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& message);

    /// The PreyAction requests of one slot (action 0 list reroll, 1 bonus
    /// reroll, 2 list choice, 3 bestiary list, 4 bestiary choice, 5 automatic
    /// extension option 0..2).
    void requestListReroll(int preyId);
    void requestBonusReroll(int preyId);
    void selectMonsterFromList(int preyId, int index);
    void requestFreezeAndShowBestiaryMonsters(int preyId);
    void selectMonsterFromBestiary(int preyId, int raceId);
    void setAutomaticExtendType(int preyId, int option);

Q_SIGNALS:
    void sendPreyAction(const ::tibia::protobuf::protocol::GameclientMessagePreyAction& PreyActionMessage);

private:
    std::shared_ptr<TPreyStorage> m_preyStorage;
};

}
}
