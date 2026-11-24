#pragma once

#include <cstdint>

namespace tibia::protobuf::protocol {
class ProtocolInventoryItem;
}
namespace tibia::appearances {
class TAppearanceInstance;
}

namespace tibia::protocol {

/// An object as the protocol names it: its id and the one extra value its kind
/// carries (CIP keeps this 24-byte record; the appearance instance factory
/// builds the object's instance from it).
struct TObjectData
{
    enum class EKind : int32_t { Plain = 0, Liquid = 1, Cumulative = 2, Hangable = 3, Upgradeable = 4, DecoItemKit = 5 };

    uint16_t id = 0;
    EKind kind = EKind::Plain;
    uint32_t liquidType = 0;
    uint8_t count = 0;
    uint32_t hookState = 0;
    uint8_t upgradeClassification = 0;
    uint16_t decoItemId = 0;

    bool isLiquid() const { return kind == EKind::Liquid; }
    bool isCumulative() const { return kind == EKind::Cumulative; }
    bool isHangable() const { return kind == EKind::Hangable; }
    bool isUpgradeable() const { return kind == EKind::Upgradeable; }
    bool isDecoItemKit() const { return kind == EKind::DecoItemKit; }

    /// An object without an extra value.
    static TObjectData plain(uint16_t Id);
    static TObjectData liquid(uint16_t Id, uint32_t LiquidType);
    static TObjectData cumulative(uint16_t Id, uint8_t Count);
    static TObjectData upgradeable(uint16_t Id, uint8_t UpgradeClassification);
    /// A decoration kit: named by its deco item (its own type while it has none).
    static TObjectData decoItemKit(uint16_t Id, uint16_t DecoItemId);
    /// The object data of an object's appearance instance: its kind follows the
    /// instance's class (liquid, cumulative, hangable, upgradeable, deco item
    /// kit, else plain). A deco item kit without a deco item names its own type.
    static TObjectData fromAppearanceInstance(const ::tibia::appearances::TAppearanceInstance& Instance);

    /// The object data of an inventory item: its kind follows the protocol's
    /// exclusive object kinds (liquid before cumulative, then upgradeable and
    /// deco item kit; any other object is plain).
    static TObjectData fromProtocolInventoryItem(const ::tibia::protobuf::protocol::ProtocolInventoryItem& item);
};

}
