#include "appearanceinstanceroledata.h"

#include "analysers/itempricestorage.h"
#include "app/clientoptions.h"
#include "config/clientoptions.h"
#include "container/containerslotinformation.h"
#include "container/qmlitemslotdata.h"

#include <cip_gen.h>

namespace tibia::gamewindow {

namespace {

using Info = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;
using Slot = container::TInventorySlot;

/// The count of a cumulative object, 1 for any other.
uint cumulativeCount(const Slot& slot) { return slot.stackable ? std::max<uint>(1, slot.count) : 1; }

/// With showExpireInContainers, the charges, else the time left.
QString alternativeSlotString(const Slot& slot, const TAppearanceInstanceRoleSources& sources)
{
    if (!sources.withOptions || !sources.showExpire)
        return {};
    if (slot.hasWearOut) {
        if (QString charges = container::chargesText(slot, sources.showExpireWhenUnused); !charges.isEmpty())
            return charges;
    }
    return container::hasClock(slot) ? container::clockText(slot, sources.showExpireWhenUnused) : QString();
}

/// The loot-value frame under (style 1) or over (2) the object.
QString lootValueImageSource(const Slot& slot, const TAppearanceInstanceRoleSources& sources, const int style)
{
    if (!sources.withOptions)
        return {};
    // gameWindowShowLootHighlighting (CIP 0x42): master gate for loot-value tint frames.
    if (!app::ClientOptions::load().options().value(QLatin1String("gameWindowShowLootHighlighting")).toBool(true))
        return {};
    if (config::TClientOptions::instance()->colorizeLootStyle() != style)
        return {};
    const int level = analysers::TItemPriceStorage::instance()->lootValueLevel(slot.typeId);
    if (level == 0)
        return {};
    return QStringLiteral("image://%1/%2")
        .arg(style == 1 ? QStringLiteral("containerslot-glowingborders") : QStringLiteral("containerslot-coloredges"))
        .arg(level - 1);
}

}

QVariant appearanceInstanceRoleData(const Slot& slot, const EAppearanceInstanceRole role, const TAppearanceInstanceRoleSources& sources,
                                    const bool ultraShortCountString)
{
    if (slot.empty())
        return {};
    const uint count = cumulativeCount(slot);
    const uint typeCount = sources.typeCount ? sources.typeCount(slot.typeId, slot.tier) : count;
    switch (role) {
    case EAppearanceInstanceRole::TypeId:
        return static_cast<uint>(slot.typeId);
    case EAppearanceInstanceRole::CumulativeCount:
        return count;
    case EAppearanceInstanceRole::TypeCount:
        return typeCount;
    case EAppearanceInstanceRole::CountString:
        return ultraShortCountString ? container::ammoText(typeCount) : container::countVariants(typeCount);
    case EAppearanceInstanceRole::AlternativeSlotString:
        return alternativeSlotString(slot, sources);
    case EAppearanceInstanceRole::LiquidType:
        return QVariant::fromValue(static_cast<Info::ELiquidType>(slot.fluidOrSplash ? slot.fluid : 0));
    case EAppearanceInstanceRole::HookDirection:
        return QVariant::fromValue(Info::EHookDirection::NONE);
    case EAppearanceInstanceRole::LootValueImageSourceUnderItem:
        return lootValueImageSource(slot, sources, 1);
    case EAppearanceInstanceRole::LootValueImageSourceOverItem:
        return lootValueImageSource(slot, sources, 2);
    case EAppearanceInstanceRole::ManagedContainerTooltip:
        return container::managedContainerTooltip(slot);
    case EAppearanceInstanceRole::UpgradeTier:
        return static_cast<uint>(slot.tier);
    case EAppearanceInstanceRole::DecoItemObjectId:
        return QVariant::fromValue(slot.decoItemId);
    }
    return {};
}

}
