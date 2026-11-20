#pragma once

#include <cstdint>

namespace tibia::appearances {

/// An object type together with what tells its instances apart: the liquid of
/// a fluid container or splash, or the tier of a classified object (CIP:
/// TObjectAppearanceInstanceTypeID).
struct TObjectAppearanceInstanceTypeID
{
    /// Which of the instance values the identifier carries.
    enum class EKind : int
    {
        Plain = 0,
        Liquid = 1,
        Tier = 4,
    };

    uint16_t typeId = 0;
    EKind kind = EKind::Plain;
    uint32_t liquid = 0;
    uint8_t tier = 0;

    /// The type alone.
    static TObjectAppearanceInstanceTypeID plain(uint16_t typeId) { return { typeId, EKind::Plain, 0, 0 }; }
    /// A fluid container or splash holding `liquid`.
    static TObjectAppearanceInstanceTypeID withLiquid(uint16_t typeId, uint32_t liquid)
    {
        return { typeId, EKind::Liquid, liquid, 0 };
    }
    /// A classified object of tier `tier`.
    static TObjectAppearanceInstanceTypeID withTier(uint16_t typeId, uint8_t tier) { return { typeId, EKind::Tier, 0, tier }; }

    bool hasLiquid() const { return kind == EKind::Liquid; }
    bool hasTier() const { return kind == EKind::Tier; }
};

} // namespace tibia::appearances
