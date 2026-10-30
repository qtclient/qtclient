#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <QString>

#include <cstdint>
#include <map>
#include <vector>

namespace tibia::exaltation {

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
};

/// The last fusion or transfer (CIP: the data TExaltationProtocolMessageHandler
/// 0x100ad0510 publishes with SHOW_EXALTATION_RESULT_DIALOG).
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

/// The exaltation forge's game data (CIP: exaltation::TExaltationStorage,
/// vtable 0x1026285b8), filled the way TExaltationProtocolMessageHandler
/// does, and the forge's game actions 292-298 (CIP:
/// TExaltationGameActionHandler 0x100acc690).
class TExaltationStorage final : public cip::tibia::exaltation::TExaltationStorage
{
    Q_OBJECT
public:
    explicit TExaltationStorage(QObject* parent = nullptr);
    ~TExaltationStorage() override;

    /// Slot 14: the dust limit as the server sends it (increases bought).
    uint8_t dustLimitRaw() const { return m_dustLimitRaw; }
    /// Slot 15.
    int currentDustLimit() const;
    /// Slot 16: a fusion's gold for the classification and tier; 0 when unknown
    /// (CIP throws).
    uint64_t fusionPrice(int classification, int tier) const;
    /// Slot 17.
    uint64_t convergenceFusionPrice(int tier) const;
    /// Slot 18: a transfer's gold for the classification and the source tier.
    uint64_t transferPrice(int classification, int tier) const { return fusionPrice(classification, tier - 1); }
    /// Slot 19.
    uint64_t convergenceTransferPrice(int tier) const;
    /// Slot 20: the exalted cores a transfer to `tier` costs.
    uint16_t transferCoreCost(int tier) const;

    /// Slots 26, 27, 30, 31.
    const std::vector<TFusionGroup>& fusionGroups() const { return m_fusionGroups; }
    const std::vector<TFusionGroup>& convergenceFusionGroups() const { return m_convergenceFusionGroups; }
    const std::vector<TTransferGroup>& transferGroups() const { return m_transferGroups; }
    const std::vector<TTransferGroup>& convergenceTransferGroups() const { return m_convergenceTransferGroups; }

    /// Slots 33 to 35.
    uint16_t currentPage() const { return m_currentPage; }
    uint16_t numberOfPages() const { return m_numberOfPages; }
    const std::vector<THistoryEntry>& history() const { return m_history; }

    /// Slots 38, 40, 42.
    uint64_t dustBalance() const;
    uint64_t sliversBalance() const;
    uint64_t coresBalance() const;

    const TExaltationResult& lastResult() const { return m_lastResult; }

    /// Slot 36 (EXALTATION_FORGE_DIALOG_CLOSED): no groups, history page 0 of 1.
    void reset();

private:
    void setDustLimitRaw(uint8_t raw);
    void setFusionGroups(std::vector<TFusionGroup> groups, std::vector<TFusionGroup> convergence);
    void setTransferGroups(std::vector<TTransferGroup> groups, std::vector<TTransferGroup> convergence);
    void setHistory(uint16_t page, uint16_t pages, std::vector<THistoryEntry> entries);

    uint8_t m_dustLimitRaw = 0;
    std::map<int, std::map<int, uint64_t>> m_classPrices;
    std::map<int, uint64_t> m_convergenceFusionPrices;
    std::map<int, uint64_t> m_convergenceTransferPrices;
    std::map<int, uint16_t> m_transferCoreCosts;
    std::vector<TFusionGroup> m_fusionGroups;
    std::vector<TFusionGroup> m_convergenceFusionGroups;
    std::vector<TTransferGroup> m_transferGroups;
    std::vector<TTransferGroup> m_convergenceTransferGroups;
    uint16_t m_currentPage = 0;
    uint16_t m_numberOfPages = 1;
    std::vector<THistoryEntry> m_history;
    TExaltationResult m_lastResult;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
