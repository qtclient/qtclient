#include <gtest/gtest.h>

#include "input/gameactioncontext.h"

using namespace tibia::input;

// A depot search item is addressed as Position(0xFFFF, 0x20 depot / 0x21 inbox,
// index): the server finds the searched type's index-th object there.
TEST(GameActionContext, DepotSearchItemsHaveTheirClassicPosition)
{
    const TLocationDepotSearchDepotItemGameActionContext depot(3);
    const TLocationDepotSearchInboxItemGameActionContext inbox(7);
    EXPECT_EQ(protocolPosition(&depot), Position(UINT16_MAX, 0x20, 3));
    EXPECT_EQ(protocolPosition(&inbox), Position(UINT16_MAX, 0x21, 7));
    EXPECT_EQ(protocolStackPosition(&depot), 3);
    EXPECT_EQ(protocolStackPosition(&inbox), 7);
}

TEST(GameActionContext, DepotSearchPositionsGiveBackTheirLocation)
{
    const TGameActionContextPtr depot = locationContextFromPosition(Position(UINT16_MAX, 0x20, 3), 3);
    const TGameActionContextPtr inbox = locationContextFromPosition(Position(UINT16_MAX, 0x21, 7), 7);
    ASSERT_TRUE(depot);
    ASSERT_TRUE(inbox);
    EXPECT_EQ(depot->gameActionContextType(), EGameActionContextType::LOCATION_DEPOT_SEARCH_DEPOT_ITEM);
    EXPECT_EQ(inbox->gameActionContextType(), EGameActionContextType::LOCATION_DEPOT_SEARCH_INBOX_ITEM);
    EXPECT_EQ(static_cast<const TLocationDepotSearchGameActionContext*>(depot.get())->index(), 3u);
    EXPECT_EQ(static_cast<const TLocationDepotSearchGameActionContext*>(inbox.get())->index(), 7u);
}

TEST(GameActionContext, InventoryAndContainerPositionsAreUnchanged)
{
    const TGameActionContextPtr inventory = locationContextFromPosition(Position(UINT16_MAX, 5, 0), 0);
    const TGameActionContextPtr container = locationContextFromPosition(Position(UINT16_MAX, 0x40 | 2, 4), 4);
    ASSERT_TRUE(inventory);
    ASSERT_TRUE(container);
    EXPECT_EQ(inventory->gameActionContextType(), EGameActionContextType::LOCATION_INVENTORY);
    EXPECT_EQ(protocolPosition(inventory.get()), Position(UINT16_MAX, 5, 0));
    EXPECT_EQ(container->gameActionContextType(), EGameActionContextType::LOCATION_CONTAINER_WITH_SLOT);
    EXPECT_EQ(protocolPosition(container.get()), Position(UINT16_MAX, 0x40 | 2, 4));
}
