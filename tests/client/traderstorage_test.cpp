#include "appearances/appearancetype.h"
#include "game/traderstorage.h"

#include <gtest/gtest.h>

using tibia::appearances::TAppearanceType;
using tibia::appearances::TNpcTradeData;
using tibia::game::TTraderStorage;

namespace {

TNpcTradeData trade(const QString& npc, const uint32_t salePrice, const uint32_t buyPrice)
{
    TNpcTradeData data;
    data.name = npc;
    data.salePrice = salePrice;
    data.buyPrice = buyPrice;
    return data;
}

}

TEST(TraderStorage, RecordsTheNpcsThatBuyAnObject)
{
    TAppearanceType sword;
    sword.setId(3264);
    sword.addNpcTradeData(trade(QStringLiteral("Rashid"), 0, 25));
    sword.addNpcTradeData(trade(QStringLiteral("Sam"), 50, 0));
    TAppearanceType shield;
    shield.setId(3412);
    shield.addNpcTradeData(trade(QStringLiteral("Rashid"), 0, 30));
    TAppearanceType rope;
    rope.setId(3003);

    TTraderStorage storage;
    storage.addAppearanceTypes({ &sword, &shield, &rope });

    EXPECT_EQ(storage.objectTypesBoughtBy(QStringLiteral("Rashid")), (std::set<uint16_t>{ 3264, 3412 }));
    // Sam only sells.
    EXPECT_TRUE(storage.objectTypesBoughtBy(QStringLiteral("Sam")).empty());
    EXPECT_TRUE(storage.objectTypesBoughtBy(QStringLiteral("Nobody")).empty());
}

TEST(TraderStorage, AddsToTheTradersOfEarlierCalls)
{
    TAppearanceType sword;
    sword.setId(3264);
    sword.addNpcTradeData(trade(QStringLiteral("Rashid"), 0, 25));
    TAppearanceType axe;
    axe.setId(3274);
    axe.addNpcTradeData(trade(QStringLiteral("Rashid"), 0, 10));

    TTraderStorage storage;
    storage.addAppearanceTypes({ &sword });
    storage.addAppearanceTypes({ &axe });
    EXPECT_EQ(storage.objectTypesBoughtBy(QStringLiteral("Rashid")), (std::set<uint16_t>{ 3264, 3274 }));
}
