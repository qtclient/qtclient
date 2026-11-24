#pragma once

#include <cip_gen.h>

#include <protocol.pb.h>

#include <cstdint>

#include <memory>

namespace tibia::protobuf::protocol {
class GameserverMessageCloseExaltationDialog;
class GameserverMessageExaltationBaseData;
class GameserverMessageExaltationDialogRefresh;
class GameserverMessageExaltationHistory;
class GameserverMessageExaltationResult;
class GameserverMessageResourceBalance;
class GameserverMessageShowMessageDialog;
}

namespace tibia::exaltation {

class TExaltationStorage;

/// Fills the session's exaltation storage from the forge's server messages,
/// publishes the forge dialog's game actions, and sends the forge's requests.
class TExaltationProtocolMessageHandler final : public cip::tibia::exaltation::TExaltationProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TExaltationProtocolMessageHandler(std::shared_ptr<TExaltationStorage> exaltationStorage);

    /// The forge's ExaltationAction requests (action 0 fusion, 1 transfer,
    /// 2 dust to slivers, 3 slivers to cores, 4 dust limit increase).
    void requestDustToSliversConversion();
    void requestSliversToCoreConversion();
    void requestDustLimitIncrease();
    void requestFusion(uint32_t firstTypeId, uint32_t tier, uint32_t secondTypeId, bool improveChance, bool reduceTierLoss);
    void requestConvergenceFusion(uint32_t firstTypeId, uint32_t tier, uint32_t secondTypeId);
    void requestTransfer(uint32_t sourceTypeId, uint32_t tier, uint32_t targetTypeId, bool convergence);
    /// Asks for one page of the exaltation history.
    void requestHistoryPage(int page);

    void handleResourceBalanceMessage(const protobuf::protocol::GameserverMessageResourceBalance& message);
    void handleExaltationBaseDataMessage(const protobuf::protocol::GameserverMessageExaltationBaseData& message);
    void handleCloseExaltationDialogMessage(const protobuf::protocol::GameserverMessageCloseExaltationDialog& message);
    void handleExaltationDialogRefreshMessage(const protobuf::protocol::GameserverMessageExaltationDialogRefresh& message);
    void handleExaltationHistoryMessage(const protobuf::protocol::GameserverMessageExaltationHistory& message);
    void handleExaltationResultMessage(const protobuf::protocol::GameserverMessageExaltationResult& message);
    void handleShowMessageDialogMessage(const protobuf::protocol::GameserverMessageShowMessageDialog& message);

Q_SIGNALS:
    void sendExaltationAction(const ::tibia::protobuf::protocol::GameclientMessageExaltationAction& ExaltationActionMessage);
    void sendOpenExaltationHistory(const ::tibia::protobuf::protocol::GameclientMessageOpenExaltationHistory& OpenExaltationHistoryMessage);

private:
    std::shared_ptr<TExaltationStorage> m_exaltationStorage;
};

}
