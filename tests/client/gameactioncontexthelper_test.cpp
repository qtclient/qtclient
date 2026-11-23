// CIP protocol::TGameActionContextHelper (game action location/object into a
// client message's ObjectPosition / ObjectIdentifier) and
// protocol::TProtobufObjectPositionHelper (ObjectPosition <-> classic coordinate).

#include "protocol/gameactioncontexthelper.h"
#include "protocol/protobufobjectpositionhelper.h"

#include <protocol.pb.h>

#include <gtest/gtest.h>

using namespace tibia::input;
using tibia::protobuf::protocol::ContainerObjectPosition;
using tibia::protobuf::protocol::ObjectIdentifier;
using tibia::protobuf::protocol::ObjectPosition;
using tibia::protocol::TGameActionContextHelper;
using tibia::protocol::TProtobufObjectPositionHelper;

namespace {

ObjectPosition positionOf(const TGameActionContextPtr& location)
{
    ObjectPosition position;
    TGameActionContextHelper().fillObjectPosition(location, &position);
    return position;
}

Position classicOf(const ObjectPosition& position)
{
    return TProtobufObjectPositionHelper().toClassicCoordinate(position);
}

void expectPosition(const Position& actual, int32_t x, int32_t y, uint8_t z)
{
    EXPECT_EQ(actual.x, x);
    EXPECT_EQ(actual.y, y);
    EXPECT_EQ(actual.z, z);
}

}

TEST(GameActionContextHelper, MapFieldWithAndWithoutStackPosition)
{
    const auto object = positionOf(std::make_shared<TLocationWorldMapCoordinateWithStackPositionContext>(Position(100, 200, 7), 3));
    ASSERT_EQ(object.position_oneof_case(), ObjectPosition::kWorldmapPosition);
    EXPECT_EQ(object.worldmap_position().coordinate().x(), 100u);
    EXPECT_EQ(object.worldmap_position().coordinate().y(), 200u);
    EXPECT_EQ(object.worldmap_position().coordinate().z(), 7u);
    ASSERT_TRUE(object.worldmap_position().has_stack_position());
    EXPECT_EQ(object.worldmap_position().stack_position(), 3u);

    const auto field = positionOf(std::make_shared<TLocationWorldMapCoordinateContext>(Position(100, 200, 7)));
    ASSERT_EQ(field.position_oneof_case(), ObjectPosition::kWorldmapPosition);
    EXPECT_FALSE(field.worldmap_position().has_stack_position());
    expectPosition(classicOf(field), 100, 200, 7);
}

TEST(GameActionContextHelper, InventoryUnknownAndDepotSearch)
{
    const auto inventory = positionOf(std::make_shared<TLocationInventoryGameActionContext>(5));
    ASSERT_EQ(inventory.position_oneof_case(), ObjectPosition::kInventoryPosition);
    EXPECT_EQ(inventory.inventory_position().slot(), 5u);
    expectPosition(classicOf(inventory), 0xffff, 5, 0);

    const auto anywhere = positionOf(std::make_shared<TLocationUnknownContext>());
    ASSERT_EQ(anywhere.position_oneof_case(), ObjectPosition::kUnknownPosition);
    EXPECT_TRUE(anywhere.unknown_position());
    expectPosition(classicOf(anywhere), 0xffff, 0, 0);

    const auto depot = positionOf(std::make_shared<TLocationDepotSearchDepotItemGameActionContext>(4));
    ASSERT_EQ(depot.position_oneof_case(), ObjectPosition::kDepotSearchDepotPosition);
    expectPosition(classicOf(depot), 0xffff, 0x20, 4);

    const auto inbox = positionOf(std::make_shared<TLocationDepotSearchInboxItemGameActionContext>(9));
    ASSERT_EQ(inbox.position_oneof_case(), ObjectPosition::kDepotSearchInboxPosition);
    expectPosition(classicOf(inbox), 0xffff, 0x21, 9);
}

