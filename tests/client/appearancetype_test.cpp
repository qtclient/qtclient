#include "appearances/appearancetype.h"

#include <appearances.pb.h>

#include <gtest/gtest.h>

#include <stdexcept>

using tibia::appearances::EAppearanceCategory;
using tibia::appearances::EFixedFrameGroup;
using tibia::appearances::TAppearanceType;
using tibia::appearances::TAppearanceTypeSpriteInfo;
using tibia::appearances::TNpcTradeData;
namespace apb = tibia::protobuf::appearances;

namespace {

QSize size64(uint32_t) { return QSize(64, 64); }

apb::Appearance object(const uint32_t id)
{
    apb::Appearance appearance;
    appearance.set_id(id);
    appearance.set_name("thing");
    return appearance;
}

apb::SpriteInfo* addGroup(apb::Appearance& appearance, const apb::FIXED_FRAME_GROUP fixed, const uint32_t width,
                         const uint32_t layers, const uint32_t sprites)
{
    auto* group = appearance.add_frame_group();
    group->set_fixed_frame_group(fixed);
    auto* info = group->mutable_sprite_info();
    info->set_pattern_width(width);
    info->set_pattern_height(1);
    info->set_pattern_depth(1);
    info->set_layers(layers);
    for (uint32_t i = 0; i < sprites; ++i)
        info->add_sprite_id(100 + i);
    return info;
}

}

TEST(AppearanceType, RenumbersTheFixedFrameGroups)
{
    EXPECT_EQ(tibia::appearances::fixedFrameGroupFromProtobuf(0), EFixedFrameGroup::OutfitIdle);
    EXPECT_EQ(tibia::appearances::fixedFrameGroupFromProtobuf(1), EFixedFrameGroup::OutfitMoving);
    EXPECT_EQ(tibia::appearances::fixedFrameGroupFromProtobuf(2), EFixedFrameGroup::ObjectInitial);
    EXPECT_EQ(tibia::appearances::fixedFrameGroupFromProtobuf(7), EFixedFrameGroup::Invalid);
}

TEST(AppearanceType, SpriteInfoNeedsAWholeNumberOfFrames)
{
    apb::Appearance appearance = object(1);
    addGroup(appearance, apb::FIXED_FRAME_GROUP_OBJECT_INITIAL, 2, 2, 8);
    EXPECT_TRUE(TAppearanceTypeSpriteInfo::fromProtobuf(appearance.frame_group(0).sprite_info(), size64).isValid());
    apb::Appearance broken = object(2);
    addGroup(broken, apb::FIXED_FRAME_GROUP_OBJECT_INITIAL, 2, 2, 6);
    EXPECT_FALSE(TAppearanceTypeSpriteInfo::fromProtobuf(broken.frame_group(0).sprite_info(), size64).isValid());
    EXPECT_TRUE(TAppearanceTypeSpriteInfo().isValid());

    // A group that does not fit is replaced by the empty default.
    const TAppearanceType type = TAppearanceType::fromProtobuf(EAppearanceCategory::Object, broken, size64);
    EXPECT_TRUE(type.frameGroup(EFixedFrameGroup::ObjectInitial).spriteIds().empty());
}

TEST(AppearanceType, PadsTheBoundingBoxesToEveryDirection)
{
    apb::Appearance appearance = object(1);
    apb::SpriteInfo* info = addGroup(appearance, apb::FIXED_FRAME_GROUP_OBJECT_INITIAL, 4, 1, 4);
    auto* box = info->add_bounding_box_per_direction();
    box->set_x(10);
    box->set_y(12);
    box->set_width(20);
    box->set_height(22);
    info->set_bounding_square(40);
    const TAppearanceTypeSpriteInfo built = TAppearanceTypeSpriteInfo::fromProtobuf(*info, size64);
    ASSERT_EQ(built.boundingBoxes().size(), 4u);
    EXPECT_EQ(built.boundingBoxes()[0], QRect(10, 12, 20, 22));
    // The others: the bounding square in the bottom-right of the 64x64 sprite.
    EXPECT_EQ(built.boundingBoxes()[1], QRect(QPoint(24, 24), QPoint(63, 63)));

    info->clear_bounding_square();
    const TAppearanceTypeSpriteInfo plain = TAppearanceTypeSpriteInfo::fromProtobuf(*info, size64);
    EXPECT_EQ(plain.boundingBoxes()[3], QRect(0, 0, 32, 32));
}

