#include "game/selectoutfitdialogdata.h"

#include <protocol.pb.h>

#include <algorithm>
#include <stdexcept>

namespace tibia::game {

namespace {

using ::tibia::protobuf::protocol::GameserverMessageOutfit;

/// The protocol's outfit source: owned 0, store 1, the two others as they are.
int outfitTypeFromProtocol(const uint32_t Mode)
{
    if (Mode > 3)
        throw std::runtime_error("unhandled enum value");
    return static_cast<int>(Mode);
}

/// The protocol's window mode as the dialog mode: the show-off socket (5) is
/// the dialog's mode 6, the other modes keep their value.
int dialogModeFromProtocol(const uint32_t Mode)
{
    if (Mode > 5)
        throw std::runtime_error("unhandled enum value");
    return Mode == 5 ? 6 : static_cast<int>(Mode);
}

}

TSelectOutfitDialogData::TSelectOutfitDialogData(const GameserverMessageOutfit& Message)
{
    const auto& outfit = Message.outfit();
    OutfitId = static_cast<uint16_t>(outfit.look_type());
    HeadColor = static_cast<uint8_t>(outfit.head());
    TorsoColor = static_cast<uint8_t>(outfit.body());
    LegsColor = static_cast<uint8_t>(outfit.legs());
    DetailColor = static_cast<uint8_t>(outfit.feet());
    AddOns = static_cast<uint8_t>(outfit.addons());
    const auto& mountColors = Message.mount_colors();
    MountId = static_cast<uint16_t>(Message.mount());
    MountHeadColor = static_cast<uint8_t>(mountColors.head());
    MountTorsoColor = static_cast<uint8_t>(mountColors.body());
    MountLegsColor = static_cast<uint8_t>(mountColors.legs());
    MountDetailColor = static_cast<uint8_t>(mountColors.feet());
    SummonId = static_cast<uint16_t>(Message.familiar());

    for (const auto& pair : Message.gender_pairs()) {
        const std::pair<uint16_t, uint16_t> entry(static_cast<uint16_t>(pair.male()), static_cast<uint16_t>(pair.female()));
        if (std::ranges::find(GenderPairs, entry) == GenderPairs.end())
            GenderPairs.push_back(entry);
    }

    // The current outfit counts as offered when it or its other-gender twin is.
    const uint16_t twin = otherGenderOutfit(OutfitId);
    bool outfitFound = false;
    for (const auto& option : Message.outfits()) {
        const int type = outfitTypeFromProtocol(option.mode());
        Outfits.push_back({ .Id = static_cast<uint16_t>(option.look_type()),
                            .Name = QString::fromStdString(option.name()),
                            .OfferId = type == 1 ? option.offer_id() : 0,
                            .AddOns = static_cast<uint8_t>(option.addons()),
                            .Type = type });
        if (twin == Outfits.back().Id || OutfitId == Outfits.back().Id)
            outfitFound = true;
    }
    if (!outfitFound) {
        OutfitId = Outfits.empty() ? 0 : Outfits.front().Id;
        AddOns = 0;
    }

    const auto readList = [](const auto& Options, std::vector<TMountOrSummon>& List, uint16_t& Current) {
        bool found = false;
        for (const auto& option : Options) {
            const int type = outfitTypeFromProtocol(option.mode());
            List.push_back({ .Id = static_cast<uint16_t>(option.look_type()),
                             .Name = QString::fromStdString(option.name()),
                             .OfferId = type == 1 ? option.offer_id() : 0,
                             .Type = type });
            if (Current == List.back().Id)
                found = true;
        }
        if (!found)
            Current = List.empty() ? 0 : List.front().Id;
    };
    readList(Message.mounts(), Mounts, MountId);
    readList(Message.familiars(), Summons, SummonId);

    DialogMode = dialogModeFromProtocol(Message.window_mode());
    Mounted = Message.mounted();
    HirelingCreatureId = Message.hireling_id();
    if (DialogMode == 6) {
        const auto& position = Message.podium_position();
        PodiumLocation.position.x = static_cast<int>(position.x());
        PodiumLocation.position.y = static_cast<int>(position.y());
        PodiumLocation.position.z = static_cast<uint8_t>(position.z());
        PodiumLocation.stackPosition = static_cast<uint8_t>(Message.podium_stack_position());
        PodiumLocation.itemId = static_cast<uint16_t>(Message.podium_object());
        ShowSocket = Message.podium_visible();
        ShowOutfit = Message.podium_show_outfit();
        LookDirection = static_cast<uint8_t>(Message.podium_direction());
    } else if (DialogMode == 0) {
        RandomMount = Message.random_mount();
    }
}

uint16_t TSelectOutfitDialogData::otherGenderOutfit(const uint16_t Id) const
{
    for (const auto& [male, female] : GenderPairs) {
        if (male == Id)
            return female;
        if (female == Id)
            return male;
    }
    return Id;
}

bool TSelectOutfitDialogData::isMaleOutfit(const uint16_t Id) const
{
    return std::ranges::any_of(GenderPairs, [Id](const auto& pair) { return pair.first == Id; });
}

bool TSelectOutfitDialogData::isFemaleOutfit(const uint16_t Id) const
{
    return std::ranges::any_of(GenderPairs, [Id](const auto& pair) { return pair.second == Id; });
}

}