TEST(GameActionContextHelper, ContainerSlotsAndSpecialSlots)
{
    const auto slot = positionOf(std::make_shared<TLocationContainerWithSlotGameActionContext>(2, 7));
    ASSERT_EQ(slot.position_oneof_case(), ObjectPosition::kContainerPosition);
    EXPECT_EQ(slot.container_position().container_id(), 2u);
    ASSERT_EQ(slot.container_position().slot_oneof_case(), ContainerObjectPosition::kSlot);
    EXPECT_EQ(slot.container_position().slot(), 7u);
    expectPosition(classicOf(slot), 0xffff, 0x42, 7);

    const auto fe = positionOf(std::make_shared<TLocationContainerWithSlotGameActionContext>(1, 0xfe));
    EXPECT_EQ(fe.container_position().slot_oneof_case(), ContainerObjectPosition::kSpecialSlotFe);
    expectPosition(classicOf(fe), 0xffff, 0x41, 0xfe);

    const auto ff = positionOf(std::make_shared<TLocationContainerWithSlotGameActionContext>(1, 0xff));
    EXPECT_EQ(ff.container_position().slot_oneof_case(), ContainerObjectPosition::kSpecialSlotFf);
    expectPosition(classicOf(ff), 0xffff, 0x41, 0xff);
}

TEST(GameActionContextHelper, OtherLocationsLeaveThePositionEmpty)
{
    const auto battleList = positionOf(std::make_shared<TLocationBattleListGameActionContext>());
    EXPECT_EQ(battleList.position_oneof_case(), ObjectPosition::POSITION_ONEOF_NOT_SET);
    expectPosition(classicOf(battleList), 0, 0, 0);
}

TEST(GameActionContextHelper, ObjectIdentifierOfAppearanceTypes)
{
    TGameActionContextHelper helper;
    ObjectIdentifier plain;
    helper.fillObjectIdentifier(std::make_shared<TObjectApperanceTypeGameActionContext>(3031), &plain);
    EXPECT_EQ(plain.type_id(), 3031u);
    EXPECT_FALSE(plain.has_data());

    ObjectIdentifier liquid;
    helper.fillObjectIdentifier(std::make_shared<TLiquidObjectApperanceTypeGameActionContext>(2874, 11), &liquid);
    EXPECT_EQ(liquid.type_id(), 2874u);
    EXPECT_EQ(liquid.data(), 11u);

    ObjectIdentifier tier;
    helper.fillObjectIdentifier(std::make_shared<TUpgradeableObjectApperanceTypeGameActionContext>(3079, 2), &tier);
    EXPECT_EQ(tier.data(), 2u);

    ObjectIdentifier deco;
    helper.fillObjectIdentifier(std::make_shared<TDecoItemKitObjectApperanceTypeGameActionContext>(23398, 1000), &deco);
    EXPECT_EQ(deco.type_id(), 23398u);
    EXPECT_FALSE(deco.has_data());

    ObjectIdentifier creature;
    helper.fillObjectIdentifier(std::make_shared<TObjectCreatureGameActionContext>(0x10000001), &creature);
    EXPECT_FALSE(creature.has_type_id());
}

TEST(ProtobufObjectPositionHelper, FromClassicCoordinate)
{
    TProtobufObjectPositionHelper helper;
    ObjectPosition field;
    helper.fromClassicCoordinate(&field, Position(100, 200, 7));
    ASSERT_EQ(field.position_oneof_case(), ObjectPosition::kWorldmapPosition);
    EXPECT_FALSE(field.worldmap_position().has_stack_position());

    ObjectPosition anywhere;
    helper.fromClassicCoordinate(&anywhere, Position(0xffff, 0, 0));
    EXPECT_TRUE(anywhere.unknown_position());

    ObjectPosition inventory;
    helper.fromClassicCoordinate(&inventory, Position(0xffff, 10, 0));
    EXPECT_EQ(inventory.inventory_position().slot(), 10u);

    ObjectPosition depot;
    helper.fromClassicCoordinate(&depot, Position(0xffff, 0x20, 3));
    EXPECT_EQ(depot.depot_search_depot_position().index(), 3u);

    ObjectPosition inbox;
    helper.fromClassicCoordinate(&inbox, Position(0xffff, 0x21, 3));
    EXPECT_EQ(inbox.depot_search_inbox_position().index(), 3u);

    // A container slot is always a plain slot, 0xff included.
    ObjectPosition container;
    helper.fromClassicCoordinate(&container, Position(0xffff, 0x45, 0xff));
    EXPECT_EQ(container.container_position().container_id(), 5u);
    ASSERT_EQ(container.container_position().slot_oneof_case(), ContainerObjectPosition::kSlot);
    EXPECT_EQ(container.container_position().slot(), 0xffu);

    // Unassigned classic y values (slot 11 .. 0x1f, 0x22 .. 0x3f) set nothing.
    ObjectPosition none;
    helper.fromClassicCoordinate(&none, Position(0xffff, 0x11, 0));
    EXPECT_EQ(none.position_oneof_case(), ObjectPosition::POSITION_ONEOF_NOT_SET);
}
