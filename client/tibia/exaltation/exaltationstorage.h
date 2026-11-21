#pragma once

#include "input/gameactiondispatch.h"
#include "client/gamecore.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <QString>

#include <cstdint>
#include <map>
#include <vector>

namespace tibia {
namespace exaltation {

/// One object of a fusion or transfer group (CIP: the 8-byte entries of
/// TExaltationStorage's groups).
struct TExaltationObject
{
    uint16_t typeId = 0;
    uint8_t tier = 0;
    uint32_t count = 0;

    bool operator==(const TExaltationObject&) const = default;
};

using TFusionGroup = std::vector<TExaltationObject>;

/// The objects that may give their tier and those that may take it.
struct TTransferGroup
{
    std::vector<TExaltationObject> donors;
    std::vector<TExaltationObject> receivers;

    bool operator==(const TTransferGroup&) const = default;
};

/// CIP: TExaltationStorage::THistoryEntry (0x40 bytes).
struct THistoryEntry
{
    int timestamp = 0;
    QString action;
    QString description;
    bool bonus = false;

    bool operator==(const THistoryEntry&) const = default;
};

/// The last fusion or transfer (CIP: TExaltationResultInfo, the data
/// TExaltationProtocolMessageHandler publishes with SHOW_EXALTATION_RESULT_DIALOG).
struct TExaltationResult
{
    bool isFusion = false;
    bool isConvergence = false;
    bool wasSuccessful = false;
    uint16_t inputTypeId = 0;
    uint8_t inputTier = 0;
    uint16_t resultTypeId = 0;
    uint8_t resultTier = 0;
    int fusionBonus = 0;
    uint16_t coreCount = 0;
    uint16_t bonusTypeId = 0;
    uint8_t bonusTier = 0;
};

/// The forge's prices as the base data message carries them (CIP: the
/// argument of TExaltationStorage slot 12).
struct TExaltationBaseData
{
    struct TClassTierPrice
    {
        uint8_t classification = 0;
        uint8_t tier = 0;
        uint64_t price = 0;
    };
    struct TTierPrice
    {
        uint8_t tier = 0;
        uint64_t price = 0;
    };
    struct TTierCores
    {
        uint8_t tier = 0;
        uint16_t cores = 0;
    };

    std::vector<TClassTierPrice> classPrices;
    std::vector<TTierPrice> convergenceFusionPrices;
    std::vector<TTierPrice> convergenceTransferPrices;
    std::vector<TTierCores> transferCores;
};

/// The exaltation forge's game data (CIP: exaltation::TExaltationStorage),
/// filled the way TExaltationProtocolMessageHandler does, and the forge's
/// game actions 293-299 (CIP: TExaltationGameActionHandler).
class TExaltationStorage final : public cip::tibia::exaltation::TExaltationStorage
{
    Q_OBJECT
public:
    explicit TExaltationStorage(QObject* parent = nullptr);
    ~TExaltationStorage() override;

    /// Host shim for the code that has no session yet: the current game
    /// session's storage, or null.
    static TExaltationStorage* instance();
    static void setInstance(TExaltationStorage* storage);

    /// Slot 12: adds the prices (keeps the ones the data does not name).
    void setBaseData(const TExaltationBaseData& data);
    /// Slots 13, 14: the dust limit as the server sends it (increases bought).
    void setDustLimitRaw(uint8_t raw);
    uint8_t dustLimitRaw() const { return m_dustLimitRaw; }
    /// Slot 15.
    int currentDustLimit() const;
    /// Slot 16: a fusion's gold for the classification and tier; 0 when unknown
    /// (CIP throws).
    uint64_t fusionPrice(int classification, int tier) const;
    /// Slot 17.
    uint64_t convergenceFusionPrice(int tier) const;
    /// Slot 18: a transfer's gold for the classification and the source tier.
    uint64_t transferPrice(int classification, int tier) const { return fusionPrice(classification, static_cast<uint8_t>(tier - 1)); }
    /// Slot 19.
    uint64_t convergenceTransferPrice(int tier) const;
    /// Slot 20: the exalted cores a transfer to `tier` costs.
    uint16_t transferCoreCost(int tier) const;
    /// Slot 21: the classifications with fusion prices, ascending.
    std::vector<uint8_t> classifications() const;
    /// Slot 22: the classification's priced tiers and the tier after the last.
    std::vector<uint8_t> tiers(uint8_t classification) const;
    /// Slot 23: every tier of slot 22 over all classifications, ascending.
    std::vector<uint8_t> allTiers() const;

