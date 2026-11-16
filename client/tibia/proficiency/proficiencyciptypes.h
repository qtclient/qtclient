#pragma once

// CIP-named proficiency surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::protobuf::protocol {
class GameserverMessageResourceBalance;
class GameserverMessageShapedPerkReshapeOffers;
class GameserverMessageWeaponProficiency;
class GameserverMessageWeaponProficiencyNotification;
}

namespace tibia {
namespace proficiency {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TProficiencyProtocolMessageHandler final : public cip::tibia::proficiency::TProficiencyProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::proficiency::TProficiencyProtocolMessageHandler::TProficiencyProtocolMessageHandler;
    /// A weapon's whole record: the XP, the perk picked on each level (the list
    /// grows to the highest level) and the shaped perks.
    static void handleWeaponProficiencyMessage(const ::tibia::protobuf::protocol::GameserverMessageWeaponProficiency& message);
    /// The perks a shaped slot can be reshaped into; every offer sits on the slot.
    static void handleShapedPerkReshapeOffersMessage(const ::tibia::protobuf::protocol::GameserverMessageShapedPerkReshapeOffers& message);
    /// The XP of one weapon; the wielded weapon's also moves the current progress.
    /// The unspent perk flag is only the wielded weapon's (or the notification's
    /// without a weapon).
    static void handleWeaponProficiencyNotificationMessage(const ::tibia::protobuf::protocol::GameserverMessageWeaponProficiencyNotification& message);
    static void handleResourceBalanceMessage(const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& message);
public:
};

}
}

