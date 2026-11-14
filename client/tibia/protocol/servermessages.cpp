/*
* Copyright (c) 2024-2026 qtclient contributors
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

#include "creatures/creatureprotocolapply.h"
#include "protocol/mapwire.h"
#include <protocol.pb.h>
#include "effects/effectstorage.h"
#include "client/magiceffectopacity.h"
#include "framework/core/clock.h"

#include <utility>
#include <tibia/game/gamesession.h>
#include <tibia/appearances/thing.h>
#include <tibia/creatures/attachedeffect.h>
#include <tibia/creatures/attachedeffectmanager.h>
#include <tibia/game/sessionhooks.h>
#include "worldmap/worldmapfield.h"
#include "appearances/appearanceinstance.h"
#include "container/containerciptypes.h"
#include "client/animatedtext.h"
#include <tibia/config/runtimegameconfig.h>
#include "client/item.h"
#include "protocol/protocolcodes.h"
#include "client/protocolgame.h"
#include <tibia/schedule/schedule.h>
#include "framework/net/inputmessage.h"
#include "tibia/protocol/extendedopcode.h"
#include "tibia/protocol/inventoryitemwire.h"
#include "tibia/protocol/protocolciptypes.h"
#include "tibia/protocol/protocolclassicwire.h"

#include <protocol.pb.h>

#include <framework/core/gamehooks.h>
#include "tibia/assets/thingtype.h"
#include "tibia/assets/things.h"
#include "framework/net/inputmessage.h"
#include <algorithm>
#include "tibia/sound/soundplayback.h"
#include "tibia/game/cooldownstorage.h"
#include "tibia/gamewindow/newsdialogcontroller.h"
#include "tibia/cyclopedia/iteminfostorage.h"
#include <format>
#include <idleot/stats.h>
#include <map>
#include <sstream>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>
#include <creatures/creaturestorage.h>
#include <worldmap/worldmapprotocolmessagehandler.h>
#include <tibia/worldmap/worldmapstorage.h>

#include <QString>
#include <tibia/game/gamesession.h>
#include <tibia/game/localplayersessionthing.h>
#include <tibia/game/mapsession.h>
#include <tibia/game/thingsession_forward.h>

namespace {

Position peeledMapCentral()
{
    const tibia::game::MapPos center = tibia::game::centralPosition();
    return Position(center.x, center.y, static_cast<uint8_t>(center.z));
}

static void fillInspectionAppearance(auto& dest, const tibia::container::TInventorySlot& slot)
{
    dest.typeId = slot.typeId;
    dest.count = slot.stackable ? std::max<uint16_t>(1, slot.count) : 1;
    dest.tier = slot.tier;
    dest.fluidOrSubtype = slot.fluidOrSplash ? slot.fluid : 0;
    dest.fluidOrSplash = slot.fluidOrSplash;
    ThingType* type = slot.appearance ? slot.appearance->thingType()
                                      : tibia::game::rawThingType(slot.typeId, ThingCategoryItem);
    dest.hookSouth = type && type->isHookSouth();
    dest.hookEast = type && type->isHookEast();
    dest.hasProficiency = type && type->getProficiencyId() != 0;
}


} // namespace

void ProtocolGame::parseMessage(const InputMessagePtr& msg)
{
    int opcode = -1;
    int prevOpcode = -1;
    const bool bridgePass = m_applyingClassicBridge;

    const auto handleOpcode = [&](uint8_t op) {
        // must be > so extended will be enabled before GameStart.
        if (!tibia::game::cipFeature(tibia::client::GameLoginPending)) {
            if (!m_gameInitialized && op > Proto::GameServerFirstGameOpcode) {
                tibia::game::processGameStart();
                m_gameInitialized = true;
            }
        }

        if (dispatchServerMessage(op, msg))
            return;

        // Product opcodes (not CIP): the extended-opcode transport and the
        // same-socket character switch.
        switch (op) {
            case Proto::GameServerCharacterSwitch:
                parseCharacterSwitch(msg);
                break;
            case Proto::GameServerExtendedOpcode:
                parseExtendedOpcode(msg);
                break;
            default: {
                const auto unreadSize = msg->getUnreadSize();
                const auto previewSize = std::min<size_t>(unreadSize, 32);
                const auto remainingBytes = msg->peekBytes(previewSize);
                std::stringstream hexDump;
                for (const auto byte : remainingBytes)
                    hexDump << std::format("{:02X} ", byte);
                g_logger.warning(
                    "[{}] Unhandled opcode 0x{:02X} ({}) with {} unread bytes; previous opcode: 0x{:02X} ({}); next bytes: {}",
                    tibia::game::clientRevision(), op, op, unreadSize, prevOpcode, prevOpcode, hexDump.str());
                msg->skipBytes(static_cast<uint16_t>(unreadSize));
                break;
            }
        }
    };

    try {
        while (!msg->eof()) {
            // CIP order: TR → Q → H. Structured opcodes measure without side
            // effects, enqueue, then classic bridge applies. Bridge re-entry
            // (applyClassicServerFrame) only runs handleOpcode.
            const int frameBegin = msg->getReadSize();
            opcode = msg->getU8();
            AutoStat s(STATS_PACKETS, std::format("{} (0x{:02X})", opcode, opcode));

            if (bridgePass) {
                handleOpcode(static_cast<uint8_t>(opcode));
                prevOpcode = opcode;
                continue;
            }

            auto* xlat = tibia::protocol::serverMessageTranslator();
            // Extended / product opcodes stay qtclient-local (product pack mods), not CIP queue.
            const bool productOpcode = (opcode == Proto::GameServerExtendedOpcode);
            // Structured measure (incl. LoginChallenge 1405+ pad) is length authority.
            const bool cipTranslateFirst = xlat && xlat->hasStructuredLayout(opcode);

            if (cipTranslateFirst) {
                const auto unread = msg->peekBytes(msg->getUnreadSize());
                const size_t measured = xlat->measureStructuredPayload(
                    opcode, unread.empty() ? nullptr : unread.data(), unread.size());
                // Empty structured bodies measure 0 on success — still TR→Q→H
                // (do not fall through to classic length authority).
                const bool measureOk = measured > 0
                    || (measured == 0 && tibia::protocol::emptyStructuredServerOpcode(opcode));
                if (measureOk) {
                    if (measured > 0)
                        msg->skipBytes(static_cast<uint16_t>(std::min(measured, size_t(0xffff))));
                    const int frameEnd = msg->getReadSize();
                    const auto buf = msg->getBuffer();
                    const QByteArray classic(buf.data() + frameBegin, frameEnd - frameBegin);
                    // CIP: TR → Q → H. Peeled opcodes have native handlers;
                    // classic bridge skipped (see nativeApplySkipsClassic).
                    const bool applyClassic = !tibia::protocol::nativeApplySkipsClassic(opcode);
                    tibia::protocol::enqueueTranslatedServerFrame(classic, applyClassic);
                } else if (tibia::protocol::nativeApplySkipsClassic(opcode)) {
                    // A native message that does not read: its length is unknown,
                    // so the rest of the packet cannot be read either.
                    g_logger.error("[{}] unreadable message 0x{:02X} ({} bytes left); dropping the rest of the packet",
                                   tibia::game::clientRevision(), opcode, msg->getUnreadSize());
                    msg->skipBytes(static_cast<uint16_t>(msg->getUnreadSize()));
                } else {
                    // Measure failed — classic length authority, queue observability only.
                    handleOpcode(static_cast<uint8_t>(opcode));
                    const int frameEnd = msg->getReadSize();
                    const auto buf = msg->getBuffer();
                    const QByteArray classic(buf.data() + frameBegin, frameEnd - frameBegin);
                    tibia::protocol::enqueueTranslatedServerFrame(classic, false);
                }
            } else if (productOpcode) {
                // product pack / qtclient mods: ExtendedOpcode stays qtclient-local (not CIP queue).
                handleOpcode(static_cast<uint8_t>(opcode));
            } else {
                // Bridge layouts: classic length + behavior, then queue without re-apply.
                handleOpcode(static_cast<uint8_t>(opcode));
                const int frameEnd = msg->getReadSize();
                const auto buf = msg->getBuffer();
                const QByteArray classic(buf.data() + frameBegin, frameEnd - frameBegin);
                tibia::protocol::enqueueTranslatedServerFrame(classic, false);
            }

            prevOpcode = opcode;
        }
    } catch (const stdext::exception& e) {
        const auto unread = msg->getUnreadSize();
        const auto readPos = msg->getReadPos();
        const auto remainingBytes = msg->peekBytes(unread);

        std::stringstream hexDump;
        for (const auto byte : remainingBytes) {
            hexDump << std::format("{:02X} ", byte);
        }

        g_logger.error(
            "ProtocolGame parse message exception ({} bytes, {} unread at pos {}, last opcode: 0x{:02X} ({:d}), prev opcode: 0x{:02X} ({:d}), protocol: {}): {}\n"
            "Next unread bytes: {}\n",
            msg->getMessageSize(),
            unread,
            readPos,
            opcode, opcode,
            prevOpcode, prevOpcode,
            tibia::game::getProtocolVersion(),
            e.what(),
            hexDump.str()
        );

        std::ofstream packet("packet.log", std::ios::app);
        if (!packet.is_open()) {
            return;
        }

        packet << std::format(
            "[EXCEPTION] {} unread bytes at pos: {}, protocol: {}, current: 0x{:02X} ({:d}), prev: 0x{:02X} ({:d}), error: {}\nBytes: {}\n",
            unread, readPos,
            tibia::game::getProtocolVersion(),
            opcode, opcode,
            prevOpcode, prevOpcode,
            e.what(),
            hexDump.str()
        );
    }
}


void ProtocolGame::parseCharacterSwitch(const InputMessagePtr& msg)
{
    const auto characterName = msg->getString();
    tibia::game::processCharacterSwitch(characterName);
    // The server follows with a full login burst for the new body on this socket.
    m_localPlayer = tibia::game::localPlayer();
    m_gameInitialized = false;
}


void ProtocolGame::parsePendingGame(const InputMessagePtr&)
{
    //set player to pending game state
    tibia::game::processPendingGame();
}



void ProtocolGame::parsePing(const InputMessagePtr&) { tibia::game::processPing(); }
void ProtocolGame::parsePingBack(const InputMessagePtr&) { tibia::game::processPingBack(); }



static void readInspectionDescriptions(const InputMessagePtr& msg, std::vector<std::pair<std::string, std::string>>& descriptions, const uint8_t count)
{
    descriptions.reserve(count);
    for (auto i = 0; std::cmp_less(i, count); ++i) {
        auto key = msg->getString();
        auto value = msg->getString();
        descriptions.emplace_back(std::move(key), std::move(value));
    }
}

static CyclopediaCharacterInspection readCyclopediaCharacterInspectionData(ProtocolGame& protocol, const InputMessagePtr& msg, const uint8_t inventoryCount, const uint32_t creatureId)
{
    CyclopediaCharacterInspection data;
    data.inspectionType = tibia::client::INSPECT_CYCLOPEDIA;
    data.creatureId = creatureId;
    data.inventoryItems.reserve(inventoryCount);

    for (auto i = 0; std::cmp_less(i, inventoryCount); ++i) {
        InspectionInventoryItem entry;
        entry.slot = msg->getU8();
        entry.name = msg->getString();
        fillInspectionAppearance(entry, protocol.getInventorySlot(msg));

        const uint8_t imbuementCount = msg->getU8();
        entry.imbuements.reserve(imbuementCount);
        for (auto j = 0; std::cmp_less(j, imbuementCount); ++j) {
            entry.imbuements.push_back(msg->getU16());
        }

        const uint8_t descCount = msg->getU8();
        readInspectionDescriptions(msg, entry.descriptions, descCount);
        data.inventoryItems.emplace_back(std::move(entry));
    }

    data.playerName = msg->getString();
    data.outfit = protocol.getOutfit(msg, false);


    const uint8_t playerDescCount = msg->getU8();
    readInspectionDescriptions(msg, data.playerDescriptions, playerDescCount);

    return data;
}

void ProtocolGame::parseAnimatedText(const InputMessagePtr& msg)
{
    const auto& position = getPosition(msg);
    const uint8_t color = msg->getU8();
    const auto& text = msg->getString();

    tibia::worldmap::TWorldMapStorage::ensureStorage()->addAnimatedText(std::make_shared<AnimatedText>(text, color), position);
}

void ProtocolGame::parseBestiaryCharmsData(const InputMessagePtr& msg)
{
    BestiaryCharmsData charmData;

    if (tibia::game::clientRevision() >= 1410) {
        charmData.resetAllCharmsCost = msg->getU64();
    } else {
        charmData.points = msg->getU32();
    }

    const uint8_t charmsAmount = msg->getU8();
    charmData.charms.reserve(charmsAmount);

    for (auto i = 0; i < charmsAmount; ++i) {
        CharmData charm;
        charm.id = msg->getU8();
        charm.asignedStatus = false;
        charm.raceId = 0;
        charm.removeRuneCost = 0;
        if (tibia::game::clientRevision() >= 1410) {
            charm.tier = msg->getU8();
            charm.unlocked = msg->getU8() == 1;
        } else {
            charm.name = msg->getString();
            charm.description = msg->getString();
            msg->getU8(); // Unknown byte
            charm.unlockPrice = msg->getU16();
            charm.unlocked = msg->getU8() == 1;
            charm.tier = charm.unlocked ? 1 : 0;
        }
        if (charm.unlocked) {
            bool asigned = true;
            if (tibia::game::clientRevision() < 1410) {
                asigned = static_cast<bool>(msg->getU8());
            }

            if (asigned) {
                charm.asignedStatus = true;
                charm.raceId = msg->getU16();
                charm.removeRuneCost = msg->getU32();
            }
        } else if (tibia::game::clientRevision() < 1410) {
            msg->getU8(); // ??
        }

        charmData.charms.emplace_back(charm);
    }

    if (tibia::game::clientRevision() >= 1410) {
        charmData.availableCharmSlots = msg->getU8();
    } else {
        msg->getU8(); // ??
    }

    const uint16_t finishedMonstersSize = msg->getU16();
    charmData.finishedMonsters.reserve(finishedMonstersSize);

    for (auto i = 0; i < finishedMonstersSize; ++i) {
        uint32_t raceId;
        if (tibia::game::clientRevision() >= 1410) {
            raceId = msg->getU32();
        } else {
            raceId = msg->getU16();
        }
        charmData.finishedMonsters.emplace_back(raceId);
    }

    tibia::game::processUpdateBestiaryCharmsData(charmData);
}


void ProtocolGame::parseExtendedOpcode(const InputMessagePtr& msg)
{
    const uint8_t opcode = msg->getU8();
    const auto& buffer = msg->getString();

    if (opcode == 0) {
        m_enableSendExtendedOpcode = true;
        return;
    }
    if (opcode == 2) {
        parsePingBack(msg);
        return;
    }
    tibia::protocol::ExtendedOpcodeReader::instance().publish(opcode, buffer);
}

Outfit ProtocolGame::getOutfit(const InputMessagePtr& msg, const bool parseMount/* = true*/) const
{
    Outfit outfit;

    uint16_t lookType = tibia::game::cipFeature(tibia::client::GameLooktypeU16) ? msg->getU16() : msg->getU8();

    if (lookType != 0) {
        outfit.setCategory(ThingCategoryCreature);
        const uint8_t head = msg->getU8();
        const uint8_t body = msg->getU8();
        const uint8_t legs = msg->getU8();
        const uint8_t feet = msg->getU8();
        const uint8_t addons = tibia::game::cipFeature(tibia::client::GamePlayerAddons) ? msg->getU8() : 0;

        if (!tibia::game::isValidDatId(lookType, ThingCategoryCreature)) {
            g_logger.traceError("invalid outfit looktype {}", lookType);
            lookType = 0;
        }

        outfit.setId(lookType);
        outfit.setHead(head);
        outfit.setBody(body);
        outfit.setLegs(legs);
        outfit.setFeet(feet);
        outfit.setAddons(addons);
    } else {
        uint16_t lookTypeEx = msg->getU16();
        if (lookTypeEx == 0) {
            outfit.setCategory(ThingCategoryEffect);
            outfit.setAuxId(13); // invisible effect id
        } else {
            if (!tibia::game::isValidDatId(lookTypeEx, ThingCategoryItem)) {
                g_logger.traceError("invalid outfit looktypeex {}", lookTypeEx);
                lookTypeEx = 0;
            }
            outfit.setCategory(ThingCategoryItem);
            outfit.setAuxId(lookTypeEx);
        }
    }

    if (tibia::game::cipFeature(tibia::client::GamePlayerMounts) && parseMount) {
        const uint16_t mount = msg->getU16();
        if (tibia::game::clientRevision() >= 1281 && mount != 0) {
            const uint8_t head = msg->getU8();
            const uint8_t body = msg->getU8();
            const uint8_t legs = msg->getU8();
            const uint8_t feet = msg->getU8();
            outfit.setMountColors(head, body, legs, feet);
        }
        outfit.setMount(mount);
    }


    return outfit;
}

