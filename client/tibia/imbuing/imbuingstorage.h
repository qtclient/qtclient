#pragma once

#include <cip_gen.h>

#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

namespace tibia {
namespace imbuing {

/// An object type with a count: an imbuement's astral source (with its
/// name) or a resource the player owns (no name).
struct TImbuementSource
{
    uint16_t objectTypeId = 0;
    uint32_t count = 0;
    QString name;
};

/// One imbuement the server offers or an item carries.
struct TImbuement
{
    uint32_t id = 0;
    QString name;
    QString description;
    /// 0 basic, 1 intricate, 2 powerful.
    uint32_t tier = 0;
    uint32_t iconId = 0;
    /// Seconds.
    uint64_t duration = 0;
    std::vector<TImbuementSource> astralSources;
    uint32_t goldCost = 0;
};

/// One slot of the item being imbued.
struct TImbuementSlot
{
    uint32_t index = 0;
    TImbuement imbuement;
    /// Seconds left.
    uint64_t remainingDuration = 0;
    uint32_t clearingGoldCost = 0;
    bool isEmpty = true;
};

/// What the imbuing dialog shows, as the server's last imbuing dialog refresh
/// left it.
class TImbuingStorage final : public cip::tibia::imbuing::TImbuingStorage
{
    Q_OBJECT
public:
    /// The refresh's window type.
    enum EMode : uint32_t
    {
        MODE_NONE = 0,
        MODE_ITEM = 1,
        MODE_SCROLL = 2,
    };

    explicit TImbuingStorage(QObject* parent = nullptr);
    ~TImbuingStorage() override;

    uint32_t mode() const { return m_mode; }
    bool scrollAvailable() const { return m_scrollAvailable; }
    uint16_t itemTypeId() const { return m_itemTypeId; }
    const std::optional<uint8_t>& itemTier() const { return m_itemTier; }
    const std::vector<TImbuementSlot>& slots() const { return m_slots; }
    const std::vector<TImbuement>& imbuements() const { return m_imbuements; }
    /// The resources the player owns (inventory and stash), as the server
    /// listed them.
    const std::vector<TImbuementSource>& ownedObjects() const { return m_ownedObjects; }

    /// Replaces everything and emits imbuingDataChanged.
    void setImbuingData(uint32_t mode, bool scrollAvailable, uint16_t itemTypeId, std::optional<uint8_t> itemTier,
                        std::vector<TImbuementSlot> slots, std::vector<TImbuement> imbuements, std::vector<TImbuementSource> ownedObjects);

    /// product: a resource bought outside the imbuing window (gold-token material purchase,
    /// mod/imbuing/imbuingmaterialmenu) counts until the server's next window update.
    void addOwned(uint16_t typeId, uint32_t count);

private:
    uint32_t m_mode = MODE_NONE;
    bool m_scrollAvailable = false;
    uint16_t m_itemTypeId = 0;
    std::optional<uint8_t> m_itemTier;
    std::vector<TImbuementSlot> m_slots;
    std::vector<TImbuement> m_imbuements;
    std::vector<TImbuementSource> m_ownedObjects;
};

}
}
