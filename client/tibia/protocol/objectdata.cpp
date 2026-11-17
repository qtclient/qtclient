#include "protocol/objectdata.h"

#include <tibia/assets/things.h>

#include <protocol.pb.h>

namespace tibia::protocol {

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
