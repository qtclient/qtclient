#pragma once

#include "input/gameaction.h"

#include <QString>

#include <cstdint>
#include <utility>
#include <vector>

namespace tibia::protobuf::protocol {
class GameserverMessageOutfit;
}

namespace tibia {
namespace game {

/// What the outfit dialog shows and edits: the current outfit, mount and
/// summon, the dialog mode, the offered outfits, mounts and summons and, for a
/// show-off socket, the podium. Made by the player handler from the server's
/// outfit message, or by the character info for its selection mode; the
/// outfit dialog storage keeps the latest one.
class TSelectOutfitDialogData
{
public:
    /// One offered outfit: owned (type 0) or a store offer (type 1, with its
    /// offer id).
    struct TOutfit
    {
        uint16_t Id = 0;
        QString Name;
        uint32_t OfferId = 0;
        uint8_t AddOns = 0;
        int Type = 0;
    };

    /// One offered mount or summon.
    struct TMountOrSummon
    {
        uint16_t Id = 0;
        QString Name;
        uint32_t OfferId = 0;
        int Type = 0;
    };

    TSelectOutfitDialogData() = default;
    /// Reads the outfit message. A current outfit (or its other-gender twin),
    /// mount or summon that is not offered falls back to the first offered
    /// one, or none (an outfit loses its add-ons); a store entry keeps its
    /// offer id; the podium is read for a show-off socket and the random mount
    /// flag for the own outfit.
    explicit TSelectOutfitDialogData(const ::tibia::protobuf::protocol::GameserverMessageOutfit& Message);

    /// The pair partner of `Id`, or the id itself.
    uint16_t otherGenderOutfit(uint16_t Id) const;
    /// Whether `Id` is the male (first) outfit of a pair.
    bool isMaleOutfit(uint16_t Id) const;
    /// Whether `Id` is the female (second) outfit of a pair.
    bool isFemaleOutfit(uint16_t Id) const;

    uint16_t OutfitId = 0;
    uint8_t HeadColor = 0;
    uint8_t TorsoColor = 0;
    uint8_t LegsColor = 0;
    uint8_t DetailColor = 0;
    uint8_t AddOns = 0;
    uint16_t MountId = 0;
    uint8_t MountHeadColor = 0;
    uint8_t MountTorsoColor = 0;
    uint8_t MountLegsColor = 0;
    uint8_t MountDetailColor = 0;
    uint16_t SummonId = 0;
    /// TSelectOutfitDialogController::EDialogMode.
    int DialogMode = 0;
    bool Mounted = true;
    bool RandomMount = false;
    uint32_t HirelingCreatureId = 0;
    /// Male/female outfit pairs, each once.
    std::vector<std::pair<uint16_t, uint16_t>> GenderPairs;
    std::vector<TOutfit> Outfits;
    std::vector<TMountOrSummon> Mounts;
    std::vector<TMountOrSummon> Summons;
    /// The show-off socket (dialog mode ConfigureShowOffSocket).
    input::TPodiumLocation PodiumLocation;
    bool ShowSocket = false;
    bool ShowOutfit = false;
    uint8_t LookDirection = 2;
};

}
}