tibia::container::TInventorySlot ProtocolGame::getInventorySlot(const InputMessagePtr& msg, int id)
{
    // Shared wire reader with structured SetInventory fill (ProtocolInventoryItem).
    const int unread = msg->getUnreadSize();
    const auto bytes = msg->peekBytes(unread);
    tibia::protocol::ClassicReader reader{ bytes.data(), bytes.size(), 0 };
    ::tibia::protobuf::protocol::ProtocolInventoryItem wire;
    if (!tibia::protocol::readInventoryItem(reader, &wire, id)) {
        const uint16_t badId = id != 0 ? static_cast<uint16_t>(id)
                                       : static_cast<uint16_t>(wire.item_id());
        throw Exception("ProtocolGame::getInventorySlot: unable to resolve item with invalid id {}", badId);
    }
    msg->skipBytes(static_cast<uint16_t>(reader.i));
    return tibia::container::TInventorySlot::fromProtocolInventoryItem(wire);
}

Position ProtocolGame::getPosition(const InputMessagePtr& msg)
{
    const uint16_t x = msg->getU16();
    const uint16_t y = msg->getU16();
    const uint8_t z = msg->getU8();

    return { x, y, z };
}


namespace
{
    constexpr uint8_t TASK_BOARD_TALISMAN_PATHS = 4;
    constexpr uint8_t TASK_BOARD_WEEKLY_BASE_SLOTS = 6;
    constexpr uint8_t TASK_BOARD_WEEKLY_EXPANDED_SLOTS = 9;
    constexpr uint32_t TASK_BOARD_SECONDS_PER_DAY = 24 * 60 * 60;

    using TaskBoardRaceIdSet = std::unordered_set<uint16_t>;

    uint8_t getTaskBoardTalismanMaxLevel(const uint8_t pathIndex)
    {
        switch (pathIndex) {
            case 0:
            case 1:
            case 2:
                return 166;
            case 3:
                return 180;
            default:
                return 166;
        }
    }

    uint16_t getTaskBoardTalismanBonusHundredths(const uint8_t level, const uint8_t pathIndex)
    {
        if (level == 0) {
            return 0;
        }

        switch (pathIndex) {
            case 0:
            case 1:
            case 2: {
                if (level <= 26) {
                    return static_cast<uint16_t>(250 + (static_cast<uint32_t>(level) - 1U) * 50U);
                }
                return static_cast<uint16_t>(std::min<uint32_t>(1500U + (static_cast<uint32_t>(level) - 26U) * 25U, 5000U));
            }
            case 3: {
                if (level <= 20) {
                    return static_cast<uint16_t>(static_cast<uint32_t>(level) * 100U);
                }
                return static_cast<uint16_t>(std::min<uint32_t>(2000U + (static_cast<uint32_t>(level) - 20U) * 50U, 10000U));
            }
            default:
                return 0;
        }
    }

    uint8_t getRemainingDaysUntil(const uint32_t unixTimestamp)
    {
        if (unixTimestamp == 0) {
            return 0;
        }

        const auto now = static_cast<uint32_t>(std::time(nullptr));
        if (unixTimestamp <= now) {
            return 0;
        }

        const auto remainingSeconds = unixTimestamp - now;
        const auto rawDays = (remainingSeconds + TASK_BOARD_SECONDS_PER_DAY - 1) / TASK_BOARD_SECONDS_PER_DAY;
        return static_cast<uint8_t>(std::min<uint32_t>(rawDays, 255));
    }

