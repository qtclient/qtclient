#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <client/staticdata.h>

#include <QString>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace tibia::imbuing {

/// One slot of the item being imbued (CIP: the storage's slot entries read
/// by 0x100c68810).
struct TImbuementSlot
{
    int index = 0;
    bool isEmpty = true;
    Imbuement imbuement{};
    /// Seconds left.
    uint32_t remainingDuration = 0;
    uint32_t clearingGoldCost = 0;
};

/// What the imbuing dialog shows (CIP: imbuing::TImbuingStorage, filled by
/// TImbuingProtocolMessageHandler::refresh 0x100e4e4d0).
class TImbuingStorage final : public cip::tibia::imbuing::TImbuingStorage
{
    Q_OBJECT
public:
    enum EMode : uint8_t
    {
        MODE_NONE = 0,
        MODE_ITEM = 1,
        MODE_SCROLL = 2,
    };

    explicit TImbuingStorage(QObject* parent = nullptr);

    EMode mode() const { return m_mode; }
    bool scrollAvailable() const { return m_scrollAvailable; }
    uint16_t itemTypeId() const { return m_itemTypeId; }
    uint8_t itemTier() const { return m_itemTier; }
    const std::vector<TImbuementSlot>& slots() const { return m_slots; }
    const std::vector<Imbuement>& imbuements() const { return m_imbuements; }
    /// How many of a resource the player owns (inventory and stash); 0 when
    /// the server did not list it.
    uint32_t ownedCount(uint16_t typeId, bool* listed = nullptr) const;
    /// product: a resource bought outside the imbuing window (gold-token material purchase,
    /// mod/imbuing/imbuingmaterialmenu) counts until the server's next window update.
    void addOwned(uint16_t typeId, uint32_t count);

Q_SIGNALS:
    /// CIP: TImbuingProtocolMessageHandler's requestShowOrRefreshImbuingDialog.
    void requestShowOrRefreshImbuingDialog();
    /// CIP: requestCloseImbuingDialog (close message and death).
    void requestCloseImbuingDialog();
    /// CIP 0x100e4f950: requestShowResultDialog.
    void requestShowResultDialog(const QString& message);

private:
    void refresh(EMode mode, uint16_t typeId, uint8_t tier, std::vector<TImbuementSlot> slots, std::vector<Imbuement> imbuements, const std::vector<ItemPtr>* owned);

    EMode m_mode = MODE_NONE;
    bool m_scrollAvailable = false;
    uint16_t m_itemTypeId = 0;
    uint8_t m_itemTier = 0;
    std::vector<TImbuementSlot> m_slots;
    std::vector<Imbuement> m_imbuements;
    std::unordered_map<uint16_t, uint32_t> m_owned;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
