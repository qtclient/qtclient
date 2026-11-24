#pragma once

#include "container/containerciptypes.h"

#include <QVariant>

#include <functional>

namespace tibia {
namespace analysers {
class TItemPriceStorage;
}
namespace config {
class TClientOptions;
}
namespace gamewindow {

/// The object roles list models share (CIP's appearance-instance role helper), in CIP's order.
enum class EAppearanceInstanceRole
{
    TypeId,
    CumulativeCount,
    TypeCount,
    CountString,
    AlternativeSlotString,
    LiquidType,
    HookDirection,
    LootValueImageSourceUnderItem,
    LootValueImageSourceOverItem,
    ManagedContainerTooltip,
    UpgradeTier,
    DecoItemObjectId,
};

/// What a model gives the helper besides the object. A model without them (the imbued objects)
/// shows no expiry text and no loot-value frames.
struct TAppearanceInstanceRoleSources
{
    /// The client options and item prices are there.
    bool withOptions = false;
    /// The loot-value frames' options (colorizeLootStyle,
    /// gameWindowShowLootHighlighting) and prices; none without them.
    const config::TClientOptions* clientOptions = nullptr;
    analysers::TItemPriceStorage* prices = nullptr;
    /// Options showExpireInContainers and showExpireWhenUnused.
    bool showExpire = true;
    bool showExpireWhenUnused = false;
    /// How many objects of a type and tier the player has; without it the type count is the
    /// object's own count. The count text shows the type count.
    std::function<uint(uint16_t typeId, uint8_t tier)> typeCount;
};

/// The value of `role` for the object; `ultraShortCountString` gives the count text its short form
/// ("12k", "65k+"). An empty slot has no data.
QVariant appearanceInstanceRoleData(const container::TInventorySlot& slot, EAppearanceInstanceRole role,
                                    const TAppearanceInstanceRoleSources& sources, bool ultraShortCountString);

}
}