    std::vector<uint16_t> getAllMonsterRaceIds()
    {
        const auto& races = tibia::game::getAllRaces();
        std::vector<uint16_t> raceIds;
        raceIds.reserve(races.size());

        for (const auto& race : races) {
            // race.raceId is uint32_t; bound to uint16_t for the preferred-slot packet.
            if (race.boss || race.raceId == 0 || race.raceId > std::numeric_limits<uint16_t>::max()) {
                continue;
            }
            raceIds.emplace_back(static_cast<uint16_t>(race.raceId));
        }

        std::sort(raceIds.begin(), raceIds.end());
        raceIds.erase(std::unique(raceIds.begin(), raceIds.end()), raceIds.end());
        return raceIds;
    }

    constexpr uint16_t getSoulsealPointsForCategory(const uint32_t category) noexcept
    {
        // (category + 1) * 10 — max category that fits uint16_t is 6552; beyond that, clamp.
        const uint32_t points = (category + 1U) * 10U;
        return static_cast<uint16_t>(std::min<uint32_t>(points, std::numeric_limits<uint16_t>::max()));
    }

    constexpr uint8_t toSoulsealCategoryIndex(const RaceType& raceData) noexcept
    {
        return raceData.hasCategory
            ? static_cast<uint8_t>(std::min<uint32_t>(raceData.category, std::numeric_limits<uint8_t>::max()))
            : 0;
    }

    constexpr bool isTaskBoardSoulsealRace(const RaceType& raceData) noexcept
    {
        return raceData.hasCategory && !raceData.boss
            && raceData.raceId != 0
            && raceData.raceId <= std::numeric_limits<uint16_t>::max();
    }

    TaskBoardSoulsealEntryData makeTaskBoardSoulsealEntry(const uint16_t raceId, const RaceType& raceData, const bool done)
    {
        TaskBoardSoulsealEntryData entry;
        entry.raceId = raceId;
        entry.category = toSoulsealCategoryIndex(raceData);
        entry.soulsealPoints = raceData.hasCategory ? getSoulsealPointsForCategory(raceData.category) : 0;
        entry.done = done ? 1 : 0;
        entry.outfit = raceData.outfit;
        if (raceData.name.empty()) {
            entry.name = std::to_string(raceId);
        } else {
            entry.name = raceData.name;
        }
        return entry;
    }

    TaskBoardRaceIdSet readTaskBoardMasteredRaceIds(const InputMessagePtr& msg)
    {
        const uint16_t masteredCount = msg->getU16();
        TaskBoardRaceIdSet masteredRaceIds;
        masteredRaceIds.reserve(masteredCount);

        for (auto i = 0; std::cmp_less(i, masteredCount); ++i) {
            const uint16_t raceId = msg->getU16();
            if (raceId != 0) {
                masteredRaceIds.insert(raceId);
            }
        }

        return masteredRaceIds;
    }

    std::vector<TaskBoardSoulsealEntryData> buildTaskBoardSoulsealEntries(TaskBoardRaceIdSet&& masteredRaceIds)
    {
        const auto& races = tibia::game::getAllRaces();
        std::vector<TaskBoardSoulsealEntryData> entries;
        entries.reserve(races.size());

        for (const auto& raceData : races) {
            if (!isTaskBoardSoulsealRace(raceData)) {
                continue;
            }

            const auto raceId = static_cast<uint16_t>(raceData.raceId);
            const bool done = masteredRaceIds.erase(raceId) > 0;
            entries.emplace_back(makeTaskBoardSoulsealEntry(raceId, raceData, done));
        }

        for (const auto raceId : masteredRaceIds) {
            entries.emplace_back(makeTaskBoardSoulsealEntry(raceId, tibia::game::raceData(raceId), true));
        }

        return entries;
    }

    std::map<std::string, uint32_t> toBountyHeaderMap(const TaskBoardBountyHeaderData& header)
    {
        return {
            { "rerollPoints", header.rerollPoints },
            { "claimDaily", header.claimDaily },
            { "difficulty", header.difficulty }
        };
    }

    std::map<std::string, uint32_t> toBountyMonsterMap(const TaskBoardBountyMonsterData& monster)
    {
        return {
            { "taskIndex", monster.taskIndex },
            { "raceId", monster.raceId },
            { "currentKills", monster.currentKills },
            { "totalKills", monster.totalKills },
            { "rewardXp", monster.rewardXp },
            { "rewardPoints", monster.rewardPoints },
            { "rewardReroll", monster.rewardReroll },
            { "rewardDust", monster.rewardDust },
            { "rarity", monster.rarity },
            { "isActive", monster.isActive },
            { "isCompleted", monster.isCompleted }
        };
    }

    std::map<std::string, uint32_t> toTalismanMap(const TaskBoardTalismanData& talisman)
    {
        return {
            { "currentValue", talisman.currentValue },
            { "nextValue", talisman.nextValue },
            { "upgradeCost", talisman.upgradeCost },
            { "isActiveUpgrade", talisman.isActiveUpgrade }
        };
    }

    std::map<std::string, uint32_t> toPreferredSlotMap(const TaskBoardPreferredSlotData& slot)
    {
        return {
            { "slot", slot.slot },
            { "locked", slot.locked },
            { "preferred", slot.preferred },
            { "unwanted", slot.unwanted },
            { "price", slot.price }
        };
    }

    std::map<std::string, uint32_t> toWeeklyHeaderMap(const TaskBoardWeeklyHeaderData& header)
    {
        return {
            { "difficulty", header.difficulty },
            { "currentPlayerLevel", header.currentPlayerLevel },
            { "remainingDays", header.remainingDays },
            { "totalTaskSlots", header.totalTaskSlots },
            { "maxExperience", header.maxExperience },
            { "maxDeliveryExperience", header.maxDeliveryExperience },
            { "completedKillTasks", header.completedKillTasks },
            { "completedDeliveryTasks", header.completedDeliveryTasks },
            { "pointsEarned", header.pointsEarned },
            { "soulsealsEarned", header.soulsealsEarned },
            { "extraSlot", header.extraSlot }
        };
    }

    std::map<std::string, uint32_t> toWeeklyMonsterMap(const TaskBoardWeeklyMonsterData& monster)
    {
        return {
            { "raceId", monster.raceId },
            { "current", monster.current },
            { "total", monster.total },
            { "state", monster.state }
        };
    }

    std::map<std::string, uint32_t> toWeeklyItemMap(const TaskBoardWeeklyItemData& item)
    {
        return {
            { "slotIndex", item.slotIndex },
            { "itemId", item.itemId },
            { "current", item.current },
            { "total", item.total },
            { "claimed", item.claimed },
            { "state", item.state }
        };
    }

    std::map<std::string, std::string> toShopItemMap(const TaskBoardShopItemData& item)
    {
        return {
            { "id", std::to_string(item.id) },
            { "offerType", std::to_string(item.offerType) },
            { "title", item.title },
            { "description", item.description },
            { "price", std::to_string(item.price) },
            { "bought", std::to_string(item.bought) },
            { "lookType", std::to_string(item.lookType) },
            { "lookAddons", std::to_string(item.lookAddons) },
            { "itemId", std::to_string(item.itemId) },
            { "optionalItemId", std::to_string(item.optionalItemId) },
            { "maxPurchases", std::to_string(item.maxPurchases) },
            { "currentPurchases", std::to_string(item.currentPurchases) },
            { "nextCost", std::to_string(item.nextCost) }
        };
    }

}

void ProtocolGame::parseTaskBoardData(const InputMessagePtr& msg)
{
    const auto subtype = static_cast<tibia::client::TaskBoardType_t>(msg->getU8());
    switch (subtype) {
        case tibia::client::TASK_BOARD_BOUNTY:
            parseTaskBoardBountyData(msg);
            break;
        case tibia::client::TASK_BOARD_WEEKLY:
            parseTaskBoardWeeklyData(msg);
            break;
        case tibia::client::TASK_BOARD_HUNT_SHOP:
            parseTaskBoardShopData(msg);
            break;
        default:
            throw stdext::exception("[ProtocolGame::parseTaskBoardData] Unknown subtype {}", static_cast<uint8_t>(subtype));
    }
}

void ProtocolGame::parseTaskBoardBountyData(const InputMessagePtr& msg)
{
    TaskBoardBountyHeaderData headerData;
    std::vector<TaskBoardBountyMonsterData> monsters;
    std::vector<TaskBoardTalismanData> talismans;
    std::vector<TaskBoardPreferredSlotData> preferredSlots;

    const uint8_t offerCount = msg->getU8();
    monsters.reserve(offerCount);

    for (auto i = 0; std::cmp_less(i, offerCount); ++i) {
        TaskBoardBountyMonsterData monster;
        monster.taskIndex = msg->getU8();
        monster.raceId = msg->getU16();
        monster.totalKills = msg->getU16();
        monster.rewardXp = msg->getU32();
        monster.rewardPoints = msg->getU8();
        monster.currentKills = msg->getU16();
        const auto bountyState = msg->getU8(); // retail button state: 0 available, 1 active, 2 claimable, 3 claimed/completed
        // Winter 2026: rarity 3 = platinum (8×). No rewardDust on OTC wire yet.
        monster.rarity = std::min<uint8_t>(msg->getU8(), 3);
        monster.rewardDust = 0;
        // Server does not expose per-monster reroll reward; assume 1 for UI display.
        monster.rewardReroll = 1;
        monster.isActive = (bountyState == 1 || bountyState == 2) ? 1 : 0;
        monster.isCompleted = bountyState == 3 ? 1 : 0;
        monsters.emplace_back(monster);
    }

    headerData.rerollPoints = msg->getU8();
    const auto rerollMode = static_cast<tibia::client::TaskBoardBountyRerollMode_t>(msg->getU8());
    headerData.claimDaily = rerollMode == tibia::client::TASK_BOARD_BOUNTY_REROLL_DAILY_CLAIMABLE ? 1 : 0;
    headerData.difficulty = std::clamp<uint8_t>(msg->getU8() + 1, 1, 4);

    talismans.reserve(TASK_BOARD_TALISMAN_PATHS);
    for (uint8_t i = 0; std::cmp_less(i, TASK_BOARD_TALISMAN_PATHS); ++i) {
        TaskBoardTalismanData talisman;
        const uint8_t currentLevel = msg->getU8();
        msg->getU8(); // multiplier2 is unused on server
        talisman.isActiveUpgrade = msg->getU8();
        talisman.upgradeCost = msg->getU16();
        talisman.currentValue = getTaskBoardTalismanBonusHundredths(currentLevel, i);

        const uint8_t maxLevel = getTaskBoardTalismanMaxLevel(i);
        if (currentLevel >= maxLevel || talisman.upgradeCost == 0) {
            talisman.nextValue = 0;
        } else {
            talisman.nextValue = getTaskBoardTalismanBonusHundredths(static_cast<uint8_t>(currentLevel + 1), i);
        }
        talismans.emplace_back(talisman);
    }

    const uint8_t preferredSlotCount = msg->getU8();
    preferredSlots.reserve(preferredSlotCount);
    for (auto i = 0; std::cmp_less(i, preferredSlotCount); ++i) {
        TaskBoardPreferredSlotData slot;
        slot.slot = i + 1;
        slot.locked = msg->getU8() == 0 ? 1 : 0;
        slot.preferred = msg->getU16();
        slot.unwanted = msg->getU16();
        slot.price = 0;
        preferredSlots.emplace_back(slot);
    }

    std::vector<std::map<std::string, uint32_t>> monsterData;
    monsterData.reserve(monsters.size());
    for (const auto& monster : monsters) {
        monsterData.emplace_back(toBountyMonsterMap(monster));
    }

    std::vector<std::map<std::string, uint32_t>> talismanData;
    talismanData.reserve(talismans.size());
    for (const auto& talisman : talismans) {
        talismanData.emplace_back(toTalismanMap(talisman));
    }

    tibia::game::notify("onBountyTaskData", toBountyHeaderMap(headerData), monsterData, talismanData);

    std::vector<std::map<std::string, uint32_t>> preferredSlotData;
    preferredSlotData.reserve(preferredSlots.size());
    for (const auto& slot : preferredSlots) {
        preferredSlotData.emplace_back(toPreferredSlotMap(slot));
    }

    tibia::game::notify("onBountyPreferredData", preferredSlotData, 0, getAllMonsterRaceIds());
}

