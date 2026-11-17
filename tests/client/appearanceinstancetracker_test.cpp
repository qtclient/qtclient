// CIP appearances::TAppearanceInstanceTracker and sprites::TAppearanceTypeSpritesheetMapping.
#include "appearances/appearanceinstancetracker.h"
#include "appearances/appearancetype.h"
#include "sprites/appearancetypespritesheetmapping.h"
#include "sprites/spritesheetsmetadatamanager.h"

#include <appearances.pb.h>

#include <QStringList>

#include <gtest/gtest.h>

using tibia::appearances::EAppearanceCategory;
using tibia::appearances::TAppearanceInstanceTracker;
using tibia::appearances::TAppearanceType;
using tibia::sprites::TAppearanceTypeSpritesheetMapping;
using tibia::sprites::TSpritesheetsMetadataManager;
namespace apb = tibia::protobuf::appearances;

namespace {

QSize size32(uint32_t) { return QSize(32, 32); }

TAppearanceType object(const uint32_t id, std::initializer_list<uint32_t> sprites)
{
    apb::Appearance appearance;
    appearance.set_id(id);
    auto* info = appearance.add_frame_group()->mutable_sprite_info();
    appearance.mutable_frame_group(0)->set_fixed_frame_group(apb::FIXED_FRAME_GROUP_OBJECT_INITIAL);
    info->set_pattern_width(1);
    info->set_pattern_height(1);
    info->set_pattern_depth(1);
    info->set_layers(1);
    for (const uint32_t sprite : sprites)
        info->add_sprite_id(sprite);
    if (sprites.size() > 1) {
        auto* animation = info->mutable_animation();
        for (size_t i = 0; i < sprites.size(); ++i) {
            auto* phase = animation->add_sprite_phase();
            phase->set_duration_min(100);
            phase->set_duration_max(100);
        }
    }
    return TAppearanceType::fromProtobuf(EAppearanceCategory::Object, appearance, size32);
}

QStringList* record(TAppearanceInstanceTracker& tracker, QStringList& loads)
{
    QObject::connect(&tracker, &TAppearanceInstanceTracker::loadSpritesheetWithID,
                     [&loads](const QString& id) { loads.append(id); });
    return &loads;
}

TSpritesheetsMetadataManager metadata()
{
    TSpritesheetsMetadataManager manager;
    manager.addSpritesheet("a", 0, 99, 0);
    manager.addSpritesheet("b", 100, 199, 0);
    return manager;
}

}

TEST(AppearanceTypeSpritesheetMapping, StoresTheSpritesheetsOfEverySprite)
{
    TSpritesheetsMetadataManager sheets = metadata();
    TAppearanceTypeSpritesheetMapping mapping;
    const TAppearanceType type = object(7, { 5, 150, 6, 900 });
    mapping.addAppearanceType(sheets, type);
    EXPECT_EQ(mapping.spritesheetIdsOf(type), (std::set<QString>{ "a", "b" }));
    EXPECT_TRUE(mapping.spritesheetIdsOf(object(8, { 1 })).empty());
}

TEST(AppearanceInstanceTracker, TheFirstInstanceLoadsTheSpritesheets)
{
    TSpritesheetsMetadataManager sheets = metadata();
    auto mapping = std::make_shared<TAppearanceTypeSpritesheetMapping>();
    const TAppearanceType type = object(7, { 150, 5 });
    mapping->addAppearanceType(sheets, type);
    TAppearanceInstanceTracker tracker(mapping);
    QStringList loads;
    record(tracker, loads);

    tracker.instanceCreated(type);
    tracker.instanceCreated(type);
    EXPECT_EQ(tracker.instanceCount(type), 2);
    ASSERT_EQ(loads.count(), 2);
    EXPECT_EQ(loads.at(0), "a");
    EXPECT_EQ(loads.at(1), "b");

    tracker.instanceDestroyed(type);
    tracker.instanceDestroyed(type);
    tracker.instanceDestroyed(type); // below zero: reported and ignored
    EXPECT_EQ(tracker.instanceCount(type), 0);
    tracker.instanceCreated(type);
    EXPECT_EQ(loads.count(), 4);
}

TEST(AppearanceInstanceTracker, LoadsTheSpritesheetsOfTypesOnce)
{
    TSpritesheetsMetadataManager sheets = metadata();
    auto mapping = std::make_shared<TAppearanceTypeSpritesheetMapping>();
    const TAppearanceType first = object(7, { 150, 5 });
    const TAppearanceType second = object(8, { 160 });
    mapping->addAppearanceType(sheets, first);
    mapping->addAppearanceType(sheets, second);
    TAppearanceInstanceTracker tracker(mapping);
    QStringList loads;
    record(tracker, loads);
    tracker.loadSpritesheetsOf({ &first, &second });
    ASSERT_EQ(loads.count(), 2);
    EXPECT_EQ(loads.at(0), "a");
    EXPECT_EQ(loads.at(1), "b");
}
