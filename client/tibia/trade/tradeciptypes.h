#pragma once

// CIP-named trade surfaces (promoted from cipwire).

#include "client/gamecore.h"
#include "shared/cip_typed.h"

#include <client/outfit.h>
#include <cip_gen.h>

#include <QObject>
#include <QString>

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace tibia::trade {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

struct TCharacterTradeConditionEntry {
    QString notification;
    int status = 1; // TibiaEnums.EYesNoWarning: Yes=0 No=1 Warning=2
};

struct TCharacterTradeHighlightEntry {
    int highlightId = 0;
    QString highlight;
    QString category;
    QString categoryIcon;
};

struct TCharacterTradeItemEntry {
    int id = 0;
    QString name;
    int amount = 0;
    QString description;
    int upgradeTier = 0;
};

class TCharacterTradeProtocolMessageHandler final : public cip::tibia::trade::TCharacterTradeProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::trade::TCharacterTradeProtocolMessageHandler::TCharacterTradeProtocolMessageHandler;
};

/// CipSoft CharacterTradeDialog data from onCharacterTrade* publishes.
class TCharacterTradeStorage final : public cip::tibia::trade::TCharacterTradeStorage
{
    Q_OBJECT
public:
    /// CipSoft CharacterTradeConfiguration dataType (Crystal S 0x80 / CIP ECharacterTradeDataType).
    enum DataType {
        Conditions = 0,
        Items = 1,
        StoreItems = 2,
        Highlights = 3,
        Summary = 4,
    };
    Q_ENUM(DataType)

    explicit TCharacterTradeStorage(QObject* parent = nullptr);
    static TCharacterTradeStorage* instance();

    const QString& characterName() const { return m_characterName; }
    int characterLevel() const { return m_characterLevel; }
    uint8_t vocationClientId() const { return m_vocationClientId; }
    const Outfit& outfit() const { return m_outfit; }
    uint8_t worldType() const { return m_worldType; }
    uint32_t startTimestamp() const { return m_startTimestamp; }
    uint32_t endTimestamp() const { return m_endTimestamp; }
    uint16_t minimumBid() const { return m_minimumBid; }
    const std::vector<TCharacterTradeConditionEntry>& conditions() const { return m_conditions; }
    const std::vector<TCharacterTradeItemEntry>& items() const { return m_items; }
    const std::vector<TCharacterTradeItemEntry>& storeItems() const { return m_storeItems; }
    const std::vector<TCharacterTradeHighlightEntry>& highlights() const { return m_highlights; }
    uint16_t summaryFee() const { return m_summaryFee; }
    const std::vector<TCharacterTradeConditionEntry>& confirmations() const { return m_confirmations; }
    bool hasUnfulfilledConditions() const;
    bool hasConfirmationError() const;

Q_SIGNALS:
    void infoUpdated(tibia::trade::TCharacterTradeStorage::DataType dataType);

private:
    static TCharacterTradeStorage* s_instance;
    std::vector<client::gamecore::Hook> m_hooks;

    QString m_characterName;
    int m_characterLevel = 0;
    uint8_t m_vocationClientId = 0;
    Outfit m_outfit;
    uint8_t m_worldType = 0;
    uint32_t m_startTimestamp = 0;
    uint32_t m_endTimestamp = 0;
    uint16_t m_minimumBid = 0;
    std::vector<TCharacterTradeConditionEntry> m_conditions;
    std::vector<TCharacterTradeItemEntry> m_items;
    std::vector<TCharacterTradeItemEntry> m_storeItems;
    std::vector<TCharacterTradeHighlightEntry> m_highlights;
    uint16_t m_summaryFee = 0;
    std::vector<TCharacterTradeConditionEntry> m_confirmations;
};

class TNPCTradeProtocolMessageHandler final : public cip::tibia::trade::TNPCTradeProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::trade::TNPCTradeProtocolMessageHandler::TNPCTradeProtocolMessageHandler;
};

class TPlayerTradeGameActionHandler final : public cip::tibia::trade::TPlayerTradeGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::trade::TPlayerTradeGameActionHandler::TPlayerTradeGameActionHandler;
};

class TPlayerTradeProtocolMessageHandler final : public cip::tibia::trade::TPlayerTradeProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::trade::TPlayerTradeProtocolMessageHandler::TPlayerTradeProtocolMessageHandler;
};

} // namespace tibia::trade
