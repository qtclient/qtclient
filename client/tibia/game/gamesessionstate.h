#pragma once


#include <QElapsedTimer>
#include "clockservice.h"
#include <tibia/schedule/schedule.h>
#include <array>

#include "client/declarations.h"
#include "game/objectreference.h"

#include <optional>
#include "protocol/clientmessages.h"
#include "client/staticdata.h"
#include "container/containerciptypes.h"

#include <QColor>
#include <QPoint>
#include <QRect>
#include <QSize>
#include "network/gameserverconnection.h"
#include "input/playermovementintenthandler.h"

namespace tibia::game {
class TGameserverGameSession;
}

namespace tibia::input {
class TPlayerMovementIntentHandler;
class TPlayerMovementGameActionHandler;
}

namespace tibia::game {

/// The state the gameserver session and the session-dump replay share (the
/// role of CIP game::TGameSessionBase): connection, login and world lifecycle, the
/// local player, combat modes, attack/follow targets and VIPs. Reached through
/// the `tibia::game::*` session functions (gamesession.h).
class TGameSessionState
{
    friend class tibia::game::TGameserverGameSession;

public:
    ~TGameSessionState();
    void terminate();

    /// Game connection lost (`clean` = peer close, no error reported).
    void processConnectionError(std::string_view message, bool clean);

    /// Qtclient native H (Ping/PingBack peel) — same side effects as classic parsePing*.
    /// Qtclient native H (WorldEntered peel) — same side effects as classic parseEnterGame
    /// (enter notify + gated processGameStart once per login).
    /// Login / session lifecycle (game connection + gamesession).
    void processGameStart();
    /// Qtclient native H (SetInventory / DeleteInventory peels) — same side effects as
    /// classic parseAddInventoryItem / parseRemoveInventoryItem.
    void processInventoryChange(uint8_t slot, const ::tibia::container::TInventorySlot& item);
    /// Server message side effects (`tibia::game::process*` in gamesession).
    void processCharacterSwitch(std::string_view characterName);
    void processUpdateTarget(uint32_t seq, uint32_t targetId);
    void processOpenContainer(uint8_t containerId, const ::tibia::container::TInventorySlot& containerItem,
                              std::string_view name, uint8_t capacity, bool hasParent,
                              const std::vector<::tibia::container::TInventorySlot>& items, bool isUnlocked,
                              bool hasPages, bool showSearch, uint16_t containerSize, uint16_t firstIndex);
    void processContainerAddItem(uint8_t containerId, const ::tibia::container::TInventorySlot& item, uint16_t slot);
    void processContainerUpdateItem(uint8_t containerId, uint16_t slot,
                                   const ::tibia::container::TInventorySlot& item);
    void processContainerRemoveItem(uint8_t containerId, uint16_t slot,
                                    const ::tibia::container::TInventorySlot& lastItem);

    /// Server message side effects (`tibia::game::process*` in gamesession) — session / UI wire.
    /// CIP SessionDumpStart (S 0x04); optional session key / dump id.
    /// CIP SessionDumpMarker (S 0x05); named cue inside a dump / tutorial.
    /// CIP SoundTrigger / Anthem (S 0x85): ambient (0), music (1), anthem (2).

    /// CIP / Crystal OutfitMemorial (S 0xB0) full golden + token tabs.

    // channel related


    // vip related

    // tutorial hint

    // outfit

    // npc trade (Phase 5.0-D: appearance slots / typeId counts — no ItemPtr in notify)

    // player trade
    static void processOwnTrade(std::string_view name, const std::vector<::tibia::container::TInventorySlot>& items);
    static void processCounterTrade(std::string_view name, const std::vector<::tibia::container::TInventorySlot>& items);

    // edit text/list

    // questlog

    // cyclopedia

public:
    void resetGameStates();
    void endSession();

public:
    void processDisconnect();

    void processEnterGame();

