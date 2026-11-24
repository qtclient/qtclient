#pragma once

#include "analysers/analyserstorages.h"

#include <QColor>
#include <QString>

#include <cstdint>
#include <map>
#include <memory>
#include <unordered_map>

namespace tibia {
namespace appearances {
class TAppearancesManager;
}
namespace game {
class TPremiumTimeStorage;
}
namespace analysers {

/// The item values of the loot and waste analysers, the loot value levels and the colours of
/// coloured item names (CIP: tibia::cyclopedia::TItemPriceStorage, config "itemprices").
/// One instance per game client, made with the client's appearances and premium time.
class TItemPriceStorage final : public cip::tibia::cyclopedia::TItemPriceStorage, public config::ICharacterConfiguration
{
    Q_OBJECT
public:
    /// The primary loot value source of an item (+0x70; items without an entry are Npc).
    enum ValueSource
    {
        Npc = 0,
        Market = 1,
    };

    /// The NPC prices come from `AppearancesManager`'s types; the loot price
    /// follows `PremiumTimeStorage`'s premium state.
    TItemPriceStorage(std::shared_ptr<appearances::TAppearancesManager> AppearancesManager,
                      std::shared_ptr<game::TPremiumTimeStorage> PremiumTimeStorage, QObject* parent = nullptr);

    QString configurationName() const override;
    QJsonObject configurationToJson() const override;
    void configurationFromJson(const QJsonObject& json) override;
    void resetConfiguration() override;

    /// Coins at face value, else the best price an NPC pays for the item.
    quint32 npcBuyPrice(uint16_t typeId) const;
    /// The best price an NPC sells the item for.
    quint32 npcSalePrice(uint16_t typeId) const;
    /// The current world's market average; asks for the statistics once while there are none.
    quint64 marketPrice(uint16_t typeId);
    /// True if the own value changed.
    bool setCustomSalePrice(uint16_t typeId, quint32 price);
    quint32 customSalePrice(uint16_t typeId) const;
    /// A premium player's own value, else the item's source falling back to the other one.
    quint64 lootPrice(uint16_t typeId);
    /// The source falling back to the other one, NPC sale prices instead of NPC buy prices.
    quint64 wastePrice(uint16_t typeId);
    /// 0 for no value, 1..5 by the loot price.
    int lootValueLevel(uint16_t typeId);
    QColor lootValueLevelColor(int level) const;
    /// "{#rrggbb|name}" in the colour of the item's loot value level.
    QString colouredItemName(uint16_t typeId, const QString& name);
    /// True if the source changed.
    bool setPrimaryLootValueSource(uint16_t typeId, int source);
    int primaryLootValueSource(uint16_t typeId) const;
    void setWorldName(const QString& worldName);
    bool hasMarketPricesForWorld() const;
    void requestMarketPricesIfMissing();
    /// The market statistics of the current world.
    void setMarketPrices(std::unordered_map<uint16_t, quint64> prices);

public Q_SLOTS:
    void onPremiumTimeChanged();

private:
    std::shared_ptr<appearances::TAppearancesManager> m_appearancesManager;
    std::shared_ptr<game::TPremiumTimeStorage> m_premiumTimeStorage;
    std::unordered_map<uint16_t, quint64> m_customSalePrices;
    std::map<QString, std::unordered_map<uint16_t, quint64>> m_marketPrices;
    std::map<uint16_t, int> m_primaryLootValueSources;
    QString m_worldName;
    bool m_isPremium = false;
    bool m_marketStatisticsRequested = false;
    std::map<int, QColor> m_lootValueLevelColors;
};

}
}