void ProtocolGame::parseTaskBoardWeeklyData(const InputMessagePtr& msg)
{
    TaskBoardWeeklyHeaderData headerData;
    std::vector<TaskBoardWeeklyMonsterData> monsters;
    std::vector<TaskBoardWeeklyItemData> items;

    const uint16_t anyCreatureTotalKills = msg->getU16();
    const uint16_t anyCreatureCurrentKills = msg->getU16();

    const uint8_t killTasksCount = msg->getU8();
    monsters.reserve(killTasksCount + ((anyCreatureTotalKills > 0 || anyCreatureCurrentKills > 0) ? 1 : 0));

    if (anyCreatureTotalKills > 0 || anyCreatureCurrentKills > 0) {
        TaskBoardWeeklyMonsterData anyCreatureTask;
        anyCreatureTask.raceId = 0;
        anyCreatureTask.total = anyCreatureTotalKills;
        anyCreatureTask.current = anyCreatureCurrentKills;
        anyCreatureTask.state = (anyCreatureTotalKills > 0 && anyCreatureCurrentKills >= anyCreatureTotalKills) ? 1 : 0;
        monsters.emplace_back(anyCreatureTask);
    }

    for (auto i = 0; std::cmp_less(i, killTasksCount); ++i) {
        TaskBoardWeeklyMonsterData task;
        task.raceId = msg->getU16();
        task.total = msg->getU16();
        task.current = msg->getU16();
        task.state = (task.total > 0 && task.current >= task.total) ? 1 : 0;
        monsters.emplace_back(task);
    }

    const uint8_t deliveryTasksCount = msg->getU8();
    items.reserve(deliveryTasksCount);
    for (auto i = 0; std::cmp_less(i, deliveryTasksCount); ++i) {
        TaskBoardWeeklyItemData task;
        task.slotIndex = msg->getU8();
        task.itemId = msg->getU16();
        msg->getU8(); // unknown1
        msg->getU8(); // unknown2
        task.total = msg->getU32();
        task.current = msg->getU32();
        task.claimed = msg->getU8();
        task.state = (task.claimed != 0 || (task.total > 0 && task.current >= task.total)) ? 1 : 0;
        items.emplace_back(task);
    }

    const uint8_t difficultyMultiplier = msg->getU8();
    headerData.maxExperience = msg->getU32();
    headerData.maxDeliveryExperience = msg->getU32();
    headerData.completedKillTasks = msg->getU8();
    headerData.completedDeliveryTasks = msg->getU8();
    const uint8_t weeklyProgressFinished = msg->getU8();
    const uint8_t unlockedDifficulty = msg->getU8();
    const uint32_t resetTimestamp = msg->getU32();
    const uint8_t weeklyTaskExpansion = msg->getU8();
    headerData.pointsEarned = msg->getU32();
    headerData.soulsealsEarned = msg->getU32();

    const bool hasGeneratedTasks = anyCreatureTotalKills > 0 || anyCreatureCurrentKills > 0 || killTasksCount > 0 || deliveryTasksCount > 0 || weeklyProgressFinished != 0;
    headerData.difficulty = hasGeneratedTasks ? std::clamp<uint8_t>(difficultyMultiplier + 1, 1, 4) : 0;
    headerData.currentPlayerLevel = tibia::game::localPlayerLevel();
    headerData.remainingDays = getRemainingDaysUntil(resetTimestamp);
    headerData.totalTaskSlots = weeklyTaskExpansion != 0 ? TASK_BOARD_WEEKLY_EXPANDED_SLOTS : TASK_BOARD_WEEKLY_BASE_SLOTS;
    headerData.extraSlot = weeklyTaskExpansion != 0 ? 1 : 0;

    auto headerMap = toWeeklyHeaderMap(headerData);
    headerMap["unlockedDifficulty"] = std::clamp<uint8_t>(unlockedDifficulty + 1, 1, 4);
    headerMap["weeklyProgressFinished"] = weeklyProgressFinished;

    std::vector<std::map<std::string, uint32_t>> monsterData;
    monsterData.reserve(monsters.size());
    for (const auto& monster : monsters) {
        monsterData.emplace_back(toWeeklyMonsterMap(monster));
    }

    std::vector<std::map<std::string, uint32_t>> itemData;
    itemData.reserve(items.size());
    for (const auto& item : items) {
        itemData.emplace_back(toWeeklyItemMap(item));
    }

    tibia::game::notify("onWeeklyTaskData", headerMap, monsterData, itemData);
}

void ProtocolGame::parseTaskBoardShopData(const InputMessagePtr& msg)
{
    std::vector<TaskBoardShopItemData> shopItems;
    const uint8_t offersCount = msg->getU8();
    shopItems.reserve(offersCount);

    for (auto i = 0; std::cmp_less(i, offersCount); ++i) {
        TaskBoardShopItemData item;
        item.id = i;

        const auto offerType = static_cast<tibia::client::TaskBoardShopOfferType_t>(msg->getU8());
        item.offerType = static_cast<uint8_t>(offerType);

        if (offerType == tibia::client::TASK_BOARD_SHOP_OFFER_BONUS_PROMOTION) {
            const uint16_t purchasedDisplayValue = msg->getU16();
            item.nextCost = msg->getU32();
            const uint8_t status = msg->getU8();

            item.price = item.nextCost;
            item.currentPurchases = purchasedDisplayValue > 0 ? purchasedDisplayValue - 1 : 0;
            item.bought = (status == tibia::client::TASK_BOARD_SHOP_STATUS_BOUGHT || item.nextCost == 0) ? 1 : 0;
        } else {
            item.title = msg->getString();
            item.description = msg->getString();
            const uint32_t looktypeOrItemId = msg->getU32();

            uint8_t addons = 0;
            if (offerType == tibia::client::TASK_BOARD_SHOP_OFFER_OUTFIT) {
                addons = msg->getU8();
            }

            if (offerType == tibia::client::TASK_BOARD_SHOP_OFFER_ITEM_DOUBLE)
                item.optionalItemId = msg->getU32();

            item.price = msg->getU32();
            const uint8_t status = msg->getU8();

            if (offerType == tibia::client::TASK_BOARD_SHOP_OFFER_OUTFIT) {
                item.lookType = looktypeOrItemId;
                item.lookAddons = addons;
            } else if (offerType == tibia::client::TASK_BOARD_SHOP_OFFER_MOUNT) {
                item.lookType = looktypeOrItemId;
            } else {
                item.itemId = looktypeOrItemId;
            }

            item.bought = status == tibia::client::TASK_BOARD_SHOP_STATUS_BOUGHT ? 1 : 0;
        }

        shopItems.emplace_back(item);
    }

    std::vector<std::map<std::string, std::string>> shopData;
    shopData.reserve(shopItems.size());
    for (const auto& item : shopItems) {
        shopData.emplace_back(toShopItemMap(item));
    }

    tibia::game::notify("onTaskHuntingShopData", shopData);
}

void ProtocolGame::parseTaskHuntingBasicData(const InputMessagePtr& msg)
{
    if (tibia::game::cipFeature(tibia::client::GameTaskboard)) {
        auto soulsealEntries = buildTaskBoardSoulsealEntries(readTaskBoardMasteredRaceIds(msg));
        tibia::game::notify("onSoulsealsData", soulsealEntries);
    } else {
        const uint16_t preys = msg->getU16();
        for (auto i = 0; std::cmp_less(i, preys); ++i) {
            msg->getU16(); // RaceID
            msg->getU8(); // Difficult
        }
        const uint8_t options = msg->getU8();
        for (auto i = 0; std::cmp_less(i, options); ++i) {
            msg->getU8(); // Difficult
            msg->getU8(); // Stars
            msg->getU16(); // First kill
            msg->getU16(); // First reward
            msg->getU16(); // Second kill
            msg->getU16(); // Second reward
        }
    }
}

void ProtocolGame::parseClientCheck(const InputMessagePtr& msg)
{
    // CIP ClientCheck request (S 0x63). Crystal sendClientCheck sends u32 count + bytes.
    const uint32_t size = msg->getU32();
    std::vector<uint8_t> data;
    data.reserve(size);
    for (uint32_t i = 0; i < size; ++i)
        data.push_back(msg->getU8());
    // Acknowledge with the CIP client opcode (C 0x63).
    sendClientCheck(data);
    tibia::game::notify("onClientCheck", data);
}




void ProtocolGame::parseRestingAreaState(const InputMessagePtr& msg)
{
    const uint8_t zone = msg->getU8();
    const uint8_t state = msg->getU8();
    const auto& message = msg->getString();

    tibia::game::notify("onRestingAreaState", zone, state, message);
}

