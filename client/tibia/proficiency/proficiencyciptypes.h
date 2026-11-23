#pragma once

// CIP-named proficiency surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

#include <memory>

namespace tibia::protobuf::protocol {
class GameserverMessageResourceBalance;
class GameserverMessageShapedPerkReshapeOffers;
class GameserverMessageWeaponProficiency;
class GameserverMessageWeaponProficiencyNotification;
}

namespace tibia {
namespace container {
class TInventoryContainer;
}
namespace proficiency {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TProficiencyStorage;

class TProficiencyProtocolMessageHandler final : public cip::tibia::proficiency::TProficiencyProtocolMessageHandler
{
    Q_OBJECT
public:
    /// Fills the game client's `storage`; the session's `inventory` tells the
    /// wielded weapon.
    TProficiencyProtocolMessageHandler(TProficiencyStorage* storage, std::shared_ptr<container::TInventoryContainer> inventory);

    TProficiencyStorage* storage() const { return m_storage; }

    /// A weapon's whole record: the XP, the perk picked on each level (the list
    /// grows to the highest level) and the shaped perks.
    void handleWeaponProficiencyMessage(const ::tibia::protobuf::protocol::GameserverMessageWeaponProficiency& message);
    /// The perks a shaped slot can be reshaped into; every offer sits on the slot.
    void handleShapedPerkReshapeOffersMessage(const ::tibia::protobuf::protocol::GameserverMessageShapedPerkReshapeOffers& message);
    /// The XP of one weapon; the wielded weapon's (the left hand's object) also
    /// moves the current progress. The unspent perk flag is only the wielded
    /// weapon's (or the notification's without a weapon).
    void handleWeaponProficiencyNotificationMessage(const ::tibia::protobuf::protocol::GameserverMessageWeaponProficiencyNotification& message);
    /// The lunar ascension orbs' balance.
    void handleResourceBalanceMessage(const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& message);

private:
    TProficiencyStorage* m_storage = nullptr;
    std::shared_ptr<container::TInventoryContainer> m_inventory;
};

}
}

