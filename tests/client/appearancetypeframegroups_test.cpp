#include "appearances/appearancetype.h"

#include <gtest/gtest.h>

using tibia::appearances::EAppearanceCategory;
using tibia::appearances::EFixedFrameGroup;
using tibia::appearances::TAppearanceType;
using tibia::appearances::TAppearanceTypeSpriteInfo;

TEST(AppearanceTypeFrameGroups, TypeWithoutGroupsGetsAnEmptyInitialGroup)
{
    TAppearanceType type;
    type.setCategory(EAppearanceCategory::Object);
    type.completeFrameGroups();
    ASSERT_EQ(type.frameGroups().size(), 1u);
    EXPECT_TRUE(type.frameGroups().contains(EFixedFrameGroup::ObjectInitial));
}

TEST(AppearanceTypeFrameGroups, OutfitIdleOnlyGetsAMovingCopy)
{
    TAppearanceType type;
    type.setCategory(EAppearanceCategory::Outfit);
    type.addFrameGroup(EFixedFrameGroup::OutfitIdle, TAppearanceTypeSpriteInfo());
    type.completeFrameGroups();
    ASSERT_EQ(type.frameGroups().size(), 2u);
    EXPECT_TRUE(type.frameGroups().contains(EFixedFrameGroup::OutfitIdle));
    EXPECT_TRUE(type.frameGroups().contains(EFixedFrameGroup::OutfitMoving));
}

TEST(AppearanceTypeFrameGroups, OutfitMovingOnlyGetsAnIdleCopy)
{
    TAppearanceType type;
    type.setCategory(EAppearanceCategory::Outfit);
    type.addFrameGroup(EFixedFrameGroup::OutfitMoving, TAppearanceTypeSpriteInfo());
    type.completeFrameGroups();
    ASSERT_EQ(type.frameGroups().size(), 2u);
    EXPECT_TRUE(type.frameGroups().contains(EFixedFrameGroup::OutfitIdle));
}

TEST(AppearanceTypeFrameGroups, ObjectKeepsItsGroups)
{
    TAppearanceType type;
    type.setCategory(EAppearanceCategory::Object);
    type.addFrameGroup(EFixedFrameGroup::OutfitIdle, TAppearanceTypeSpriteInfo());
    type.completeFrameGroups();
    ASSERT_EQ(type.frameGroups().size(), 1u);
    EXPECT_FALSE(type.frameGroups().contains(EFixedFrameGroup::ObjectInitial));
}

TEST(AppearanceTypeFrameGroups, OutfitWithoutGroupsGetsInitialIdleAndMoving)
{
    TAppearanceType type;
    type.setCategory(EAppearanceCategory::Outfit);
    type.completeFrameGroups();
    EXPECT_EQ(type.frameGroups().size(), 3u);
}