void ProtocolGame::parseCyclopediaCharacterInfo(const InputMessagePtr& msg)
{
    const auto type = static_cast<tibia::client::CyclopediaCharacterInfoType_t>(msg->getU8());

    // 0: Send no error
    // 1: No data available at the moment.
    // 2: You are not allowed to see this character's data.
    // 3: You are not allowed to inspect this character.
    const uint8_t errorCode = msg->getU8();
    if (errorCode > 0) {
        tibia::game::notify("onCyclopediaCharacterInfoError", static_cast<int>(type), static_cast<int>(errorCode));
        return;
    }

    switch (type) {
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_BASEINFORMATION:
        {
            const auto& name = msg->getString();
            const auto& vocation = msg->getString();
            const uint16_t level = msg->getU16();
            const auto& outfit = getOutfit(msg, false);
            const bool ownCharacter = msg->getU8() != 0; // store summary & titles visible
            if (tibia::game::cipFeature(tibia::client::GameTournamentPackets)) {
                msg->getU8(); // ???
            }
            const auto& title = msg->getString();
            tibia::game::notify("onCyclopediaCharacterBaseInformation", name, vocation, level, outfit, ownCharacter, title);
            break;
        }
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_GENERALSTATS:
        {
            CyclopediaCharacterGeneralStats stats;
            stats.experience = msg->getU64();
            stats.level = msg->getU16();
            stats.levelPercent = tibia::game::cipFeature(tibia::client::GameLevelPercentU16) ? msg->getU16() / 100 : msg->getU8();
            stats.baseExpGain = msg->getU16();
            if (tibia::game::cipFeature(tibia::client::GameTournamentPackets)) {
                msg->getU32(); // tournament exp(deprecated)
            }
            stats.lowLevelExpBonus = msg->getU16();
            stats.XpBoostPercent = msg->getU16();
            stats.staminaExpBonus = msg->getU16();
            stats.XpBoostBonusRemainingTime = msg->getU16();
            stats.canBuyXpBoost = msg->getU8();
            stats.health = msg->getU32();
            stats.maxHealth = msg->getU32();
            stats.mana = msg->getU32();
            stats.maxMana = msg->getU32();
            stats.soul = msg->getU8();
            stats.staminaMinutes = msg->getU16();
            stats.regenerationCondition = msg->getU16();
            stats.offlineTrainingTime = msg->getU16();
            stats.speed = msg->getU16();
            stats.baseSpeed = msg->getU16();
            stats.capacity = msg->getU32();
            stats.baseCapacity = msg->getU32();
            stats.freeCapacity = msg->getU32();
            msg->getU8();
            msg->getU8();
            stats.magicLevel = msg->getU16();
            stats.baseMagicLevel = msg->getU16();
            stats.loyaltyMagicLevel = msg->getU16();
            stats.magicLevelPercent = msg->getU16();

            std::vector<std::vector<uint16_t>> skills;

            for (int_fast32_t skill = tibia::client::Fist; skill <= tibia::client::Fishing; ++skill) {
                msg->getU8(); // Hardcoded Skill Ids
                const uint16_t skillLevel = msg->getU16();
                const uint16_t baseSkill = msg->getU16();
                msg->getU16(); // base + loyalty bonus(?)
                const uint16_t skillPercent = msg->getU16() / 100;
                skills.push_back({ skillLevel, baseSkill, skillPercent });
            }

            const uint8_t combatCount = msg->getU8();
            std::vector<std::tuple<uint8_t, uint16_t>> combats;

            for (auto i = 0; i < combatCount; ++i) {
                const uint8_t element = msg->getU8();
                const uint16_t specializedMagicLevel = msg->getU16();
                combats.emplace_back(element, specializedMagicLevel);
            }

            tibia::game::processCyclopediaCharacterGeneralStats(stats, skills, combats);
            break;
        }
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_COMBATSTATS:
        {
            std::vector<std::vector<uint16_t>> additionalSkillsArray;

            if (tibia::game::cipFeature(tibia::client::GameAdditionalSkills)) {
                // Critical, Life Leech, Mana Leech
                for (uint16_t skill = tibia::client::CriticalChance; skill <= tibia::client::ManaLeechAmount; ++skill) {
                    if (!tibia::game::cipFeature(tibia::client::GameLeechAmount)) {
                        if (skill == tibia::client::LifeLeechAmount || skill == tibia::client::ManaLeechAmount) {
                            continue;
                        }
                    }

                    const uint16_t skillLevel = msg->getU16();
                    msg->getU16();
                    additionalSkillsArray.push_back({ skill, skillLevel });
                }
            }

            std::vector<std::vector<uint16_t>> forgeSkillsArray;

            if (tibia::game::cipFeature(tibia::client::GameForgeSkillStats)) {
                // forge skill stats
                const uint8_t lastSkill = tibia::game::clientRevision() >= 1332 ? tibia::client::LastSkill : tibia::client::Momentum + 1;
                for (uint16_t skill = tibia::client::Fatal; skill < lastSkill; ++skill) {
                    const uint16_t skillLevel = msg->getU16();
                    msg->getU16();
                    forgeSkillsArray.push_back({ skill, skillLevel });
                }
            }

            msg->getU16(); // Cleave Percent
            msg->getU16(); // Magic Shield Capacity Flat
            msg->getU16(); // Magic Shield Capacity Percent

            std::vector<uint16_t> perfectShotDamageRangesArray;

            for (auto i = 1; i <= 5; i++) {
                const uint16_t perfectShotDamageRange = msg->getU16();
                perfectShotDamageRangesArray.emplace_back(perfectShotDamageRange);
            }

            msg->getU16(); // Damage reflection

            CyclopediaCharacterCombatStats data;
            data.haveBlessings = msg->getU8();
            msg->getU8(); // total blessings

            data.weaponMaxHitChance = msg->getU16();
            data.weaponElement = msg->getU8();
            data.weaponElementDamage = msg->getU8();
            data.weaponElementType = msg->getU8();
            data.armor = msg->getU16();
            data.defense = msg->getU16();
            const double mitigation = msg->getDouble();

            const uint8_t combatCount = msg->getU8();
            std::vector<std::tuple<uint8_t, uint16_t>> combatsArray;

            for (auto i = 0; i < combatCount; ++i) {
                const uint8_t element = msg->getU8();
                const uint16_t clientModifier = msg->getU16();
                combatsArray.emplace_back(element, clientModifier);
            }

            const uint8_t concoctionsCount = msg->getU8();
            std::vector<std::tuple<uint16_t, uint16_t>> concoctionsArray;

            for (auto i = 0; i < concoctionsCount; ++i) {
                const uint16_t concoctionFirst = msg->getU16();
                const uint16_t concoctionSecond = msg->getU16();
                concoctionsArray.emplace_back(concoctionFirst, concoctionSecond);
            }

            tibia::game::processCyclopediaCharacterCombatStats(data, mitigation, additionalSkillsArray, forgeSkillsArray, perfectShotDamageRangesArray, combatsArray, concoctionsArray);
            break;
        }
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_RECENTDEATHS:
        {
            CyclopediaCharacterRecentDeaths data;
            const uint16_t currentPage = msg->getU16();
            const uint16_t pageCount = msg->getU16();
            tibia::game::notify("onCyclopediaCharacterPagination", static_cast<int>(type), static_cast<int>(currentPage), static_cast<int>(pageCount));

            const uint16_t entriesCount = msg->getU16();
            for (auto i = 0; i < entriesCount; ++i) {
                RecentDeathEntry entry;
                entry.timestamp = msg->getU32();
                entry.cause = msg->getString();
                data.entries.emplace_back(entry);
            }

            tibia::game::processCyclopediaCharacterRecentDeaths(data);
            break;
        }
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_RECENTPVPKILLS:
        {
            CyclopediaCharacterRecentPvPKills data;
            const uint16_t currentPage = msg->getU16();
            const uint16_t pageCount = msg->getU16();
            tibia::game::notify("onCyclopediaCharacterPagination", static_cast<int>(type), static_cast<int>(currentPage), static_cast<int>(pageCount));

            const uint16_t entriesCount = msg->getU16();
            for (auto i = 0; i < entriesCount; ++i) {
                RecentPvPKillEntry entry;
                entry.timestamp = msg->getU32();
                entry.description = msg->getString();
                entry.status = msg->getU8();
                data.entries.emplace_back(entry);
            }

            tibia::game::processCyclopediaCharacterRecentPvpKills(data);
            break;
        }
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_ACHIEVEMENTS:
        {
            const uint16_t points = msg->getU16();
            const uint16_t totalSecrets = msg->getU16();
            const uint16_t count = msg->getU16();
            // id, unlock timestamp, secret, name, description, grade (the last three only for secret ones)
            std::vector<std::tuple<uint16_t, uint32_t, bool, std::string, std::string, uint8_t>> achievements;
            for (auto i = 0; i < count; ++i) {
                const uint16_t id = msg->getU16();
                const uint32_t timestamp = msg->getU32();
                const bool secret = msg->getU8() != 0;
                std::string name;
                std::string description;
                uint8_t grade = 0;
                if (secret) {
                    name = msg->getString();
                    description = msg->getString();
                    grade = msg->getU8();
                }
                achievements.emplace_back(id, timestamp, secret, name, description, grade);
            }
            tibia::game::notify("onCyclopediaCharacterAchievements", static_cast<int>(points), static_cast<int>(totalSecrets), achievements);
            break;
        }
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_ITEMSUMMARY:
        {
            CyclopediaCharacterItemSummary data;

            const uint16_t inventoryItemsCount = msg->getU16();
            for (auto i = 0; i < inventoryItemsCount; ++i) {
                ItemSummary item;
                const uint16_t itemId = msg->getU16();
                const auto& thingType = tibia::game::thingType(itemId, ThingCategoryItem);
                const uint16_t classification = thingType ? thingType->getClassification() : 0;

                uint8_t itemTier = 0;
                if (classification > 0) {
                    itemTier = msg->getU8();
                }

                item.itemId = itemId;
                item.tier = itemTier;
                item.amount = msg->getU32();
                data.inventory.emplace_back(item);
            }

            const uint16_t storeItemsCount = msg->getU16();
            for (auto i = 0; i < storeItemsCount; ++i) {
                ItemSummary item;
                const uint16_t itemId = msg->getU16();
                const auto& thingType = tibia::game::thingType(itemId, ThingCategoryItem);
                const uint16_t classification = thingType ? thingType->getClassification() : 0;

                uint8_t itemTier = 0;
                if (classification > 0) {
                    itemTier = msg->getU8();
                }

                item.itemId = itemId;
                item.tier = itemTier;
                item.amount = msg->getU32();
                data.store.emplace_back(item);
            }

            const uint16_t stashItemsCount = msg->getU16();
            for (auto i = 0; i < stashItemsCount; ++i) {
                ItemSummary item;
                const uint16_t itemId = msg->getU16();
                const auto& thing = tibia::game::thingType(itemId, ThingCategoryItem);
                if (!thing) {
                    continue;
                }
                const uint16_t classification = thing->getClassification();

                uint8_t itemTier = 0;
                if (classification > 0) {
                    itemTier = msg->getU8();
                }

                item.itemId = itemId;
                item.tier = itemTier;
                item.amount = msg->getU32();
                data.stash.emplace_back(item);
            }

            const uint16_t depotItemsCount = msg->getU16();
            for (auto i = 0; i < depotItemsCount; ++i) {
                ItemSummary item;
                const uint16_t itemId = msg->getU16();
                const auto& thingType = tibia::game::thingType(itemId, ThingCategoryItem);
                const uint16_t classification = thingType ? thingType->getClassification() : 0;

                uint8_t itemTier = 0;
                if (classification > 0) {
                    itemTier = msg->getU8();
                }

                item.itemId = itemId;
                item.tier = itemTier;
                item.amount = msg->getU32();
                data.depot.emplace_back(item);
            }

            const uint16_t inboxItemsCount = msg->getU16();
            for (auto i = 0; i < inboxItemsCount; ++i) {
                ItemSummary item;
                const uint16_t itemId = msg->getU16();
                const auto& thingType = tibia::game::thingType(itemId, ThingCategoryItem);
                const uint16_t classification = thingType ? thingType->getClassification() : 0;

                uint8_t itemTier = 0;
                if (classification > 0) {
                    itemTier = msg->getU8();
                }

                item.itemId = itemId;
                item.tier = itemTier;
                item.amount = msg->getU32();
                data.inbox.emplace_back(item);
            }

            tibia::game::processCyclopediaCharacterItemSummary(data);
            break;
        }
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_OUTFITSMOUNTS:
        {
            const uint16_t outfitsSize = msg->getU16();
            std::vector<CharacterInfoOutfits> outfits;

            for (auto i = 0; i < outfitsSize; ++i) {
                CharacterInfoOutfits outfit;
                outfit.lookType = msg->getU16();
                outfit.name = msg->getString();
                outfit.addons = msg->getU8();
                outfit.type = msg->getU8(); // store / quest / none
                outfit.isCurrent = msg->getU32(); // 1000 = true / 0 = false
                outfits.emplace_back(outfit);
            }

            OutfitColorStruct currentOutfit{};
            if (outfitsSize > 0) {
                currentOutfit.lookHead = msg->getU8();
                currentOutfit.lookBody = msg->getU8();
                currentOutfit.lookLegs = msg->getU8();
                currentOutfit.lookFeet = msg->getU8();
            }

            const uint16_t mountsSize = msg->getU16();
            std::vector<CharacterInfoMounts> mounts;

            for (auto i = 0; i < mountsSize; ++i) {
                CharacterInfoMounts mount;
                mount.mountId = msg->getU16();
                mount.name = msg->getString();
                mount.type = msg->getU8(); // store / quest / none
                mount.isCurrent = msg->getU32(); // 1000 = true / 0 = false
                mounts.emplace_back(mount);
            }

            if (mountsSize > 0) {
                currentOutfit.lookMountHead = msg->getU8();
                currentOutfit.lookMountBody = msg->getU8();
                currentOutfit.lookMountLegs = msg->getU8();
                currentOutfit.lookMountFeet = msg->getU8();
            }

            const uint16_t familiarsSize = msg->getU16();
            std::vector<CharacterInfoFamiliar> familiars;

            for (auto i = 0; i < familiarsSize; ++i) {
                CharacterInfoFamiliar familiar;
                familiar.lookType = msg->getU16();
                familiar.name = msg->getString();
                familiar.type = msg->getU8(); // quest / none
                familiar.isCurrent = msg->getU32(); // 1000 = true / 0 = false
                familiars.emplace_back(familiar);
            }

            tibia::game::processCyclopediaCharacterAppearances(currentOutfit, outfits, mounts, familiars);
            break;
        }
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_STORESUMMARY:
        {
            const uint32_t xpBoostTime = msg->getU32();
            const uint32_t dailyRewardXpBoostTime = msg->getU32();

            std::vector<std::tuple<std::string, uint8_t>> blessings;
            const uint8_t blessingCount = msg->getU8();

            for (auto i = 0; i < blessingCount; ++i) {
                const auto& blessingName = msg->getString();
                const uint8_t blessingObtained = msg->getU8();
                blessings.emplace_back(blessingName, blessingObtained);
            }

            const uint8_t preySlotsUnlocked = msg->getU8();
            const uint8_t preyWildcards = msg->getU8();
            bool hasPermanentWeeklyTaskExpansion = false;
            if (tibia::game::cipFeature(tibia::client::GameTaskboard)) {
                hasPermanentWeeklyTaskExpansion = static_cast<bool>(msg->getU8());
            }
            const uint8_t instantRewards = msg->getU8();
            const bool hasCharmExpansion = static_cast<bool>(msg->getU8());
            const uint8_t hirelingsObtained = msg->getU8();

            std::vector<uint16_t> hirelingSkills;
            const uint8_t hirelingSkillsCount = msg->getU8();

            for (auto i = 0; i < hirelingSkillsCount; ++i) {
                const uint8_t skill = msg->getU8();
                hirelingSkills.emplace_back(static_cast<uint16_t>(skill + 1000));
            }

            const uint8_t hirelingOutfitsCount = msg->getU8();
            for (auto i = 0; std::cmp_less(i, hirelingOutfitsCount); ++i) {
                msg->getU8(); // outfit ID
            }

            std::vector<std::tuple<uint16_t, std::string, uint8_t>> houseItems;
            const uint16_t houseItemsCount = msg->getU16();

            for (auto i = 0; i < houseItemsCount; ++i) {
                const uint16_t itemId = msg->getU16();
                const auto& itemName = msg->getString();
                const uint8_t count = msg->getU8();
                houseItems.emplace_back(itemId, itemName, count);
            }

            tibia::game::notify("onParseCyclopediaStoreSummary", xpBoostTime, dailyRewardXpBoostTime, blessings, preySlotsUnlocked, preyWildcards, hasPermanentWeeklyTaskExpansion, instantRewards,hasCharmExpansion, hirelingsObtained, hirelingSkills, houseItems);
            break;
        }
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_INSPECTION:
        {
            const uint8_t inventoryCount = msg->getU8();
            auto data = readCyclopediaCharacterInspectionData(*this, msg, inventoryCount, 0);
            tibia::game::processCyclopediaCharacterInspection(data);
            break;
        }
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_BADGES:
        {
            const uint8_t showAccountInformation = msg->getU8();
            const uint8_t playerOnline = msg->getU8();
            const uint8_t playerPremium = msg->getU8();
            const auto& loyaltyTitle = msg->getString();

            const uint8_t badgesSize = msg->getU8();
            std::vector<std::tuple<uint32_t, std::string>> badgesVector;

            for (auto i = 0; i < badgesSize; ++i) {
                const uint32_t badgeId = msg->getU32();
                const auto& badgeName = msg->getString();
                badgesVector.emplace_back(badgeId, badgeName);
            }

            tibia::game::processCyclopediaCharacterGeneralStatsBadge(showAccountInformation, playerOnline, playerPremium, loyaltyTitle, badgesVector);
            break;
        }
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_TITLES:
        {
            const uint8_t currentTitle = msg->getU8();
            const uint8_t titlesSize = msg->getU8();
            // id, name, description, permanent, unlocked
            std::vector<std::tuple<uint8_t, std::string, std::string, bool, bool>> titles;
            for (auto i = 0; i < titlesSize; ++i) {
                const uint8_t id = msg->getU8();
                const auto& name = msg->getString();
                const auto& description = msg->getString();
                const bool permanent = msg->getU8() != 0;
                const bool unlocked = msg->getU8() != 0;
                titles.emplace_back(id, name, description, permanent, unlocked);
            }
            tibia::game::notify("onCyclopediaCharacterTitles", static_cast<int>(currentTitle), titles);
            break;
        }
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_WHEEL:
        {
            break;
        }
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_OFFENCESTATS:
        {
            CyclopediaCharacterOffenceStats data;

            data.critChanceTotal = msg->getDouble();
            data.critChanceEquipament = msg->getDouble();
            if (tibia::game::clientRevision() >= 1510) {
                data.critChanceFlat = msg->getDouble();
            }
            data.critChanceImbuement = msg->getDouble();
            data.critChanceWheel = msg->getDouble();
            data.critChanceConcoction = msg->getDouble();

            // Critical hit damage
            data.critDamageTotal = msg->getDouble();
            data.critDamageEquipament = msg->getDouble();
            if (tibia::game::clientRevision() >= 1510) {
                data.critDamageFlat = msg->getDouble();
            }
            data.critDamageImbuement = msg->getDouble();
            data.critDamageWheel = msg->getDouble();
            data.critDamageConcoction = msg->getDouble();

            // Life leech amount
            data.lifeLeechTotal = msg->getDouble();
            data.lifeLeechEquipament = msg->getDouble();
            data.lifeLeechImbuement = msg->getDouble();
            data.lifeLeechWheel = msg->getDouble();
            data.lifeLeechEventBonus = msg->getDouble();

            // Mana leech amount
            data.manaLeechTotal = msg->getDouble();
            data.manaLeechEquipament = msg->getDouble();
            data.manaLeechImbuement = msg->getDouble();
            data.manaLeechWheel = msg->getDouble();
            data.manaLeechEventBonus = msg->getDouble();

            // Onslaught
            data.onslaught = msg->getDouble();
            data.onslaughtBase = msg->getDouble();
            data.onslaughtBonus = msg->getDouble();
            data.onslaughtEventBonus = msg->getDouble();

            data.cleavePercent = msg->getDouble();

            // Perfect shot range
            auto limitRange = (tibia::game::clientRevision() >= 1510) ? 7 : 5;
            for (int i = 0; i < limitRange; i++) {
                data.perfectShotDamage.push_back(msg->getU16());
            }

            data.flatDamage = msg->getU16();
            data.flatDamageBase = msg->getU16();
            data.flatDamageWheel = msg->getU16();

            data.weaponAttack = msg->getU16();
            data.weaponFlatModifier = msg->getU16();
            data.weaponDamage = msg->getU16();
            data.weaponSkillType = msg->getU8();
            data.weaponSkillLevel = msg->getU16();
            data.weaponSkillModifier = msg->getU16();
            data.weaponElement = msg->getU8();
            // damage conversion
            data.weaponElementDamage = msg->getDouble(); // % amount
            data.weaponElementType = msg->getU8(); // new element

            const uint8_t accuracyCount = msg->getU8(); // distance fighting accuracy
            for (auto i = 0; std::cmp_less(i, accuracyCount); ++i) {
                CyclopediaCharacterOffenceStats::AccuracyData acc;
                acc.range = msg->getU8(); // range
                acc.chance = msg->getDouble(); // change to hit (placeholder)
                data.weaponAccuracy.push_back(acc);
            }

            if (tibia::game::clientRevision() >= 1510) {
                data.damagePowerfulFoes = msg->getDouble(); // damage against powerful foes
                const uint16_t targetCount = msg->getU16(); // damage against specific targets FOR
                for (auto i = 0; std::cmp_less(i, targetCount); ++i) {
                    CyclopediaCharacterOffenceStats::TargetBonus bonus;
                    bonus.name = msg->getString(); // placeholder
                    bonus.value = msg->getDouble(); // 1.25
                    data.damageSpecificTargets.push_back(bonus);
                }
                const uint8_t elementCount = msg->getU8(); // critical chance by type
                for (auto i = 0; std::cmp_less(i, elementCount); ++i) {
                    CyclopediaCharacterOffenceStats::ElementModifier mod;
                    mod.element = msg->getU8(); // element id
                    mod.value = msg->getDouble(); // modifier
                    data.damageElements.push_back(mod);
                }
                data.offensiveRuneDamage = msg->getDouble(); // +x% for offensive runes
                data.autoAttackDamage = msg->getDouble(); // +x% for auto-attack
                const uint8_t critDmgElemCount = msg->getU8(); // critical damage by type
                for (auto i = 0; std::cmp_less(i, critDmgElemCount); ++i) {
                    CyclopediaCharacterOffenceStats::ElementModifier mod;
                    mod.element = msg->getU8(); // element id
                    mod.value = msg->getDouble(); // modifier
                    data.critDamageElements.push_back(mod);
                }
                data.critDamageOffensiveRunes = msg->getDouble(); // crit dmg: +x% for offensive runes
                data.critDamageAutoAttack = msg->getDouble(); // crit dmg: +x% for auto-attack

                data.lifeGainHit = msg->getU16(); // life gain on hit
                data.manaGainHit = msg->getU16(); // mana gain on hit
                data.lifeGainKill = msg->getU16(); // life gain on kill
                data.manaGainKill = msg->getU16(); // mana gain on kill

                const uint8_t adExtraDmgCount = msg->getU8();
                for (auto i = 0; std::cmp_less(i, adExtraDmgCount); ++i) {
                    CyclopediaCharacterOffenceStats::SkillBonus bonus;
                    bonus.skillId = msg->getU8(); // skill id, uses same enums as in HardcodedSkillIds
                    bonus.valueA = msg->getDouble(); // value a
                    bonus.valueB = msg->getDouble(); // value b
                    data.extraDamageSkills.push_back(bonus);
                }
                const uint8_t spellExtraCount = msg->getU8();
                for (auto i = 0; std::cmp_less(i, spellExtraCount); ++i) {
                    CyclopediaCharacterOffenceStats::SkillBonus bonus;
                    bonus.skillId = msg->getU8(); // skill id, uses same enums as in HardcodedSkillIds
                    bonus.valueA = msg->getDouble(); // value a
                    bonus.valueB = msg->getDouble(); // value b
                    data.extraDamageSpells.push_back(bonus);
                }
                const uint8_t spellExtraHealingCount = msg->getU8();
                for (auto i = 0; std::cmp_less(i, spellExtraHealingCount); ++i) {
                    CyclopediaCharacterOffenceStats::SkillBonus bonus;
                    bonus.skillId = msg->getU8(); // skill id, uses same enums as in HardcodedSkillIds
                    bonus.valueA = msg->getDouble(); // value a
                    bonus.valueB = msg->getDouble(); // value b
                    data.extraHealingSpells.push_back(bonus);
                }
            }
            if (tibia::game::clientRevision() >= 1521) {
                data.damageHighHp = msg->getDouble(); // damage to targets above 95% hp
                data.damageLowHp = msg->getDouble(); // damage to targets below 30% hp
                data.armorPenetration = msg->getDouble(); // armor penetration multiplier
                const uint8_t elemPierceBonuses = msg->getU8(); // elemental pierce
                for (auto i = 0; std::cmp_less(i, elemPierceBonuses); ++i) {
                    CyclopediaCharacterOffenceStats::ElementModifier mod;
                    mod.element = msg->getU8(); // element id
                    mod.value = msg->getDouble(); // modifier
                    data.elementalPierce.push_back(mod);
                }
            }

            tibia::game::processCyclopediaCharacterOffenceStats(data);
            break;
        }
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_DEFENCESTATS:
        {
            CyclopediaCharacterDefenceStats data;

            data.dodgeTotal = msg->getDouble();
            data.dodgeBase = msg->getDouble();
            data.dodgeBonus = msg->getDouble();
            msg->getDouble(); // unused
            data.dodgeWheel = msg->getDouble();

            data.magicShieldCapacity = msg->getU32();
            data.magicShieldCapacityFlat = msg->getU16();
            data.magicShieldCapacityPercent = msg->getDouble();

            data.reflectPhysical = msg->getU16();
            data.armor = msg->getU16();
            if (tibia::game::cipFeature(tibia::client::GameVocationMonk)) {
                msg->getU16(); // MANTRA
            }

            data.defense = msg->getU16();
            data.defenseEquipment = msg->getU16();
            data.defenseSkillType = msg->getU8();
            data.shieldingSkill = msg->getU16();
            data.defenseWheel = msg->getU16();
            if (tibia::game::clientRevision() < 1530)
                msg->getU16(); // unused (removed in 15.25)

            data.mitigation = msg->getDouble();
            data.mitigationBase = msg->getDouble();
            data.mitigationEquipment = msg->getDouble();
            data.mitigationShield = msg->getDouble();
            data.mitigationWheel = msg->getDouble();
            data.mitigationCombatTactics = (tibia::game::clientRevision() < 1530) ? msg->getDouble() : 0.0;
            const uint8_t combatsCount = msg->getU8();
            for (int i = 0; i < combatsCount; ++i) {
                uint8_t elementType = msg->getU8();
                if (elementType == 0x04) {
                    CyclopediaCharacterDefenceStats::ElementalResistance resistance;
                    resistance.element = msg->getU8();
                    resistance.value = msg->getDouble();
                    data.resistances.push_back(resistance);
                }
            }

            tibia::game::processCyclopediaCharacterDefenceStats(data);
            break;
        }
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_MISCSTATS:
        {
            CyclopediaCharacterMiscStats data;

            data.momentumTotal = msg->getDouble();
            data.momentumBase = msg->getDouble();
            data.momentumBonus = msg->getDouble();
            data.momentumWheel = msg->getDouble();
            msg->getDouble(); // unused

            data.dodgeTotal = msg->getDouble();
            data.dodgeBase = msg->getDouble();
            data.dodgeBonus = msg->getDouble();
            data.dodgeWheel = msg->getDouble();

            data.damageReflectionTotal = msg->getDouble();
            data.damageReflectionBase = msg->getDouble();
            data.damageReflectionBonus = msg->getDouble();

            data.haveBlesses = msg->getU8();
            data.totalBlesses = msg->getU8();

            const uint8_t concoctionsCount = msg->getU8();
            for (int i = 0; i < concoctionsCount; ++i) {
                CyclopediaCharacterMiscStats::Concoction concoction;
                concoction.id = msg->getU16();
                msg->getU8(); // unused
                msg->getU8(); // unused
                concoction.duration = msg->getU32();
                data.concoctions.push_back(concoction);
            }

            const uint8_t activeFoodsCount = msg->getU8();
            for (auto j = 0; std::cmp_less(j, activeFoodsCount); ++j) {
                CyclopediaCharacterMiscStats::Food food;
                food.id = msg->getU16();
                msg->getU8(); // unused
                msg->getU8(); // unused
                food.duration = msg->getU32();
                data.activeFoods.push_back(food);
            }

            const uint8_t weaponProficiencyAugmentsCount = msg->getU8();
            for (auto j = 0; std::cmp_less(j, weaponProficiencyAugmentsCount); ++j) {
                CyclopediaCharacterMiscStats::Augment augment;
                augment.spellId = msg->getU16();
                augment.type = msg->getU8();
                augment.value = msg->getDouble();
                data.weaponProficiencyAugments.push_back(augment);
            }

            const uint8_t wheelAugmentsCount = msg->getU8();
            for (auto j = 0; std::cmp_less(j, wheelAugmentsCount); ++j) {
                CyclopediaCharacterMiscStats::Augment augment;
                augment.spellId = msg->getU16();
                augment.type = msg->getU8();
                augment.value = msg->getDouble();
                data.wheelAugments.push_back(augment);
            }

            const uint8_t equippedAugmentsCount = msg->getU8();
            for (auto j = 0; std::cmp_less(j, equippedAugmentsCount); ++j) {
                CyclopediaCharacterMiscStats::Augment augment;
                augment.spellId = msg->getU16();
                augment.type = msg->getU8();
                augment.value = msg->getDouble();
                data.equippedAugments.push_back(augment);
            }
            tibia::game::processCyclopediaCharacterMiscStats(data);
            break;
        }
    }
}


