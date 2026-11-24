#include "protocol/objectdata.h"

#include "appearances/appearanceinstance.h"

#include <tibia/assets/things.h>

#include <protocol.pb.h>

namespace tibia::protocol {

TObjectData TObjectData::plain(const uint16_t Id)
{
    TObjectData data;
    data.id = Id;
    return data;
}

TObjectData TObjectData::liquid(const uint16_t Id, const uint32_t LiquidType)
{
    TObjectData data;
    data.id = Id;
    data.kind = EKind::Liquid;
    data.liquidType = LiquidType;
    return data;
}

TObjectData TObjectData::cumulative(const uint16_t Id, const uint8_t Count)
{
    TObjectData data;
    data.id = Id;
    data.kind = EKind::Cumulative;
    data.count = Count;
    return data;
}

TObjectData TObjectData::upgradeable(const uint16_t Id, const uint8_t UpgradeClassification)
{
    TObjectData data;
    data.id = Id;
    data.kind = EKind::Upgradeable;
    data.upgradeClassification = UpgradeClassification;
    return data;
}

TObjectData TObjectData::fromAppearanceInstance(const ::tibia::appearances::TAppearanceInstance& Instance)
{
    using namespace ::tibia::appearances;
    TObjectData data;
    data.id = Instance.id();
    if (const auto* liquidInstance = dynamic_cast<const TLiquidObjectAppearanceInstance*>(&Instance)) {
        data.kind = EKind::Liquid;
        data.liquidType = liquidInstance->fluid();
    } else if (const auto* cumulativeInstance = dynamic_cast<const TCumulativeObjectAppearanceInstance*>(&Instance)) {
        data.kind = EKind::Cumulative;
        data.count = static_cast<uint8_t>(cumulativeInstance->count());
    } else if (const auto* hangableInstance = dynamic_cast<const THangableObjectAppearanceInstance*>(&Instance)) {
        data.kind = EKind::Hangable;
        data.hookState = hangableInstance->hangSide();
    } else if (const auto* upgradeableInstance = dynamic_cast<const TUpgradeableObjectAppearanceInstance*>(&Instance)) {
        data.kind = EKind::Upgradeable;
        data.upgradeClassification = upgradeableInstance->tier();
    } else if (const auto* decoItemKitInstance = dynamic_cast<const TDecoItemKitAppearanceInstance*>(&Instance)) {
        data.kind = EKind::DecoItemKit;
        data.decoItemId = decoItemKitInstance->decoItemId();
        if (data.decoItemId != 0)
            data.id = data.decoItemId;
    }
    return data;
}

TObjectData TObjectData::fromProtocolInventoryItem(const ::tibia::protobuf::protocol::ProtocolInventoryItem& item)
{
    TObjectData data;
    data.id = static_cast<uint16_t>(item.item_id());
    const auto& special = tibia::appearances::TAppearancesManager::instance().specialAppearancesForProtocolParsing();
    if (special.isLiquid(data.id)) {
        data.kind = EKind::Liquid;
        data.liquidType = item.count_or_subtype();
    } else if (special.isCumulative(data.id)) {
        data.kind = EKind::Cumulative;
        data.count = static_cast<uint8_t>(item.count_or_subtype());
    } else if (special.isUpgradeable(data.id)) {
        data.kind = EKind::Upgradeable;
        data.upgradeClassification = static_cast<uint8_t>(item.tier());
    } else if (special.isDecoKit(data.id)) {
        data.kind = EKind::DecoItemKit;
        data.decoItemId = static_cast<uint16_t>(item.deco_item_id());
    }
    return data;
}

}