    void processGameEnd();

public:
    // login related
    /// The live session starts a login: the game state resets and `connection`
    /// becomes the session's game connection (the session connects it).
    void loginWorld(std::string_view worldName, std::string_view characterName,
                    std::shared_ptr<tibia::network::TGameserverSessionNetwork> connection);
    /// Opens a replay session. Decoded server messages are delivered with deliverReplayPacket.
    void startDecodedReplay();
    void deliverReplayPacket(const uint8_t* data, size_t size);
    void finishDecodedReplay();
    void cancelLogin();
    void forceLogout();

    // walk related
    bool walk(tibia::client::Direction direction);

    // item related (map/inventory by id + pos + stack — no ThingPtr play API)
    /// Use item / inventory item on a creature by id (no Map Creature resolve).
    void useOnCreature(const TObjectReference& object, uint32_t creatureId);
    std::optional<TObjectReference> findItemInContainers(uint32_t itemId, int subType, uint8_t tier);

    // container related (ids via TContainerStorage; no Container GameObject)
    /// Open from CIP typeId + position. previousContainerId >= 0 reuses that window.

    /// Phase 5.0-D: ephemeral Item for protocol wire only (controllers use CIP slots).

    // attack/follow related (creature ids; wire already sends ids)
    void attack(uint32_t creatureId);
    void follow(uint32_t creatureId);

    // talk related
    // 15.25+ crosshair / needPosition spells: aim tile appended to the next say (>= 1530)
    void setNextTalkAim(const Position& pos) { m_nextTalkAim = pos; }
    Position takeNextTalkAim()
    {
        const Position pos = m_nextTalkAim;
        m_nextTalkAim = Position(static_cast<int32_t>(UINT16_MAX), static_cast<int32_t>(UINT16_MAX), UINT8_MAX);
        return pos;
    }
    // The next say's aim byte (CIP Talk encoder): 1 crosshair, 2 at mouse position (both with the
    // aim tile), 3 at target or self (no tile); 0 = 1 when an aim tile is set.
    void setNextTalkAimMode(const uint8_t mode) { m_nextTalkAimMode = mode; }
    uint8_t takeNextTalkAimMode() { return std::exchange(m_nextTalkAimMode, 0); }
    // Cyclopedia map donate to respawn (ClientCyclopediaMapAction 0xDB)
    /// Cyclopedia map action 2: reset viewpoints progress for a subarea.

    // channel related

    // party related

    // Team Finder (C 0x2C / 0x2D) — Crystal parseLeaderFinderWindow / parseMemberFinderWindow

    // Depot search (C 0x29 / 0x92–0x95)

    // outfit related


    // vip related

    // npc trade related

    // player trade related

    // house window and editable items related


    // CIP wire formats: RuleViolationReport 0xF2, BugReport 0xE6, GuildMessage 0x9B

    // questlog related

    // 870 only

    // 910 only — cyclopedia / item-info by type id (no ItemPtr at UI).

    // >= 970 modal dialog

    // >= 984 browse field

    // >= 1080 ingame store

    //void reportRuleViolation2();

    // qtclient only


    void setProtocolVersion(uint16_t version);

    void setClientVersion(uint16_t version);
    int getClientVersion() { return m_clientVersion; }


    bool canPerformGameAction() const;

    bool isOnline() { return m_online; }
    bool isSwitchingCharacter() { return m_switchingCharacter; }
    bool isDead() { return m_dead; }
    bool isSceneFrozen() { return m_sceneFrozen; }
    void setSceneFrozen(bool frozen);
    bool isAttacking();
    bool isConnectionOk();