namespace {
    DailyRewardDay parseRewardDay(const InputMessagePtr& msg)
    {
        DailyRewardDay day;
        day.redeemMode = msg->getU8(); // reward type
        day.itemsToSelect = 0; // reward type
        if (day.redeemMode == 1) {
            // select x items from the list
            day.itemsToSelect = msg->getU8(); // reward type
            const uint8_t itemListSize = msg->getU8();
            for (auto listIndex = 0; listIndex < itemListSize; ++listIndex) {
                DailyRewardItem item;
                item.itemId = msg->getU16(); // Item ID
                item.name = msg->getString(); // Item name
                item.weight = msg->getU32(); // Item weight
                day.selectableItems.emplace_back(item);
            }
        } else if (day.redeemMode == 2) {
            // no choice, click to redeem all
            const uint8_t itemListSize = msg->getU8();
            for (auto listIndex = 0; listIndex < itemListSize; ++listIndex) {
                const uint8_t bundleType = msg->getU8(); // type of reward
                DailyRewardBundle bundle;
                bundle.bundleType = bundleType;

                switch (bundleType) {
                    case 1: {
                        // Items
                        bundle.itemId = msg->getU16(); // Item ID
                        bundle.name = msg->getString(); // Item name
                        bundle.count = msg->getU8(); // Item Count
                        break;
                    }
                    case 2: {
                        // Prey Wildcards
                        bundle.itemId = 0;
                        bundle.name = "Prey Wildcards";
                        bundle.count = msg->getU8(); // Prey Wildcards Count
                        break;
                    }
                    case 3: {
                        // XP Boost
                        bundle.itemId = msg->getU16(); // XP Boost Minutes
                        bundle.name = "XP Boost";
                        bundle.count = 0;
                        break;
                    }
                    default:
                        // Invalid type
                        break;
                }
                day.bundleItems.emplace_back(bundle);
            }
        }

        return day;
    }
}