    /// Slots 24 to 31.
    void setFusionGroups(std::vector<TFusionGroup> groups);
    void setConvergenceFusionGroups(std::vector<TFusionGroup> groups);
    const std::vector<TFusionGroup>& fusionGroups() const { return m_fusionGroups; }
    const std::vector<TFusionGroup>& convergenceFusionGroups() const { return m_convergenceFusionGroups; }
    void setTransferGroups(std::vector<TTransferGroup> groups);
    void setConvergenceTransferGroups(std::vector<TTransferGroup> groups);
    const std::vector<TTransferGroup>& transferGroups() const { return m_transferGroups; }
    const std::vector<TTransferGroup>& convergenceTransferGroups() const { return m_convergenceTransferGroups; }

    /// Slots 32 to 35.
    void setHistory(uint16_t page, uint16_t pages, std::vector<THistoryEntry> entries);
    uint16_t currentPage() const { return m_currentPage; }
    uint16_t numberOfPages() const { return m_numberOfPages; }
    const std::vector<THistoryEntry>& history() const { return m_history; }

    /// Slot 36 (EXALTATION_FORGE_DIALOG_CLOSED): no fusion or transfer groups,
    /// history page 0 of 1.
    void reset();

    /// Slots 37 to 42: the exalted currencies as ResourceBalance reports them.
    void setDustBalance(uint16_t balance);
    uint16_t dustBalance() const { return m_dustBalance; }
    void setSliversBalance(uint16_t balance);
    uint16_t sliversBalance() const { return m_sliversBalance; }
    void setCoresBalance(uint16_t balance);
    uint16_t coresBalance() const { return m_coresBalance; }

    const TExaltationResult& lastResult() const { return m_lastResult; }

Q_SIGNALS:
    /// The protocol handler's actions (CIP: TExaltationProtocolMessageHandler's
    /// publishGameAction; qtclient handles those messages here).
    void publishGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    /// CIP TExaltationProtocolMessageHandler::handleResourceBalanceMessage.
    void handleResourceBalanceMessage(int type, qint64 balance);
    /// CIP TExaltationProtocolMessageHandler::handleShowMessageDialogMessage.
    void handleShowMessageDialogMessage(int type, const QString& message);

    /// CIP TExaltationGameActionHandler.
    bool handleGameAction(const input::IGameAction& action);
    void handleExaltationForgeDialogClosedGameAction();
    void handleExaltationForgeFusionGameAction(const input::TGameActionExaltationForgeFusion& action);
    void handleExaltationForgeConvergenceFusionGameAction(const input::TGameActionExaltationForgeConvergenceFusion& action);
    void handleExaltationForgeTransferGameAction(const input::TGameActionExaltationForgeTransfer& action);
    void handleExaltationForgeHistoryNextPageGameAction();
    void handleExaltationForgeHistoryPreviousPageGameAction();

    uint8_t m_dustLimitRaw = 0;
    std::map<uint8_t, std::map<uint8_t, uint64_t>> m_classPrices;
    std::map<uint8_t, uint64_t> m_convergenceFusionPrices;
    std::map<uint8_t, uint64_t> m_convergenceTransferPrices;
    std::map<uint8_t, uint16_t> m_transferCoreCosts;
    std::vector<TFusionGroup> m_fusionGroups;
    std::vector<TFusionGroup> m_convergenceFusionGroups;
    std::vector<TTransferGroup> m_transferGroups;
    std::vector<TTransferGroup> m_convergenceTransferGroups;
    uint16_t m_currentPage = 0;
    uint16_t m_numberOfPages = 1;
    std::vector<THistoryEntry> m_history;
    uint16_t m_dustBalance = 0;
    uint16_t m_sliversBalance = 0;
    uint16_t m_coresBalance = 0;
    TExaltationResult m_lastResult;
    std::vector<client::gamecore::Hook> m_hooks;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
