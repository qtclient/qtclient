#pragma once

#include "input/gameactiondispatch.h"
#include <set>

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
namespace config {
class TCharacterConfigurationManager;
}
namespace input {
class IGameAction;
}
namespace quickloot {

class TManagedContainerStorage;

/// The session's managed container messages, into its managed container
/// storage; it sends the character's quick loot list whenever the character
/// configuration is loaded or the list changes.
class TManagedContainersProtocolMessageHandler final : public cip::tibia::quickloot::TManagedContainersProtocolMessageHandler
{
    Q_OBJECT
public:
    /// Follows the manager's configurationLoadedForCharacter and its quick loot
    /// list storage's configurationChangedFromUserInput.
    explicit TManagedContainersProtocolMessageHandler(TManagedContainerStorage* storage,
                                                      config::TCharacterConfigurationManager* characterConfigurationManager = nullptr,
                                                      QObject* parent = nullptr);
    /// Replaces the stored assignments, then the fallback flag. A category outside
    /// the known ones rejects the whole message.
    void handleUpdateManagedContainersMessage(const ::tibia::protobuf::protocol::GameserverMessageUpdateManagedContainers& message);
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
    /// Sends the quick loot list of `listType` (0 black-, 1 whitelist), the
    /// object type ids in ascending order.
    static void sendQuickLootBlackWhitelist(int listType, const std::set<uint16_t>& objectTypeIds);
    /// One ManagedContainer action for an object category; select actions
    /// carry the chosen container.
    static void sendManagedContainer(EManagedContainerAction action, int category, const Position& position = {}, uint16_t objectId = 0,
                                     int stackPosition = 0);

public Q_SLOTS:
    /// Once the character's configuration is loaded: sends its active quick
    /// loot list.
    void onQuickLootBlackWhitelistConfigurationChanged();

private:
    TManagedContainerStorage* m_storage;
    config::TCharacterConfigurationManager* m_characterConfigurationManager = nullptr;
};

/// The quick-loot game actions: loot, the managed loot / obtain containers,
/// the fallback flag and the loot black/whitelist.
class TQuickLootGameActionHandler final : public cip::tibia::quickloot::TQuickLootGameActionHandler
{
    Q_OBJECT
public:
    explicit TQuickLootGameActionHandler(QObject* parent = nullptr);
    ~TQuickLootGameActionHandler() override;

    bool handleGameAction(const input::IGameAction& action);

private:
    /// A full list refuses the object with a status line.
    void handleAddToBlackWhitelistStatusAction(uint16_t objectId);

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}