void ProtocolGame::parseDailyReward(const InputMessagePtr& msg)
{
    DailyRewardData data;
    data.days = msg->getU8(); // Reward count (7 days)

    for (auto i = 1; i <= data.days; ++i) {
        data.freeRewards.push_back(parseRewardDay(msg)); // Free account
        data.premiumRewards.push_back(parseRewardDay(msg)); // Premium account
    }

    const uint8_t bonusCount = msg->getU8();
    for (auto i = 0; i < bonusCount; ++i) {
        DailyRewardBonus bonus;
        bonus.name = msg->getString(); // Bonus name
        bonus.id = msg->getU8(); // Bonus ID
        data.bonuses.push_back(bonus);
    }

    data.maxUnlockableDragons = msg->getU8(); // max unlockable "dragons" for free accounts

    tibia::game::notify("onDailyReward", data);
}




static std::string friendJsonEscape(const std::string& in)
{
    std::string out;
    out.reserve(in.size());
    for (const unsigned char c : in) {
        if (c == '"' || c == '\\')
            out.push_back('\\');
        if (c < 0x20)
            continue;
        out.push_back(static_cast<char>(c));
    }
    return out;
}

static std::string friendOutfitJson(const InputMessagePtr& msg)
{
    const uint16_t type = msg->getU16();
    uint8_t head = 0, body = 0, legs = 0, feet = 0, addons = 0;
    if (type != 0) {
        head = msg->getU8();
        body = msg->getU8();
        legs = msg->getU8();
        feet = msg->getU8();
        addons = msg->getU8();
    } else {
        msg->getU16();
    }
    return "{\"type\":" + std::to_string(type)
        + ",\"head\":" + std::to_string(head)
        + ",\"body\":" + std::to_string(body)
        + ",\"legs\":" + std::to_string(legs)
        + ",\"feet\":" + std::to_string(feet)
        + ",\"addons\":" + std::to_string(addons) + "}";
}

