#pragma once

#include <QString>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace tibia::appearances {
class TAppearanceInstance;
}

namespace tibia::gamewindow {

/// CIP gamewindow::TInspectObjectData: one entry of an inspection list, as the
/// inspect object and inspect player dialogs show it.
struct TInspectObjectData
{
    /// The inventory slot of a player's object (1..13); 99 when the entry has none.
    static constexpr int NoInventorySlot = 99;

    TInspectObjectData() = default;
    TInspectObjectData(const QString& Name, const int& InventorySlot,
                       const std::vector<std::pair<QString, QString>>& Details,
                       const std::shared_ptr<appearances::TAppearanceInstance>& Appearance, const QString& AmountText,
                       const std::vector<uint32_t>& ImbuementSlots)
        : name(Name)
        , inventorySlot(InventorySlot)
        , details(Details)
        , appearance(Appearance)
        , amountText(AmountText)
        , imbuementSlots(ImbuementSlots)
    {
    }

    QString name;
    int inventorySlot = NoInventorySlot;
    /// The server's description lines, label and value.
    std::vector<std::pair<QString, QString>> details;
    /// The object's (or the player's outfit's) instance; null when the entry has neither.
    std::shared_ptr<appearances::TAppearanceInstance> appearance;
    /// The stack's count when a cumulative object holds more than one, else empty.
    QString amountText;
    std::vector<uint32_t> imbuementSlots;
};

} // namespace tibia::gamewindow
