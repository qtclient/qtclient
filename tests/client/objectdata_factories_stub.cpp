// Test stand-in: the plain TObjectData factories of protocol/objectdata.cpp
// (that file also converts appearance instances and protobuf items, which this
// header-only test target does not link). Keep in step with objectdata.cpp.
#include "protocol/objectdata.h"

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

}