TEST(AppearanceType, ConvertsTheFlags)
{
    apb::Appearance appearance = object(7);
    auto* flags = appearance.mutable_flags();
    flags->mutable_bank()->set_waypoints(150);
    flags->set_container(true);
    flags->set_wrap(false);
    flags->set_clockexpire(true);
    flags->mutable_lenshelp()->set_id(1112);
    flags->mutable_market()->set_category(tibia::protobuf::shared::ITEM_CATEGORY_ARMORS);
    flags->mutable_market()->set_trade_as_object_id(7);
    flags->mutable_market()->set_show_as_object_id(8);
    flags->add_restrict_to_vocation(tibia::protobuf::shared::VOCATION_KNIGHT);
    const TAppearanceType type = TAppearanceType::fromProtobuf(EAppearanceCategory::Object, appearance, size64);
    EXPECT_EQ(type.getId(), 7);
    EXPECT_TRUE(type.isGround());
    EXPECT_EQ(type.getGroundSpeed(), 150);
    EXPECT_TRUE(type.isContainer());
    // Wrap counts when the flag is present at all.
    EXPECT_TRUE(type.isWrapable());
    EXPECT_TRUE(type.hasExpire());
    EXPECT_EQ(type.getLensHelp(), 1112);
    ASSERT_TRUE(type.isMarketable());
    EXPECT_EQ(type.marketProperties()->name, QStringLiteral("thing"));
    EXPECT_EQ(type.marketProperties()->showAs, 8);
    EXPECT_EQ(type.marketCategory(), 1);
    EXPECT_EQ(type.restrictVocationMask(), 1);

    // Lens help ids the client does not know are dropped; an object traded as
    // another one has no market data of its own.
    flags->mutable_lenshelp()->set_id(1113);
    flags->mutable_market()->set_trade_as_object_id(9);
    const TAppearanceType other = TAppearanceType::fromProtobuf(EAppearanceCategory::Object, appearance, size64);
    EXPECT_FALSE(other.hasLensHelp());
    EXPECT_FALSE(other.isMarketable());

}

TEST(AppearanceType, CyclopediaObjectsGetTheirOwnMarketCategory)
{
    apb::Appearance appearance = object(3);
    appearance.mutable_flags()->mutable_cyclopediaitem()->set_cyclopedia_type(3);
    const TAppearanceType type = TAppearanceType::fromProtobuf(EAppearanceCategory::Object, appearance, size64);
    EXPECT_EQ(type.marketCategory(), 28);
    EXPECT_EQ(type.getCyclopediaType(), 3u);
}

TEST(AppearanceType, MergesTheOffersOfOneNpc)
{
    apb::Appearance appearance = object(3);
    auto add = [&](const char* name, const char* location, uint32_t sale, uint32_t buy, uint32_t currency) {
        auto* npc = appearance.mutable_flags()->add_npcsaledata();
        npc->set_name(name);
        npc->set_location(location);
        npc->set_sale_price(sale);
        npc->set_buy_price(buy);
        if (currency)
            npc->set_currency_object_type_id(currency);
    };
    add("Hal", "Venore", 0, 10, 0);
    add("Hal", "Ab'Dendriel", 50, 5, 0);
    add("Hal", "Carlin", 40, 7, 0);
    add("Hal", "Thais", 30, 0, 22118);
    const TAppearanceType type = TAppearanceType::fromProtobuf(EAppearanceCategory::Object, appearance, size64);
    const std::vector<TNpcTradeData>& npcs = type.npcTradeData();
    ASSERT_EQ(npcs.size(), 2u);
    EXPECT_EQ(npcs[0].locations, (QStringList{ QStringLiteral("Ab'Dendriel"), QStringLiteral("Carlin"), QStringLiteral("Venore") }));
    EXPECT_EQ(npcs[0].buyPrice, 10u);
    EXPECT_EQ(npcs[0].salePrice, 40u);
    EXPECT_FALSE(npcs[0].currency.has_value());
    EXPECT_EQ(npcs[1].currencyObjectId(), 22118);
}

TEST(AppearanceType, CountsPhasesOverTheFrameGroups)
{
    apb::Appearance appearance = object(5);
    apb::SpriteInfo* idle = addGroup(appearance, apb::FIXED_FRAME_GROUP_OUTFIT_IDLE, 1, 1, 1);
    (void)idle;
    apb::SpriteInfo* moving = addGroup(appearance, apb::FIXED_FRAME_GROUP_OUTFIT_MOVING, 1, 1, 0);
    for (int phase = 0; phase < 3; ++phase) {
        moving->add_sprite_id(200 + phase);
        auto* sprite = moving->mutable_animation()->add_sprite_phase();
        sprite->set_duration_min(100);
        sprite->set_duration_max(100);
    }
    const TAppearanceType type = TAppearanceType::fromProtobuf(EAppearanceCategory::Outfit, appearance, size64);
    EXPECT_EQ(type.getTotalPhases(), 4);
    EXPECT_EQ(type.getAnimationPhases(), 3);
    EXPECT_EQ(type.getIdleAnimationPhases(), 1);
    EXPECT_EQ(type.spriteIdAt(0, 0, 0, 0, 0), 100u);
    EXPECT_EQ(type.spriteIdAt(0, 0, 0, 0, 2), 201u);
    EXPECT_FALSE(type.spriteIdAt(1, 0, 0, 0, 0).has_value());
    EXPECT_EQ(type.getSize(), QSize(2, 2));
    EXPECT_THROW((void)type.frameGroup(EFixedFrameGroup::ObjectInitial), std::logic_error);
}
