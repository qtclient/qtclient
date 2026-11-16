#pragma once

// CIP-named quickloot surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <client/position.h>

#include <QByteArray>
#include <QObject>

#include <cstdint>

namespace tibia::protobuf::protocol {
class GameserverMessageUpdateManagedContainers;
}

namespace tibia {
namespace input {
struct TGameAction;
}
namespace quickloot {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TManagedContainersProtocolMessageHandler final : public cip::tibia::quickloot::TManagedContainersProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::quickloot::TManagedContainersProtocolMessageHandler::TManagedContainersProtocolMessageHandler;
    /// Replaces the stored assignments, then the fallback flag. A category outside
    /// the known ones rejects the whole message.
    static void handleUpdateManagedContainersMessage(const ::tibia::protobuf::protocol::GameserverMessageUpdateManagedContainers& message);
    /// The ManagedContainer message's actions.
    enum EManagedContainerAction : uint8_t
    {
        SelectLootContainer = 0,
        ClearLootContainer = 1,
        OpenLootContainer = 2,
        SelectObtainContainer = 4,
        ClearObtainContainer = 5,
        OpenObtainContainer = 6,
    };

    /// Tells the server whether loot falls back to the main backpack.
    static void setLootContainerFallback(bool useMainBackpack);
    /// Loots the object; with `allCorpsesInArea` a corpse on the map loots
    /// every corpse around it.
    static void quickLoot(const Position& position, uint16_t objectId, int stackPosition, bool allCorpsesInArea);
    /// Loots every corpse around the player.
    static void quickLootAreaAtPlayer();
    /// One ManagedContainer action for an object category; select actions
    /// carry the chosen container.
    static void sendManagedContainer(EManagedContainerAction action, int category, const Position& position = {}, uint16_t objectId = 0,
                                     int stackPosition = 0);
};

class TQuickLootBlackWhitelistStorage final : public cip::tibia::quickloot::TQuickLootBlackWhitelistStorage
{
    Q_OBJECT
public:
    using cip::tibia::quickloot::TQuickLootBlackWhitelistStorage::TQuickLootBlackWhitelistStorage;
};

/// The quick-loot game actions: loot, the managed loot / obtain containers,
/// the fallback flag and the loot black/whitelist.
class TQuickLootGameActionHandler final : public cip::tibia::quickloot::TQuickLootGameActionHandler
{
    Q_OBJECT
public:
    explicit TQuickLootGameActionHandler(QObject* parent = nullptr);
    ~TQuickLootGameActionHandler() override;

    static bool handleGameAction(const input::TGameAction& action);

private:
    /// A full list refuses the object with a status line.
    static void handleAddToBlackWhitelistStatusAction(uint16_t objectId);
};

}
}