static std::string friendGroupName(const uint8_t group)
{
    if (group == 3)
        return "close";
    if (group == 1)
        return "contacts";
    return "friends";
}

void ProtocolGame::parseFriendSystemData(const InputMessagePtr& msg)
{
    // S 0x74 types from the CIP enum at . List bodies are the
    // product row layout Crystal writes (guid, name, group, outfit).
    const uint8_t dataType = msg->getU8();
    switch (dataType) {
        case 0: { // ERROR
            std::string error;
            if (msg->getUnreadSize() >= 2)
                error = msg->getString();
            while (msg->getUnreadSize() > 0)
                msg->getU8();
            if (error.empty())
                tibia::game::notify("onFriendSystemCannotUse");
            else
                tibia::game::notify("onFriendSystemError", error);
            break;
        }
        case 1: // FRIENDINVITE_NOTICE
            while (msg->getUnreadSize() > 0)
                msg->getU8();
            tibia::game::notify("onFriendSystemInviteNotice");
            break;
        case 7: // FRIENDSHIP_NOTICE
            while (msg->getUnreadSize() > 0)
                msg->getU8();
            tibia::game::notify("onFriendSystemFriendshipNotice");
            break;
        case 2: { // FRIENDLIST
            std::string json = "{\"type\":\"friends\",\"friends\":[";
            const uint16_t count = msg->getU16();
            for (uint16_t i = 0; i < count; ++i) {
                if (i)
                    json += ',';
                const uint32_t guid = msg->getU32();
                const auto name = friendJsonEscape(msg->getString());
                const std::string group = friendGroupName(msg->getU8());
                const uint16_t level = msg->getU16();
                const auto vocation = friendJsonEscape(msg->getString());
                const bool online = msg->getU8() != 0;
                json += "{\"guid\":" + std::to_string(guid)
                    + ",\"name\":\"" + name + "\",\"group\":\"" + group
                    + "\",\"level\":" + std::to_string(level)
                    + ",\"vocation\":\"" + vocation
                    + "\",\"status\":\"" + (online ? "online" : "offline")
                    + "\",\"outfit\":" + friendOutfitJson(msg) + "}";
            }
            json += "]}";
            tibia::game::notify("onFriendSystemJson", json);
            break;
        }
        case 3: { // INVITELIST
            auto readInvites = [&](std::string& json) {
                const uint16_t count = msg->getU16();
                for (uint16_t i = 0; i < count; ++i) {
                    if (i)
                        json += ',';
                    const uint32_t guid = msg->getU32();
                    const auto name = friendJsonEscape(msg->getString());
                    const uint32_t created = msg->getU32();
                    json += "{\"guid\":" + std::to_string(guid) + ",\"name\":\"" + name
                        + "\",\"created\":" + std::to_string(created) + "}";
                }
            };
            std::string json = "{\"type\":\"invitations\",\"received\":[";
            readInvites(json);
            json += "],\"sent\":[";
            readInvites(json);
            json += "]}";
            tibia::game::notify("onFriendSystemJson", json);
            break;
        }
        case 4: { // BLACKLIST
            std::string json = "{\"type\":\"blacklist\",\"entries\":[";
            const uint16_t count = msg->getU16();
            for (uint16_t i = 0; i < count; ++i) {
                if (i)
                    json += ',';
                const uint32_t guid = msg->getU32();
                const auto name = friendJsonEscape(msg->getString());
                const uint32_t created = msg->getU32();
                json += "{\"guid\":" + std::to_string(guid) + ",\"name\":\"" + name
                    + "\",\"created\":" + std::to_string(created) + "}";
            }
            json += "]}";
            tibia::game::notify("onFriendSystemJson", json);
            break;
        }
        case 5: { // ACCOUNTSEARCH
            const uint32_t accountId = msg->getU32();
            const bool online = msg->getU8() != 0;
            const bool premium = msg->getU8() != 0;
            const uint32_t lastLogin = msg->getU32();
            std::string json = "{\"type\":\"searchResult\",\"account\":{\"accountId\":"
                + std::to_string(accountId)
                + ",\"online\":" + (online ? "true" : "false")
                + ",\"premium\":" + (premium ? "true" : "false")
                + ",\"lastLogin\":" + std::to_string(lastLogin)
                + ",\"characters\":[";
            const uint16_t count = msg->getU16();
            for (uint16_t i = 0; i < count; ++i) {
                if (i)
                    json += ',';
                const uint32_t guid = msg->getU32();
                const uint32_t creatureId = msg->getU32();
                const auto name = friendJsonEscape(msg->getString());
                const uint16_t level = msg->getU16();
                const auto vocation = friendJsonEscape(msg->getString());
                const auto world = friendJsonEscape(msg->getString());
                const bool charOnline = msg->getU8() != 0;
                json += "{\"guid\":" + std::to_string(guid)
                    + ",\"creatureId\":" + std::to_string(creatureId)
                    + ",\"name\":\"" + name + "\",\"level\":" + std::to_string(level)
                    + ",\"vocation\":\"" + vocation + "\",\"world\":\"" + world
                    + "\",\"online\":" + (charOnline ? "true" : "false")
                    + ",\"outfit\":" + friendOutfitJson(msg) + "}";
            }
            json += "]}}";
            tibia::game::notify("onFriendSystemJson", json);
            break;
        }
        case 6: { // BADGELIST
            std::string json = "{\"type\":\"badges\",\"badges\":[";
            const uint16_t count = msg->getU16();
            for (uint16_t i = 0; i < count; ++i) {
                if (i)
                    json += ',';
                const uint32_t id = msg->getU32();
                const auto name = friendJsonEscape(msg->getString());
                const auto description = friendJsonEscape(msg->getString());
                const auto icon = friendJsonEscape(msg->getString());
                const bool unlocked = msg->getU8() != 0;
                const bool visible = msg->getU8() != 0;
                json += "{\"id\":" + std::to_string(id) + ",\"name\":\"" + name
                    + "\",\"description\":\"" + description + "\",\"icon\":\"" + icon
                    + "\",\"unlocked\":" + (unlocked ? "true" : "false")
                    + ",\"visible\":" + (visible ? "true" : "false") + "}";
            }
            json += "]}";
            tibia::game::notify("onFriendSystemJson", json);
            break;
        }
        case 8: { // SETTINGS
            std::string json = "{\"type\":\"settings\",\"settings\":[";
            const uint8_t count = msg->getU8();
            for (uint8_t i = 0; i < count; ++i) {
                if (i)
                    json += ',';
                const uint8_t settingType = msg->getU8();
                const bool showCharacter = msg->getU8() != 0;
                const bool showAccount = msg->getU8() != 0;
                const bool allowInspect = msg->getU8() != 0;
                const bool enableCharacter = msg->getU8() != 0;
                const bool enableAccount = msg->getU8() != 0;
                const bool enableInspect = msg->getU8() != 0;
                json += "{\"settingType\":" + std::to_string(settingType)
                    + ",\"showCharacterInfo\":" + (showCharacter ? "true" : "false")
                    + ",\"showAccountInfo\":" + (showAccount ? "true" : "false")
                    + ",\"allowInspect\":" + (allowInspect ? "true" : "false")
                    + ",\"enableShowCharacterInfo\":" + (enableCharacter ? "true" : "false")
                    + ",\"enableShowAccountInfo\":" + (enableAccount ? "true" : "false")
                    + ",\"enableAllowInspect\":" + (enableInspect ? "true" : "false") + "}";
            }
            json += "]}";
            tibia::game::notify("onFriendSystemJson", json);
            break;
        }
        default:
            while (msg->getUnreadSize() > 0)
                msg->getU8();
            tibia::game::notify("onFriendSystemData", dataType);
            break;
    }
}

// 0x5F - parse destiny wheel window