    uint32_t getAttackingCreatureId() const { return m_attackingCreatureId; }
    uint32_t getFollowingCreatureId() const { return m_followingCreatureId; }
    // Local-only attack target (no ClientAttack packet). Used when the server AI
    // syncs the target via extended opcode (vanilla has no S→C set-attack).
    void setAttackingCreatureId(uint32_t creatureId);
    void setCanReportBugs(const bool enable) { m_canReportBugs = enable; }
    void setCanExivaOptions(const bool enable) { m_CanExivaOptions = enable; }
    // the server's BugReportsAllowed state: 0 allowed, else the refusal reason; -1 before the server sent it
    void setBugReportState(const int state) { m_bugReportState = state; }
    int getBugReportState() const { return m_bugReportState; }
    bool canExivaOptions() { return m_CanExivaOptions; }
    void setExpertPvpMode(const bool enable) { m_expertPvpMode = enable; }
    bool getExpertPvpMode() { return m_expertPvpMode; }
    /// The session has a character: from the login until the logout.
    bool hasLocalPlayer() const { return m_playerPresent; }
    /// The live session's player movement intent handler (none in a session dump).
    tibia::input::TPlayerMovementIntentHandler* movementIntent();
    /// Host inputs of the live session's player movement intent handler: the
    /// aware range and the pack's unreachable-goal claim.
    static tibia::input::TPlayerMovementIntentHandler::Environment playerMovementIntentEnvironment();
    /// A login (or character switch) starts the character's session.
    void beginPlayerSession(std::string_view characterName);
    std::string getCharacterName() { return m_characterName; }
    std::string getWorldName() { return m_worldName; }

    int findEmptyContainerId();

    // market related

    // prey related

    // Task Board / SoulSeals / Offline Training related

    // exiva related

    // forge related
    /// CIP GameclientMessageSetClientOptions (0x2E): option 1 = always turn towards the
    /// direction of movement.

    // imbuing related

    // weapon proficiency related




    // highscore related


    // quickLoot related
    void requestQuickLootBlackWhiteList(uint8_t filter, uint16_t size, const std::vector<uint16_t>& listedItems);


    // cyclopedia related
    /// Friend system action 0x0E: the current character title (0 = none).
    /// C 0x81 with a JSON body (friends, invites, search, badges, settings).
    /// ChangeOutfit type 2 (CIP SET_SHOW_OFF_SOCKET_CONFIGURATION): podium look.
    /// ChangeOutfit type 3 (CIP SET_HIRELING_OUTFIT): hireling creature id.

    //whell of destiny 

    void updateMapLatency() {
        if (!m_mapUpdateTimer.first) {
            m_mapUpdatedAt = m_mapUpdateTimer.second.ticksElapsed();
            m_mapUpdateTimer.first = true;
        }
    }

public:
    void setFollowingCreatureId(uint32_t creatureId);

    bool m_playerPresent{ false };
    uint32_t m_attackingCreatureId{ 0 };
    uint32_t m_followingCreatureId{ 0 };
    /// The game connection of the running session (null when offline).
    std::shared_ptr<tibia::network::TGameserverSessionNetwork> m_gameConnection;
    tibia::schedule::TScheduledTaskPtr m_checkConnectionEvent;

    bool m_online{ false };
    bool m_switchingCharacter{ false };
    bool m_dead{ false };
    bool m_sceneFrozen{ false };
    bool m_expertPvpMode{ false };
    bool m_connectionFailWarned{ false };
    bool m_canReportBugs{ false };
    int m_bugReportState{ -1 };
    bool m_CanExivaOptions{ false };

    Position m_nextTalkAim{ static_cast<int32_t>(UINT16_MAX), static_cast<int32_t>(UINT16_MAX), UINT8_MAX };
    uint8_t m_nextTalkAimMode{ 0 };

    uint16_t m_mapUpdatedAt{ 0 };
    std::pair<uint16_t, tibia::shared::TFrameStopWatch> m_mapUpdateTimer = { true, tibia::shared::TFrameStopWatch{} };

    uint16_t m_protocolVersion{ 0 };
    uint16_t m_clientVersion{ 0 };
    uint32_t m_seq{ 0 };

    std::string m_characterName;
    std::string m_worldName;

};

/// The live session state.
TGameSessionState& gameSessionState();

}
