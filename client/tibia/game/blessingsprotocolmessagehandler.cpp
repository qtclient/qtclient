#include "game/blessingsprotocolmessagehandler.h"

#include <protocol.pb.h>

#include <stdexcept>
#include <utility>
#include <vector>

namespace tibia::game {

int blessingFromProtocol(const uint32_t Value)
{
    switch (Value) {
        case 0x1:
        case 0x2:
        case 0x4:
        case 0x8:
        case 0x10:
        case 0x20:
        case 0x40:
        case 0x80:
        case 0x100:
            return static_cast<int>(Value);
        default:
            throw std::invalid_argument("Invalid enum value");
    }
}

int blessingHistoryColorFromProtocol(const uint32_t Value)
{
    if (Value > 6)
        throw std::invalid_argument("invalid enum value");
    return static_cast<int>(Value);
}

/// Reads the blessings, the history, the death penalty and the three flags
/// into the dialog data and asks for the dialog.
void TBlessingsProtocolMessageHandler::handleBlessingsDialogMessage(
    const ::tibia::protobuf::protocol::GameserverMessageBlessingsDialog& BlessingsDialogMessage)
{
    TBlessingDialogData data;

    std::vector<TBlessingDialogData::TBlessing> blessings;
    blessings.reserve(BlessingsDialogMessage.blessings_size());
    for (const auto& blessing : BlessingsDialogMessage.blessings())
        blessings.emplace_back(blessingFromProtocol(blessing.blessing()), static_cast<uint8_t>(blessing.count()),
                               static_cast<uint8_t>(blessing.store_count()));
    data.setBlessings(std::move(blessings));

    std::vector<TBlessingDialogData::THistoryEntry> history;
    history.reserve(BlessingsDialogMessage.history_size());
    for (const auto& entry : BlessingsDialogMessage.history()) {
        const int color = blessingHistoryColorFromProtocol(entry.color());
        history.emplace_back(entry.timestamp(), color, QString::fromStdString(entry.text()));
    }
    data.setHistory(std::move(history));

    data.setDeathPenalty(TBlessingDialogData::TDeathPenalty(
        static_cast<uint8_t>(BlessingsDialogMessage.promotion()), static_cast<uint8_t>(BlessingsDialogMessage.pvp_min_xp_loss()),
        static_cast<uint8_t>(BlessingsDialogMessage.pvp_max_xp_loss()), static_cast<uint8_t>(BlessingsDialogMessage.pve_xp_loss()),
        static_cast<uint8_t>(BlessingsDialogMessage.equip_pvp_loss()), static_cast<uint8_t>(BlessingsDialogMessage.equip_pvp_loss_tenth()),
        static_cast<uint8_t>(BlessingsDialogMessage.equip_pve_loss()), static_cast<uint8_t>(BlessingsDialogMessage.equip_pve_loss_tenth())));
    data.setPromoted(BlessingsDialogMessage.premium());
    data.setHasSkull(BlessingsDialogMessage.skull());
    data.setHasAmuletOfLoss(BlessingsDialogMessage.amulet_of_loss());

    Q_EMIT requestBlessingsDialog(data);
}

}
