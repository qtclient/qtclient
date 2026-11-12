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

#include <tibia/creatures/creaturespeedformula.h>
#include <tibia/game/gamesession.h>
#include <tibia/appearances/thing.h>
#include <tibia/creatures/attachedeffect.h>
#include <tibia/creatures/attachedeffectmanager.h>
#include <tibia/creatures/paperdoll.h>
#include <tibia/creatures/paperdollmanager.h>
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


tibia::appearances::TAppearanceInstancePtr makeServerEffectAppearance(const uint16_t effectId)
{
    const uint16_t drawId = tibia::game::resolveEffectId(effectId);
    if (!tibia::game::isValidDatId(drawId, ThingCategoryEffect)) {
        g_logger.traceError("invalid effect id {}", effectId);
        return nullptr;
    }
    ThingType* type = tibia::game::rawThingType(drawId, ThingCategoryEffect);
    const auto appearance = tibia::appearances::TAppearanceInstanceFactory::fromThingType(type);
    auto* effect = dynamic_cast<tibia::appearances::TEffectInstance*>(appearance.get());
    if (!effect)
        return nullptr;
    if (const auto* variant = tibia::game::findEffectVariant(effectId)) {
        effect->setUnderCreatures(variant->underCreatures);
        effect->setVariantId(effectId);
    }
    return appearance;
}

struct MappedThingPeek {
    bool byId = false;
    Position pos;
    int stack = -1;
    uint32_t creatureId = 0;
};

MappedThingPeek peekMappedThingHeader(const InputMessagePtr& msg)
{
    MappedThingPeek peek;
    const auto mark = msg->getReadPos();
    const uint16_t x = msg->getU16();
    if (x != 0xffff) {
        peek.pos = Position(x, msg->getU16(), msg->getU8());
        peek.stack = msg->getU8();
    } else {
        peek.byId = true;
        peek.creatureId = msg->getU32();
    }
    msg->setReadPos(mark);
    return peek;
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

        const int readPos = msg->getReadPos();
        if (callLuaField<bool>("onOpcode", op, msg))
            return;
        msg->setReadPos(readPos);

        if (dispatchServerMessage(op, msg))
            return;

        // Product / Mehah opcodes (not CIP protobuf). Keep off the CIP queue path
        // via ExtendedOpcode when live; bridge may still see them.
        switch (op) {
            case Proto::GameServerCharacterSwitch:
                parseCharacterSwitch(msg);
                break;
            case Proto::GameServerUpdateNeeded:
                parseUpdateNeeded(msg);
                break;
            case Proto::GameServerExtendedOpcode:
                parseExtendedOpcode(msg);
                break;
            case Proto::GameServerChangeMapAwareRange:
                parseChangeMapAwareRange(msg);
                break;
            case Proto::GameServerAttchedEffect:
                parseAttachedEffect(msg);
                break;
            case Proto::GameServerDetachEffect:
                parseDetachEffect(msg);
                break;
            case Proto::GameServerCreatureShader:
                parseCreatureShader(msg);
                break;
            case Proto::GameServerMapShader:
                parseMapShader(msg);
                break;
            case Proto::GameServerCreatureTyping:
                parseCreatureTyping(msg);
                break;
            case Proto::GameServerAttachedPaperdoll:
                parseAttachedPaperdoll(msg);
                break;
            case Proto::GameServerDetachPaperdoll:
                parseDetachPaperdoll(msg);
                break;
            case Proto::GameServerFeatures:
                parseFeatures(msg);
                break;
            case Proto::GameServerFloorDescription:
                parseFloorDescription(msg);
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

void ProtocolGame::parseLogin(const InputMessagePtr& msg) const
{
    const uint32_t playerId = msg->getU32();
    const uint16_t serverBeat = msg->getU16();

    if (tibia::game::cipFeature(tibia::client::GameNewSpeedLaw)) {
        tibia::creatures::setWireSpeedFormula(msg->getDouble(), msg->getDouble(), msg->getDouble());
    }

    bool canReportBugs = false;
    if (!tibia::game::cipFeature(tibia::client::GameDynamicBugReporter)) {
        canReportBugs = msg->getU8() > 0;
    }

    if (tibia::game::clientRevision() >= 1054) {
        msg->getU8(); // can change pvp frame option
    }

    if (tibia::game::clientRevision() >= 1058) {
        const uint8_t expertModeEnabled = msg->getU8();
        tibia::game::setExpertPvpMode(expertModeEnabled);
    }

    if (tibia::game::cipFeature(tibia::client::GameIngameStore)) {
        // URL to ingame store images
        std::string url = msg->getString();

        // premium coin package size
        // e.g you can only buy packs of 25, 50, 75, .. coins in the market
        const uint16_t coinsPacketSize = msg->getU16();
        tibia::game::notify("onStoreInit", url, coinsPacketSize);
    }

    if (tibia::game::clientRevision() >= 1281) {
        const bool exivaEnabled = static_cast<bool>(msg->getU8());// exiva button enabled (bool)
        tibia::game::setCanExivaOptions(exivaEnabled);
        if (tibia::game::cipFeature(tibia::client::GameTournamentPackets)) {
            msg->getU8(); // Tournament button (bool)
        }
    }

    tibia::game::localPlayerSetId(playerId);
    tibia::game::setServerBeat(serverBeat);
    tibia::game::setCanReportBugs(canReportBugs);

    tibia::game::processLogin();
}

void ProtocolGame::parseCharacterSwitch(const InputMessagePtr& msg)
{
    const auto characterName = msg->getString();
    tibia::game::processCharacterSwitch(characterName);
    // The server follows with a full login burst for the new body on this socket.
    m_localPlayer = tibia::game::localPlayer();
    m_gameInitialized = false;
}

void ProtocolGame::parseBugReport(const InputMessagePtr& msg)
{
    // CIP BugReportsAllowed: 0 = allowed; any other value is a refusal reason.
    const uint8_t state = msg->getU8();
    tibia::game::setBugReportState(state);
    tibia::game::setCanReportBugs(state == 0);
}

void ProtocolGame::parseTextForReport(const InputMessagePtr& msg)
{
    const uint32_t id = msg->getU32();
    const auto text = msg->getString();
    const auto name = msg->getString();
    tibia::game::notify("onTextForReport", id, text, name);
}

void ProtocolGame::parseMultiOfflineTrainingDialog(const InputMessagePtr& /*msg*/)
{
    tibia::game::notify("onMultiOfflineTrainingDialog");
}

void ProtocolGame::parseNpcChatWindow(const InputMessagePtr& msg)
{
    const uint8_t status = msg->getU8();
    if (status != 0) {
        tibia::game::notify("onNpcChatWindowClose");
        return;
    }
    NpcChatWindowData data;

    const uint8_t npcCount = msg->getU8();
    data.npcIds.reserve(npcCount);
    for (auto i = 0; std::cmp_less(i, npcCount); ++i) {
        data.npcIds.push_back(msg->getU32());
    }

    const uint8_t buttonCount = msg->getU8();
    data.buttons.reserve(buttonCount);
    for (auto i = 0; std::cmp_less(i, buttonCount); ++i) {
        NpcButton button;
        button.id = msg->getU8();
        button.text = msg->getString();
        data.buttons.push_back(button);
    }

    tibia::game::notify("onNpcChatWindow", data);
}

void ProtocolGame::parsePendingGame(const InputMessagePtr&)
{
    //set player to pending game state
    tibia::game::processPendingGame();
}

void ProtocolGame::parseEnterGame(const InputMessagePtr&)
{
    // Native H + classic bridge share Game::processWorldEntered (m_gameInitialized gate).
    tibia::game::processWorldEntered();
}

void ProtocolGame::parseStoreButtonIndicators(const InputMessagePtr& msg)
{
    const bool sale = msg->getU8() != 0;
    const bool isNew = msg->getU8() != 0;
    tibia::game::notify("onStoreButtonIndicators", sale, isNew);
}

void ProtocolGame::parseSetStoreDeepLink(const InputMessagePtr& msg)
{
    // StoreOpenParameters: the open action, its detail (premium page / useful
    // thing u8, offer id u32, category and subcategory or search text), then
    // the sort order and service type.
    const uint8_t openAction = msg->getU8();
    uint32_t detail = 0;
    std::string category;
    std::string subCategory;
    switch (openAction) {
        case 1:
        case 3:
            detail = msg->getU8();
            break;
        case 2:
            category = msg->getString();
            subCategory = msg->getString();
            break;
        case 4:
            detail = msg->getU32();
            break;
        case 5:
            category = msg->getString();
            break;
        default:
            break;
    }
    const uint8_t sortOrder = msg->getU8();
    const uint8_t serviceType = msg->getU8();
    tibia::game::notify("onStoreButtonDeeplink", openAction, detail, category, subCategory, sortOrder, serviceType);
}

void ProtocolGame::parseBlessings(const InputMessagePtr& msg) const
{
    const uint16_t blessings = msg->getU16(); // glowing effect indicator
    uint8_t blessVisualState = 0;
    if (tibia::game::clientRevision() >= 1200) {
        blessVisualState = msg->getU8(); // 1 = Disabled | 2 = normal | 3 = green
    }
    tibia::game::localPlayerSetBlessings(blessings, blessVisualState);
}

void ProtocolGame::parsePreset(const InputMessagePtr& msg)
{
    const uint32_t preset = msg->getU32();
    tibia::game::notify("onPreset", preset);
}

void ProtocolGame::parseRequestPurchaseData(const InputMessagePtr& msg)
{
    const uint32_t offerId = msg->getU32();
    const uint8_t productType = msg->getU8(); // 1 name change, 2 world transfer, 3 hireling, 4 main character
    tibia::game::notify("onRequestPurchaseData", offerId, productType);
}

void ProtocolGame::parseResourceBalance(const InputMessagePtr& msg) const
{
    const auto type = static_cast<tibia::client::ResourceTypes_t>(msg->getU8());
    uint64_t value;
    switch (type) {
        // 14.10+
        case tibia::client::RESOURCE_CHARM:
        case tibia::client::RESOURCE_MINOR_CHARM:
        case tibia::client::RESOURCE_MAX_CHARM:
        case tibia::client::RESOURCE_MAX_MINOR_CHARM:
        // 15.13+
        case tibia::client::RESOURCE_BOUNTY_POINTS:
        case tibia::client::RESOURCE_SOULSEALS:
        case tibia::client::RESOURCE_BOUNTY_TASK_REROLL_TOKENS:
        case tibia::client::RESOURCE_COSMETIC_TOKENS:
            value = msg->getU32();
            break;
        default:
            value = msg->getU64();
            break;
    }
    tibia::game::localPlayerSetResourceBalance(type, value);
}

void ProtocolGame::parseWorldTime(const InputMessagePtr& msg)
{
    const uint8_t hour = msg->getU8();
    const uint8_t min = msg->getU8();
    tibia::world::setWorldTime(hour, min);
    tibia::game::notify("onChangeWorldTime", hour, min);
}

void ProtocolGame::parseStore(const InputMessagePtr& msg) const
{
    if (tibia::game::clientRevision() <= 1100) {
        parseCoinBalance(msg);
    }

    const uint16_t categoryCount = msg->getU16();
    std::vector<StoreCategory> categories;

    for (auto i = 0; i < categoryCount; ++i) {
        StoreCategory category;
        category.name = msg->getString();

        if (tibia::game::clientRevision() < 1291) {
            msg->getString();
        }

        if (tibia::game::cipFeature(tibia::client::GameIngameStoreHighlights)) {
            category.state = msg->getU8();
        } else {
            category.state = 0;
        }

        const uint8_t iconCount = msg->getU8();
        for (auto j = 0; j < iconCount; ++j) {
            category.icons.push_back(msg->getString());
        }

        category.parent = msg->getString();
        categories.push_back(category);
    }

    if (tibia::game::clientRevision() >= 1332) {
        msg->getU8();
        msg->getU8();
    }

    std::vector<StoreCategory> organizedCategories;

    for (const auto& category : categories) {
        if (category.parent.empty()) {
            StoreCategory mainCategory = category;
            mainCategory.subCategories.clear();

            for (const auto& subCategory : categories) {
                if (subCategory.parent == category.name) {
                    mainCategory.subCategories.push_back(subCategory);
                }
            }

            organizedCategories.push_back(mainCategory);

            for (const auto& subCategory : mainCategory.subCategories) {
                organizedCategories.push_back(subCategory);
            }
        }
    }

    tibia::game::notify("onParseStoreGetCategories", organizedCategories);
}

void ProtocolGame::parseCoinBalance(const InputMessagePtr& msg) const
{
    const bool update = static_cast<bool>(msg->getU8());
    if (update) {
        // amount of coins that can be used to buy prodcuts
        // in the ingame store
        const uint32_t coins = msg->getU32(); // coins
        tibia::game::localPlayerSetResourceBalance(tibia::client::RESOURE_COIN_NORMAL, coins);

        // amount of coins that can be sold in market
        // or be transfered to another player
        const uint32_t transferrableCoins = msg->getU32(); // transferableCoins
        tibia::game::localPlayerSetResourceBalance(tibia::client::RESOURE_COIN_TRANSFERRABLE, transferrableCoins);

        if (tibia::game::clientRevision() >= 1281) {
            const uint32_t auctionCoins = msg->getU32();
            tibia::game::localPlayerSetResourceBalance(tibia::client::RESOURE_COIN_AUCTION, auctionCoins);
            if (tibia::game::cipFeature(tibia::client::GameTournamentPackets)) {
                const uint32_t tournamentCoins = msg->getU32();
                tibia::game::localPlayerSetResourceBalance(tibia::client::RESOURE_COIN_TOURNAMENT, tournamentCoins);
            }
        }
    }
}

void ProtocolGame::parseCoinBalanceUpdating(const InputMessagePtr& msg)
{
    if (tibia::game::clientRevision() >= 1291) {
        // Crystal's gamestore packs 0xF2 (updating) and 0xDF (balance) in one
        // message; the trailing DF fields are consumed here.
        const uint8_t action = msg->getU8();
        if (action == 0) {
            return;
        }

        msg->getU8(); // 0xDF
        msg->getU8(); // update flag
        const uint32_t getTibiaCoins = msg->getU32();
        const uint32_t getTransferableCoins = msg->getU32();
        tibia::game::localPlayerSetResourceBalance(tibia::client::RESOURE_COIN_NORMAL, getTibiaCoins);
        tibia::game::localPlayerSetResourceBalance(tibia::client::RESOURE_COIN_TRANSFERRABLE, getTransferableCoins);
        if (tibia::game::clientRevision() >= 1281) {
            const uint32_t auctionCoins = msg->getU32();
            tibia::game::localPlayerSetResourceBalance(tibia::client::RESOURE_COIN_AUCTION, auctionCoins);
        }
        if (tibia::game::cipFeature(tibia::client::GameTournamentPackets)) {
            const uint32_t tournamentCoins = msg->getU32();
            tibia::game::localPlayerSetResourceBalance(tibia::client::RESOURE_COIN_TOURNAMENT, tournamentCoins);
        }
        tibia::game::notify("onParseStoreGetCoin", getTibiaCoins, getTransferableCoins);
    } else {
        // coin balance can be updating and might not be accurate
        msg->getU8(); // == 1; // isUpdating
    }
}

void ProtocolGame::parseCompleteStorePurchase(const InputMessagePtr& msg) const
{
    if (tibia::game::clientRevision() >= 1291) {
        const uint8_t successType = msg->getU8(); // 0 purchase, 1 coin transfer
        const auto& purchaseStatus = msg->getString();
        tibia::game::notify("onParseStoreGetPurchaseStatus", purchaseStatus, successType);
    } else {
        msg->getU8(); // not used

        const auto& message = msg->getString();
        const uint32_t coins = msg->getU32();
        const uint32_t transferableCoins = msg->getU32();
        tibia::game::notify("onParseStoreGetCoin", coins, transferableCoins);
        tibia::game::notify("onParseStoreGetPurchaseStatus", message);
    }
}

void ProtocolGame::parseStoreTransactionHistory(const InputMessagePtr& msg) const
{
    uint32_t currentPage;
    uint32_t pageCount;
    if (tibia::game::clientRevision() <= 1096) {
        msg->getU16(); // currentPage
        msg->getU8(); // hasNextPage (bool)
    } else {
        currentPage = msg->getU32();
        pageCount = msg->getU32();
    }

    const uint8_t entries = msg->getU8();
    std::vector<std::tuple<uint32_t, uint8_t, int32_t, uint8_t, std::string>> historyData;
    // Per entry: transaction id and whether the server has details for it.
    std::vector<std::tuple<uint32_t, uint8_t>> historyDetails;
    for (auto i = 0; i < entries; ++i) {
        if (tibia::game::clientRevision() >= 1291) {
            const uint32_t transactionId = msg->getU32();
            const uint32_t time = msg->getU32();
            const uint8_t mode = msg->getU8(); //0 = normal, 1 = gift, 2 = refund
            const uint32_t rawAmount = msg->getU32();
            int32_t amount;
            if (rawAmount > INT32_MAX) {
                amount = -static_cast<int32_t>(UINT32_MAX - rawAmount + 1);
            } else {
                amount = static_cast<int32_t>(rawAmount);
            }
            const uint8_t coinType = msg->getU8(); // 0 = transferable tibia coin, 1 = normal tibia coin
            const auto& productName = msg->getString();
            const uint8_t details = msg->getU8();
            historyData.emplace_back(time, mode, amount, coinType, productName);
            historyDetails.emplace_back(transactionId, details);
        } else {
            const uint32_t time = msg->getU32();
            const uint8_t productType = msg->getU8();
            const uint32_t coinChange = msg->getU32();
            const auto& productName = msg->getString();
            historyData.emplace_back(time, productType, coinChange, 1, productName);
        }
    }

    tibia::game::notify("onParseStoreGetHistory", currentPage, pageCount, historyData, historyDetails);
}

void ProtocolGame::parseStoreOffers(const InputMessagePtr& msg)
{
    if (tibia::game::clientRevision() >= 1291) {
        StoreData storeData;
        storeData.categoryName = msg->getString();
        storeData.redirectId = msg->getU32();

        msg->getU8(); // -- sort by 0 - most popular, 1 - alphabetically, 2 - newest
        const uint8_t dropMenuShowAll = msg->getU8();
        for (auto i = 0; i < dropMenuShowAll; ++i) {
            const auto& menu = msg->getString();
            storeData.menuFilter.push_back(menu);
        }

        uint16_t stringLength = msg->getU16();
        msg->skipBytes(stringLength); // tfs send string , canary send u16

        if (tibia::game::clientRevision() >= 1310) {
            const uint16_t disableReasonsSize = msg->getU16();

            for (auto i = 0; i < disableReasonsSize; ++i) {
                const auto& reason = msg->getString();
                storeData.disableReasons.push_back(reason);
            }
        }

        // Tibia sends one offer layout for every category (Home included):
        // quantities, visualisation, try-on, collection, popularity, new-until,
        // configurable, then the bundled products (name + visualisation each).
        const auto parseSubOffer = [&msg]() {
            SubOffer subOffer{};
            subOffer.id = msg->getU32();
            subOffer.count = msg->getU16();
            subOffer.price = msg->getU32();
            subOffer.coinType = msg->getU8();
            subOffer.disabled = msg->getU8() == 1;
            if (subOffer.disabled) {
                const uint8_t reason = msg->getU8();
                for (auto k = 0; k < reason; ++k) {
                    if (tibia::game::clientRevision() >= 1300) {
                        subOffer.reasonIdDisable = msg->getU16();
                    } else {
                        msg->getString();
                    }
                }
            }
            subOffer.state = msg->getU8();
            if (subOffer.state == tibia::client::GameStoreInfoStatesType_t::STATE_SALE) {
                subOffer.validUntil = msg->getU32();
                subOffer.basePrice = msg->getU32();
            }
            return subOffer;
        };
        const auto parseVisual = [&msg](StoreProduct& visual) {
            visual.type = msg->getU8();
            switch (visual.type) {
                case tibia::client::GameStoreInfoType_t::SHOW_NONE: visual.icon = msg->getString(); break;
                case tibia::client::GameStoreInfoType_t::SHOW_MOUNT:
                case tibia::client::GameStoreInfoType_t::SHOW_ITEM: visual.typeId = msg->getU16(); break;
                case tibia::client::GameStoreInfoType_t::SHOW_OUTFIT:
                    visual.typeId = msg->getU16();
                    visual.outfitHead = msg->getU8();
                    visual.outfitBody = msg->getU8();
                    visual.outfitLegs = msg->getU8();
                    visual.outfitFeet = msg->getU8();
                    break;
                case tibia::client::GameStoreInfoType_t::SHOW_HIRELING:
                    visual.sex = msg->getU8();
                    visual.maleOutfitId = msg->getU16();
                    visual.femaleOutfitId = msg->getU16();
                    visual.outfitHead = msg->getU8();
                    visual.outfitBody = msg->getU8();
                    visual.outfitLegs = msg->getU8();
                    visual.outfitFeet = msg->getU8();
                    break;
                default: break;
            }
        };
        const auto parseProducts = [&msg, &parseVisual](const uint16_t count, std::vector<StoreProduct>& products) {
            for (auto j = 0; j < count; ++j) {
                StoreProduct product;
                product.name = msg->getString();
                parseVisual(product);
                products.push_back(product);
            }
        };

        const uint16_t offersCount = msg->getU16();
        if (storeData.categoryName == "Home") {
            for (auto i = 0; i < offersCount; ++i) {
                HomeOffer offer{};
                offer.name = msg->getString();
                offer.unknownByte = msg->getU8();
                for (auto j = 0; j < offer.unknownByte; ++j)
                    offer.subOffers.push_back(parseSubOffer());
                if (!offer.subOffers.empty()) {
                    const SubOffer& first = offer.subOffers.front();
                    offer.id = first.id;
                    offer.unknownU16 = first.count;
                    offer.price = first.price;
                    offer.coinType = first.coinType;
                    offer.disabled = first.disabled;
                    offer.disabledReasonIndex = first.reasonIdDisable;
                    offer.unknownByte2 = first.state;
                }

                StoreProduct visual;
                parseVisual(visual);
                offer.type = visual.type;
                offer.icon = visual.icon;
                if (visual.type == tibia::client::GameStoreInfoType_t::SHOW_MOUNT)
                    offer.mountClientId = visual.typeId;
                else if (visual.type == tibia::client::GameStoreInfoType_t::SHOW_ITEM)
                    offer.itemType = visual.typeId;
                else if (visual.type == tibia::client::GameStoreInfoType_t::SHOW_OUTFIT)
                    offer.sexId = visual.typeId;
                offer.sex = visual.sex;
                offer.maleOutfitId = visual.maleOutfitId;
                offer.femaleOutfitId = visual.femaleOutfitId;
                offer.outfit = { visual.outfitHead, visual.outfitBody, visual.outfitLegs, visual.outfitFeet };

                offer.tryOnType = msg->getU8();
                offer.collection = msg->getU16();
                offer.popularityScore = msg->getU16();
                offer.stateNewUntil = msg->getU32();
                offer.userConfiguration = msg->getU8();
                offer.productsCapacity = msg->getU16();
                parseProducts(offer.productsCapacity, offer.products);

                storeData.homeOffers.push_back(offer);
            }

            const uint8_t bannerCount = msg->getU8();

            for (auto i = 0; i < bannerCount; ++i) {
                Banner banner;
                banner.image = msg->getString();
                banner.bannerType = msg->getU8();
                banner.offerId = msg->getU32();
                banner.unknownByte1 = msg->getU8();
                banner.unknownByte2 = msg->getU8();
                storeData.banners.push_back(banner);
            }

            storeData.bannerDelay = msg->getU8();

            tibia::game::notify("onParseStoreCreateHome", storeData);
            return;
        }

        for (auto i = 0; i < offersCount; ++i) {
            StoreOffer offer;
            offer.name = msg->getString();

            const uint8_t subOffersCount = msg->getU8();
            for (auto j = 0; j < subOffersCount; ++j)
                offer.subOffers.push_back(parseSubOffer());

            offer.type = msg->getU8();
            if (offer.type == tibia::client::GameStoreInfoType_t::SHOW_NONE) {
                offer.icon = msg->getString();
            } else if (offer.type == tibia::client::GameStoreInfoType_t::SHOW_MOUNT) {
                offer.mountId = msg->getU16();
            } else if (offer.type == tibia::client::GameStoreInfoType_t::SHOW_ITEM) {
                offer.itemId = msg->getU16();
            } else if (offer.type == tibia::client::GameStoreInfoType_t::SHOW_OUTFIT) {
                offer.outfitId = msg->getU16();
                offer.outfitHead = msg->getU8();
                offer.outfitBody = msg->getU8();
                offer.outfitLegs = msg->getU8();
                offer.outfitFeet = msg->getU8();
            } else if (offer.type == tibia::client::GameStoreInfoType_t::SHOW_HIRELING) {
                offer.sex = msg->getU8();
                offer.maleOutfitId = msg->getU16();
                offer.femaleOutfitId = msg->getU16();
                offer.outfitHead = msg->getU8();
                offer.outfitBody = msg->getU8();
                offer.outfitLegs = msg->getU8();
                offer.outfitFeet = msg->getU8();
            }

            offer.tryOnType = msg->getU8();

            if (tibia::game::clientRevision() <= 1310) {
                auto test = msg->getString();
            } else {
                offer.collection = msg->getU16();
            }

            offer.popularityScore = msg->getU16();
            offer.stateNewUntil = msg->getU32();
            offer.configurable = msg->getU8() == 1;
            offer.productsCapacity = msg->getU16();
            parseProducts(offer.productsCapacity, offer.products);
            storeData.storeOffers.push_back(offer);
        }

        if (storeData.categoryName == "Search") {
            storeData.tooManyResults = msg->getU8() == 1;
        }

        tibia::game::notify("onParseStoreCreateProducts", storeData);
    } else {
        StoreData storeData;
        storeData.categoryName = msg->getString(); // categoryName

        const uint16_t offersCount = msg->getU16();
        for (auto i = 0; i < offersCount; ++i) {
            StoreOffer offer;
            offer.id = msg->getU32(); // offerId
            offer.name = msg->getString(); // offerName
            offer.description = msg->getString(); // offerDescription
            offer.price = msg->getU32(); // price

            const uint8_t highlightState = msg->getU8();
            if (highlightState == 2 && tibia::game::cipFeature(tibia::client::GameIngameStoreHighlights) && tibia::game::clientRevision() >= 1097) {
                offer.state = tibia::client::GameStoreInfoStatesType_t::STATE_SALE;
                offer.stateNewUntil = msg->getU32(); // saleValidUntilTimestamp
                offer.basePrice = msg->getU32(); // basePrice
            } else {
                offer.state = highlightState;
            }

            offer.disabled = msg->getU8() == 1;
            if (tibia::game::cipFeature(tibia::client::GameIngameStoreHighlights) && offer.disabled) {
                offer.reasonIdDisable = msg->getString(); // disabledReason
            }

            const uint8_t iconCount = msg->getU8();
            for (auto j = 0; j < iconCount; ++j) {
                offer.icon = msg->getString(); // icon
            }

            const uint16_t subOffersCount = msg->getU16();

            for (auto j = 0; j < subOffersCount; ++j) {
                SubOffer subOffer;
                subOffer.name = msg->getString(); // name
                subOffer.description = msg->getString(); // description

                const uint8_t subIconsCount = msg->getU8();
                for (auto k = 0; k < subIconsCount; ++k) {
                    subOffer.icons.push_back(msg->getString()); // icon
                }
                subOffer.parent = msg->getString(); // serviceType
                offer.subOffers.push_back(subOffer);
            }

            storeData.storeOffers.push_back(offer);
        }

        tibia::game::notify("onParseStoreCreateProducts", storeData);
    }
}

void ProtocolGame::parseStoreError(const InputMessagePtr& msg) const
{
    const uint8_t errorType = msg->getU8();
    const auto& message = msg->getString();

    tibia::game::notify("onParseStoreError", message, errorType);
}

void ProtocolGame::parseUnjustifiedStats(const InputMessagePtr& msg)
{
    const uint8_t killsDay = msg->getU8(); //dayProgress %
    const uint8_t killsDayRemaining = msg->getU8();
    const uint8_t killsWeek = msg->getU8(); //weekProgress %
    const uint8_t killsWeekRemaining = msg->getU8();
    const uint8_t killsMonth = msg->getU8(); //monthProgress %
    const uint8_t killsMonthRemaining = msg->getU8();
    const uint8_t skullTime = msg->getU8();

    tibia::game::setUnjustifiedPoints({ .killsDay = killsDay, .killsDayRemaining = killsDayRemaining, .killsWeek = killsWeek, .killsWeekRemaining =
        killsWeekRemaining,
        .killsMonth = killsMonth, .killsMonthRemaining = killsMonthRemaining, .skullTime = skullTime
    });
}

void ProtocolGame::parsePvpSituations(const InputMessagePtr& msg)
{
    const uint8_t openPvpSituations = msg->getU8();
    tibia::game::setOpenPvpSituations(openPvpSituations);
}

void ProtocolGame::parsePlayerHelpers(const InputMessagePtr& msg) const
{
    const uint32_t creatureId = msg->getU32();
    const uint16_t helpers = msg->getU16();

    // Existence only — processPlayerHelpers does not use Creature methods.
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (!storage->creatureAt(creatureId)) {
            g_logger.traceDebug("ProtocolGame::parsePlayerHelpers: could not get creature with id {}", creatureId);
            return;
        }
    } else if (!tibia::game::creatureThingById(creatureId)) {
        g_logger.traceDebug("ProtocolGame::parsePlayerHelpers: could not get creature with id {}", creatureId);
        return;
    }

    tibia::game::processPlayerHelpers(helpers);
}

void ProtocolGame::parseReadyForSecondaryConnection(const InputMessagePtr& msg)
{
    // CIP ReadyForSecondaryConnection (S 0x0B): stability-socket token. Not GMActions.
    const auto& token = msg->getString();
    tibia::game::processReadyForSecondaryConnection(token);
}

void ProtocolGame::parseUpdateNeeded(const InputMessagePtr& msg)
{
    const auto& signature = msg->getString();
    tibia::game::processUpdateNeeded(signature);
}

void ProtocolGame::parseLoginError(const InputMessagePtr& msg)
{
    const auto& error = msg->getString();
    if (tibia::game::clientRevision() >= 1523 && msg->getUnreadSize() > 0) {
        msg->getU8(); // reason
    }
    tibia::game::processLoginError(error);
}

void ProtocolGame::parseLoginAdvice(const InputMessagePtr& msg)
{
    const auto& message = msg->getString();
    tibia::game::processLoginAdvice(message);
}

void ProtocolGame::parseLoginWait(const InputMessagePtr& msg)
{
    const auto& message = msg->getString();
    const uint8_t time = msg->getU8();

    tibia::game::processLoginWait(message, time);
}

void ProtocolGame::parseSessionEnd(const InputMessagePtr& msg)
{
    const uint8_t reason = msg->getU8();
    tibia::game::processSessionEnd(reason);
}

void ProtocolGame::parseSessionCreatureData(const InputMessagePtr& msg)
{
    // CIP CreatureData (S 0x03): one nested creature. Field contents are not
    // recovered, so this handler drains the remaining bytes after an optional
    // leading creature id. Crystal does not send this opcode.
    uint32_t creatureId = 0;
    if (msg->getUnreadSize() >= 4)
        creatureId = msg->getU32();
    while (msg->getUnreadSize() > 0)
        msg->getU8();
    tibia::game::notify("onCreatureData", creatureId);
}

void ProtocolGame::parseSessionDumpStart(const InputMessagePtr& msg)
{
    // CIP SessionDumpStart (S 0x04). Optional leading string is the recovered
    // field; SessionDumpStart field contents after that are not decoded, so
    // this handler drains the remaining bytes. Crystal does not send it.
    std::string sessionKey;
    if (msg->getUnreadSize() >= 2)
        sessionKey = msg->getString();
    while (msg->getUnreadSize() > 0)
        msg->getU8();
    tibia::game::processSessionDumpStart(sessionKey);
}

void ProtocolGame::parseSessionDumpMarker(const InputMessagePtr& msg)
{
    // CIP SessionDumpMarker (S 0x05): optional leading marker string. Marker
    // field contents after that string are not decoded, so this handler
    // drains the remaining bytes.
    std::string marker;
    if (msg->getUnreadSize() >= 2)
        marker = msg->getString();
    while (msg->getUnreadSize() > 0)
        msg->getU8();
    tibia::game::processSessionDumpMarker(marker);
}

void ProtocolGame::parsePartyLeft(const InputMessagePtr&)
{
    // CIP PartyLeft (S 0x06) — empty body (protobuf serializer writes no fields).
    // Crystal sendPartyLeft matches.
    tibia::game::notify("onPartyLeft");
}

void ProtocolGame::parsePing(const InputMessagePtr&) { tibia::game::processPing(); }
void ProtocolGame::parsePingBack(const InputMessagePtr&) { tibia::game::processPingBack(); }

void ProtocolGame::parseLoginChallenge(const InputMessagePtr& msg)
{
    const uint32_t timestamp = msg->getU32();
    const uint8_t random = msg->getU8();

    if (tibia::game::clientRevision() >= 1405) {
        msg->skipBytes(1);
    }

    sendLoginPacket(timestamp, random);
}

void ProtocolGame::parseDeath(const InputMessagePtr& msg)
{
    uint8_t penality = 100;
    uint8_t deathType = tibia::client::DeathRegular;

    if (tibia::game::cipFeature(tibia::client::GameDeathType)) {
        deathType = msg->getU8();
    }

    if (tibia::game::cipFeature(tibia::client::GamePenalityOnDeath) && deathType == tibia::client::DeathRegular) {
        penality = msg->getU8();
    }

    if (tibia::game::clientRevision() >= 1281) {
        msg->getU8(); // (bool) can use death redemption
    }

    tibia::game::processDeath(deathType, penality);
}

void ProtocolGame::parseFloorDescription(const InputMessagePtr& msg)
{
    const auto& pos = getPosition(msg);
    const uint8_t floor = msg->getU8();

    if (tibia::game::isSceneFrozen()) {
        const auto& range = tibia::game::awareRange();
        setFloorDescription(msg, pos.x - range.left, pos.y - range.top, floor, range.horizontal(), range.vertical(), pos.z - floor, 0, false);
        return;
    }

    if (pos.z == floor) {
        const auto& oldPos = m_localPlayer->getPosition();
        if (!m_mapKnown) {
            m_localPlayer->setPosition(pos);
        }

        tibia::game::setCentralPosition({ pos.x, pos.y, pos.z });

        if (!m_mapKnown) {
            tibia::schedule::post([] { tibia::game::notify("onMapKnown"); });
            m_mapKnown = true;
        }

        tibia::schedule::post([] { tibia::game::notify("onMapDescription"); });
        if (m_localPlayer)
            m_localPlayer->handleTeleport();
        tibia::game::notify("onTeleport", m_localPlayer, pos, oldPos);
    }

    const auto& range = tibia::game::awareRange();
    setFloorDescription(msg, pos.x - range.left, pos.y - range.top, floor, range.horizontal(), range.vertical(), pos.z - floor, 0);
    tibia::worldmap::notifyWindowChanged(pos.z == floor);

    tibia::game::updateMapLatency();
}

void ProtocolGame::parseMapDescription(const InputMessagePtr& msg)
{
    const auto& pos = getPosition(msg);
    const auto& oldPos = m_localPlayer->getPosition();
    const bool frozen = tibia::game::isSceneFrozen();

    if (!frozen) {
        if (!m_mapKnown) {
            m_localPlayer->setPosition(pos);
        }

        tibia::game::setCentralPosition({ pos.x, pos.y, pos.z });
    }

    const auto& range = tibia::game::awareRange();
    if (frozen) {
        setMapDescription(msg, pos.x - range.left, pos.y - range.top, pos.z, range.horizontal(), range.vertical(), false);
        return;
    }

    // CipSoft: abort in-flight TCreatureMovementTween before re-placing the
    // window. Doing this after setMapDescription let onAppear start a tween that
    // handleTeleport then killed — first steps after floor/map load looked skipped.
    if (m_localPlayer && oldPos != pos)
        m_localPlayer->handleTeleport();

    setMapDescription(msg, pos.x - range.left, pos.y - range.top, pos.z, range.horizontal(), range.vertical());
    tibia::worldmap::notifyWindowChanged(true);

    if (!m_mapKnown) {
        tibia::schedule::post([] { tibia::game::notify("onMapKnown"); });
        m_mapKnown = true;
    }

    tibia::schedule::post([] { tibia::game::notify("onMapDescription"); });
    tibia::game::notify("onTeleport", m_localPlayer, pos, oldPos);
}

void ProtocolGame::parseMapMoveNorth(const InputMessagePtr& msg)
{
    auto pos = tibia::game::cipFeature(tibia::client::GameMapMovePosition) ? getPosition(msg) : peeledMapCentral();
    --pos.y;

    const auto& range = tibia::game::awareRange();
    if (tibia::game::isSceneFrozen()) {
        setMapDescription(msg, pos.x - range.left, pos.y - range.top, pos.z, range.horizontal(), 1, false);
        return;
    }

    setMapDescription(msg, pos.x - range.left, pos.y - range.top, pos.z, range.horizontal(), 1);
    tibia::game::setCentralPosition({ pos.x, pos.y, pos.z });
    tibia::worldmap::notifyWindowChanged(false);
}

void ProtocolGame::parseMapMoveEast(const InputMessagePtr& msg)
{
    auto pos = tibia::game::cipFeature(tibia::client::GameMapMovePosition) ? getPosition(msg) : peeledMapCentral();
    ++pos.x;

    const auto& range = tibia::game::awareRange();
    if (tibia::game::isSceneFrozen()) {
        setMapDescription(msg, pos.x + range.right, pos.y - range.top, pos.z, 1, range.vertical(), false);
        return;
    }

    setMapDescription(msg, pos.x + range.right, pos.y - range.top, pos.z, 1, range.vertical());
    tibia::game::setCentralPosition({ pos.x, pos.y, pos.z });
    tibia::worldmap::notifyWindowChanged(false);
}

void ProtocolGame::parseMapMoveSouth(const InputMessagePtr& msg)
{
    auto pos = tibia::game::cipFeature(tibia::client::GameMapMovePosition) ? getPosition(msg) : peeledMapCentral();
    ++pos.y;

    const auto& range = tibia::game::awareRange();
    if (tibia::game::isSceneFrozen()) {
        setMapDescription(msg, pos.x - range.left, pos.y + range.bottom, pos.z, range.horizontal(), 1, false);
        return;
    }

    setMapDescription(msg, pos.x - range.left, pos.y + range.bottom, pos.z, range.horizontal(), 1);
    tibia::game::setCentralPosition({ pos.x, pos.y, pos.z });
    tibia::worldmap::notifyWindowChanged(false);
}

void ProtocolGame::parseMapMoveWest(const InputMessagePtr& msg)
{
    auto pos = tibia::game::cipFeature(tibia::client::GameMapMovePosition) ? getPosition(msg) : peeledMapCentral();
    --pos.x;

    const auto& range = tibia::game::awareRange();
    if (tibia::game::isSceneFrozen()) {
        setMapDescription(msg, pos.x - range.left, pos.y - range.top, pos.z, 1, range.vertical(), false);
        return;
    }

    setMapDescription(msg, pos.x - range.left, pos.y - range.top, pos.z, 1, range.vertical());
    tibia::game::setCentralPosition({ pos.x, pos.y, pos.z });
    tibia::worldmap::notifyWindowChanged(false);
}

void ProtocolGame::parseUpdateTile(const InputMessagePtr& msg)
{
    const auto& tilePos = getPosition(msg);
    if (tibia::game::isSceneFrozen()) {
        setTileDescription(msg, tilePos, false);
        return;
    }

    setTileDescription(msg, tilePos);
}

void ProtocolGame::parseTileAddThing(const InputMessagePtr& msg)
{
    const auto& pos = getPosition(msg);
    const int stackPos = tibia::game::cipFeature(tibia::client::GameTileAddThingWithStackpos) ? msg->getU8() : -1;
    const auto& thing = getThing(msg);

    if (tibia::game::isSceneFrozen()) {
        return;
    }

    tibia::worldmap::TWorldmapProtocolMessageHandler* const worldmapHandler =
        tibia::worldmap::TWorldmapProtocolMessageHandler::liveInstance();
    tibia::worldmap::TWorldMapStorage* const storage =
        worldmapHandler ? worldmapHandler->storage() : tibia::worldmap::TWorldMapStorage::ensureStorage();

    // Appearance + creatureId only (no ThingPtr field mutate). LocalPlayerSessionThing
    // prewalk clear must run on the lastParsedCreatureId path — the Thing
    // branch was unreachable when getCreature always sets that id.
    (void)thing;
    if (const uint32_t id = lastParsedCreatureId(); id != 0) {
        if (id == tibia::game::localPlayerId())
            tibia::game::localPlayerResetPreWalk();
        if (worldmapHandler)
            worldmapHandler->applyCreatureIdToField(id, pos, stackPos);
        else
            storage->placeCreatureId(id, pos, stackPos);
    } else if (lastParsedItemAppearance()) {
        if (worldmapHandler)
            worldmapHandler->applyAppearanceToField(lastParsedItemAppearance(), pos, stackPos);
        else
            storage->placeAppearance(lastParsedItemAppearance(), pos, stackPos);
    }
}

void ProtocolGame::parseTileTransformThing(const InputMessagePtr& msg)
{
    if (tibia::game::isSceneFrozen()) {
        skipMappedThing(msg);
        getThing(msg);
        return;
    }

    // Stack / creatureId only — no ThingPtr field mutate (CipSoft ChangeOnMap).
    const auto peek = peekMappedThingHeader(msg);
    skipMappedThing(msg);
    getThing(msg);
    const uint32_t newCreatureId = lastParsedCreatureId();

    tibia::worldmap::TWorldmapProtocolMessageHandler* const worldmapHandler =
        tibia::worldmap::TWorldmapProtocolMessageHandler::liveInstance();
    tibia::worldmap::TWorldMapStorage* const mapStorage =
        worldmapHandler ? worldmapHandler->storage() : tibia::worldmap::TWorldMapStorage::ensureStorage();

    auto placeReplacement = [&](const Position& at, const int stackPos) {
        if (newCreatureId != 0) {
            if (newCreatureId == tibia::game::localPlayerId())
                tibia::game::localPlayerResetPreWalk();
            if (worldmapHandler)
                worldmapHandler->applyCreatureIdToField(newCreatureId, at, stackPos);
            else
                mapStorage->placeCreatureId(newCreatureId, at, stackPos);
        } else if (lastParsedItemAppearance()) {
            if (worldmapHandler)
                worldmapHandler->applyAppearanceToField(lastParsedItemAppearance(), at, stackPos);
            else
                mapStorage->placeAppearance(lastParsedItemAppearance(), at, stackPos);
        }
    };

    if (peek.byId) {
        // Id form is always a creature. Outfit/turn may already be in storage from getCreature.
        const uint32_t id = peek.creatureId;
        Position fieldPos;
        if (tibia::creatures::TCreatureStorage* creatureStorage = tibia::creatures::TCreatureStorage::instance()) {
            if (const auto row = creatureStorage->creatureAt(id))
                fieldPos = row->position();
        }
        if (tibia::worldmap::TWorldMapField* tile = fieldPos.isValid() ? tibia::worldmap::fieldAt(fieldPos) : nullptr) {
            const int stackPos = tile->creatureStackPos(id);
            if (stackPos >= 0) {
                mapStorage->removeCreatureFromFields(id, fieldPos);
                placeReplacement(tile->getPosition(), stackPos);
            }
        }
        return;
    }

    if (tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(peek.pos)) {
        if (const uint32_t stackCreatureId = tile->stackCreatureId(peek.stack)) {
            if (mapStorage->removeCreatureFromFields(stackCreatureId, peek.pos)) {
                placeReplacement(peek.pos, peek.stack);
                return;
            }
        }
        if (tile->removeThingAtStackPos(peek.stack)) {
            placeReplacement(peek.pos, peek.stack);
            return;
        }
    }
    // 0x6B turns still carry id/direction in the replacement — getCreature already
    // applied the update when the body is known. Don't treat that as a hard miss.
    if (newCreatureId != 0)
        return;
    g_logger.traceError("ProtocolGame::parseTileTransformThing: no thing at {} stackpos {}", peek.pos, peek.stack);
}

void ProtocolGame::parseTileRemoveThing(const InputMessagePtr& msg) const
{
    if (tibia::game::isSceneFrozen()) {
        skipMappedThing(msg);
        return;
    }

    // Stack / creatureId only — no ThingPtr field mutate (CipSoft DeleteOnMap).
    const auto peek = peekMappedThingHeader(msg);
    skipMappedThing(msg);

    tibia::worldmap::TWorldmapProtocolMessageHandler* const worldmapHandler =
        tibia::worldmap::TWorldmapProtocolMessageHandler::liveInstance();
    tibia::worldmap::TWorldMapStorage* const mapStorage =
        worldmapHandler ? worldmapHandler->storage() : tibia::worldmap::TWorldMapStorage::ensureStorage();

    if (peek.byId) {
        const uint32_t creatureId = peek.creatureId;
        if (!mapStorage->removeCreatureFromFields(creatureId)) {
            g_logger.traceError("ProtocolGame::parseTileRemoveThing: no creature with id {}", creatureId);
            return;
        }
        // CIP DeleteOnMap leaves KnownCreature identity (name/outfit); only the
        // field slot goes away. Destroying the row blanked nameplates on the
        // next KnownCreature reappear (wire has no name).
        if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance())
            storage->leaveMap(creatureId);
        return;
    }

    if (tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(peek.pos)) {
        if (const uint32_t stackCreatureId = tile->stackCreatureId(peek.stack)) {
            if (mapStorage->removeCreatureFromFields(stackCreatureId, peek.pos)) {
                if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance())
                    storage->leaveMap(stackCreatureId);
                return;
            }
        }
        if (tile->removeThingAtStackPos(peek.stack))
            return;
    }
    if (!tibia::game::isAwareOf({ peek.pos.x, peek.pos.y, peek.pos.z })) {
        g_logger.debug("ProtocolGame::parseTileRemoveThing: no thing at unaware {} stackpos {}", peek.pos, peek.stack);
    } else {
        g_logger.traceError("ProtocolGame::parseTileRemoveThing: no thing at {} stackpos {}", peek.pos, peek.stack);
    }
}

void ProtocolGame::parseCreatureMove(const InputMessagePtr& msg)
{
    if (tibia::game::isSceneFrozen()) {
        skipMappedThing(msg);
        getPosition(msg);
        return;
    }

    const auto peek = peekMappedThingHeader(msg);
    const Position& oldPos = peek.pos;
    const int oldStack = peek.stack;
    const uint32_t byId = peek.byId ? peek.creatureId : 0;

    tibia::worldmap::TWorldmapProtocolMessageHandler* const worldmapHandler =
        tibia::worldmap::TWorldmapProtocolMessageHandler::liveInstance();
    tibia::worldmap::TWorldMapStorage* const mapStorage =
        worldmapHandler ? worldmapHandler->storage() : tibia::worldmap::TWorldMapStorage::ensureStorage();

    // Resolve creature id without requiring Map Creature when possible.
    uint32_t creatureId = byId;
    if (!creatureId) {
        if (const auto* tile = tibia::worldmap::fieldAt(oldPos))
            creatureId = tile->stackCreatureId(oldStack);
    }
    const auto& thing = getMappedThing(msg);
    if (!creatureId && thing && thing->isCreature())
        creatureId = thing->getId();

    const auto& newPos = getPosition(msg);

    if (!creatureId) {
        g_logger.traceError("ProtocolGame::parseCreatureMove: no creature found to move from {} stackpos {}", oldPos, oldStack);
        return;
    }

    if (!mapStorage->applyCreatureMove(creatureId, oldPos, oldStack, newPos, byId != 0)) {
        g_logger.traceError("ProtocolGame::parseCreatureMove: unable to remove creature {} from {} stackpos {}", creatureId, oldPos, oldStack);
    }
}

void ProtocolGame::parseOpenContainer(const InputMessagePtr& msg)
{
    const uint8_t containerId = msg->getU8();
    const auto containerItem = getInventorySlot(msg);
    const auto& name = msg->getString();
    const uint8_t capacity = msg->getU8();
    const bool hasParent = static_cast<bool>(msg->getU8());

    bool isUnlocked = true;
    bool hasPages = false;
    bool showSearch = false;
    uint16_t containerSize = 0;
    uint16_t firstIndex = 0;

    if (tibia::game::clientRevision() >= 1281)
        showSearch = static_cast<bool>(msg->getU8());

    if (tibia::game::cipFeature(tibia::client::GameContainerPagination)) {
        isUnlocked = static_cast<bool>(msg->getU8()); // drag and drop
        hasPages = static_cast<bool>(msg->getU8()); // pagination
        containerSize = msg->getU16(); // container size
        firstIndex = msg->getU16(); // first index
    }

    const uint8_t itemCount = msg->getU8();
    std::vector<tibia::container::TInventorySlot> items;
    items.reserve(itemCount);

    for (auto i = 0; i < itemCount; i++)
        items.push_back(getInventorySlot(msg));

    if (tibia::game::cipFeature(tibia::client::GameContainerFilter)) {
        msg->getU8(); // category
        const uint8_t categoriesSize = msg->getU8();
        for (auto i = 0; i < categoriesSize; ++i) {
            msg->getU8(); // id
            msg->getString(); // name
        }
    }

    if (tibia::game::clientRevision() >= 1340) {
        msg->getU8(); // isMoveable
        msg->getU8(); // isHolding
    }

    tibia::game::processOpenContainer(containerId, containerItem, name, capacity, hasParent, items, isUnlocked, hasPages,
                                showSearch, containerSize, firstIndex);
}

void ProtocolGame::parseCloseContainer(const InputMessagePtr& msg)
{
    const uint8_t containerId = msg->getU8();
    tibia::game::processCloseContainer(containerId);
}

void ProtocolGame::parseContainerAddItem(const InputMessagePtr& msg)
{
    const uint8_t containerId = msg->getU8();
    const uint16_t slot = tibia::game::cipFeature(tibia::client::GameContainerPagination) ? msg->getU16() : 0;
    tibia::game::processContainerAddItem(containerId, getInventorySlot(msg), slot);
}

void ProtocolGame::parseContainerUpdateItem(const InputMessagePtr& msg)
{
    const uint8_t containerId = msg->getU8();
    const uint16_t slot = tibia::game::cipFeature(tibia::client::GameContainerPagination) ? msg->getU16() : msg->getU8();
    tibia::game::processContainerUpdateItem(containerId, slot, getInventorySlot(msg));
}

void ProtocolGame::parseContainerRemoveItem(const InputMessagePtr& msg)
{
    const uint8_t containerId = msg->getU8();
    uint16_t slot;

    tibia::container::TInventorySlot lastItem;
    if (tibia::game::cipFeature(tibia::client::GameContainerPagination)) {
        slot = msg->getU16();

        const uint16_t itemId = msg->getU16();
        if (itemId != 0)
            lastItem = getInventorySlot(msg, itemId);
    } else {
        slot = msg->getU8();
    }

    tibia::game::processContainerRemoveItem(containerId, slot, lastItem);
}

void ProtocolGame::parseBosstiaryInfo(const InputMessagePtr& msg)
{
    const uint16_t bosstiaryRaceLast = msg->getU16();
    std::vector<BosstiaryData> bossData;

    for (auto i = 0; i < bosstiaryRaceLast; ++i) {
        BosstiaryData boss;
        boss.raceId = msg->getU32();
        boss.category = msg->getU8();
        boss.kills = msg->getU32();
        msg->getU8();
        if (tibia::game::clientRevision() >= 1320) {
            boss.isTrackerActived = msg->getU8();
        }
        bossData.emplace_back(boss);
    }

    tibia::game::processBosstiaryInfo(bossData);
}

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

    if (tibia::game::cipFeature(tibia::client::GameWingsAurasEffectsShader)) {
        data.outfit.setWing(msg->getU16());
        data.outfit.setAura(msg->getU16());
        data.outfit.setEffect(msg->getU16());
        data.outfit.setShader(msg->getString());
    }

    const uint8_t playerDescCount = msg->getU8();
    readInspectionDescriptions(msg, data.playerDescriptions, playerDescCount);

    return data;
}

void ProtocolGame::parseCyclopediaItemDetail(const InputMessagePtr& msg)
{
    const uint8_t windowsType = msg->getU8(); // 1 = character, 0 = item
    const uint8_t inspectionType = msg->getU8();  // InspectObjectTypes
    const uint32_t creatureId = msg->getU32();

    if (std::cmp_equal(windowsType, 1)) {
        const uint8_t inventoryCount = msg->getU8();
        auto data = readCyclopediaCharacterInspectionData(*this, msg, inventoryCount, creatureId);
        // CipSoft InspectPlayerDialog hooks onParseCharacterInspection; Character Info
        // hooks onParseCyclopediaCharacterInspection — dual-emit so both paths receive.
        tibia::game::notify("onParseCharacterInspection", data);
        tibia::game::notify("onParseCyclopediaCharacterInspection", data);
        return;
    }

    ItemInspectionData data;
    data.inspectionType = inspectionType;
    data.creatureId = creatureId;
    msg->getU8(); // 0x01 constant
    data.name = msg->getString();
    fillInspectionAppearance(data, getInventorySlot(msg));

    const uint8_t imbuementCount = msg->getU8();
    data.imbuements.reserve(imbuementCount);
    for (auto i = 0; std::cmp_less(i, imbuementCount); ++i) {
        data.imbuements.push_back(msg->getU16());
    }

    const uint8_t descCount = msg->getU8();
    readInspectionDescriptions(msg, data.descriptions, descCount);
    tibia::game::processItemDetail(data);
}

void ProtocolGame::parseInspectionState(const InputMessagePtr& msg)
{
    const auto creatureId = msg->getU32();
    const auto state = msg->getU8();
    tibia::game::processInspectionState(creatureId, state);
}

void ProtocolGame::parseAddInventoryItem(const InputMessagePtr& msg)
{
    const uint8_t slot = msg->getU8();
    tibia::game::processInventoryChange(slot, getInventorySlot(msg));
}

void ProtocolGame::parseRemoveInventoryItem(const InputMessagePtr& msg)
{
    const uint8_t slot = msg->getU8();
    tibia::game::processInventoryChange(slot, {});
}

void ProtocolGame::parseOpenNpcTrade(const InputMessagePtr& msg)
{
    std::string npcName;
    if (tibia::game::cipFeature(tibia::client::GameNameOnNpcTrade)) {
        npcName = msg->getString();
    }

    uint16_t currencyId = 0;
    std::string currencyName;
    if (tibia::game::clientRevision() >= 1281) {
        currencyId = msg->getU16();
        currencyName = msg->getString();
    }
    tibia::game::notify("onOpenNpcTradeHeader", npcName, currencyId, currencyName);

    const uint16_t listCount = tibia::game::clientRevision() >= 900 ? msg->getU16() : msg->getU8();
    std::vector<std::tuple<tibia::container::TInventorySlot, std::string, uint32_t, uint32_t, uint32_t>> items;

    const auto normalizeNpcTradePrice = [](const uint32_t price) {
        return price == UINT32_MAX ? 0 : price;
    };

    for (auto i = 0; i < listCount; ++i) {
        const uint16_t itemId = msg->getU16();
        const uint8_t itemCount = msg->getU8();

        const auto& itemName = msg->getString();
        const uint32_t itemWeight = msg->getU32();
        const uint32_t itemBuyPrice = normalizeNpcTradePrice(msg->getU32());
        const uint32_t itemSellPrice = normalizeNpcTradePrice(msg->getU32());

        items.emplace_back(tibia::container::TInventorySlot::fromTypeId(itemId, itemCount), itemName, itemWeight,
                           itemBuyPrice, itemSellPrice);
    }

    tibia::game::processOpenNpcTrade(items);
}

void ProtocolGame::parsePlayerGoods(const InputMessagePtr& msg) const
{
    // 12.x NOTE: this u64 is parsed only, because TFS stil sends it, we use resource balance in this protocol
    uint64_t money = 0;
    if (tibia::game::clientRevision() >= 1281) {
        money = tibia::game::localPlayerTotalMoney();
    } else {
        if (tibia::game::cipFeature(tibia::client::GameDoublePlayerGoodsMoney))
            money = msg->getU64();
        else
            money = msg->getU32();
    }

    const uint16_t itemsListSize = tibia::game::clientRevision() >= 1334 ? msg->getU16() : msg->getU8();
    std::vector<std::pair<uint16_t, uint16_t>> goods;

    for (auto i = 0; i < itemsListSize; ++i) {
        const uint16_t itemId = msg->getU16();
        const uint16_t itemAmount = tibia::game::cipFeature(tibia::client::GameDoubleShopSellAmount) ? msg->getU16() : msg->getU8();

        goods.emplace_back(itemId, itemAmount);
    }

    tibia::game::processPlayerGoods(money, goods);
}

void ProtocolGame::parseCloseNpcTrade(const InputMessagePtr&) { tibia::game::processCloseNpcTrade(); }

void ProtocolGame::parseOwnTrade(const InputMessagePtr& msg)
{
    const auto& name = tibia::game::formatCreatureName(msg->getString());

    const uint8_t count = msg->getU8();
    std::vector<tibia::container::TInventorySlot> items;
    items.reserve(count);

    for (auto i = 0; i < count; i++)
        items.push_back(getInventorySlot(msg));

    tibia::game::processOwnTrade(name, items);
}

void ProtocolGame::parseCounterTrade(const InputMessagePtr& msg)
{
    const auto& name = tibia::game::formatCreatureName(msg->getString());

    const uint8_t count = msg->getU8();
    std::vector<tibia::container::TInventorySlot> items;
    items.reserve(count);

    for (auto i = 0; i < count; i++)
        items.push_back(getInventorySlot(msg));

    tibia::game::processCounterTrade(name, items);
}

void ProtocolGame::parseCloseTrade(const InputMessagePtr&) { tibia::game::processCloseTrade(); }

void ProtocolGame::parseWorldLight(const InputMessagePtr& msg)
{
    const auto intensity = msg->getU8();
    const auto color = msg->getU8();
    tibia::game::applyWorldLight(intensity, color);
}

void ProtocolGame::parseMagicEffect(const InputMessagePtr& msg)
{
    const auto& pos = getPosition(msg);
    if (tibia::game::getProtocolVersion() >= 1203) {
        uint8_t effectType = msg->getU8();
        while (effectType != tibia::client::MAGIC_EFFECTS_END_LOOP) {
            switch (effectType) {
                case tibia::client::MAGIC_EFFECTS_DELAY:
                case tibia::client::MAGIC_EFFECTS_DELTA: {
                    msg->getU8(); // ?
                    break;
                }

                case tibia::client::MAGIC_EFFECTS_CREATE_DISTANCEEFFECT:
                case tibia::client::MAGIC_EFFECTS_CREATE_DISTANCEEFFECT_REVERSED: {
                    const uint16_t shotId = tibia::game::cipFeature(tibia::client::GameEffectU16) ? msg->getU16() : msg->getU8();
                    const auto offsetX = static_cast<int8_t>(msg->getU8());
                    const auto offsetY = static_cast<int8_t>(msg->getU8());
                    const uint16_t effectSource = tibia::game::cipFeature(tibia::client::GameEffectSource) ? msg->getU8() : 0;
                    if (!tibia::game::isValidDatId(shotId, ThingCategoryMissile)) {
                        g_logger.traceError("invalid missile id {}", shotId);
                        break;
                    }

                    ThingType* type = tibia::game::rawThingType(shotId, ThingCategoryMissile);
                    const auto appearance = tibia::appearances::TAppearanceInstanceFactory::fromThingType(type);
                    auto* missile = dynamic_cast<tibia::appearances::TMissileEffectInstance*>(appearance.get());
                    if (!missile)
                        break;
                    missile->setSource(static_cast<tibia::client::MagicEffectSources>(effectSource));
                    if (effectType == tibia::client::MAGIC_EFFECTS_CREATE_DISTANCEEFFECT)
                        missile->setPath(pos, Position(pos.x + offsetX, pos.y + offsetY, pos.z));
                    else
                        missile->setPath(Position(pos.x + offsetX, pos.y + offsetY, pos.z), pos);
                    tibia::worldmap::TWorldMapStorage::ensureStorage()->addMissileAppearance(appearance);
                    break;
                }

                case tibia::client::MAGIC_EFFECTS_CREATE_EFFECT: {
                    const uint16_t effectId = tibia::game::cipFeature(tibia::client::GameEffectU16) ? msg->getU16() : msg->getU8();
                    const uint16_t effectSource = tibia::game::cipFeature(tibia::client::GameEffectSource) ? msg->getU8() : 0;
                    if (const auto appearance = makeServerEffectAppearance(effectId)) {
                        if (auto* effect = dynamic_cast<tibia::appearances::TEffectInstance*>(appearance.get()))
                            effect->setSource(static_cast<tibia::client::MagicEffectSources>(effectSource));
                        tibia::worldmap::TWorldMapStorage::ensureStorage()->addEffectAppearance(appearance, pos);
                    }
                    break;
                }

                case tibia::client::MAGIC_EFFECTS_CREATE_SOUND_MAIN_EFFECT: {
                    const uint8_t soundSource = msg->getU8(); // Source
                    const uint16_t soundId = msg->getU16(); // Sound ID
                    tibia::sound::playProtocolSoundMain(soundSource, soundId, pos);
                    break;
                }

                case tibia::client::MAGIC_EFFECTS_CREATE_SOUND_SECONDARY_EFFECT: {
                    const uint8_t soundEnum = msg->getU8(); // ENUM
                    const uint8_t soundSource = msg->getU8(); // Source
                    const uint16_t soundId = msg->getU16(); // Sound ID
                    tibia::sound::playProtocolSoundSecondary(soundEnum, soundSource, soundId, pos);
                    break;
                }
                default:
                    break;
            }

            effectType = msg->getU8();
        }

        return;
    }

    uint16_t effectId = tibia::game::cipFeature(tibia::client::GameMagicEffectU16) ? msg->getU16() : msg->getU8();

    if (tibia::game::clientRevision() <= 750) {
        effectId += 1; //hack to fix effects in earlier clients
    }

    if (const auto appearance = makeServerEffectAppearance(effectId))
        tibia::worldmap::TWorldMapStorage::ensureStorage()->addEffectAppearance(appearance, pos);
}

void ProtocolGame::parseRemoveMagicEffect(const InputMessagePtr& msg)
{
    const auto& pos = getPosition(msg);
    const uint16_t effectId = tibia::game::resolveEffectId(tibia::game::cipFeature(tibia::client::GameEffectU16) ? msg->getU16() : msg->getU8());

    if (!tibia::game::isValidDatId(effectId, ThingCategoryEffect)) {
        g_logger.warning("[ProtocolGame::parseRemoveMagicEffect] - Invalid effectId type {}", effectId);
        return;
    }

    const auto& tile = tibia::worldmap::fieldAt(pos);
    if (!tile)
        return;

    if (!tile->removeEffectByClientId(effectId))
        return;
}

void ProtocolGame::parseAnimatedText(const InputMessagePtr& msg)
{
    const auto& position = getPosition(msg);
    const uint8_t color = msg->getU8();
    const auto& text = msg->getString();

    tibia::worldmap::TWorldMapStorage::ensureStorage()->addAnimatedText(std::make_shared<AnimatedText>(text, color), position);
}

void ProtocolGame::parseAnthem(const InputMessagePtr& msg)
{
    // Opcode 0x85 is CIP SoundTrigger / overloaded on 13.x+:
    // - Ambient stream: type 0 + u16 ambient id
    // - Music (and login-music mute id=0): type 1 + u16 music id
    // - Anthem track: type 2 + u16 anthem id
    // GameEvents onSoundTrigger reaches tibia::sound::TSoundProtocolMessageHandler.
    const uint8_t type = msg->getU8();
    if (type <= 2) {
        const uint16_t soundId = msg->getU16();
        tibia::game::processSoundTrigger(type, soundId);
    }
}

void ProtocolGame::parseDistanceMissile(const InputMessagePtr& msg)
{
    const auto& fromPos = getPosition(msg);
    const auto& toPos = getPosition(msg);

    const uint16_t shotId = tibia::game::cipFeature(tibia::client::GameDistanceEffectU16) ? msg->getU16() : msg->getU8();
    if (!tibia::game::isValidDatId(shotId, ThingCategoryMissile)) {
        g_logger.traceError("invalid missile id {}", shotId);
        return;
    }

    ThingType* type = tibia::game::rawThingType(shotId, ThingCategoryMissile);
    const auto appearance = tibia::appearances::TAppearanceInstanceFactory::fromThingType(type);
    auto* missile = dynamic_cast<tibia::appearances::TMissileEffectInstance*>(appearance.get());
    if (!missile)
        return;
    missile->setPath(fromPos, toPos);
    tibia::worldmap::TWorldMapStorage::ensureStorage()->addMissileAppearance(appearance);
}

void ProtocolGame::parseForgeResult(const InputMessagePtr& msg)
{
    ForgeResultData forgeResult;
    forgeResult.actionType = msg->getU8();
    forgeResult.convergence = msg->getU8() == 1;
    forgeResult.success = msg->getU8() == 1;
    forgeResult.leftItemId = msg->getU16();
    forgeResult.leftTier = msg->getU8();
    forgeResult.rightItemId = msg->getU16();
    forgeResult.rightTier = msg->getU8();
    forgeResult.bonus = 0;
    forgeResult.coreCount = 0;

    if (forgeResult.actionType == 1) {
        msg->getU8(); // Bonus type always none for transfer
    } else {
        forgeResult.bonus = msg->getU8();// Roll fusion bonus
        // Core kept
        if (forgeResult.bonus == 2) {
            forgeResult.coreCount = msg->getU8();
        } else if (forgeResult.bonus >= 4 && forgeResult.bonus <= 8) {
            forgeResult.leftItemId = msg->getU16();
            forgeResult.leftTier = msg->getU8();
        }
    }

    tibia::game::notify("forgeResultData", forgeResult);
}

void ProtocolGame::parseItemClasses(const InputMessagePtr& msg)
{
    ForgeConfigData forgeConfig;

    const uint8_t classSize = msg->getU8();
    forgeConfig.classPrices.reserve(classSize);
    for (auto i = 0; i < classSize; ++i) {
        ForgeClassTierPrices classEntry;
        classEntry.classId = msg->getU8();

        // tiers
        const uint8_t tiersSize = msg->getU8();
        classEntry.tiers.reserve(tiersSize);
        for (auto j = 0; j < tiersSize; ++j) {
            ForgeTierPrice tierEntry;
            tierEntry.tier = msg->getU8();
            tierEntry.price = msg->getU64();
            classEntry.tiers.emplace_back(tierEntry);
        }

        forgeConfig.classPrices.emplace_back(classEntry);
    }

    const bool hasDynamicForgeVariables = tibia::game::cipFeature(tibia::client::GameDynamicForgeVariables);
    forgeConfig.hasConvergence = tibia::game::cipFeature(tibia::client::GameForgeConvergence);

    if (hasDynamicForgeVariables) {
        const uint8_t grades = msg->getU8();
        forgeConfig.fusionGrades.reserve(grades);
        for (auto i = 0; i < grades; ++i) {
            ForgeGradeData gradeEntry;
            gradeEntry.tier = msg->getU8();
            gradeEntry.exaltedCores = msg->getU8();
            forgeConfig.fusionGrades.emplace_back(gradeEntry);
        }

        if (forgeConfig.hasConvergence) {
            // Convergence fusion prices per tier
            const uint8_t totalConvergenceFusion = msg->getU8();
            forgeConfig.convergenceFusionPrices.reserve(totalConvergenceFusion);
            for (auto i = 0; i < totalConvergenceFusion; ++i) {
                ForgeTierPrice priceEntry;
                priceEntry.tier = msg->getU8();
                priceEntry.price = msg->getU64();
                forgeConfig.convergenceFusionPrices.emplace_back(priceEntry);
            }

            // Convergence transfer prices per tier
            const uint8_t totalConvergenceTransfer = msg->getU8();
            forgeConfig.convergenceTransferPrices.reserve(totalConvergenceTransfer);
            for (auto i = 0; i < totalConvergenceTransfer; ++i) {
                ForgeTierPrice priceEntry;
                priceEntry.tier = msg->getU8();
                priceEntry.price = msg->getU64();
                forgeConfig.convergenceTransferPrices.emplace_back(priceEntry);
            }
        }

        if (tibia::game::clientRevision() >= 1530) {
            // 15.25: 15-byte variables tail is one u8 dust level above 100.
            const uint8_t dustLevelRaw = msg->getU8();
            forgeConfig.maxDustLevel = 100 + dustLevelRaw;
            forgeConfig.maxDustCap = (std::min<uint16_t>(dustLevelRaw, 225) + 5) * 20;
            forgeConfig.dustPercent = 75;
            forgeConfig.dustToSliver = 20;
            forgeConfig.sliverToCore = 50;
            forgeConfig.dustPercentUpgrade = 30;
            forgeConfig.normalDustFusion = 100;
            forgeConfig.convergenceDustFusion = 130;
            forgeConfig.normalDustTransfer = 100;
            forgeConfig.convergenceDustTransfer = 160;
            forgeConfig.fusionChanceBase = 50;
            forgeConfig.fusionChanceImproved = 15;
            forgeConfig.fusionReduceTierLoss = 50;
            tibia::game::notify("forgeData", forgeConfig);
            return;
        }
        forgeConfig.dustPercent = msg->getU8();
        forgeConfig.dustToSliver = msg->getU8();
        forgeConfig.sliverToCore = msg->getU8();
        forgeConfig.dustPercentUpgrade = msg->getU8();
        if (tibia::game::clientRevision() >= 1316) {
            forgeConfig.maxDustLevel = msg->getU16();
            forgeConfig.maxDustCap = msg->getU16();
        } else {
            forgeConfig.maxDustLevel = msg->getU8();
            forgeConfig.maxDustCap = msg->getU8();
        }
        forgeConfig.normalDustFusion = msg->getU8();
        if (forgeConfig.hasConvergence) {
            forgeConfig.convergenceDustFusion = msg->getU8();
        }
        forgeConfig.normalDustTransfer = msg->getU8();
        if (forgeConfig.hasConvergence) {
            forgeConfig.convergenceDustTransfer = msg->getU8();
        }
        forgeConfig.fusionChanceBase = msg->getU8();
        forgeConfig.fusionChanceImproved = msg->getU8();
        forgeConfig.fusionReduceTierLoss = msg->getU8();


    } else {
        uint8_t totalForgeValues = 11;
        if (tibia::game::clientRevision() >= 1316) {
            totalForgeValues = 13;
        }

        if (forgeConfig.hasConvergence) {
            totalForgeValues = totalForgeValues + 2;
        }

        for (auto i = 0; i < totalForgeValues; ++i) {
            msg->getU8(); // Forge values
        }
    }

    tibia::game::notify("forgeData", forgeConfig);
}

void ProtocolGame::parseCreatureMark(const InputMessagePtr& msg)
{
    const uint32_t creatureId = msg->getU32();
    const uint8_t color = msg->getU8();

    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (!storage->creatureAt(creatureId)) {
            g_logger.traceDebug("ProtocolGame::parseCreatureMark: could not get creature with id {}", creatureId);
            return;
        }
    }
    tibia::creatures::protocolAddTimedSquare(creatureId, color);
}

void ProtocolGame::parseTrappers(const InputMessagePtr& msg)
{
    const uint8_t numTrappers = msg->getU8();

    if (numTrappers > 8) {
        g_logger.traceError("ProtocolGame::parseTrappers: too many trappers");
    }

    for (auto i = 0; i < numTrappers; ++i) {
        const uint32_t creatureId = msg->getU32();
        // Existence only — trapper mark not yet wired to Creature methods.
        if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
            if (!storage->creatureAt(creatureId)) {
                g_logger.traceDebug("ProtocolGame::parseTrappers: could not get creature with id {}", creatureId);
            }
        } else if (!tibia::game::creatureThingById(creatureId)) {
            g_logger.traceDebug("ProtocolGame::parseTrappers: could not get creature with id {}", creatureId);
        }

        //TODO: set creature as trapper
    }
}

void ProtocolGame::parseOpenForge(const InputMessagePtr& msg)
{
    ForgeOpenData data;

    const uint16_t fusionCount = msg->getU16();
    data.fusionItems.reserve(fusionCount);
    for (auto i = 0; i < fusionCount; ++i) {
        ForgeItemInfo item;
        msg->getU8(); // unknown count of friend items
        item.id = msg->getU16();
        item.tier = msg->getU8();
        item.count = msg->getU16();
        data.fusionItems.emplace_back(item);
    }

    const uint16_t convergenceFusionCount = msg->getU16();
    data.convergenceFusion.reserve(convergenceFusionCount);
    for (auto i = 0; i < convergenceFusionCount; ++i) {
        const uint8_t items = msg->getU8();
        std::vector<ForgeItemInfo> slotItems;
        slotItems.reserve(items);
        for (auto j = 0; j < items; ++j) {
            ForgeItemInfo item;
            item.id = msg->getU16();
            item.tier = msg->getU8();
            item.count = msg->getU16();
            slotItems.emplace_back(item);
        }
        data.convergenceFusion.emplace_back(slotItems);
    }

    const uint8_t transferTotalCount = msg->getU8();
    data.transfers.reserve(transferTotalCount);
    for (auto i = 0; i < transferTotalCount; ++i) {
        ForgeTransferData transfer;
        const uint16_t donorCount = msg->getU16();
        transfer.donors.reserve(donorCount);
        for (auto j = 0; j < donorCount; ++j) {
            ForgeItemInfo donor;
            donor.id = msg->getU16();
            donor.tier = msg->getU8();
            donor.count = msg->getU16();
            transfer.donors.emplace_back(donor);
        }
        const uint16_t receiverCount = msg->getU16();
        transfer.receivers.reserve(receiverCount);
        for (auto j = 0; j < receiverCount; ++j) {
            ForgeItemInfo receiver;
            receiver.id = msg->getU16();
            receiver.count = msg->getU16();
            receiver.tier = 0;
            transfer.receivers.emplace_back(receiver);
        }
        data.transfers.emplace_back(transfer);
    }

    const uint8_t convergenceTransferCount = msg->getU8();
    data.convergenceTransfers.reserve(convergenceTransferCount);
    for (auto i = 0; i < convergenceTransferCount; ++i) {
        ForgeTransferData transfer;
        const uint16_t donorCount = msg->getU16();
        transfer.donors.reserve(donorCount);
        for (auto j = 0; j < donorCount; ++j) {
            ForgeItemInfo donor;
            donor.id = msg->getU16();
            donor.tier = msg->getU8();
            donor.count = msg->getU16();
            transfer.donors.emplace_back(donor);
        }
        const uint16_t receiverCount = msg->getU16();
        transfer.receivers.reserve(receiverCount);
        for (auto j = 0; j < receiverCount; ++j) {
            ForgeItemInfo receiver;
            receiver.id = msg->getU16();
            receiver.count = msg->getU16();
            receiver.tier = 0;
            transfer.receivers.emplace_back(receiver);
        }
        data.convergenceTransfers.emplace_back(transfer);
    }
    data.dustLevel = (tibia::game::clientRevision() >= 1530) ? msg->getU8() : msg->getU16();

    return tibia::game::notify("onOpenForge", data);
}

void ProtocolGame::setCreatureVocation(const InputMessagePtr& msg, const uint32_t creatureId) const
{
    const uint8_t vocationId = msg->getU8();
    tibia::creatures::protocolSetVocation(creatureId, vocationId);
}

namespace {
    void readCreatureIconList(const InputMessagePtr& msg, std::vector<std::tuple<uint8_t, uint8_t, uint16_t>>& icons)
    {
        const uint8_t sizeIcons = msg->getU8();
        const bool extraIconByte = tibia::game::clientRevision() >= 1530;
        for (auto i = 0; i < sizeIcons; ++i) {
            const uint8_t icon = msg->getU8(); // icon.serialize
            const uint8_t category = msg->getU8(); // icon.category -- 0x00 = monster // 0x01 = player?
            const uint16_t count = msg->getU16(); // icon.count
            if (extraIconByte) {
                msg->getU8(); // 15.25 trailing per-icon byte
            }
            icons.emplace_back(icon, category, count);
        }
    }
}

void ProtocolGame::addCreatureIcon(const InputMessagePtr& msg, const uint32_t creatureId) const
{
    std::vector<std::tuple<uint8_t, uint8_t, uint16_t>> icons; // icon, category, count
    readCreatureIconList(msg, icons);

    tibia::creatures::protocolSetIcons(creatureId, icons);
}

void ProtocolGame::parseBrowseForgeHistory(const InputMessagePtr& msg)
{
    const uint16_t pageNumber = msg->getU16();
    const uint16_t lastPage = msg->getU16();
    const uint8_t historyCount = msg->getU8();
    std::vector<ForgeHistory> historyList;
    historyList.reserve(historyCount);
    for (auto i = 0; i < historyCount; ++i) {
        ForgeHistory history;
        history.createdAt = msg->getU32();
        history.actionType = msg->getU8();
        history.description = msg->getString();
        history.bonus = msg->getU8();
        historyList.emplace_back(history);
    }

    tibia::game::notify("onBrowseForgeHistory", pageNumber, lastPage, historyCount, historyList);
}

void ProtocolGame::parseCloseForgeWindow(const InputMessagePtr& /*msg*/)
{
    tibia::game::notify("onCloseForgeCloseWindows");
}

void ProtocolGame::parseCreatureData(const InputMessagePtr& msg)
{
    const uint32_t creatureId = msg->getU32();
    const uint8_t type = msg->getU8();

    // Session Creature Thing is LocalPlayerSessionThing-only; prefer storage for existence / party notifies.
    const auto& creature = tibia::game::creatureThingById(creatureId);
    if (!creature) {
        if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
            if (!storage->creatureAt(creatureId))
                g_logger.traceDebug("ProtocolGame::parseCreatureData: could not get creature with id {}", creatureId);
        }
    }

    switch (type) {
        case 0: // creature update
            getCreature(msg);
            break;
        case 11: { // creature mana percent
            const uint8_t manaPercent = msg->getU8();
            tibia::game::notify("onPartyMemberManaChange", creatureId, manaPercent);
            break;
        }
        case 12: { // creature show status (party list / minimap)
            const uint8_t showStatus = msg->getU8();
            tibia::game::notify("onPartyMemberShowStatusChange", creatureId, showStatus != 0);
            break;
        }
        case 13: // player vocation
            setCreatureVocation(msg, creatureId);
            break;
        case 14: // creature icons
            addCreatureIcon(msg, creatureId);
            break;
        case 15: {
            // 15.30 creature status flags / product Echo Raid glow (0x8B type 0x0F):
            // u8 count + count×u8 state (0=Echo Warden, 1=Empowered). Must consume or the
            // stream desyncs. Sent when the state is set and right after the creature is
            // first described to this client.
            const uint8_t n = msg->getU8();
            uint8_t state = 0;
            for (uint8_t i = 0; i < n; ++i) {
                state = msg->getU8();
            }
            tibia::creatures::protocolSetEchoRaidColor(creatureId, static_cast<int8_t>(state));
            break;
        }
        case 16: { // party health: percent + absolute (product; avoids overloading map 0x8C)
            const uint8_t healthPercent = msg->getU8();
            const uint32_t health = msg->getU32();
            const uint32_t maxHealth = msg->getU32();
            tibia::creatures::protocolSetHealthPercent(creatureId, healthPercent);
            tibia::game::notify("onPartyMemberHealthChange", creatureId, healthPercent, health, maxHealth);
            break;
        }
        case 17: { // party mana: percent + absolute (product)
            const uint8_t manaPercent = msg->getU8();
            const uint32_t mana = msg->getU32();
            const uint32_t maxMana = msg->getU32();
            tibia::game::notify("onPartyMemberManaChange", creatureId, manaPercent, mana, maxMana);
            break;
        }
        case 18: { // party vocation + level (product)
            const uint8_t vocationId = msg->getU8();
            const uint16_t level = msg->getU16();
            tibia::creatures::protocolSetVocation(creatureId, vocationId);
            tibia::game::notify("onPartyMemberInfoChange", creatureId, vocationId, level);
            break;
        }
        default:
            g_logger.warning("ProtocolGame::parseCreatureData: unhandled type {} for creature {} — stream may desync", type, creatureId);
            break;
    }
}

void ProtocolGame::parseCreatureHealth(const InputMessagePtr& msg)
{
    const uint32_t creatureId = msg->getU32();
    const uint8_t healthPercent = msg->getU8();

    // Existence check before protocolSet* (which getOrCreate) — storage is authority.
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (!storage->creatureAt(creatureId)) {
            g_logger.traceDebug("ProtocolGame::parseCreatureHealth: could not get creature with id {}", creatureId);
        }
    }
    tibia::creatures::protocolSetHealthPercent(creatureId, healthPercent);
}

void ProtocolGame::parseCreatureLight(const InputMessagePtr& msg)
{
    const uint32_t creatureId = msg->getU32();

    Light light;
    light.intensity = msg->getU8();
    light.color = msg->getU8();

    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (!storage->creatureAt(creatureId)) {
            g_logger.traceDebug("ProtocolGame::parseCreatureLight: could not get creature with id {}", creatureId);
        }
    }
    tibia::creatures::protocolSetLight(creatureId, light);
}

void ProtocolGame::parseCreatureOutfit(const InputMessagePtr& msg) const
{
    const uint32_t creatureId = msg->getU32();
    const Outfit outfit = getOutfit(msg);

    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (!storage->creatureAt(creatureId)) {
            g_logger.traceDebug("ProtocolGame::parseCreatureOutfit: could not get creature with id {}", creatureId);
        }
    }
    tibia::creatures::protocolSetOutfit(creatureId, outfit);
}

void ProtocolGame::parseCreatureSpeed(const InputMessagePtr& msg)
{
    // CIP classic: u32 creatureId; u16 speed (no baseSpeed — that lives on PlayerData).
    const uint32_t creatureId = msg->getU32();
    const uint16_t speed = msg->getU16();

    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (!storage->creatureAt(creatureId)) {
            g_logger.traceDebug("ProtocolGame::parseCreatureSpeed: could not get creature with id {}", creatureId);
        }
    }
    tibia::creatures::protocolSetSpeed(creatureId, speed, 0);
}

void ProtocolGame::parseCreatureSkulls(const InputMessagePtr& msg)
{
    const uint32_t creatureId = msg->getU32();
    const uint8_t skull = msg->getU8();

    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (!storage->creatureAt(creatureId)) {
            g_logger.traceDebug("ProtocolGame::parseCreatureSkulls: could not get creature with id {}", creatureId);
        }
    }
    tibia::creatures::protocolSetSkull(creatureId, skull);
}

void ProtocolGame::parseCreatureShields(const InputMessagePtr& msg)
{
    const uint32_t creatureId = msg->getU32();
    const uint8_t shield = msg->getU8();

    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (!storage->creatureAt(creatureId)) {
            g_logger.traceDebug("ProtocolGame::parseCreatureShields: could not get creature with id {}", creatureId);
        }
    }
    tibia::creatures::protocolSetShield(creatureId, shield);
}

void ProtocolGame::parseCreatureUnpass(const InputMessagePtr& msg)
{
    const uint32_t creatureId = msg->getU32();
    const bool unpass = static_cast<bool>(msg->getU8());

    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (!storage->creatureAt(creatureId)) {
            g_logger.traceDebug("ProtocolGame::parseCreatureUnpass: could not get creature with id {}", creatureId);
        }
    }
    tibia::creatures::protocolSetPassable(creatureId, !unpass);
}

void ProtocolGame::parseEditText(const InputMessagePtr& msg)
{
    const uint32_t id = msg->getU32();

    uint32_t itemId;
    if (tibia::game::clientRevision() >= 1010 || tibia::game::cipFeature(tibia::client::GameItemShader)) {
        itemId = getInventorySlot(msg).typeId;
    } else {
        itemId = msg->getU16();
    }

    const uint16_t maxLength = msg->getU16();

    const auto& text = msg->getString();
    const auto& writer = msg->getString();

    bool recentlyTraded = false;
    if (tibia::game::clientRevision() >= 1281) {
        recentlyTraded = msg->getU8() != 0;
    }

    std::string date;
    if (tibia::game::cipFeature(tibia::client::GameWritableDate)) {
        date = msg->getString();
    }

    tibia::game::processEditText(id, itemId, maxLength, text, writer, date, recentlyTraded);
}

void ProtocolGame::parseEditList(const InputMessagePtr& msg)
{
    const uint8_t doorId = msg->getU8();
    const uint32_t id = msg->getU32();
    const auto& text = msg->getString();

    tibia::game::processEditList(id, doorId, text);
}

void ProtocolGame::parsePremiumTrigger(const InputMessagePtr& msg)
{
    const uint8_t triggerCount = msg->getU8();
    std::vector<uint8_t> triggers;

    for (auto i = 0; i < triggerCount; ++i) {
        triggers.push_back(msg->getU8());
    }

    if (tibia::game::clientRevision() <= 1096) {
        msg->getU8(); // == 1; // something
    }

    tibia::game::notify("onPremiumTrigger", triggers);
}

void ProtocolGame::parsePlayerInfo(const InputMessagePtr& msg) const
{
    const bool premium = static_cast<bool>(msg->getU8()); // premium

    uint32_t premiumExpiration = 0;
    if (tibia::game::cipFeature(tibia::client::GamePremiumExpiration)) {
        premiumExpiration = msg->getU32(); // premium expiration used for premium advertisement
    }

    const uint8_t vocation = msg->getU8(); // vocation

    if (tibia::game::cipFeature(tibia::client::GamePrey)) {
        const bool reachedMain = msg->getU8() != 0; // (bool) prey / mainland unlocked
        tibia::game::localPlayerSetHasReachedMain(reachedMain);
        tibia::game::notify("onHasReachedMain", reachedMain);
    }

    const uint16_t spellCount = msg->getU16();
    std::vector<uint16_t> spells;

    for (auto i = 0; i < spellCount; ++i) {
        if (tibia::game::cipFeature(tibia::client::GameUshortSpell)) {
            spells.push_back(msg->getU16()); // spell id
        } else {
            spells.push_back(msg->getU8()); // spell id
        }
    }

    bool magicShield = false;
    if (tibia::game::clientRevision() >= 1281) {
        magicShield = msg->getU8() != 0;
    }

    tibia::game::localPlayerSetMagicShieldVocation(magicShield);
    tibia::game::localPlayerSetPremium(premium);
    tibia::game::localPlayerSetVocation(vocation);
    tibia::game::localPlayerSetSpells(spells);

    tibia::game::notify("onPremiumExpiration", premium, premiumExpiration);
    // After hasReachedMain / vocation are applied — product chrome gates on mainland.
    tibia::game::notify("onPlayerInfo", premium, static_cast<int>(vocation),
                                 tibia::game::localPlayerHasReachedMain());
}

void ProtocolGame::parsePlayerStats(const InputMessagePtr& msg) const
{
    const uint32_t health = tibia::game::cipFeature(tibia::client::GameDoubleHealth) ? msg->getU32() : msg->getU16();
    const uint32_t maxHealth = tibia::game::cipFeature(tibia::client::GameDoubleHealth) ? msg->getU32() : msg->getU16();
    uint32_t freeCapacity = tibia::game::cipFeature(tibia::client::GameDoubleFreeCapacity) ? msg->getU32() : msg->getU16();
    if (tibia::game::clientRevision() > 772) {
        // todo: We only know scaling started some time after 7.72; the 772 cutoff is a placeholder until we find the exact version.
        freeCapacity /= 100;
    }

    uint32_t totalCapacity = 0;
    if (tibia::game::clientRevision() < 1281 && tibia::game::cipFeature(tibia::client::GameTotalCapacity)) {
        totalCapacity = msg->getU32() / 100.f;
    }

    const uint64_t experience = tibia::game::cipFeature(tibia::client::GameDoubleExperience) ? msg->getU64() : msg->getU32();
    const uint16_t level = tibia::game::cipFeature(tibia::client::GameLevelU16) ? msg->getU16() : msg->getU8();
    const uint16_t levelPercent = tibia::game::cipFeature(tibia::client::GameLevelPercentU16) ? msg->getU16() : static_cast<uint16_t>(msg->getU8());

    if (tibia::game::cipFeature(tibia::client::GameExperienceBonus)) {
        if (tibia::game::clientRevision() <= 1096) {
            const double experienceBonus = msg->getDouble();
            tibia::game::localPlayerSetExperienceRate(tibia::client::EXP_BASE, experienceBonus * 100);
        } else {
            const uint16_t baseXpGain = msg->getU16();
            tibia::game::localPlayerSetExperienceRate(tibia::client::EXP_BASE, baseXpGain);
            if (tibia::game::clientRevision() < 1281) {
                const uint16_t voucherAddend = msg->getU16();
                tibia::game::localPlayerSetExperienceRate(tibia::client::EXP_VOUCHER, voucherAddend);
            }
            const uint16_t grindingAddend = msg->getU16();
            tibia::game::localPlayerSetExperienceRate(tibia::client::EXP_LOWLEVEL, grindingAddend);
            const uint16_t storeBoostAddend = msg->getU16();
            tibia::game::localPlayerSetExperienceRate(tibia::client::EXP_XPBOOST, storeBoostAddend);
            const uint16_t huntingBoostFactor = msg->getU16();
            tibia::game::localPlayerSetExperienceRate(tibia::client::EXP_STANINAMULTIPLIER, huntingBoostFactor);
        }
    }

    const uint32_t mana = tibia::game::cipFeature(tibia::client::GameDoubleHealth) ? msg->getU32() : msg->getU16();
    const uint32_t maxMana = tibia::game::cipFeature(tibia::client::GameDoubleHealth) ? msg->getU32() : msg->getU16();
    uint32_t manaShield = 0;
    uint32_t maxManaShield = 0;

    if (tibia::game::clientRevision() < 1281) {
        const uint8_t magicLevel = msg->getU8();
        const uint8_t baseMagicLevel = tibia::game::cipFeature(tibia::client::GameSkillsBase) ? msg->getU8() : magicLevel;
        const uint8_t magicLevelPercent = msg->getU8();

        tibia::game::localPlayerSetMagicLevelDetail(baseMagicLevel, magicLevelPercent * 100);
        tibia::game::localPlayerSetMagicLevel(magicLevel, magicLevelPercent);
        tibia::game::localPlayerSetBaseMagicLevel(baseMagicLevel);
    }

    const uint8_t soul = tibia::game::cipFeature(tibia::client::GameSoul) ? msg->getU8() : 0;
    const uint16_t stamina = tibia::game::cipFeature(tibia::client::GamePlayerStamina) ? msg->getU16() : 0;
    const uint16_t baseSpeed = tibia::game::cipFeature(tibia::client::GameSkillsBase) ? msg->getU16() : 0;
    const uint16_t regeneration = tibia::game::cipFeature(tibia::client::GamePlayerRegenerationTime) ? msg->getU16() : 0;
    const uint16_t training = tibia::game::cipFeature(tibia::client::GameOfflineTrainingTime) ? msg->getU16() : 0;

    if (tibia::game::clientRevision() >= 1097) {
        tibia::game::localPlayerSetStoreExpBoostTime(msg->getU16()); // xp boost time (seconds)
        tibia::game::localPlayerSetStoreExpBoostPurchasable(msg->getU8() != 0); // enables exp boost in the store
    }

    if (tibia::game::clientRevision() >= 1281) {
        if (tibia::game::cipFeature(tibia::client::GameDoubleHealth)) {
            manaShield = msg->getU32(); // remaining mana shield
            maxManaShield = msg->getU32(); // total mana shield
        } else {
            manaShield = msg->getU16(); // remaining mana shield
            maxManaShield = msg->getU16(); // total mana shield
        }
    }

    tibia::game::localPlayerSetHealth(health, maxHealth);
    tibia::game::localPlayerSetFreeCapacity(freeCapacity);
    if (tibia::game::clientRevision() < 1281 && tibia::game::cipFeature(tibia::client::GameTotalCapacity)) {
        tibia::game::localPlayerSetTotalCapacity(totalCapacity);
    }
    tibia::game::localPlayerSetExperience(experience);
    tibia::game::localPlayerSetLevel(level, levelPercent);
    tibia::game::localPlayerSetMana(mana, maxMana);
    if (tibia::game::clientRevision() >= 1281)
        tibia::game::localPlayerSetManaShield(manaShield, maxManaShield);
    else
        tibia::game::localPlayerSetManaShield(0, 0);
    tibia::game::localPlayerSetStamina(stamina);
    tibia::game::localPlayerSetSoul(soul);
    tibia::game::localPlayerSetBaseSpeed(baseSpeed);
    tibia::game::localPlayerSetRegenerationTime(regeneration);
    tibia::game::localPlayerSetOfflineTrainingTime(training);
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance())
        storage->syncLocalPlayerStatsFromMap();
}

void ProtocolGame::parsePlayerSkills(const InputMessagePtr& msg) const
{
    if (tibia::game::clientRevision() >= 1281) {
        // magic level
        const uint16_t magicLevel = msg->getU16();
        const uint16_t baseMagicLevel = msg->getU16();
        const uint16_t loyaltyMagicLevel = msg->getU16();
        const uint16_t percentHundredths = msg->getU16();

        tibia::game::localPlayerSetMagicLevelDetail(loyaltyMagicLevel, percentHundredths);
        tibia::game::localPlayerSetMagicLevel(magicLevel, percentHundredths / 100);
        tibia::game::localPlayerSetBaseMagicLevel(baseMagicLevel);
    }

    for (int_fast32_t skill = tibia::client::Fist; skill <= tibia::client::Fishing; ++skill) {
        const uint16_t level = tibia::game::cipFeature(tibia::client::GameDoubleSkills) ? msg->getU16() : msg->getU8();

        uint16_t baseLevel;
        if (tibia::game::cipFeature(tibia::client::GameSkillsBase)) {
            baseLevel = tibia::game::cipFeature(tibia::client::GameBaseSkillU16) ? msg->getU16() : msg->getU8();
        } else {
            baseLevel = level;
        }

        uint16_t levelPercent = 0;
        uint16_t loyaltyLevel = baseLevel;
        uint16_t percentHundredths = 0;

        if (tibia::game::clientRevision() >= 1281) {
            loyaltyLevel = msg->getU16();
            percentHundredths = msg->getU16();
            levelPercent = percentHundredths / 100;
        } else {
            levelPercent = msg->getU8();
            percentHundredths = levelPercent * 100;
        }

        tibia::game::localPlayerSetSkillDetail(static_cast<tibia::client::Skill>(skill), loyaltyLevel, percentHundredths);
        tibia::game::localPlayerSetSkill(static_cast<tibia::client::Skill>(skill), level, levelPercent);
        tibia::game::localPlayerSetBaseSkill(static_cast<tibia::client::Skill>(skill), baseLevel);
    }

    if (tibia::game::cipFeature(tibia::client::GameAdditionalSkills)) {
        // Critical, Life Leech, Mana Leech
        for (int_fast32_t skill = tibia::client::CriticalChance; skill <= tibia::client::ManaLeechAmount; ++skill) {
            if (!tibia::game::cipFeature(tibia::client::GameLeechAmount)) {
                if (skill == tibia::client::LifeLeechAmount || skill == tibia::client::ManaLeechAmount) {
                    continue;
                }
            }

            const uint16_t level = msg->getU16();
            const uint16_t baseLevel = msg->getU16();
            tibia::game::localPlayerSetSkill(static_cast<tibia::client::Skill>(skill), level, 0);
            tibia::game::localPlayerSetBaseSkill(static_cast<tibia::client::Skill>(skill), baseLevel);
        }
    }

    if (tibia::game::cipFeature(tibia::client::GameConcotions)) {
        msg->getU8();
    }

    if (tibia::game::cipFeature(tibia::client::GameForgeSkillStats)) {
        const uint8_t lastSkill = tibia::game::clientRevision() >= 1332 ? tibia::client::LastSkill : tibia::client::Momentum + 1;
        for (int_fast32_t skill = tibia::client::Fatal; skill < lastSkill; ++skill) {
            const uint16_t level = msg->getU16();
            const uint16_t baseLevel = msg->getU16();
            tibia::game::localPlayerSetSkill(static_cast<tibia::client::Skill>(skill), level, 0);
            tibia::game::localPlayerSetBaseSkill(static_cast<tibia::client::Skill>(skill), baseLevel);
        }

        // bonus cap
        const uint32_t capacity = msg->getU32(); // base + bonus capacity
        msg->getU32(); // base capacity

        tibia::game::localPlayerSetTotalCapacity(capacity);
    }

    if (tibia::game::cipFeature(tibia::client::GameCharacterSkillStats)) {
        //msg->getU8; // GameConcotions ??
        const uint32_t capacity = msg->getU32() / 100; // base + bonus capacity
        const uint32_t baseCapacity = msg->getU32() / 100; // base capacity
        tibia::game::localPlayerSetBaseCapacity(baseCapacity);
        tibia::game::localPlayerSetTotalCapacity(capacity);
        // Flat Damage and Healing Total
        const uint16_t flatBonus = msg->getU16();
        tibia::game::localPlayerSetFlatDamageHealing(flatBonus);

        // Weapon attack and element info
        const uint16_t attackValue = msg->getU16();
        const uint8_t attackElement = msg->getU8();
        tibia::game::localPlayerSetAttackInfo(attackValue, attackElement);

        // Converted Damage
        const double convertedDamage = msg->getDouble();
        const uint8_t convertedElement = msg->getU8();
        tibia::game::localPlayerSetConvertedDamage(convertedDamage, convertedElement);

        // Imbuements
        const double lifeLeech = msg->getDouble();
        const double manaLeech = msg->getDouble();
        const double critChance = msg->getDouble();
        const double critDamage = msg->getDouble();
        const double onslaught = msg->getDouble();
        tibia::game::localPlayerSetImbuements(lifeLeech, manaLeech, critChance, critDamage, onslaught);

        // Defense info
        const uint16_t defense = msg->getU16();
        const uint16_t armor = msg->getU16();
        uint16_t mantra = 0;
        if (tibia::game::cipFeature(tibia::client::GameVocationMonk)) {
            mantra = msg->getU16(); // monk voc
        }
        const double mitigation = msg->getDouble();
        const double dodge = msg->getDouble();
        const uint16_t damageReflection = msg->getU16();
        tibia::game::localPlayerSetDefenseInfo(defense, armor, mantra, mitigation, dodge, damageReflection);

        // Combat absorb values
        const uint8_t combatsCount = msg->getU8();
        std::map<uint8_t, double> absorbValues;
        for (int i = 0; i < combatsCount; i++) {
            const uint8_t combatType = msg->getU8();
            const double value = msg->getDouble();
            absorbValues[combatType] = value;
        }
        tibia::game::localPlayerSetCombatAbsorbValues(absorbValues);

        // Forge bonuses
        const double momentum = msg->getDouble();
        const double transcendence = msg->getDouble();
        const double amplification = msg->getDouble();
        tibia::game::localPlayerSetForgeBonuses(momentum, transcendence, amplification);
    }
}

void ProtocolGame::parsePlayerState(const InputMessagePtr& msg) const
{
    uint64_t states;
    uint8_t rottenBloodState = 0;
    if (tibia::game::clientRevision() >= 1281) {
        states = tibia::game::clientRevision() >= 1405 ? msg->getU64() : msg->getU32();
        if (tibia::game::cipFeature(tibia::client::GamePlayerStateCounter)) {
            rottenBloodState = msg->getU8();
        }
    } else {
        states = tibia::game::cipFeature(tibia::client::GamePlayerStateU16) ? msg->getU16() : msg->getU8();
    }

    tibia::game::localPlayerSetStatesAndRottenBlood(states, rottenBloodState);
}

void ProtocolGame::parsePlayerCancelAttack(const InputMessagePtr& msg)
{
    const uint32_t seq = tibia::game::cipFeature(tibia::client::GameAttackSeq) ? msg->getU32() : 0;
    if (tibia::game::clientRevision() >= 1530)
        msg->getU32(); // 15.25 second u32
    tibia::game::processAttackCancel(seq);
}

void ProtocolGame::parsePlayerModes(const InputMessagePtr& msg)
{
    // GameTacticsWithoutFightMode only drops fightMode from the client->server packet:
    // CIP 15.25+ servers still send fight, chase, secure (and pvp) in 0xA7.
    const auto fightMode = static_cast<tibia::client::FightModes>(msg->getU8());
    const auto chaseMode = static_cast<tibia::client::ChaseModes>(msg->getU8());
    const bool safeMode = static_cast<bool>(msg->getU8());
    const auto pvpMode = static_cast<tibia::client::PVPModes>(tibia::game::cipFeature(tibia::client::GamePVPMode) ? msg->getU8() : 0);

    tibia::game::processPlayerModes(fightMode, chaseMode, safeMode, pvpMode);
}

void ProtocolGame::parseSpellCooldown(const InputMessagePtr& msg)
{
    const uint16_t spellId = tibia::game::cipFeature(tibia::client::GameUshortSpell) ? msg->getU16() : msg->getU8();
    const uint32_t delay = msg->getU32();

    if (auto* storage = tibia::game::TCooldownStorage::instance())
        storage->applySpellCooldown(int(spellId), delay);
    else
        tibia::game::notify("onSpellCooldown", spellId, delay);
}

void ProtocolGame::parseSpellGroupCooldown(const InputMessagePtr& msg)
{
    const uint8_t groupId = msg->getU8();
    const uint32_t delay = msg->getU32();

    if (auto* storage = tibia::game::TCooldownStorage::instance())
        storage->applySpellGroupCooldown(int(groupId), delay);
    else
        tibia::game::notify("onSpellGroupCooldown", groupId, delay);
}

void ProtocolGame::parseMultiUseCooldown(const InputMessagePtr& msg)
{
    const uint32_t delay = msg->getU32();
    if (auto* storage = tibia::game::TCooldownStorage::instance())
        storage->applyMultiUseCooldown(delay);
    else
        tibia::game::notify("onMultiUseCooldown", delay);
}

void ProtocolGame::parseTalk(const InputMessagePtr& msg)
{
    uint32_t statement = 0;
    if (tibia::game::cipFeature(tibia::client::GameMessageStatements)) {
        statement = msg->getU32(); // channel statement guid
    }

    const auto& name = tibia::game::formatCreatureName(msg->getString());

    if (statement > 0 && tibia::game::clientRevision() >= 1281) {
        msg->getU8(); // suffix
    }

    const uint16_t level = tibia::game::cipFeature(tibia::client::GameMessageLevel) ? msg->getU16() : 0;

    auto messageByte = msg->getU8();
    const tibia::client::MessageMode mode = Proto::translateMessageModeFromServer(messageByte);
    uint16_t channelId = 0;
    Position pos;

    switch (mode) {
        case tibia::client::MessagePotion:
        case tibia::client::MessageSay:
        case tibia::client::MessageWhisper:
        case tibia::client::MessageYell:
        case tibia::client::MessageMonsterSay:
        case tibia::client::MessageMonsterYell:
        case tibia::client::MessageNpcTo:
        case tibia::client::MessageBarkLow:
        case tibia::client::MessageBarkLoud:
        case tibia::client::MessageSpell:
        case tibia::client::MessageNpcFromStartBlock:
            pos = getPosition(msg);
            break;
        case tibia::client::MessageChannel:
        case tibia::client::MessageChannelManagement:
        case tibia::client::MessageChannelHighlight:
        case tibia::client::MessageGamemasterChannel:
            channelId = msg->getU16();
            break;
        case tibia::client::MessageNpcFrom:
        case tibia::client::MessagePrivateTo:
        case tibia::client::MessagePrivateFrom:
        case tibia::client::MessageGamemasterBroadcast:
        case tibia::client::MessageGamemasterPrivateFrom:
            break;
        default:
            throw Exception("ProtocolGame::parseTalk: unknown message mode {}", messageByte);
    }

    const auto& text = msg->getString();

    tibia::game::processTalk(name, level, mode, text, channelId, pos, statement);
}

void ProtocolGame::parseChannelList(const InputMessagePtr& msg)
{
    const uint8_t channelListSize = msg->getU8();
    std::vector<std::tuple<uint16_t, std::string>> channelList;

    for (auto i = 0; i < channelListSize; ++i) {
        const uint16_t channelId = msg->getU16();
        const auto& channelName = msg->getString();
        channelList.emplace_back(channelId, channelName);
    }

    tibia::game::processChannelList(channelList);
}

void ProtocolGame::parseOpenChannel(const InputMessagePtr& msg)
{
    const uint16_t channelId = msg->getU16();
    const auto& channelName = msg->getString();

    if (tibia::game::cipFeature(tibia::client::GameChannelPlayerList)) {
        const uint16_t joinedPlayers = msg->getU16();
        for (auto i = 0; i < joinedPlayers; ++i) {
            tibia::game::formatCreatureName(msg->getString()); // player name
        }

        const uint16_t invitedPlayers = msg->getU16();
        for (auto i = 0; i < invitedPlayers; ++i) {
            tibia::game::formatCreatureName(msg->getString()); // player name
        }
    }

    tibia::game::processOpenChannel(channelId, channelName);
}

void ProtocolGame::parseOpenPrivateChannel(const InputMessagePtr& msg)
{
    const auto& name = tibia::game::formatCreatureName(msg->getString());
    tibia::game::processOpenPrivateChannel(name);
}

void ProtocolGame::parseOpenOwnPrivateChannel(const InputMessagePtr& msg)
{
    const uint16_t channelId = msg->getU16();
    const auto& channelName = msg->getString();
    if (tibia::game::cipFeature(tibia::client::GameChannelPlayerList)) {
        const uint16_t joinedPlayers = msg->getU16();
        for (auto i = 0; i < joinedPlayers; ++i) {
            tibia::game::formatCreatureName(msg->getString()); // player name
        }

        const uint16_t invitedPlayers = msg->getU16();
        for (auto i = 0; i < invitedPlayers; ++i) {
            tibia::game::formatCreatureName(msg->getString()); // player name
        }
    }

    tibia::game::processOpenOwnPrivateChannel(channelId, channelName);
}

void ProtocolGame::parseCloseChannel(const InputMessagePtr& msg)
{
    const uint16_t channelId = msg->getU16();
    tibia::game::processCloseChannel(channelId);
}

void ProtocolGame::parseEditGuildMessage(const InputMessagePtr& msg)
{
    tibia::game::notify("onEditGuildMessage", msg->getString());
}

void ProtocolGame::parseOutfitMemorial(const InputMessagePtr& msg)
{
    // Crystal outfit_memorial.lua / CIP S 0xB0:
    // 3× u32 golden costs, 3× (u16 count + strings) golden owners,
    // 3× (u16 silver, u16 gold) token prices, 3× (u16 count + strings) token owners.
    // Owner list order matches storage value tiers: [1]=base, [2]=one addon, [3]=full.
    OutfitMemorialData data;
    const auto readOwners = [&msg](std::vector<std::string>& out) {
        const uint16_t count = msg->getU16();
        out.reserve(count);
        for (uint16_t i = 0; i < count; ++i)
            out.push_back(msg->getString());
    };

    data.baseGoldenOutfitCost = msg->getU32();
    data.oneAddonGoldenOutfitCost = msg->getU32();
    data.fullGoldenOutfitCost = msg->getU32();
    readOwners(data.baseGoldenOutfitOwners);
    readOwners(data.oneAddonGoldenOutfitOwners);
    readOwners(data.fullGoldenOutfitOwners);

    data.baseTokenOutfitSilverToken = msg->getU16();
    data.baseTokenOutfitGoldToken = msg->getU16();
    data.oneAddonTokenOutfitSilverToken = msg->getU16();
    data.oneAddonTokenOutfitGoldToken = msg->getU16();
    data.fullTokenOutfitSilverToken = msg->getU16();
    data.fullTokenOutfitGoldToken = msg->getU16();
    readOwners(data.baseTokenOutfitOwners);
    readOwners(data.oneAddonTokenOutfitOwners);
    readOwners(data.fullTokenOutfitOwners);

    tibia::game::processOutfitMemorial(data);
}

void ProtocolGame::parseTextMessage(const InputMessagePtr& msg)
{
    const uint8_t code = msg->getU8();
    const tibia::client::MessageMode mode = Proto::translateMessageModeFromServer(code);
    std::string text;
    int channelId = -1;

    g_logger.debug("[ProtocolGame::parseTextMessage] code: {}, mode: {}", code, std::to_string(mode));

    switch (mode) {
        case tibia::client::MessageChannelManagement:
            channelId = msg->getU16();
            text = msg->getString();
            break;
        case tibia::client::MessageGuild:
        case tibia::client::MessagePartyManagement:
        case tibia::client::MessageParty:
        {
            channelId = msg->getU16();
            text = msg->getString();
            break;
        }
        case tibia::client::MessageDamageDealed:
        case tibia::client::MessageDamageReceived:
        case tibia::client::MessageDamageOthers:
        {
            const auto& pos = getPosition(msg);
            std::array<uint32_t, 2> value{};
            std::array<uint8_t, 2> color{};

            // physical damage
            value[0] = msg->getU32();
            color[0] = msg->getU8();

            // magic damage
            value[1] = msg->getU32();
            color[1] = msg->getU8();
            text = msg->getString();

            for (auto j = 0; j < 2; j++) {
                if (value[j] == 0) {
                    continue;
                }

                tibia::worldmap::TWorldMapStorage::ensureStorage()->addAnimatedText(std::make_shared<AnimatedText>(std::to_string(value[j]), color[j]), pos);
                tibia::game::processNumericalEffect(mode, pos, value[j], color[j]);
            }
            break;
        }
        case tibia::client::MessageHeal:
        case tibia::client::MessageMana:
        case tibia::client::MessageHealOthers:
        {
            const auto& pos = getPosition(msg);
            const uint32_t value = msg->getU32();
            const uint8_t color = msg->getU8();
            text = msg->getString();

            tibia::worldmap::TWorldMapStorage::ensureStorage()->addAnimatedText(std::make_shared<AnimatedText>(std::to_string(value), color), pos);
            if (value != 0)
                tibia::game::processNumericalEffect(mode, pos, value, color);
            break;
        }
        case tibia::client::MessageExp:
        case tibia::client::MessageExpOthers:
        {
            const auto& pos = getPosition(msg);
            const uint64_t value = tibia::game::clientRevision() >= 1332 ? msg->getU64() : msg->getU32();
            const uint8_t color = msg->getU8();
            text = msg->getString();

            tibia::worldmap::TWorldMapStorage::ensureStorage()->addAnimatedText(std::make_shared<AnimatedText>(std::to_string(value), color), pos);
            if (value != 0)
                tibia::game::processNumericalEffect(mode, pos, value, color);
            break;
        }
        case tibia::client::MessageInvalid:
            throw Exception("ProtocolGame::parseTextMessage: unknown message mode {}", code);
        default:
            break;
    }

    if (text.empty()) {
        text = msg->getString();
    }

    if (channelId >= 0)
        tibia::game::notify("onChannelTextMessage", mode, static_cast<uint16_t>(channelId), text);
    tibia::game::processTextMessage(mode, text);
}

void ProtocolGame::parseCancelWalk(const InputMessagePtr& msg)
{
    const auto direction = static_cast<tibia::client::Direction>(msg->getU8());
    tibia::game::processWalkCancel(direction);
}

void ProtocolGame::parseWalkWait(const InputMessagePtr& msg) const
{
    const uint16_t millis = msg->getU16();
    m_localPlayer->lockWalk(millis);
}

void ProtocolGame::parseFloorChangeUp(const InputMessagePtr& msg)
{
    const tibia::game::MapAwareRange range = tibia::game::awareRange();

    auto pos = tibia::game::cipFeature(tibia::client::GameMapMovePosition) ? getPosition(msg) : peeledMapCentral();
    --pos.z;

    auto newPos = pos;
    ++newPos.x;
    ++newPos.y;
    // CipSoft: drop TCreatureMovementTween before floor tiles are placed so
    // onAppear during setFloorDescription cannot start a tween that is then aborted.
    if (m_localPlayer)
        m_localPlayer->handleTeleport();

    int skip = 0;
    if (pos.z == tibia::config::mapSeaFloor()) {
        for (auto i = tibia::config::mapSeaFloor() - tibia::config::mapAwareUndergroundFloorRange(); i >= 0; --i) {
            skip = setFloorDescription(msg, pos.x - range.left, pos.y - range.top, i, range.horizontal(), range.vertical(), 8 - i, skip);
        }
    } else if (pos.z > tibia::config::mapSeaFloor()) {
        setFloorDescription(msg, pos.x - range.left, pos.y - range.top, pos.z - tibia::config::mapAwareUndergroundFloorRange(), range.horizontal(), range.vertical(), 3, skip);
    }

    tibia::game::setCentralPosition({ newPos.x, newPos.y, newPos.z });
    tibia::worldmap::notifyWindowChanged(true);
    tibia::game::notify("onTeleport", m_localPlayer, newPos, pos);
}

void ProtocolGame::parseFloorChangeDown(const InputMessagePtr& msg)
{
    const tibia::game::MapAwareRange range = tibia::game::awareRange();

    auto pos = tibia::game::cipFeature(tibia::client::GameMapMovePosition) ? getPosition(msg) : peeledMapCentral();
    ++pos.z;

    auto newPos = pos;
    --newPos.x;
    --newPos.y;
    if (m_localPlayer)
        m_localPlayer->handleTeleport();

    int skip = 0;
    if (pos.z == tibia::config::mapUndergroundFloorRange()) {
        int j;
        int i;
        for (i = pos.z, j = -1; i <= pos.z + tibia::config::mapAwareUndergroundFloorRange(); ++i, --j) {
            skip = setFloorDescription(msg, pos.x - range.left, pos.y - range.top, i, range.horizontal(), range.vertical(), j, skip);
        }
    } else if (pos.z > tibia::config::mapUndergroundFloorRange() && pos.z < tibia::config::mapMaxZ() - 1) {
        setFloorDescription(msg, pos.x - range.left, pos.y - range.top, pos.z + tibia::config::mapAwareUndergroundFloorRange(), range.horizontal(), range.vertical(), -3, skip);
    }

    tibia::game::setCentralPosition({ newPos.x, newPos.y, newPos.z });
    tibia::worldmap::notifyWindowChanged(true);
    tibia::game::notify("onTeleport", m_localPlayer, newPos, pos);
}

void ProtocolGame::parseOpenOutfitWindow(const InputMessagePtr& msg) const
{
    auto currentOutfit = getOutfit(msg);

    // mount color bytes are required here regardless of having one
    if (tibia::game::clientRevision() >= 1281) {
        if (currentOutfit.getMount() == 0) {
            const uint8_t head = msg->getU8();
            const uint8_t body = msg->getU8();
            const uint8_t legs = msg->getU8();
            const uint8_t feet = msg->getU8();
            currentOutfit.setMountColors(head, body, legs, feet);
        }

        currentOutfit.setFamiliar(msg->getU16()); // current familiar looktype
    }

    std::vector<std::tuple<uint16_t, std::string, uint8_t, uint8_t>> outfitList;
    // store offer id per list entry (0 unless the mode is 1)
    std::vector<uint32_t> outfitOffers;
    std::vector<uint32_t> mountOffers;
    std::vector<uint32_t> familiarOffers;

    if (tibia::game::cipFeature(tibia::client::GameNewOutfitProtocol)) {
        const uint16_t outfitCount = tibia::game::clientRevision() >= 1281 ? msg->getU16() : msg->getU8();
        for (auto i = 0; i < outfitCount; ++i) {
            const uint16_t outfitId = msg->getU16();
            const auto& outfitName = msg->getString();
            const uint8_t outfitAddons = msg->getU8();
            uint8_t outfitMode = 0;
            uint32_t offerId = 0;
            if (tibia::game::clientRevision() >= 1281) {
                outfitMode = msg->getU8(); // mode: 0x00 - available, 0x01 store (requires U32 store offerId), 0x02 golden outfit tooltip (hardcoded)
                if (outfitMode == 1) {
                    offerId = msg->getU32();
                }
            }
            outfitOffers.emplace_back(offerId);

            outfitList.emplace_back(outfitId, outfitName, outfitAddons, outfitMode);
        }
    } else {
        uint16_t outfitStart;
        uint16_t outfitEnd;
        if (tibia::game::cipFeature(tibia::client::GameLooktypeU16)) {
            outfitStart = msg->getU16();
            outfitEnd = msg->getU16();
        } else {
            outfitStart = msg->getU8();
            outfitEnd = msg->getU8();
        }

        for (auto i = outfitStart; i <= outfitEnd; ++i) {
            outfitList.emplace_back(i, "", 0, 0);
        }
    }

    std::vector<std::tuple<uint16_t, std::string, uint8_t>> mountList;

    if (tibia::game::cipFeature(tibia::client::GamePlayerMounts)) {
        const uint16_t mountCount = tibia::game::clientRevision() >= 1281 ? msg->getU16() : msg->getU8();
        for (auto i = 0; i < mountCount; ++i) {
            const uint16_t mountId = msg->getU16(); // mount type
            const auto& mountName = msg->getString(); // mount name
            uint8_t mountMode = 0;
            uint32_t offerId = 0;
            if (tibia::game::clientRevision() >= 1281) {
                mountMode = msg->getU8(); // mode: 0x00 - available, 0x01 store (requires U32 store offerId)
                if (mountMode == 1) {
                    offerId = msg->getU32();
                }
            }

            mountList.emplace_back(mountId, mountName, mountMode);
            mountOffers.emplace_back(offerId);
        }
    }

    std::vector<std::tuple<uint16_t, std::string> > familiarList;
    if (tibia::game::cipFeature(tibia::client::GamePlayerFamiliars)) {
        const uint16_t familiarCount = msg->getU16();
        for (auto i = 0; i < familiarCount; ++i) {
            const uint16_t familiarLookType = msg->getU16(); // familiar lookType
            const auto& familiarName = msg->getString(); // familiar name
            const uint8_t familiarMode = msg->getU8(); // 0x00 // mode: 0x00 - available, 0x01 store (requires U32 store offerId)
            uint32_t offerId = 0;
            if (familiarMode == 1) {
                offerId = msg->getU32();
            }
            familiarList.emplace_back(familiarLookType, familiarName);
            familiarOffers.emplace_back(offerId);
        }
    }

    if (tibia::game::clientRevision() >= 1281) {
        // window mode: 0 own outfit (random mount follows), 3 hireling (u32 id),
        // 4/5 male/female outfit pairs, 5 also the podium (Tibia)
        const uint8_t tryOutfitMode = msg->getU8();
        const bool mounted = msg->getU8() != 0;
        bool randomMount = false;
        std::vector<std::tuple<uint16_t, uint16_t>> genderPairs;
        Position podiumPos;
        uint16_t podiumItemId = 0;
        uint8_t podiumStackpos = 0;
        uint8_t podiumVisible = 1;
        uint8_t podiumShowOutfit = 1;
        uint8_t podiumDirection = 2;
        if (tryOutfitMode == 0) {
            randomMount = msg->getU8() != 0;
        } else if (tryOutfitMode == 3) {
            msg->getU32();
        } else if (tryOutfitMode == 4 || tryOutfitMode == 5) {
            const uint16_t pairCount = msg->getU16();
            for (auto i = 0; i < pairCount; ++i) {
                const uint16_t male = msg->getU16();
                const uint16_t female = msg->getU16();
                genderPairs.emplace_back(male, female);
            }
            if (tryOutfitMode == 5) {
                podiumPos.x = msg->getU16();
                podiumPos.y = msg->getU16();
                podiumPos.z = msg->getU8();
                podiumItemId = msg->getU16();
                podiumStackpos = msg->getU8();
                podiumVisible = msg->getU8();
                podiumShowOutfit = msg->getU8();
                podiumDirection = msg->getU8();
            }
        }
        tibia::game::notify("onOutfitWindowOptions", tryOutfitMode, mounted, randomMount, genderPairs, outfitOffers,
                                     mountOffers, familiarOffers, podiumPos, podiumItemId, podiumStackpos, podiumVisible,
                                     podiumShowOutfit, podiumDirection);
    }

    std::vector<std::tuple<uint16_t, std::string>> wingList;
    std::vector<std::tuple<uint16_t, std::string>> auraList;
    std::vector<std::tuple<uint16_t, std::string>> effectList;
    std::vector<std::tuple<uint16_t, std::string>> shaderList;

    if (tibia::game::cipFeature(tibia::client::GameWingsAurasEffectsShader)) {
        const uint8_t wingCount = msg->getU8();
        for (auto i = 0; i < wingCount; ++i) {
            const uint16_t wingId = msg->getU16();
            const auto& wingName = msg->getString();
            wingList.emplace_back(wingId, wingName);
        }

        const uint8_t auraCount = msg->getU8();
        for (auto i = 0; i < auraCount; ++i) {
            const uint16_t auraId = msg->getU16();
            const auto& auraName = msg->getString();
            auraList.emplace_back(auraId, auraName);
        }

        const uint8_t effectCount = msg->getU8();
        for (auto i = 0; i < effectCount; ++i) {
            const uint16_t effectId = msg->getU16();
            const auto& effectName = msg->getString();
            effectList.emplace_back(effectId, effectName);
        }

        const uint8_t shaderCount = msg->getU8();
        for (auto i = 0; i < shaderCount; ++i) {
            const uint16_t shaderId = msg->getU16();
            const auto& shaderName = msg->getString();
            shaderList.emplace_back(shaderId, shaderName);
        }
    }

    tibia::game::processOpenOutfitWindow(currentOutfit, outfitList, mountList, familiarList, wingList, auraList, effectList, shaderList);
}

void ProtocolGame::parseExivaRestrictions(const InputMessagePtr& msg)
{
    const bool allowAll = static_cast<bool>(msg->getU8());
    const bool allowOwnGuild = static_cast<bool>(msg->getU8());
    const bool allowOwnParty = static_cast<bool>(msg->getU8());
    const bool allowVipList = static_cast<bool>(msg->getU8());
    const bool allowPlayerWhitelist = static_cast<bool>(msg->getU8());
    const bool allowGuildWhitelist = static_cast<bool>(msg->getU8());

    std::vector<std::string> characterWhiteList;
    const uint16_t addedPlayersSize = msg->getU16();
    characterWhiteList.reserve(addedPlayersSize);
    for (auto i = 0; std::cmp_less(i, addedPlayersSize); ++i) {
        characterWhiteList.emplace_back(msg->getString());
    }

    std::vector<std::string> removeCharacter;
    const uint16_t removedPlayersSize = msg->getU16();
    removeCharacter.reserve(removedPlayersSize);
    for (auto i = 0; std::cmp_less(i, removedPlayersSize); ++i) {
        removeCharacter.emplace_back(msg->getString());
    }

    std::vector<std::string> guildWhiteList;
    const uint16_t addedGuildsSize = msg->getU16();
    guildWhiteList.reserve(addedGuildsSize);
    for (auto i = 0; std::cmp_less(i, addedGuildsSize); ++i) {
        guildWhiteList.emplace_back(msg->getString());
    }

    std::vector<std::string> removeGuild;
    const uint16_t removedGuildsSize = msg->getU16();
    removeGuild.reserve(removedGuildsSize);
    for (auto i = 0; std::cmp_less(i, removedGuildsSize); ++i) {
        removeGuild.emplace_back(msg->getString());
    }

    tibia::game::notify("onReceiveExivaOptions",
        allowAll, allowOwnGuild, allowOwnParty, allowVipList,
        allowPlayerWhitelist, allowGuildWhitelist,
        characterWhiteList, removeCharacter, guildWhiteList, removeGuild);
}

void ProtocolGame::parseQuestTracker(const InputMessagePtr& msg)
{
    const uint8_t messageType = msg->getU8();
    switch (messageType) {
        case 1: {
            const uint8_t remainingQuests = msg->getU8();
            const uint8_t missionCount = msg->getU8();
            std::vector<std::tuple<uint16_t, uint16_t, std::string, std::string, std::string>> missions;
            for (uint8_t i = 0; i < missionCount; ++i) {
                const uint16_t missionId = msg->getU16();
                uint16_t questId = 0;
                if (tibia::game::clientRevision() >= 1410) {
                    questId = msg->getU16();
                }
                const std::string& questName = msg->getString();
                const std::string& missionName = msg->getString();
                const std::string& missionDesc = msg->getString();
                missions.emplace_back(questId, missionId, questName, missionName, missionDesc);
            }
            return tibia::game::notify("onQuestTracker", remainingQuests, missions);
        }
        case 0: {
            // Same entry layout as the full list (CIP reader): mission id first.
            const uint16_t missionId = msg->getU16();
            uint16_t questId = 0;
            if (tibia::game::clientRevision() >= 1410) {
                questId = msg->getU16();
            }
            std::string questName = "";
            if (tibia::game::clientRevision() >= 1410) {
                questName = msg->getString();
            }
            const std::string& missionName = msg->getString();
            const std::string& missionDesc = msg->getString();
            return tibia::game::notify("onUpdateQuestTracker", questId, missionId, questName, missionName, missionDesc);
        }
    }
}

void ProtocolGame::parseKillTracker(const InputMessagePtr& msg)
{
    const std::string& monsterName = msg->getString();
    const Outfit& monsterOutfit = getOutfit(msg, false);
    const uint8_t corpseItemsSize = msg->getU8();

    std::vector<tibia::container::TInventorySlot> dropItems;
    dropItems.reserve(corpseItemsSize);
    for (auto i = 0; i < corpseItemsSize; ++i)
        dropItems.push_back(getInventorySlot(msg));

    tibia::game::notify("onKillTracker", monsterName, monsterOutfit, dropItems);
}

void ProtocolGame::parseVipAdd(const InputMessagePtr& msg)
{
    uint32_t iconId = 0;
    std::string desc;
    bool notifyLogin = false;
    std::vector<uint8_t> groupIDs;

    const uint32_t id = msg->getU32();
    const auto& name = tibia::game::formatCreatureName(msg->getString());
    if (tibia::game::cipFeature(tibia::client::GameAdditionalVipInfo)) {
        desc = msg->getString();
        iconId = msg->getU32();
        notifyLogin = static_cast<bool>(msg->getU8());
    }

    const uint32_t status = msg->getU8();
    if (tibia::game::cipFeature(tibia::client::GameVipGroups)) {
        const uint8_t vipGroupSize = msg->getU8();
        groupIDs.reserve(vipGroupSize);
        for (auto i = 0; i < vipGroupSize; ++i) {
            const uint8_t groupID = msg->getU8();
            groupIDs.push_back(groupID);
        }
    }

    tibia::game::processVipAdd(id, name, status, desc, iconId, notifyLogin, groupIDs);
}

void ProtocolGame::parseVipState(const InputMessagePtr& msg)
{
    const uint32_t id = msg->getU32();
    const uint32_t status = tibia::game::cipFeature(tibia::client::GameLoginPending) ? msg->getU8() : 1;

    tibia::game::processVipStateChange(id, status);
}

void ProtocolGame::parseVipLogout(const InputMessagePtr& msg)
{
    // On QT client this operation is being processed on the 'parseVipState', now this opcode if for groups
    if (tibia::game::cipFeature(tibia::client::GameVipGroups)) {
        const uint8_t vipGroupSize = msg->getU8();
        std::vector<std::tuple<uint8_t, std::string, bool>> vipGroups;
        for (auto i = 0; i < vipGroupSize; ++i) {
            const uint8_t groupId = msg->getU8();
            const auto& groupName = msg->getString();
            const bool canEditGroup = static_cast<bool>(msg->getU8());
            vipGroups.emplace_back(groupId, groupName, canEditGroup);
        }
        const uint8_t groupsAmountLeft = msg->getU8();
        tibia::game::processVipGroupChange(vipGroups, groupsAmountLeft);
    } else {
        const uint32_t id = msg->getU32();
        tibia::game::processVipStateChange(id, 0);
    }
}

void ProtocolGame::parseBestiaryRaces(const InputMessagePtr& msg)
{
    std::vector<CyclopediaBestiaryRace> bestiaryData;

    const uint16_t bestiaryRaceLast = msg->getU16();
    for (auto i = 0; i < bestiaryRaceLast; ++i) {
        CyclopediaBestiaryRace race;
        race.race = i;
        race.bestClass = msg->getString();
        race.count = msg->getU16();
        race.unlockedCount = msg->getU16();
        bestiaryData.emplace_back(race);
    }

    tibia::game::processParseBestiaryRaces(bestiaryData);
}

void ProtocolGame::parseBestiaryOverview(const InputMessagePtr& msg)
{
    const auto& raceName = msg->getString();

    const uint16_t raceSize = msg->getU16();
    std::vector<BestiaryOverviewMonsters> data;

    for (auto i = 0; i < raceSize; ++i) {
        const uint16_t raceId = msg->getU16();
        if (tibia::game::clientRevision() >= 1530)
            msg->getU8(); // 15.25 known/unlocked flag (before progress)
        const uint8_t progress = msg->getU8();
        uint8_t occurrence = 0;
        uint16_t creatureAnimusMasteryBonus = 0;
        if (progress > 0) {
            occurrence = msg->getU8();
            if (tibia::game::clientRevision() >= 1530)
                msg->getU8(); // 15.25 extra trailing byte in the progress block
        }
        if (tibia::game::clientRevision() >= 1340) {
            creatureAnimusMasteryBonus = msg->getU16(); // Creature Animous Bonus
        }
        BestiaryOverviewMonsters monster;
        monster.id = raceId;
        monster.currentLevel = progress;
        monster.occurrence = occurrence;
        monster.creatureAnimusMasteryBonus = creatureAnimusMasteryBonus;
        data.emplace_back(monster);
    }

    uint16_t animusMasteryPoints = 0;
    if (tibia::game::clientRevision() >= 1340) {
        animusMasteryPoints = msg->getU16(); // Animus Mastery Points
    }

    tibia::game::processParseBestiaryOverview(raceName, data, animusMasteryPoints);
}

void ProtocolGame::parseBestiaryMonsterData(const InputMessagePtr& msg)
{
    BestiaryMonsterData data;
    data.id = msg->getU16();
    data.bestClass = msg->getString();
    data.currentLevel = msg->getU8();

    if (tibia::game::clientRevision() >= 1340) {
        data.AnimusMasteryBonus = msg->getU16(); // Animus Mastery Bonus
        data.AnimusMasteryPoints = msg->getU16(); // Animus Mastery Points
    } else {
        data.AnimusMasteryBonus = 0;
        data.AnimusMasteryPoints = 0;
    }

    data.killCounter = msg->getU32();
    data.thirdDifficulty = msg->getU16();
    data.secondUnlock = msg->getU16();
    data.lastProgressKillCount = msg->getU16();
    data.difficulty = msg->getU8();
    data.ocorrence = 0;
    if (tibia::game::clientRevision() < 1530 || data.currentLevel != 0) {
        data.ocorrence = msg->getU8();
        if (tibia::game::clientRevision() >= 1530)
            data.leaderKilled = msg->getU8() != 0;

        const uint8_t lootCount = msg->getU8();
        for (auto i = 0; i < lootCount; ++i) {
            LootItem lootItem;
            lootItem.itemId = msg->getU16();
            lootItem.diffculty = msg->getU8();
            lootItem.specialEvent = msg->getU8();

            const bool shouldAddItem = lootItem.itemId != 0;
            if (shouldAddItem) {
                lootItem.name = msg->getString();
                lootItem.amount = msg->getU8();
            }
            data.loot.emplace_back(lootItem);
        }
    }

    if (data.currentLevel > 1) {
        data.charmValue = msg->getU16();
        data.attackMode = msg->getU8();
        data.castsSpells = msg->getU8() != 0;
        data.maxHealth = msg->getU32();
        data.experience = msg->getU32();
        data.speed = msg->getU16();
        data.armor = msg->getU16();
        data.mitigation = msg->getDouble();
    }

    if (data.currentLevel > 2) {
        const uint8_t elementsCount = msg->getU8();
        for (auto i = 0; i < elementsCount; ++i) {
            const uint8_t elementId = msg->getU8();
            const uint16_t elementValue = msg->getU16();
            data.combat[elementId] = elementValue;
        }

        const uint16_t locationCount = msg->getU16();
        if (locationCount > 0) {
            data.location = msg->getString();
        }
    }
    if (tibia::game::clientRevision() < 1410) {
        if (data.currentLevel > 3) {
            const bool hasCharm = static_cast<bool>(msg->getU8());
            if (hasCharm) {
                msg->getU8();
                msg->getU32();
            } else {
                msg->getU8();
            }
        }
    }

    tibia::game::processUpdateBestiaryMonsterData(data);
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

void ProtocolGame::parseTutorialHint(const InputMessagePtr& msg)
{
    const uint8_t id = msg->getU8();
    tibia::game::processTutorialHint(id);
}

void ProtocolGame::parseAutomapFlag(const InputMessagePtr& msg)
{
    if (tibia::game::clientRevision() >= 1200) {
        // Tibia 12+: 0xDD is the "Cyclopedia Map Data" packet family (CyclopediaMapData_t).
        const uint8_t subtype = msg->getU8();
        if (subtype != 0) {
            if (tibia::game::clientRevision() < 1525) {
                throw Exception("ProtocolGame::parseAutomapFlag: unhandled cyclopedia map data subtype {}", subtype);
            }

            switch (subtype) {
                case 1: { // DiscoveryData
                    std::vector<std::vector<int>> mains;
                    for (int k = 0, n = msg->getU16(); k < n; ++k) {
                        const int areaId = msg->getU16();
                        const int status = msg->getU8();
                        const int progress = msg->getU8();
                        mains.push_back({ areaId, status, progress });
                    }
                    std::vector<int> discovered;
                    for (int k = 0, n = msg->getU16(); k < n; ++k)
                        discovered.push_back(msg->getU16());
                    std::vector<int> discoverable;
                    for (int k = 0, n = msg->getU16(); k < n; ++k)
                        discoverable.push_back(msg->getU16());
                    tibia::game::notify("onCyclopediaMapDiscoveryData", mains, discovered, discoverable);
                    return;
                }
                case 5: { // SetDiscoveryArea
                    const int areaId = msg->getU16();
                    const int active = msg->getU8();
                    const int poiTarget = msg->getU8();
                    std::vector<std::vector<int>> points;
                    for (int k = 0, n = msg->getU8(); k < n; ++k) {
                        const Position pos = getPosition(msg);
                        const int state = msg->getU8();
                        points.push_back({ pos.x, pos.y, static_cast<int>(pos.z), state });
                    }
                    tibia::game::notify("onCyclopediaMapDiscoveryArea", areaId, active, poiTarget, points);
                    return;
                }
                case 6: { // Passage — CIP CyclopediaMapDataPassage (coord + race ids)
                    // Classic OTC wire (Crystal): u16 n; { Position, u8 raceCount, u16 raceId×N }×n.
                    // CIP protobuf has the same payload; product fills race lists as empty and
                    // resolves pin tooltips from static map.dat NPCs at the coordinate.
                    std::vector<std::vector<int>> passages;
                    for (int k = 0, n = msg->getU16(); k < n; ++k) {
                        const Position pos = getPosition(msg);
                        std::vector<int> row{ pos.x, pos.y, static_cast<int>(pos.z) };
                        const int raceCount = msg->getU8();
                        row.push_back(raceCount);
                        for (int r = 0; r < raceCount; ++r)
                            row.push_back(msg->getU16());
                        passages.push_back(std::move(row));
                    }
                    tibia::game::notify("onCyclopediaMapPassages", passages);
                    return;
                }
                case 9: { // Donations
                    const uint64_t goal = msg->getU64();
                    std::vector<std::vector<double>> areas;
                    for (int k = 0, n = msg->getU8(); k < n; ++k) {
                        const uint16_t areaId = msg->getU16();
                        const uint8_t improvedRespawnActive = msg->getU8();
                        const uint64_t donatedAmount = msg->getU64();
                        areas.push_back({ static_cast<double>(areaId), static_cast<double>(improvedRespawnActive), static_cast<double>(donatedAmount) });
                    }
                    tibia::game::notify("onCyclopediaMapDonations", goal, areas);
                    return;
                }
                case 10: { // SetCurrentArea
                    const int areaId = msg->getU16();
                    tibia::game::notify("onCyclopediaMapCurrentArea", areaId);
                    return;
                }
                case 11: { // SetExploringArea
                    const int subAreaId = msg->getU16();
                    tibia::game::notify("onCyclopediaMapExploringArea", subAreaId);
                    return;
                }
                default:
                    throw Exception("ProtocolGame::parseAutomapFlag: unhandled cyclopedia map data subtype {}", subtype);
            }
        }
    }

    const auto& pos = getPosition(msg);
    const uint8_t icon = msg->getU8();
    const auto& description = msg->getString();

    const bool remove = tibia::game::cipFeature(tibia::client::GameMinimapRemove) ? static_cast<bool>(msg->getU8()) : false;
    if (!remove) {
        tibia::game::processAddAutomapFlag(pos, icon, description);
    } else {
        tibia::game::processRemoveAutomapFlag(pos, icon, description);
    }
}

void ProtocolGame::parseQuestLog(const InputMessagePtr& msg)
{
    const uint16_t questsCount = msg->getU16();
    std::vector<std::tuple<uint16_t, std::string, bool>> questList;

    for (auto i = 0; i < questsCount; ++i) {
        const uint16_t id = msg->getU16();
        const auto& questName = msg->getString();
        const bool questCompleted = static_cast<bool>(msg->getU8());
        questList.emplace_back(id, questName, questCompleted);
    }

    tibia::game::processQuestLog(questList);
}

void ProtocolGame::parseQuestLine(const InputMessagePtr& msg)
{
    const uint16_t questId = msg->getU16();

    const uint8_t missionCount = msg->getU8();
    std::vector<std::tuple<std::string, std::string, uint16_t>> questMissions;

    for (auto i = 0; i < missionCount; ++i) {
        auto missionId = 0;
        if (tibia::game::clientRevision() >= 1200) {
            missionId = msg->getU16();
        }
        const auto& missionName = msg->getString();
        const auto& missionDescrition = msg->getString();
        questMissions.emplace_back(missionName, missionDescrition, missionId);
    }

    tibia::game::processQuestLine(questId, questMissions);
}

void ProtocolGame::parseChannelEvent(const InputMessagePtr& msg)
{
    const uint16_t channelId = msg->getU16();
    const auto& channelName = msg->getString();
    const uint8_t channelType = msg->getU8();

    tibia::game::notify("onChannelEvent", channelId, channelName, channelType);
}

void ProtocolGame::parseItemInfo(const InputMessagePtr& msg) const
{
    // CIP ObjectInfo (S 0xF4): qtclient always reads u16 countOrSubType (Crystal matches qtclient).
    const uint8_t listCount = msg->getU8();
    std::vector<std::tuple<uint16_t, std::string>> itemList;

    for (auto i = 0; i < listCount; ++i) {
        const uint16_t typeId = msg->getU16();
        msg->getU16(); // countOrSubType — descriptions keyed by typeId
        const auto& desc = msg->getString();
        itemList.emplace_back(typeId, desc);
    }

    if (auto* storage = tibia::cyclopedia::TItemInfoStorage::instance())
        storage->applyItemInfo(itemList);
    else
        tibia::game::notify("onItemInfo", itemList);
}

static inline uint32_t readPackedCount1500(const InputMessagePtr& msg) {
    const uint8_t b1 = msg->getU8();
    if (b1 < 0x40) {
        return b1;
    } else if (b1 < 0x80) {
        const uint8_t b2 = msg->getU8();
        return (static_cast<uint32_t>(b1 - 0x40) << 8) | static_cast<uint32_t>(b2);
    } else {
        const uint8_t b2 = msg->getU8();
        const uint8_t b3 = msg->getU8();
        const uint8_t b4 = msg->getU8();
        return (static_cast<uint32_t>(b2) << 16) | (static_cast<uint32_t>(b3) << 8) | static_cast<uint32_t>(b4);
    }
}

void ProtocolGame::parsePlayerInventory(const InputMessagePtr& msg)
{
    const uint16_t size = msg->getU16();

    constexpr uint16_t MAX_INVENTORY_TYPES = 10000;
    if (size > MAX_INVENTORY_TYPES) {
        g_logger.warning("[game_actionBar][parsePlayerInventory]: inventory size {} exceeds maximum allowed {}", size, MAX_INVENTORY_TYPES);
        return;
    }

    std::map<std::pair<uint16_t, uint8_t>, uint32_t> inventoryCounts;

    for (uint16_t i = 0; std::cmp_less(i, size); ++i) {
        const uint16_t itemId = msg->getU16();
        const uint8_t attribute = msg->getU8();

        const uint32_t amount = tibia::game::getProtocolVersion() < 1500 ? msg->getU16() : readPackedCount1500(msg);

        uint8_t tier = 0;
        if (const auto thingType = tibia::game::thingType(itemId, ThingCategoryItem)) {
            if (std::cmp_greater(thingType->getClassification(), 0)) {
                tier = attribute;
            }
        }

        const auto key = std::make_pair(itemId, tier);
        auto& entry = inventoryCounts[key];
        const uint64_t sum = static_cast<uint64_t>(entry) + amount;
        entry = static_cast<uint32_t>(std::min<uint64_t>(sum, (std::numeric_limits<uint32_t>::max)()));
    }

    if (const auto& localPlayer = tibia::game::localPlayer()) {
        localPlayer->setInventoryCountCache(std::move(inventoryCounts));
        tibia::game::notify("updateInventoryItems");
    }
}

void ProtocolGame::parseModalDialog(const InputMessagePtr& msg)
{
    const uint32_t windowId = msg->getU32();
    const auto& title = msg->getString();
    const auto& message = msg->getString();

    const uint8_t buttonsCount = msg->getU8();
    std::vector<std::tuple<uint8_t, std::string>> buttonList;

    for (auto i = 0; i < buttonsCount; ++i) {
        const auto& value = msg->getString();
        const uint8_t buttonId = msg->getU8();
        buttonList.emplace_back(buttonId, value);
    }

    const uint8_t choicesCount = msg->getU8();
    std::vector<std::tuple<uint8_t, std::string>> choiceList;

    for (auto i = 0; i < choicesCount; ++i) {
        const auto& value = msg->getString();
        const uint8_t choideId = msg->getU8();
        choiceList.emplace_back(choideId, value);
    }

    uint8_t enterButton;
    uint8_t escapeButton;
    if (tibia::game::clientRevision() > 970) {
        escapeButton = msg->getU8();
        enterButton = msg->getU8();
    } else {
        enterButton = msg->getU8();
        escapeButton = msg->getU8();
    }

    const bool priority = static_cast<bool>(msg->getU8());

    tibia::game::processModalDialog(windowId, title, message, buttonList, enterButton, escapeButton, choiceList, priority);
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

void ProtocolGame::parseChangeMapAwareRange(const InputMessagePtr& msg)
{
    const uint8_t xRange = msg->getU8();
    const uint8_t yRange = msg->getU8();

    tibia::game::setAwareRange({
        static_cast<uint8_t>(xRange / 2 - (xRange + 1) % 2),
        static_cast<uint8_t>(yRange / 2 - (yRange + 1) % 2),
        static_cast<uint8_t>(xRange / 2),
        static_cast<uint8_t>(yRange / 2),
    });

    tibia::game::notify("onMapChangeAwareRange", xRange, yRange);
}

void ProtocolGame::parseCreaturesMark(const InputMessagePtr& msg)
{
    const uint32_t creatureId = msg->getU32();
    const bool isLegacyProtocol = tibia::game::clientRevision() < 1076;
    uint8_t squareType;
    uint8_t squareColor;

    if (isLegacyProtocol) {
        squareType = 0;
        squareColor = msg->getU8();
    } else {
        squareType = msg->getU8();
        squareColor = msg->getU8();
    }

    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (!storage->creatureAt(creatureId)) {
            g_logger.traceDebug("ProtocolGame::parseCreaturesMark: could not get creature with id {}", creatureId);
            return;
        }
    }

    if (isLegacyProtocol) {
        tibia::creatures::protocolAddTimedSquare(creatureId, squareColor);
        return;
    }

    if (squareType == 0) {
        tibia::creatures::protocolHideStaticSquare(creatureId);
        tibia::creatures::protocolRemoveTimedSquare(creatureId);
        return;
    }

    if (squareType == 2) {
        tibia::creatures::protocolShowStaticSquare(creatureId,
                                                    Color::from8bit(squareColor != 0 ? squareColor : 1));
    } else {
        tibia::creatures::protocolAddTimedSquare(creatureId, squareColor);
    }
}

void ProtocolGame::parseCreatureType(const InputMessagePtr& msg)
{
    const uint32_t creatureId = msg->getU32();
    const uint8_t type = msg->getU8();

    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (!storage->creatureAt(creatureId)) {
            g_logger.traceDebug("ProtocolGame::parseCreatureType: could not get creature with id {}", creatureId);
            return;
        }
    }
    const bool isPlayer = type == Proto::CreatureTypePlayer;
    const bool isNpc = type == Proto::CreatureTypeNpc;
 tibia::creatures::ensureFromProtocol(creatureId, {}, {}, /*direction=*/2, type, isPlayer, isNpc);
}

void ProtocolGame::setMapDescription(const InputMessagePtr& msg, const int x, const int y, const int z, const int width, const int height, const bool apply)
{
    int startz;
    int endz;
    int zstep;

    if (z > tibia::config::mapSeaFloor()) {
        startz = z - tibia::config::mapAwareUndergroundFloorRange();
        endz = std::min<int>(z + tibia::config::mapAwareUndergroundFloorRange(), tibia::config::mapMaxZ());
        zstep = 1;
    } else {
        startz = tibia::config::mapSeaFloor();
        endz = 0;
        zstep = -1;
    }

    int skip = 0;
    for (auto nz = startz; nz != endz + zstep; nz += zstep) {
        skip = setFloorDescription(msg, x, y, nz, width, height, z - nz, skip, apply);
    }

    if (apply) {
        tibia::game::updateMapLatency();
    }
}

int ProtocolGame::setFloorDescription(const InputMessagePtr& msg, const int x, const int y, const int z, const int width, const int height, const int offset, int skip, const bool apply)
{
    for (auto nx = 0; nx < width; ++nx) {
        for (auto ny = 0; ny < height; ++ny) {
            const Position tilePos(x + nx + offset, y + ny + offset, z);
            if (skip == 0) {
                skip = setTileDescription(msg, tilePos, apply);
            } else {
                if (apply) {
                    if (tibia::worldmap::TWorldmapProtocolMessageHandler* handler =
                            tibia::worldmap::TWorldmapProtocolMessageHandler::liveInstance())
                        handler->clearField(tilePos);
                    else
                        tibia::worldmap::TWorldMapStorage::ensureStorage()->clearField(tilePos);
                }
                --skip;
            }
        }
    }
    return skip;
}

int ProtocolGame::setTileDescription(const InputMessagePtr& msg, const Position position, const bool apply)
{
    tibia::worldmap::TWorldmapProtocolMessageHandler* const worldmapHandler =
        apply ? tibia::worldmap::TWorldmapProtocolMessageHandler::liveInstance() : nullptr;

    if (apply) {
        if (worldmapHandler)
            worldmapHandler->clearField(position);
        else
            tibia::worldmap::TWorldMapStorage::ensureStorage()->clearField(position);
    }

    bool gotEffect = false;
    for (auto stackPos = 0; stackPos < 256; ++stackPos) {
        if (msg->peekU16() >= 0xff00) {
            return msg->getU16() & 0xff;
        }

        if (tibia::game::cipFeature(tibia::client::GameEnvironmentEffect) && !gotEffect) {
            msg->getU16(); // environment effect
            gotEffect = true;
            continue;
        }

        if (apply && stackPos > tibia::config::tileMaxThings()) {
            g_logger.traceError("ProtocolGame::setTileDescription: too many things, pos={}, stackpos={}", position, stackPos);
        }

        const auto& thing = getThing(msg);
        if (!apply) {
            continue;
        }

        (void)thing;
        if (const uint32_t id = lastParsedCreatureId(); id != 0) {
            if (id == tibia::game::localPlayerId())
                tibia::game::localPlayerResetPreWalk();
            if (worldmapHandler)
                worldmapHandler->applyCreatureIdToField(id, position, stackPos);
            else
                tibia::worldmap::TWorldMapStorage::ensureStorage()->placeCreatureId(id, position, stackPos);
        } else if (lastParsedItemAppearance()) {
            if (worldmapHandler)
                worldmapHandler->applyAppearanceToField(lastParsedItemAppearance(), position, stackPos);
            else
                tibia::worldmap::TWorldMapStorage::ensureStorage()->placeAppearance(lastParsedItemAppearance(), position,
                                                                                    stackPos);
        }
    }

    return 0;
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

    if (tibia::game::cipFeature(tibia::client::GameWingsAurasEffectsShader) && parseMount) {
        const uint16_t wings = msg->getU16();
        outfit.setWing(wings);

        const uint16_t auras = msg->getU16();
        outfit.setAura(auras);

        const uint16_t effects = msg->getU16();
        outfit.setEffect(effects);

        outfit.setShader(msg->getString());
    }

    return outfit;
}

ThingPtr ProtocolGame::getThing(const InputMessagePtr& msg)
{
    m_lastParsedCreatureId = 0;
    m_lastParsedItemAppearance.reset();
    const uint16_t id = msg->getU16();
    if (id == 0) {
        throw Exception("ProtocolGame::getThing: invalid thing id");
    }

    if (id == Proto::UnknownCreature || id == Proto::OutdatedCreature || id == Proto::Creature) {
        getCreature(msg, id);
        return nullptr;
    }

    // Item: decode appearance attrs into factory — no Item::create on the field path.
    m_lastParsedItemAppearance = getItemAppearance(msg, id);
    return nullptr;
}

ThingPtr ProtocolGame::getMappedThing(const InputMessagePtr& msg) const
{
    const uint16_t x = msg->getU16();
    if (x != 0xffff) {
        const uint16_t y = msg->getU16();
        const uint8_t z = msg->getU8();

        const uint8_t stackpos = msg->getU8();
        assert(stackpos != UINT8_MAX);

        const Position& pos{ x, y, z };
        const auto& tile = tibia::worldmap::fieldAt(pos);
        if (tile) {
            // LocalPlayerSessionThing Map keep-alive only (CIP field stack is appearance + creatureId).
            if (const ThingPtr thing = tile->getThing(stackpos))
                return thing;
            // CIP ChangeOnMap/DeleteOnMap resolve by stackpos on TWorldMapField — items are
            // appearances and non-local creatures are storage ids, not Map ThingPtrs.
            // Occupied slots are not misses; callers use peek.stack + removeThingAtStackPos.
            if (stackpos < tile->thingCount()
                && (tile->appearanceAt(stackpos) || tile->stackCreatureId(stackpos) != 0))
                return nullptr;
        }

        const int thingCount = tile ? tile->getThingCount() : -1;
        // Far-teleport removes often target tiles the client already dropped.
        if (!tibia::game::isAwareOf({ pos.x, pos.y, pos.z })) {
            g_logger.debug(
                "ProtocolGame::getMappedThing: no thing at unaware pos:{} stackpos:{} (tileThings={})",
                pos, stackpos, thingCount);
        } else {
            g_logger.traceError(
                "ProtocolGame::getMappedThing: no thing at pos:{} stackpos:{} (tileThings={}, stackOutOfRange={})",
                pos, stackpos, thingCount, thingCount >= 0 && stackpos >= thingCount);
        }
    } else {
        const uint32_t creatureId = msg->getU32();
        // Id form: LocalPlayerSessionThing keep-alive only. Callers must use peek.creatureId +
        // removeCreatureFromFields / placeCreatureId for non-local creatures.
        if (m_localPlayer && creatureId == m_localPlayer->getId())
            return std::static_pointer_cast<Thing>(m_localPlayer);
        return nullptr;
    }

    return nullptr;
}

void ProtocolGame::skipMappedThing(const InputMessagePtr& msg) const
{
    const uint16_t x = msg->getU16();
    if (x != 0xffff) {
        msg->getU16(); // y
        msg->getU8(); // z
        msg->getU8(); // stackpos
        return;
    }

    msg->getU32(); // creature id
}

CreaturePtr ProtocolGame::getCreature(const InputMessagePtr& msg, int type)
{
    m_lastParsedCreatureId = 0;
    if (type == 0) {
        type = msg->getU16();
    }

    CreaturePtr creature;
    uint32_t protocolCreatureId = 0;
    bool parsedIsLocalPlayer = false;
    const bool known = type != Proto::UnknownCreature;
    // Phase 5.0-B/C: storage is authority. LocalPlayerSessionThing is a Game session Thing (no Map registry).
    // Non-local: no Creature construction — callers use lastParsedCreatureId + placeCreatureId.
    if (type == Proto::OutdatedCreature || type == Proto::UnknownCreature) {
        if (known) {
            protocolCreatureId = msg->getU32();
            m_lastParsedCreatureId = protocolCreatureId;
            creature = tibia::game::creatureThingById(protocolCreatureId);
            if (!creature) {
                g_logger.traceDebug("ProtocolGame::getCreature: known id {} storage path (no Map keep-alive)",
                                    protocolCreatureId);
            }
            tibia::creatures::ensureFromProtocol(protocolCreatureId, {});
        } else {
            const uint32_t removeId = msg->getU32();
            protocolCreatureId = msg->getU32();
            const uint32_t id = protocolCreatureId;
            m_lastParsedCreatureId = id;

            if (id == removeId) {
                creature = tibia::game::creatureThingById(id);
            } else {
                tibia::worldmap::TWorldMapStorage::ensureStorage()->removeCreatureFromFields(removeId);
                if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance())
                    storage->remove(removeId);
            }

            uint8_t creatureType;
            if (tibia::game::clientRevision() >= 910) {
                creatureType = msg->getU8();
            } else {
                if (id >= Proto::PlayerStartId && id < Proto::PlayerEndId)
                    creatureType = Proto::CreatureTypePlayer;
                else if (id >= Proto::MonsterStartId && id < Proto::MonsterEndId)
                    creatureType = Proto::CreatureTypeMonster;
                else
                    creatureType = Proto::CreatureTypeNpc;
            }

            uint32_t masterId = 0;
            if (tibia::game::clientRevision() >= 1281 && creatureType == Proto::CreatureTypeSummonOwn) {
                masterId = msg->getU32();
                if (m_localPlayer->getId() != masterId) {
                    creatureType = Proto::CreatureTypeSummonOther;
                }
            }

            const auto& name = tibia::game::formatCreatureName(msg->getString());
            const bool isPlayer = creatureType == Proto::CreatureTypePlayer;
            const bool isNpc = creatureType == Proto::CreatureTypeNpc;

            tibia::creatures::ensureFromProtocol(id, QString::fromStdString(name), {},
                                                 /*direction=*/2, creatureType, isPlayer, isNpc);

            parsedIsLocalPlayer = (id == m_localPlayer->getId()) ||
                (creatureType == Proto::CreatureTypePlayer && !m_localPlayer->getId()
                 && name == m_localPlayer->getName());

            if (parsedIsLocalPlayer) {
                m_localPlayer->setId(id);
                m_localPlayer->setName(name);
                m_localPlayer->setMasterId(masterId);
                m_localPlayer->setType(creatureType);
            }
        }

        const uint8_t healthPercent = msg->getU8();
        const auto direction = static_cast<tibia::client::Direction>(msg->getU8());
        const auto& outfit = getOutfit(msg);

        Light light;
        light.intensity = msg->getU8();
        light.color = msg->getU8();

        const uint16_t speed = msg->getU16();

        if (tibia::game::clientRevision() >= 1281) {
            addCreatureIcon(msg, protocolCreatureId);
        }

        if (tibia::game::clientRevision() >= 1530) {
            // AddCreature sends a second icon list (count2) inline; 0x8B type 14 does not.
            std::vector<std::tuple<uint8_t, uint8_t, uint16_t>> icons2;
            readCreatureIconList(msg, icons2);
            if (protocolCreatureId != 0 && !icons2.empty()) {
                std::vector<std::tuple<uint8_t, uint8_t, uint16_t>> merged = icons2;
                if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
                    if (const auto row = storage->creatureAt(protocolCreatureId)) {
                        merged = row->hudIcons();
                        merged.insert(merged.end(), icons2.begin(), icons2.end());
                    }
                }
                tibia::creatures::protocolSetIcons(protocolCreatureId, merged);
            }
        }

        const uint8_t skull = msg->getU8();
        const uint8_t shield = msg->getU8();

        // emblem is sent only when the creature is not known
        uint8_t emblem = 0;
        uint8_t creatureType = 0;
        uint8_t icon = 0;
        bool unpass = true;

        if (tibia::game::cipFeature(tibia::client::GameCreatureEmblems) && !known) {
            emblem = msg->getU8();
        }

        if (tibia::game::cipFeature(tibia::client::GameThingMarks)) {
            creatureType = msg->getU8();
        }

        uint32_t masterId = 0;
        if (tibia::game::clientRevision() >= 1281) {
            if (creatureType == Proto::CreatureTypeSummonOwn) {
                masterId = msg->getU32();
                if (m_localPlayer->getId() != masterId) {
                    creatureType = Proto::CreatureTypeSummonOther;
                }
            } else if (creatureType == Proto::CreatureTypePlayer) {
                uint8_t vocationId = msg->getU8();
                if (protocolCreatureId != 0)
                    tibia::creatures::protocolSetVocation(protocolCreatureId, vocationId);
            }
        }

        if (tibia::game::cipFeature(tibia::client::GameCreatureIcons)) {
            icon = msg->getU8();
        }

        if (tibia::game::cipFeature(tibia::client::GameThingMarks)) {
            const uint8_t mark = msg->getU8(); // mark

            if (tibia::game::clientRevision() < 1281) {
                msg->getU16(); // helpers
            }

            if (protocolCreatureId != 0) {
                if (mark == 0xff)
                    tibia::creatures::protocolHideStaticSquare(protocolCreatureId);
                else
                    tibia::creatures::protocolShowStaticSquare(protocolCreatureId, Color::from8bit(mark));
            }
        }

        if (tibia::game::clientRevision() >= 1281) {
            const uint8_t inspectionType = msg->getU8();
            if (protocolCreatureId != 0)
                tibia::creatures::protocolSetInspectionType(protocolCreatureId, inspectionType);
        }

        if (tibia::game::clientRevision() >= 854) {
            unpass = static_cast<bool>(msg->getU8());
        }

        if (tibia::game::cipFeature(tibia::client::GameCreaturePaperdoll)) {
            uint8_t size = msg->getU8();
            for (uint8_t i = 0; i < size; ++i) {
                const auto& paperdoll = getPaperdoll(msg);
                if (protocolCreatureId != 0)
                    tibia::creatures::protocolAttachPaperdoll(protocolCreatureId, paperdoll);
            }
        }

        std::string shader;
        if (tibia::game::cipFeature(tibia::client::GameCreatureShader)) {
            shader = msg->getString();
        }

        std::vector<uint16_t> attachedEffectList;
        if (tibia::game::cipFeature(tibia::client::GameCreatureAttachedEffect)) {
            const uint8_t listSize = msg->getU8();
            for (auto i = -1; ++i < listSize;) {
                attachedEffectList.push_back(msg->getU16());
            }
        }

        if (protocolCreatureId != 0) {
            tibia::creatures::ensureFromProtocol(protocolCreatureId, {}, outfit,
                                                 static_cast<uint8_t>(direction));
            tibia::creatures::protocolSetHealthPercent(protocolCreatureId, healthPercent);
            tibia::creatures::protocolSetDirection(protocolCreatureId, static_cast<uint8_t>(direction));
            tibia::creatures::protocolSetOutfit(protocolCreatureId, outfit);
            tibia::creatures::protocolSetSpeed(protocolCreatureId, speed, 0);
            tibia::creatures::protocolSetSkull(protocolCreatureId, skull);
            tibia::creatures::protocolSetShield(protocolCreatureId, shield);
            tibia::creatures::protocolSetPassable(protocolCreatureId, !unpass);
            tibia::creatures::protocolSetLight(protocolCreatureId, light);
            if (!shader.empty())
                tibia::creatures::protocolSetShader(protocolCreatureId, shader);
            if (emblem > 0) {
                if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
                    const auto row = storage->getOrCreate(protocolCreatureId);
                    row->setEmblem(emblem);
                    storage->publishCreatureUpdated(row);
                }
            }
            if (creatureType > 0) {
                const bool isPlayer = creatureType == Proto::CreatureTypePlayer;
                const bool isNpc = creatureType == Proto::CreatureTypeNpc;
                tibia::creatures::ensureFromProtocol(protocolCreatureId, {}, {},
                                                     static_cast<uint8_t>(direction), creatureType,
                                                     isPlayer, isNpc);
            }
            if (icon > 0)
                tibia::creatures::protocolSetIcon(protocolCreatureId, icon);

            tibia::creatures::protocolClearTemporaryAttachedEffects(protocolCreatureId);
            std::unordered_set<uint16_t> currentAttachedEffectIds;
            if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
                if (const auto row = storage->creatureAt(protocolCreatureId)) {
                    for (const auto& attachedEffect : row->attachedEffects()) {
                        if (attachedEffect)
                            currentAttachedEffectIds.insert(attachedEffect->getId());
                    }
                }
            }
            for (const auto effectId : attachedEffectList) {
                const auto& effect = g_attachedEffects.getById(effectId);
                if (effect && currentAttachedEffectIds.find(effectId) == currentAttachedEffectIds.end()) {
                    const auto& clonedEffect = effect->clone();
                    clonedEffect->setPermanent(false);
                    tibia::creatures::protocolAttachEffect(protocolCreatureId, clonedEffect);
                }
            }
        }

        if (parsedIsLocalPlayer) {
            m_localPlayer->turn(direction);
            m_localPlayer->setOutfit(outfit, true, false);
            m_localPlayer->setMasterId(masterId);
            if (!m_localPlayer->isKnown())
                tibia::game::localPlayerSetKnown(true);
        }
    } else if (type == Proto::Creature) {
        // this is send creature turn
        const uint32_t creatureId = msg->getU32();
        m_lastParsedCreatureId = creatureId;
        creature = tibia::game::creatureThingById(creatureId);

        const auto direction = static_cast<tibia::client::Direction>(msg->getU8());
        tibia::creatures::protocolSetDirection(creatureId, static_cast<uint8_t>(direction));
        if (m_localPlayer && creatureId == m_localPlayer->getId())
            m_localPlayer->turn(direction);

        if (tibia::game::clientRevision() >= 953) {
            const bool unpass = static_cast<bool>(msg->getU8());
            tibia::creatures::protocolSetPassable(creatureId, !unpass);
        }
    } else {
        throw Exception("ProtocolGame::getCreature: invalid creature opcode");
    }

    return creature;
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

tibia::appearances::TAppearanceInstancePtr ProtocolGame::getItemAppearance(const InputMessagePtr& msg, const int id)
{
    return getInventorySlot(msg, id).appearance;
}

Position ProtocolGame::getPosition(const InputMessagePtr& msg)
{
    const uint16_t x = msg->getU16();
    const uint16_t y = msg->getU16();
    const uint8_t z = msg->getU8();

    return { x, y, z };
}

void ProtocolGame::parseShowDescription(const InputMessagePtr& msg)
{
    const uint32_t offerId = msg->getU32();
    const auto& offerdescription = msg->getString();

    tibia::game::notify("onParseStoreOfferDescriptions", offerId, offerdescription);
}

void ProtocolGame::parseBestiaryTracker(const InputMessagePtr& msg)
{
    uint8_t trackerType = 0;
    if (tibia::game::clientRevision() >= 1320) {
        trackerType = msg->getU8(); // 0x00 for bestiary, 0x01 for boss
    }
    const uint8_t size = msg->getU8();
    std::vector<std::tuple<uint16_t, uint32_t, uint16_t, uint16_t, uint16_t, uint8_t>> trackerData;

    for (auto i = 0; i < size; ++i) {
        const uint16_t raceID = msg->getU16();
        const uint32_t killCount = msg->getU32();
        const uint16_t firstUnlock = msg->getU16();
        const uint16_t secondUnlock = msg->getU16();
        const uint16_t lastUnlock = msg->getU16();
        const uint8_t status = msg->getU8();
        trackerData.emplace_back(raceID, killCount, firstUnlock, secondUnlock, lastUnlock, status);
    }

    tibia::game::notify("onParseCyclopediaTracker", trackerType, trackerData);
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

void ProtocolGame::parseTaskHuntingData(const InputMessagePtr& msg)
{
    msg->getU8(); // slot
    const auto state = static_cast<tibia::client::PreyTaskstate_t>(msg->getU8()); // slot state

    switch (state) {
        case tibia::client::PREY_TASK_STATE_LOCKED:
        {
            msg->getU8(); // task slot unlocked
            break;
        }
        case tibia::client::PREY_TASK_STATE_INACTIVE:
            break;
        case tibia::client::PREY_TASK_STATE_SELECTION:
        {
            const uint16_t creatures = msg->getU16();
            for (auto i = 0; i < creatures; ++i) {
                msg->getU16(); // RaceID
                msg->getU8(); // Is unlocked
            }
            break;
        }
        case tibia::client::PREY_TASK_STATE_LIST_SELECTION:
        {
            const uint16_t creatures = msg->getU16();
            for (auto i = 0; i < creatures; ++i) {
                msg->getU16(); // RaceID
                msg->getU8(); // Is unlocked
            }
            break;
        }
        case tibia::client::PREY_TASK_STATE_ACTIVE:
        {
            msg->getU16(); // RaceID
            msg->getU8(); // Upgraded
            msg->getU16(); // Required kills
            msg->getU16(); // Current kills
            msg->getU8(); // Stars
            break;
        }
        case tibia::client::PREY_TASK_STATE_COMPLETED:
        {
            msg->getU16(); // RaceID
            msg->getU8(); // Upgraded
            msg->getU16(); // Required kills
            msg->getU16(); // Current kills
            if (tibia::game::clientRevision() >= 1285) {
                msg->getU8(); // rarity
            }
            break;
        }
    }

    msg->getU32(); // next free roll
}

void ProtocolGame::parseExperienceTracker(const InputMessagePtr& msg)
{
    const int64_t rawExp = msg->get64();
    const int64_t finalExp = msg->get64();

    tibia::game::notify("onUpdateExperience", rawExp, finalExp);
}

void ProtocolGame::parseLootContainers(const InputMessagePtr& msg)
{
    const bool quickLootFallbackToMainContainer = static_cast<bool>(msg->getU8());

    const uint8_t containersCount = msg->getU8();
    std::vector<std::tuple<uint8_t, uint16_t, uint16_t>> lootList;

    for (auto i = 0; i < containersCount; ++i) {
        const uint8_t categoryType = msg->getU8();
        const uint16_t lootContainerId = msg->getU16();
        uint16_t obtainerContainerId = 0;
        if (tibia::game::clientRevision() >= 1332) {
            obtainerContainerId = msg->getU16();
        }

        lootList.emplace_back(categoryType, lootContainerId, obtainerContainerId);
    }

    tibia::game::notify("onQuickLootContainers", quickLootFallbackToMainContainer, lootList);
}

void ProtocolGame::parseMonkData(const InputMessagePtr& msg) {
    const auto subtype = static_cast<tibia::client::VocationMonkTypes_t>(msg->getU8());
    switch (subtype) {
        case tibia::client::TYPES_MONK_HARMONY: {
            const uint8_t harmonyValue = msg->getU8();
            tibia::game::localPlayerSetHarmony(harmonyValue);
            if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance())
                storage->syncLocalPlayerStatsFromMap();
            break;
        }
        case tibia::client::TYPES_MONK_SERENE: {
            const auto hasSerene = static_cast<bool>(msg->getU8());
            tibia::game::localPlayerSetSerene(hasSerene);
            if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance())
                storage->syncLocalPlayerStatsFromMap();
            break;
        }
        case tibia::client::TYPES_MONK_VIRTUE: {
            // 15.25+: [u8 count]{u16 spellId} — stance highlight whole-replace list.
            // Emit onVirtuesYellowBorder for action bar / spell list / helper.
            const uint8_t virtueCount = msg->getU8();
            std::vector<uint16_t> spellIds;
            spellIds.reserve(virtueCount);
            for (uint8_t i = 0; i < virtueCount; ++i) {
                spellIds.push_back(msg->getU16());
            }
            tibia::game::notify("onVirtuesYellowBorder", spellIds);
            break;
        }
        default:
            break;
    }
}

void ProtocolGame::parseCyclopediaHouseAuctionMessage(const InputMessagePtr& msg)
{
    const uint32_t houseId = msg->getU32();
    const uint8_t typeValue = msg->getU8();
    const uint8_t errorCode = msg->getU8();
    // only a successful bid carries whether another bidder's limit was higher
    const bool outbidden = typeValue == 1 && errorCode == 0 && msg->getU8() != 0;
    tibia::game::notify("onCyclopediaHouseAuctionMessage", houseId, typeValue, errorCode, outbidden);
}

void ProtocolGame::parseCyclopediaHousesInfo(const InputMessagePtr& msg)
{
    std::vector<uint8_t> counts;
    const uint32_t excludedUntil = msg->getU32();
    for (auto i = 0; i < 6; ++i) {
        counts.push_back(msg->getU8());
    }

    std::vector<uint32_t> shownHouses;
    const uint8_t shownCount = msg->getU8();
    for (auto i = 0; i < shownCount; ++i) {
        shownHouses.push_back(msg->getU32());
    }

    std::vector<uint32_t> otherHouses;
    const uint16_t housesList = msg->getU16();
    for (auto i = 0; i < housesList; ++i) {
        otherHouses.push_back(msg->getU32());
    }
    tibia::game::notify("onCyclopediaHousesInfo", excludedUntil, counts, shownHouses, otherHouses);
}

void ProtocolGame::parseCyclopediaHouseList(const InputMessagePtr& msg)
{
    std::vector<CyclopediaHouseEntry> houses;
    const uint16_t housesCount = msg->getU16();
    for (auto i = 0; i < housesCount; ++i) {
        CyclopediaHouseEntry& house = houses.emplace_back();
        house.houseId = msg->getU32();
        house.isRentable = msg->getU8() != 0;

        house.state = msg->getU8();
        switch (static_cast<tibia::client::CyclopediaHouseState_t>(house.state)) {
            case tibia::client::CYCLOPEDIA_HOUSE_STATE_AVAILABLE: {
                house.bidderName = msg->getString();
                house.isBidder = static_cast<bool>(msg->getU8());
                house.bidDisableReason = msg->getU8();

                if (!house.bidderName.empty()) {
                    house.bidEnd = msg->getU32();
                    house.highestBid = msg->getU64();
                    if (house.isBidder) {
                        house.bidLimit = msg->getU64();
                    }
                }
                break;
            }
            case tibia::client::CYCLOPEDIA_HOUSE_STATE_RENTED:
            case tibia::client::CYCLOPEDIA_HOUSE_STATE_TRANSFER:
            case tibia::client::CYCLOPEDIA_HOUSE_STATE_MOVEOUT: {
                house.ownerName = msg->getString();
                house.paidUntil = msg->getU32();
                house.isOwner = static_cast<bool>(msg->getU8());
                if (house.isOwner) {
                    house.moveOutDisableReason = msg->getU8();
                    house.transferDisableReason = msg->getU8();
                }

                if (house.state == tibia::client::CYCLOPEDIA_HOUSE_STATE_TRANSFER) {
                    house.transferDate = msg->getU32();
                    house.newOwnerName = msg->getString();
                    house.newOwnerAccepted = msg->getU8() != 0;
                    house.transferPrice = msg->getU64();
                    house.isNewOwner = static_cast<bool>(msg->getU8());
                    if (house.isNewOwner) {
                        house.acceptTransferDisableReason = msg->getU8();
                        house.rejectTransferDisableReason = msg->getU8();
                    }
                    if (house.isOwner) {
                        house.cancelTransferDisableReason = msg->getU8();
                    }
                } else if (house.state == tibia::client::CYCLOPEDIA_HOUSE_STATE_MOVEOUT) {
                    house.moveOutDate = msg->getU32();
                    if (house.isOwner) {
                        house.cancelMoveOutDisableReason = msg->getU8();
                    }
                }
                break;
            }
            default:
                break;
        }
    }
    tibia::game::notify("onCyclopediaHouseList", houses);
}

void ProtocolGame::parseSupplyStash(const InputMessagePtr& msg)
{
    const uint16_t itemsCount = msg->getU16();
    std::vector<std::vector<uint32_t>> stashItems;

    for (auto i = 0; i < itemsCount; ++i) {
        uint16_t itemId = msg->getU16();
        uint32_t amount = msg->getU32();
        stashItems.push_back({ itemId, amount });
    }
    if (tibia::game::getProtocolVersion() < 1410) {
        msg->getU16(); // free slots
    }

    tibia::game::notify("onSupplyStashEnter", stashItems);
}

void ProtocolGame::parseSpecialContainer(const InputMessagePtr& msg)
{
    const auto& player = tibia::game::localPlayer();
    const bool stash = msg->getU8();
    const bool market = tibia::game::getProtocolVersion() >= 1220 && msg->getU8();
    player->setSupplyStashAvailable(stash);
    player->setMarketAvailable(market);
    tibia::game::notify("onSpecialContainersAvailable", stash, market);
}

void ProtocolGame::parsePartyAnalyzer(const InputMessagePtr& msg)
{
    // Check 1-second cooldown before executing callback
    const ticks_t currentTime = stdext::millis();
    const ticks_t cooldownPeriod = 1000; // 1 second in milliseconds
    const bool shouldExecuteCallback = (currentTime - m_lastPartyAnalyzerCall >= cooldownPeriod);

    const uint32_t startTime = msg->getU32(); // session minutes
    const uint32_t leaderID = msg->getU32(); // leader ID
    const uint8_t lootType = msg->getU8(); // price type

    const uint8_t partyMembersSize = msg->getU8();
    std::vector<PartyMemberData> membersData;
    for (auto i = 0; i < partyMembersSize; ++i) {
        const uint32_t memberID = msg->getU32(); // party member id
        const uint8_t highlight = msg->getU8(); // highlight
        const uint64_t loot = msg->getU64(); // loot
        const uint64_t supply = msg->getU64(); // supply
        const uint64_t damage = msg->getU64(); // damage
        const uint64_t healing = msg->getU64(); // healing

        membersData.emplace_back(memberID, highlight, loot, supply, damage, healing);
    }

    std::vector<PartyMemberName> membersName;
    const bool hasNamesBool = static_cast<bool>(msg->getU8());
    if (hasNamesBool) {
        const uint8_t membersNameSize = msg->getU8();
        for (auto i = 0; i < membersNameSize; ++i) {
            const uint32_t memberID = msg->getU32(); // party member id
            const std::string memberName = msg->getString(); // party member name
            membersName.emplace_back(memberID, memberName);
        }
    }

    // Every update, for frontends that keep the latest state (the Lua callback below is throttled).
    tibia::game::notify("onPartyAnalyzerUpdate", startTime, leaderID, lootType, membersData, membersName);

    // Execute callback only if cooldown has expired
    if (shouldExecuteCallback) {
        m_lastPartyAnalyzerCall = currentTime;
        tibia::game::notify("onPartyAnalyzer", startTime, leaderID, lootType, membersData, membersName);
    }
}

void ProtocolGame::parseImbuementDurations(const InputMessagePtr& msg)
{
    const uint8_t itemListCount = msg->getU8(); // amount of items to display
    std::vector<ImbuementTrackerItem> itemList;

    for (auto i = 0; i < itemListCount; ++i) {
        ImbuementTrackerItem item(msg->getU8()); // slot
        {
            const auto slot = getInventorySlot(msg);
            item.typeId = slot.typeId;
            item.count = slot.stackable ? slot.count : 1;
            item.tier = slot.tier;
            item.fluidOrSubtype = slot.fluidOrSplash ? slot.fluid : 0;
        }

        std::map<uint8_t, ImbuementSlot> slots;

        const uint8_t slotsCount = msg->getU8(); // total amount of imbuing slots on item
        item.totalSlots = slotsCount; // Store the total number of slots
        if (slotsCount == 0) {
            itemList.emplace_back(item); // Still add the item even if it has no slots for completeness
            continue;
        }

        for (auto slotIndex = 0; slotIndex < slotsCount; ++slotIndex) {
            const bool slotImbued = static_cast<bool>(msg->getU8()); // 0 - empty, 1 - imbued
            if (!slotImbued) {
                continue;
            }

            ImbuementSlot slot(slotIndex);
            slot.name = msg->getString();
            slot.iconId = msg->getU16();
            slot.duration = msg->getU32();
            slot.state = msg->getU8(); // 0 - paused, 1 - decaying
            slots.emplace(slotIndex, slot);
        }

        item.slots = slots;
        itemList.emplace_back(item);
    }

    tibia::game::notify("onUpdateImbuementTracker", itemList);
}

void ProtocolGame::parsePassiveCooldown(const InputMessagePtr& msg)
{
    // CIP PassiveAbilityData (S 0x5E). Crystal must match this CIP layout
    // (abilityId, cooldownEnum, remainingMs, totalMs, decaying) — do not bend qtclient.
    const uint8_t abilityId = msg->getU8();
    const uint8_t cooldownEnum = msg->getU8();
    if (cooldownEnum == 0) {
        const uint32_t remainingMs = msg->getU32();
        const uint32_t totalMs = msg->getU32();
        const bool decaying = msg->getU8() != 0;
        if (auto* storage = tibia::game::TCooldownStorage::instance())
            storage->applyPassiveAbilityData(int(abilityId), remainingMs, totalMs, decaying);
        else
            tibia::game::notify("onPassiveData", int(abilityId), remainingMs, totalMs, decaying);
    } else if (cooldownEnum == 1) {
        msg->getU8();
        msg->getU8();
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

void ProtocolGame::parseSpellDataModification(const InputMessagePtr& msg)
{
    // CIP SpellDataModification (S 0x60). Protobuf: wrapper = repeated
    // SpellDataModification { field1 varint spellId, field2 varint, field3 varint }.
    // Classic packing: u16 count + count × (u16 spellId, u8 mod, u8 value).
    // Crystal emits after wheel spell-grade changes (mod 0 = wheel grade).
    std::vector<std::tuple<uint16_t, uint8_t, uint8_t>> entries;
    if (msg->getUnreadSize() >= 2) {
        const uint16_t count = msg->getU16();
        for (uint16_t i = 0; i < count && msg->getUnreadSize() >= 4; ++i) {
            const uint16_t spellId = msg->getU16();
            const uint8_t mod = msg->getU8();
            const uint8_t value = msg->getU8();
            entries.emplace_back(spellId, mod, value);
        }
    }
    const uint16_t firstSpell = entries.empty() ? 0 : std::get<0>(entries[0]);
    const uint8_t firstFlags = entries.empty() ? 0 : std::get<1>(entries[0]);
    tibia::game::notify("onSpellDataModification", firstSpell, firstFlags);
}

void ProtocolGame::parseServerStop(const InputMessagePtr&)
{
    // CIP Stop (S 0xBC) — empty server-forced stop.
    tibia::game::notify("onServerStop");
}

void ProtocolGame::parseGemAtelierGemRevealed(const InputMessagePtr& msg)
{
    // CIP GemAtelierGemRevealed (S 0xC5): u32 gem id (protobuf field 1).
    const uint32_t gemId = msg->getU32();
    tibia::game::notify("onGemAtelierGemRevealed", gemId);
}

void ProtocolGame::parseTransactionDetails(const InputMessagePtr& msg)
{
    // CIP TransactionDetails (S 0xCB): optional u32 plus string. Bytes after
    // those fields are not decoded, so TransactionDetails drains the remaining
    // bytes. Crystal has no send yet.
    uint32_t transactionId = 0;
    std::string details;
    if (msg->getUnreadSize() >= 4)
        transactionId = msg->getU32();
    if (msg->getUnreadSize() >= 2)
        details = msg->getString();
    while (msg->getUnreadSize() > 0)
        msg->getU8();
    tibia::game::notify("onTransactionDetails", transactionId, details);
}

void ProtocolGame::parseHirelingNameChange(const InputMessagePtr& msg)
{
    // CIP HirelingNameChange (S 0xDB) → showHirelingNameChangeConfiguration(creatureId, objectId).
    // Crystal sendHirelingNameChange writes u32 creature + u32 object (zeros = name prompt).
    uint32_t creatureId = 0;
    uint32_t objectId = 0;
    if (msg->getUnreadSize() >= 4)
        creatureId = msg->getU32();
    if (msg->getUnreadSize() >= 4)
        objectId = msg->getU32();
    tibia::game::notify("onHirelingNameChange", creatureId, objectId);
}

void ProtocolGame::parseGameNews(const InputMessagePtr& msg)
{
    // CIP GameserverMessageShowGameNews (Crystal must match qtclient):
    // u32 news type (ENewsType / EDialogTab), u8 mode (0 = open dialog, 1 = highlight only).
    const uint32_t newsType = msg->getU32();
    const bool notificationOnly = msg->getU8() != 0;
    if (auto* news = tibia::gamewindow::TNewsButtonController::instance())
        news->onShowGameNews(static_cast<int>(newsType), notificationOnly);
    else
        tibia::game::notify("onGameNews", static_cast<int>(newsType), notificationOnly);
}

static void readTeamFinderTypeFields(const InputMessagePtr& msg, const uint8_t teamType, uint16_t& bossID, uint16_t& huntType, uint16_t& huntArea, uint16_t& questID)
{
    switch (teamType) {
        case 1:
            bossID = msg->getU16();
            break;
        case 2:
            huntType = msg->getU16();
            huntArea = msg->getU16();
            break;
        case 3:
            questID = msg->getU16();
            break;
        default:
            break;
    }
}

void ProtocolGame::parseTeamFinderLeader(const InputMessagePtr& msg)
{
    TeamFinderLeaderData data;
    data.reset = msg->getU8() != 0;
    if (data.reset) {
        tibia::game::processTeamFinderLeader(data);
        return;
    }

    data.minLevel = msg->getU16();
    data.maxLevel = msg->getU16();
    data.vocationIDs = msg->getU8();
    data.teamSlots = msg->getU16();
    data.freeSlots = msg->getU16();
    data.timestamp = msg->getU32();
    data.teamType = msg->getU8();
    readTeamFinderTypeFields(msg, data.teamType, data.bossID, data.huntType, data.huntArea, data.questID);

    const uint16_t membersSize = msg->getU16();
    data.members.reserve(membersSize);
    for (uint16_t i = 0; i < membersSize; ++i) {
        TeamFinderMember member;
        member.guid = msg->getU32();
        member.name = msg->getString();
        member.level = msg->getU16();
        member.vocation = msg->getU8();
        member.status = msg->getU8();
        data.members.push_back(std::move(member));
    }

    tibia::game::processTeamFinderLeader(data);
}

void ProtocolGame::parseTeamFinderList(const InputMessagePtr& msg)
{
    const bool rateLimited = msg->getU8() != 0;
    const uint16_t count = msg->getU16();
    std::vector<TeamFinderListEntry> entries;
    entries.reserve(count);

    for (uint16_t i = 0; i < count; ++i) {
        TeamFinderListEntry entry;
        entry.leaderGuid = msg->getU32();
        entry.leaderName = msg->getString();
        entry.minLevel = msg->getU16();
        entry.maxLevel = msg->getU16();
        entry.vocationIDs = msg->getU8();
        entry.teamSlots = msg->getU16();
        entry.membersCount = msg->getU16();
        entry.timestamp = msg->getU32();
        entry.teamType = msg->getU8();
        readTeamFinderTypeFields(msg, entry.teamType, entry.bossID, entry.huntType, entry.huntArea, entry.questID);
        entry.status = msg->getU8();
        entries.push_back(std::move(entry));
    }

    tibia::game::processTeamFinderList(rateLimited, entries);
}

void ProtocolGame::parseDepotSearchResults(const InputMessagePtr& msg)
{
    const uint16_t count = msg->getU16();
    std::vector<DepotSearchResultItem> items;
    items.reserve(count);

    for (uint16_t i = 0; i < count; ++i) {
        DepotSearchResultItem item;
        item.itemId = msg->getU16();
        if (const auto& thing = tibia::game::thingType(item.itemId, ThingCategoryItem)) {
            if (thing->getClassification() > 0)
                item.tier = msg->getU8();
        }
        item.count = msg->getU16();
        items.push_back(item);
    }

    tibia::game::processDepotSearchResults(items);
}

void ProtocolGame::parseDepotSearchDetailList(const InputMessagePtr& msg)
{
    DepotSearchDetail detail;
    detail.itemId = msg->getU16();
    if (const auto& thing = tibia::game::thingType(detail.itemId, ThingCategoryItem)) {
        if (thing->getClassification() > 0)
            detail.tier = msg->getU8();
    }

    detail.depotCount = msg->getU32();
    const uint8_t depotSize = msg->getU8();
    detail.depotItems.reserve(depotSize);
    for (uint8_t i = 0; i < depotSize; ++i) {
        const auto slot = getInventorySlot(msg);
        detail.depotItems.emplace_back(slot.typeId, slot.tier);
    }

    detail.inboxCount = msg->getU32();
    const uint8_t inboxSize = msg->getU8();
    detail.inboxItems.reserve(inboxSize);
    for (uint8_t i = 0; i < inboxSize; ++i) {
        const auto slot = getInventorySlot(msg);
        detail.inboxItems.emplace_back(slot.typeId, slot.tier);
    }

    detail.hasStash = msg->getU8() != 0;
    if (detail.hasStash) {
        detail.stashItemId = msg->getU16();
        detail.stashCount = msg->getU32();
    }

    tibia::game::processDepotSearchDetail(detail);
}

void ProtocolGame::parseCloseDepotSearch(const InputMessagePtr& /*msg*/)
{
    tibia::game::processCloseDepotSearch();
}

void ProtocolGame::parseBlessDialog(const InputMessagePtr& msg)
{
    BlessDialogData data;

    data.totalBless = msg->getU8();
    for (auto i = 0; i < data.totalBless; ++i) {
        BlessData bless{};
        bless.blessBitwise = msg->getU16();
        bless.playerBlessCount = msg->getU8();
        bless.store = msg->getU8();
        data.blesses.emplace_back(bless);
    }

    data.premium = msg->getU8();
    data.promotion = msg->getU8();
    data.pvpMinXpLoss = msg->getU8();
    data.pvpMaxXpLoss = msg->getU8();
    data.pveExpLoss = msg->getU8();
    data.equipPvpLoss = msg->getU8();
    data.equipPveLoss = msg->getU8();
    data.skull = msg->getU8();
    data.aol = msg->getU8();

    const uint8_t logCount = msg->getU8();
    for (auto i = 0; i < logCount; ++i) {
        LogData log;
        log.timestamp = msg->getU32();
        log.colorMessage = msg->getU8();
        log.historyMessage = msg->getString();
        data.logs.emplace_back(log);
    }

    tibia::game::notify("onUpdateBlessDialog", data);
}

void ProtocolGame::parseRestingAreaState(const InputMessagePtr& msg)
{
    const uint8_t zone = msg->getU8();
    const uint8_t state = msg->getU8();
    const auto& message = msg->getString();

    tibia::game::notify("onRestingAreaState", zone, state, message);
}

void ProtocolGame::parseUpdateImpactTracker(const InputMessagePtr& msg)
{
    const uint8_t analyzerType = msg->getU8();
    const uint32_t amount = msg->getU32();

    uint8_t effect = 0;  // Default effect for healing
    std::string target = "";  // Default empty target

    if (analyzerType == 1) {  // ANALYZER_DAMAGE_DEALT
        effect = msg->getU8();  // Element/combat type
    } else if (analyzerType == 2) {  // ANALYZER_DAMAGE_RECEIVED
        effect = msg->getU8();  // Element/combat type
        target = msg->getString();  // Target name
    }

    // Call the onImpactTracker callback to expose the data to Lua
    tibia::game::notify("onImpactTracker", analyzerType, amount, effect, target);
}

void ProtocolGame::parseItemsPrice(const InputMessagePtr& msg)
{
    const uint16_t priceCount = msg->getU16(); // count

    std::map<uint16_t, uint64_t> prices;
    for (auto i = 0; i < priceCount; ++i) {
        const uint16_t itemId = msg->getU16(); // item client id
        uint8_t tier = 0;
        uint64_t price = 0;
        if (tibia::game::clientRevision() >= 1281) {
            // note: vanilla client allows made-up client ids
            // their classification is assumed as 0
            if (const auto& thing = tibia::game::thingType(itemId, ThingCategoryItem);
                thing && thing->getClassification() > 0) {
                tier = msg->getU8();
            }
            price = msg->getU64(); // price
        } else {
            price = msg->getU32(); // price
        }
        if (tier == 0)
            prices[itemId] = price;
    }

    tibia::game::notify("onItemsPrice", prices);
}

void ProtocolGame::parseUpdateSupplyTracker(const InputMessagePtr& msg)
{
    const auto itemId = msg->getU16(); // item client ID

    // Call the onSupplyTracker callback to expose the data to Lua
    tibia::game::notify("onSupplyTracker", itemId);
}

void ProtocolGame::parseUpdateLootTracker(const InputMessagePtr& msg)
{
    const auto slot = getInventorySlot(msg);
    const auto& itemName = msg->getString();
    const uint16_t lootCount = slot.stackable ? std::max<uint16_t>(1, slot.count) : 1;
    tibia::game::notify("onLootStats", slot.typeId, lootCount, itemName);
}

void ProtocolGame::parseBestiaryEntryChanged(const InputMessagePtr& msg)
{
    const uint16_t raceId = msg->getU16();
    tibia::game::notify("onBestiaryEntryChanged", raceId);
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

void ProtocolGame::parseDailyRewardCollectionState(const InputMessagePtr& msg)
{
    const uint8_t state = msg->getU8();
    tibia::game::notify("onDailyRewardCollectionState", state);
}

void ProtocolGame::parseOpenRewardWall(const InputMessagePtr& msg)
{
    const uint8_t bonusShrine = msg->getU8(); // bonus shrine (1) or instant bonus (0)
    const uint32_t nextRewardTime = msg->getU32(); // next reward time
    const uint8_t dayStreakDay = msg->getU8(); // day streak day

    const uint8_t wasDailyRewardTaken = msg->getU8();
    uint16_t tokens = 0;
    std::string errorMessage = "";
    uint32_t timeLeft = 0;

    if (wasDailyRewardTaken != 0) {// taken (player already took reward?)
        errorMessage = msg->getString(); // error message
        const uint8_t token = msg->getU8();
        if (token != 0) {
            tokens = msg->getU16(); // Tokens
        }
    } else {
        msg->getU8(); // Unknown
        timeLeft = msg->getU32(); // time left to pickup reward without loosing streak
        tokens = msg->getU16(); // Tokens
    }

    const uint16_t dayStreakLevel = msg->getU16(); // day streak level

    tibia::game::notify("onOpenRewardWall", bonusShrine, nextRewardTime, dayStreakDay,
                          wasDailyRewardTaken, errorMessage, tokens, timeLeft, dayStreakLevel);
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

void ProtocolGame::parseRewardHistory(const InputMessagePtr& msg)
{
    const uint8_t historyCount = msg->getU8();
    std::vector<std::tuple<uint32_t, bool, std::string, uint16_t>> rewardHistory;
    for (auto i = 0; i < historyCount; ++i) {
        const uint32_t timestamp = msg->getU32();
        const bool isPremium = static_cast<bool>(msg->getU8());
        const auto& description = msg->getString();
        const uint32_t daystreak = msg->getU16();
        rewardHistory.emplace_back(timestamp, isPremium, description, daystreak);
    }

    tibia::game::notify("onRewardHistory", rewardHistory);
}

void ProtocolGame::parsePreyFreeRerolls(const InputMessagePtr& msg)
{
    const uint8_t slot = msg->getU8();
    const uint16_t timeLeft = msg->getU16();

    tibia::game::notify("onPreyFreeRerolls", slot, timeLeft);
}

void ProtocolGame::parsePreyTimeLeft(const InputMessagePtr& msg)
{
    const uint8_t slot = msg->getU8();
    const uint16_t timeLeft = msg->getU16();

    tibia::game::notify("onPreyTimeLeft", slot, timeLeft);
}

PreyMonster ProtocolGame::getPreyMonster(const InputMessagePtr& msg) const
{
    const auto& name = msg->getString();
    const auto& outfit = getOutfit(msg, false);
    return { .name = name, .outfit = outfit };
}

std::vector<PreyMonster> ProtocolGame::getPreyMonsters(const InputMessagePtr& msg)
{
    const uint8_t monsterListCount = msg->getU8();
    std::vector<PreyMonster> monsterList;

    for (auto i = 0; i < monsterListCount; ++i) {
        const auto& monster = getPreyMonster(msg);
        monsterList.emplace_back(monster);
    }

    return monsterList;
}

void ProtocolGame::parsePreyData(const InputMessagePtr& msg)
{
    const uint8_t slot = msg->getU8(); // slot
    const auto state = static_cast<tibia::client::PreyState_t>(msg->getU8()); // slot state

    uint32_t nextFreeReroll = 0; // next free roll
    uint8_t wildcards = 0; // wildcards
    uint8_t option = 0; // prey option toggle state

    switch (state) {
        case tibia::client::PREY_STATE_LOCKED:
        {
            const auto unlockState = static_cast<tibia::client::PreyUnlockState_t>(msg->getU8()); // prey slot unlocked
            if (tibia::game::clientRevision() > 1149) { // correct unconfirmed version
                nextFreeReroll = msg->getU32();
                wildcards = msg->getU8();
            } else {
                nextFreeReroll = msg->getU16();
            }
            return tibia::game::notify("onPreyLocked", slot, unlockState, nextFreeReroll, wildcards);
        }
        case tibia::client::PREY_STATE_INACTIVE:
        {
            if (tibia::game::clientRevision() > 1149) { // correct unconfirmed version
                nextFreeReroll = msg->getU32();
                wildcards = msg->getU8();
            } else {
                nextFreeReroll = msg->getU16();
            }
            return tibia::game::notify("onPreyInactive", slot, nextFreeReroll, wildcards);
        }
        case tibia::client::PREY_STATE_ACTIVE:
        {
            const auto& monster = getPreyMonster(msg);
            const uint8_t bonusType = msg->getU8(); // bonus type
            const uint16_t bonusValue = msg->getU16(); // bonus value
            const uint8_t bonusGrade = msg->getU8(); // bonus grade
            const uint16_t timeLeft = msg->getU16(); // time left
            if (tibia::game::clientRevision() > 1149) { // correct unconfirmed version
                nextFreeReroll = msg->getU32();
                wildcards = 0; // wildcards
                option = msg->getU8();
            } else {
                nextFreeReroll = msg->getU16();
            }

            return tibia::game::notify("onPreyActive", slot, monster.name, monster.outfit, bonusType, bonusValue, bonusGrade, timeLeft, nextFreeReroll, wildcards, option);
        }
        case tibia::client::PREY_STATE_SELECTION:
        {
            const auto& monsters = getPreyMonsters(msg);
            std::vector<std::string> monsterNames;
            std::vector<Outfit> monsterLooktypes;

            for (const auto& monster : monsters) {
                monsterNames.push_back(monster.name);
                monsterLooktypes.push_back(monster.outfit);
            }

            if (tibia::game::clientRevision() > 1149) { // correct unconfirmed version
                nextFreeReroll = msg->getU32();
                wildcards = msg->getU8();
            } else {
                nextFreeReroll = msg->getU16();
            }
            return tibia::game::notify("onPreySelection", slot, monsterNames, monsterLooktypes, nextFreeReroll, wildcards);
        }
        case tibia::client::PREY_STATE_SELECTION_CHANGE_MONSTER:
        {
            const uint8_t bonusType = msg->getU8(); // bonus type
            const uint16_t bonusValue = msg->getU16(); // bonus value
            const uint8_t bonusGrade = msg->getU8(); // bonus grade

            const auto& monsters = getPreyMonsters(msg);
            std::vector<std::string> names;
            std::vector<Outfit> outfits;

            for (const auto& monster : monsters) {
                names.push_back(monster.name);
                outfits.push_back(monster.outfit);
            }

            if (tibia::game::clientRevision() > 1149) { // correct unconfirmed version
                nextFreeReroll = msg->getU32();
                wildcards = msg->getU8();
            } else {
                nextFreeReroll = msg->getU16();
            }
            return tibia::game::notify("onPreySelectionChangeMonster", slot, names, outfits, bonusType, bonusValue, bonusGrade, nextFreeReroll, wildcards);
        }
        case tibia::client::PREY_STATE_LIST_SELECTION:
        {
            const uint16_t raceListCount = msg->getU16();
            std::vector<uint16_t> raceList;

            for (auto i = 0; i < raceListCount; ++i) {
                raceList.push_back(msg->getU16()); // RaceID
            }

            if (tibia::game::clientRevision() > 1149) { // correct unconfirmed version
                nextFreeReroll = msg->getU32();
                wildcards = msg->getU8();
            } else {
                nextFreeReroll = msg->getU16();
            }
            return tibia::game::notify("onPreyListSelection", slot, raceList, nextFreeReroll, wildcards);
        }
        case tibia::client::PREY_STATE_WILDCARD_SELECTION:
        {
            const uint8_t bonusType = msg->getU8(); // bonus type
            const uint16_t bonusValue = msg->getU16(); // bonus value
            const uint8_t bonusGrade = msg->getU8(); // bonus grade

            const uint16_t raceListCount = msg->getU16();
            std::vector<uint16_t> raceList;

            for (auto i = 0; i < raceListCount; ++i) {
                raceList.push_back(msg->getU16()); // RaceID
            }

            if (tibia::game::clientRevision() > 1149) { // correct unconfirmed version
                nextFreeReroll = msg->getU32();
                wildcards = msg->getU8();
            } else {
                nextFreeReroll = msg->getU16();
            }
            return tibia::game::notify("onPreyWildcardSelection", slot, raceList, nextFreeReroll, wildcards, bonusType, bonusValue, bonusGrade);
        }
    }
}

void ProtocolGame::parsePreyRerollPrice(const InputMessagePtr& msg)
{
    const uint32_t price = msg->getU32(); // prey reroll price
    uint8_t wildcard = 0; // prey bonus reroll price
    uint8_t directly = 0; // prey selection list price

    if (tibia::game::getProtocolVersion() >= 1230) {
        wildcard = msg->getU8();
        directly = msg->getU8();
        if (!tibia::game::cipFeature(tibia::client::GameTaskboard)) {
            msg->getU32(); // task hunting reroll price
            msg->getU32(); // task hunting reroll price
            msg->getU8(); // task hunting selection list price
            msg->getU8(); // task hunting bonus reroll price
        }
    }

    tibia::game::notify("onPreyRerollPrice", price, wildcard, directly);
}

Imbuement ProtocolGame::getImbuementInfo(const InputMessagePtr& msg)
{
    Imbuement imbuement;
    imbuement.id = msg->getU32();
    imbuement.name = msg->getString();
    imbuement.description = msg->getString();

    if (tibia::game::clientRevision() >= 1510) {
        imbuement.tier = msg->getU8(); // tier: 0 = Basic / 1 = Intricate / 2 = Powerful

        static const std::array<std::string, 3> tierNames = { "Basic", "Intricate", "Powerful" };
        if (imbuement.tier < tierNames.size()) {
            imbuement.group = tierNames[imbuement.tier];
        } else {
            imbuement.group = "Unknown";
        }
    } else {
        imbuement.group = msg->getString();
        imbuement.tier = 0;
    }

    imbuement.imageId = msg->getU16(); // iconId
    imbuement.duration = msg->getU32();

    if (tibia::game::clientRevision() < 1510) {
        imbuement.premiumOnly = msg->getU8();
    } else {
        imbuement.premiumOnly = false;
    }

    const uint8_t itemsSize = msg->getU8();
    for (auto i = 0; std::cmp_less(i, itemsSize); ++i) {
        const uint16_t itemId = msg->getU16();
        const auto& itemName = msg->getString();
        const uint16_t itemCount = msg->getU16();
        imbuement.sources.emplace_back(itemId, itemCount, itemName);
    }

    imbuement.cost = msg->getU32();

    if (tibia::game::clientRevision() < 1510) {
        imbuement.successRate = msg->getU8();
        imbuement.protectionCost = msg->getU32();
    } else {
        imbuement.successRate = 100;
        imbuement.protectionCost = 0;
    }

    return imbuement;
}

void ProtocolGame::parseImbuementWindow(const InputMessagePtr& msg)
{
    const bool isModernImbuementWindow = tibia::game::clientRevision() >= 1510;
    uint8_t windowType = tibia::client::IMBUEMENT_WINDOW_SELECT_ITEM;
    if (isModernImbuementWindow) {
        windowType = static_cast<tibia::client::Imbuement_Window_t>(msg->getU8()); // window type
        if (std::cmp_greater(windowType, static_cast<uint8_t>(tibia::client::IMBUEMENT_WINDOW_SCROLL))) {
            g_logger.warning(std::format("ProtocolGame::parseImbuementWindow: unexpected windowType {}", windowType));
            return;
        }
    }
    switch (windowType) {
        case tibia::client::IMBUEMENT_WINDOW_CHOICE: {
            if (isModernImbuementWindow) {
                msg->getU8(); // unknown
            }
            const uint16_t itemId = msg->getU16(); // item client ID
            const uint32_t unknown = msg->getU32(); // unknown
            tibia::game::notify("onOpenImbuementWindow", itemId, unknown);
            break;
        }
        case tibia::client::IMBUEMENT_WINDOW_SCROLL: {
            msg->getU8(); // unknown byte
            msg->getU8(); // unknown byte
            if (isModernImbuementWindow) {
                msg->getU8(); // unknown byte
            }
            const uint16_t imbuementsSize = msg->getU16();
            std::vector<Imbuement> imbuements;
            imbuements.reserve(imbuementsSize);
            for (uint16_t i = 0; std::cmp_less(i, imbuementsSize); ++i) {
                imbuements.push_back(getImbuementInfo(msg));
            }
            const uint32_t neededItemsListCount = msg->getU32();
            std::vector<std::pair<uint16_t, uint16_t>> neededItemsList;
            neededItemsList.reserve(neededItemsListCount);
            for (uint32_t i = 0; std::cmp_less(i, neededItemsListCount); ++i) {
                const uint16_t needItemId = msg->getU16();
                const uint16_t count = msg->getU16();
                neededItemsList.emplace_back(needItemId, count);
            }
            tibia::game::notify("onImbuementScroll", imbuements, neededItemsList);
            break;
        }
        case tibia::client::IMBUEMENT_WINDOW_SELECT_ITEM: {
            uint16_t itemId = 0;
            uint8_t tier = 0;
            if (isModernImbuementWindow) {
                msg->getU8(); // unknown
            }
            itemId = msg->getU16(); // item client ID
            const auto& thing = tibia::game::thingType(itemId, ThingCategoryItem);
            if (isModernImbuementWindow && !thing) {
                throw Exception("ProtocolGame::parseImbuementWindow: unable to resolve thing type for item id {}", itemId);
            }
            if (thing) {
                const uint16_t classification = thing->getClassification();
                if (std::cmp_greater(classification, 0)) {
                    tier = msg->getU8();
                }
            }
            const uint8_t slots = msg->getU8();
            std::unordered_map<int, std::tuple<Imbuement, uint32_t, uint32_t>> activeSlots;
            for (uint8_t i = 0; std::cmp_less(i, slots); ++i) {
                const uint8_t firstByte = msg->getU8();
                if (firstByte == 0x01) {
                    Imbuement imbuement = getImbuementInfo(msg);
                    const uint32_t duration = msg->getU32();
                    const uint32_t removalCost = msg->getU32();
                    activeSlots[i] = std::make_tuple(imbuement, duration, removalCost);
                }
            }
            const uint16_t imbuementsSize = msg->getU16();
            std::vector<Imbuement> imbuements;
            imbuements.reserve(imbuementsSize);
            for (uint16_t i = 0; std::cmp_less(i, imbuementsSize); ++i) {
                imbuements.push_back(getImbuementInfo(msg));
            }
            const uint32_t neededItemsListCount = msg->getU32();
            std::vector<std::pair<uint16_t, uint16_t>> neededItemsList;
            neededItemsList.reserve(neededItemsListCount);
            for (uint32_t i = 0; std::cmp_less(i, neededItemsListCount); ++i) {
                const uint16_t needItemId = msg->getU16();
                const uint16_t count = msg->getU16();
                neededItemsList.emplace_back(needItemId, count);
            }
            tibia::game::notify("onImbuementItem", itemId, tier, slots, activeSlots, imbuements, neededItemsList);
            break;
        }
    }
}

void ProtocolGame::parseCloseImbuementWindow(const InputMessagePtr& /*msg*/)
{
    tibia::game::notify("onCloseImbuementWindow");
}

void ProtocolGame::parseError(const InputMessagePtr& msg)
{
    const uint8_t code = msg->getU8();
    const auto& error = msg->getString();
    // Tibia's ShowMessageDialog carries one more field for types 0x50 (u32) and 0x5A (u8).
    if (code == 0x50)
        msg->getU32();
    else if (code == 0x5A)
        msg->getU8();
    tibia::game::notify("onServerError", code, error);
    tibia::game::TGameNotify::instance()->emitServerError(code, QString::fromStdString(error));
}

static uint8_t readMarketItemTier(const InputMessagePtr& msg, uint16_t itemId, int clientVersion)
{
    if (clientVersion < 1281) {
        return 0;
    }

    const auto& thing = tibia::game::thingType(itemId, ThingCategoryItem);
    if (!thing) {
        return 0;
    }

    if (thing->getClassification() <= 0) {
        return 0;
    }

    return msg->getU8();
}

void ProtocolGame::parseMarketEnter(const InputMessagePtr& msg)
{
    const uint8_t offers = msg->getU8();

    const uint16_t itemsSentCount = msg->getU16();
    std::vector<std::vector<uint16_t>> depotItems;

    for (auto i = 0; i < itemsSentCount; ++i) {
        const uint16_t itemId = msg->getU16();
        const uint8_t itemTier = readMarketItemTier(msg, itemId, tibia::game::clientRevision());
        const uint16_t count = msg->getU16();
        depotItems.push_back({ itemId, itemTier, count });
    }

    tibia::game::notify("onMarketEnter", depotItems, offers, -1, -1);
}

void ProtocolGame::parseMarketEnterOld(const InputMessagePtr& msg)
{
    const uint64_t balance = tibia::game::clientRevision() >= 981 ? msg->getU64() : msg->getU32();
    const uint8_t vocation =
        tibia::game::clientRevision() < 950 ? msg->getU8() : tibia::game::localPlayerVocation();

    const uint8_t offers = msg->getU8();
    const uint16_t itemsSent = msg->getU16();

    std::vector<std::vector<uint16_t>> depotItems;
    for (auto i = 0; i < itemsSent; ++i) {
        const uint16_t itemId = msg->getU16();
        const uint16_t count = msg->getU16();
        depotItems.push_back({ itemId, count });
    }

    tibia::game::notify("onMarketEnter", depotItems, offers, balance, vocation);
}

static tibia::client::MarketItemDescription getMarketLastAttribute(int clientVersion)
{
    if (clientVersion >= 1510) {
        return tibia::client::ITEM_DESC_LAST;
    }

    if (clientVersion >= 1500) {
        return tibia::client::ITEM_DESC_MANTRA;
    }

    if (clientVersion >= 1282) {
        return tibia::client::ITEM_DESC_CURRENTTIER;
    }

    if (clientVersion >= 1270) {
        return tibia::client::ITEM_DESC_UPGRADECLASS;
    }

    if (clientVersion >= 1100) {
        return tibia::client::ITEM_DESC_IMBUINGSLOTS;
    }

    return tibia::client::ITEM_DESC_WEIGHT;
}

static bool shouldSkipMarketAttribute(int attr)
{
    if (attr == tibia::client::ITEM_DESC_AUGMENT && !tibia::game::cipFeature(tibia::client::GameItemAugment)) {
        return true;
    }

    return false;
}

static std::unordered_map<int, std::string> readMarketDescriptions(const InputMessagePtr& msg, int clientVersion)
{
    std::unordered_map<int, std::string> descriptions;
    const auto lastAttribute = getMarketLastAttribute(clientVersion);

    for (int_fast32_t attr = tibia::client::ITEM_DESC_FIRST; attr <= lastAttribute; ++attr) {
        if (shouldSkipMarketAttribute(attr)) {
            continue;
        }

        if (msg->peekU16() != 0x00) {
            descriptions.try_emplace(attr, msg->getString());
        } else {
            msg->getU16();
        }
    }

    return descriptions;
}

static std::vector<std::vector<uint64_t>> readMarketStatsList(
    const InputMessagePtr& msg,
    uint8_t action,
    uint32_t timeThing,
    bool pricesAreU64)
{
    constexpr uint32_t kDaySeconds = 86400;

    const uint8_t count = msg->getU8();
    std::vector<std::vector<uint64_t>> out;
    out.reserve(count);

    for (uint8_t i = 0; i < count; ++i) {
        const uint32_t transactions = msg->getU32();

        uint64_t totalPrice = 0;
        uint64_t highestPrice = 0;
        uint64_t lowestPrice = 0;

        if (pricesAreU64) {
            totalPrice = msg->getU64();
            highestPrice = msg->getU64();
            lowestPrice = msg->getU64();
        } else {
            totalPrice = msg->getU32();
            highestPrice = msg->getU32();
            lowestPrice = msg->getU32();
        }

        const uint32_t day = timeThing - kDaySeconds;
        out.push_back({
            static_cast<uint64_t>(day),
            static_cast<uint64_t>(action),
            static_cast<uint64_t>(transactions),
            totalPrice,
            highestPrice,
            lowestPrice
        });
    }

    return out;
}

void ProtocolGame::parseMarketDetail(const InputMessagePtr& msg)
{
    const uint16_t itemId = msg->getU16();

    const int clientVersion = tibia::game::clientRevision();
    const bool pricesAreU64 = (clientVersion >= 1281);

    const uint8_t itemTier = readMarketItemTier(msg, itemId, clientVersion);
    auto descriptions = readMarketDescriptions(msg, clientVersion);

    const uint32_t timeThing = (time(nullptr) / 1000) * 86400;

    auto purchaseStatsList = readMarketStatsList(msg, tibia::client::MARKETACTION_BUY, timeThing, pricesAreU64);
    auto saleStatsList = readMarketStatsList(msg, tibia::client::MARKETACTION_SELL, timeThing, pricesAreU64);

    tibia::game::notify("onMarketDetail", itemId, descriptions, purchaseStatsList, saleStatsList, itemTier);
}

MarketOffer ProtocolGame::readMarketOffer(const InputMessagePtr& msg, const uint8_t action, const uint16_t var)
{
    const uint32_t timestamp = msg->getU32();
    const uint16_t counter = msg->getU16();
    uint16_t itemId = 0;
    uint8_t itemTier = 0;

    const int clientVersion = tibia::game::clientRevision();
    if (var == tibia::client::OLD_MARKETREQUEST_MY_OFFERS || var == tibia::client::MARKETREQUEST_OWN_OFFERS || var == tibia::client::OLD_MARKETREQUEST_MY_HISTORY || var == tibia::client::MARKETREQUEST_OWN_HISTORY) {
        itemId = msg->getU16();
        itemTier = readMarketItemTier(msg, itemId, clientVersion);
    } else {
        itemId = var;
    }

    const uint16_t amount = msg->getU16();
    const uint64_t price = clientVersion >= 1281 ? msg->getU64() : msg->getU32();

    std::string playerName;
    uint8_t state = tibia::client::OFFER_STATE_ACTIVE;
    if (var == tibia::client::OLD_MARKETREQUEST_MY_HISTORY || var == tibia::client::MARKETREQUEST_OWN_HISTORY) {
        state = msg->getU8();
    } else if (var == tibia::client::OLD_MARKETREQUEST_MY_OFFERS || var == tibia::client::MARKETREQUEST_OWN_OFFERS) {} else {
        playerName = msg->getString();
    }

    tibia::game::notify("onMarketReadOffer", action, amount, counter, itemId, playerName, price, state, timestamp, var, itemTier);
    return { .timestamp = timestamp, .counter = counter, .action = action, .itemId = itemId, .amount = amount, .price = price,
        .playerName = playerName, .state = state, .var = var, .itemTier = itemTier
    };
}

void ProtocolGame::parseMarketBrowse(const InputMessagePtr& msg)
{
    uint16_t var = 0;
    if (tibia::game::clientRevision() >= 1281) {
        var = msg->getU8();
        if (var == 3) {
            var = msg->getU16(); // itemId
            const auto& thing = tibia::game::thingType(var, ThingCategoryItem);
            if (thing) {
                const uint16_t classification = thing->getClassification();
                if (classification > 0) {
                    msg->getU8(); // item tier
                }
            }
        }
    } else {
        var = msg->getU16(); // itemId
    }

    const uint32_t buyOfferCount = msg->getU32();

    std::vector<MarketOffer> offers;

    for (uint32_t i = 0; i < buyOfferCount; ++i) {
        offers.push_back(readMarketOffer(msg, tibia::client::MARKETACTION_BUY, var));
    }
    const uint32_t sellOfferCount = msg->getU32();
    for (uint32_t i = 0; i < sellOfferCount; ++i) {
        offers.push_back(readMarketOffer(msg, tibia::client::MARKETACTION_SELL, var));
    }

    std::vector<std::vector<uint64_t>> intOffers;
    std::vector<std::string> nameOffers;

    for (const auto& offer : offers) {
        std::vector<uint64_t> intOffer = { offer.action, offer.amount, offer.counter, offer.itemId, offer.price, offer.state, offer.timestamp, offer.var, offer.itemTier };
        const auto& playerName = offer.playerName;
        intOffers.push_back(intOffer);
        nameOffers.push_back(playerName);
    }

    tibia::game::notify("onMarketBrowse", intOffers, nameOffers);
}

void ProtocolGame::parseBosstiaryData(const InputMessagePtr& msg)
{
    // Kills to reach prowess/expertise/mastery for bane, archfoe, nemesis,
    // then the boss points granted at each of those stages (same order).
    std::vector<uint16_t> killsForStage;
    std::vector<uint16_t> pointsForStage;
    for (auto i = 0; i < 9; ++i) {
        killsForStage.push_back(msg->getU16());
    }
    for (auto i = 0; i < 9; ++i) {
        pointsForStage.push_back(msg->getU16());
    }
    tibia::game::notify("onBosstiaryData", killsForStage, pointsForStage);
}

void ProtocolGame::parseBosstiarySlots(const InputMessagePtr& msg)
{
    BosstiarySlotsData data;

    auto getBosstiarySlot = [&msg]() -> BosstiarySlot {
        BosstiarySlot slot;
        slot.bossRace = msg->getU8();
        slot.killCount = msg->getU32();
        slot.lootBonus = msg->getU16();
        slot.killBonus = msg->getU8();
        slot.bossRaceRepeat = msg->getU8();
        slot.removePrice = msg->getU32();
        slot.inactive = msg->getU8();
        return slot;
    };

    data.playerPoints = msg->getU32();
    data.totalPointsNextBonus = msg->getU32();
    data.currentBonus = msg->getU16();
    data.nextBonus = msg->getU16();

    data.isSlotOneUnlocked = msg->getU8();
    data.bossIdSlotOne = msg->getU32();
    if (data.isSlotOneUnlocked && data.bossIdSlotOne != 0) {
        data.slotOneData = getBosstiarySlot();
    }

    data.isSlotTwoUnlocked = msg->getU8();
    data.bossIdSlotTwo = msg->getU32();
    if (data.isSlotTwoUnlocked && data.bossIdSlotTwo != 0) {
        data.slotTwoData = getBosstiarySlot();
    }

    data.isTodaySlotUnlocked = msg->getU8();
    data.boostedBossId = msg->getU32();
    if (data.isTodaySlotUnlocked && data.boostedBossId != 0) {
        data.todaySlotData = getBosstiarySlot();
    }

    data.bossesUnlocked = msg->getU8();
    if (data.bossesUnlocked) {
        const uint16_t bossesUnlockedSize = msg->getU16();
        for (auto i = 0; i < bossesUnlockedSize; ++i) {
            BossUnlocked boss;
            boss.bossId = msg->getU32();
            boss.bossRace = msg->getU8();
            data.bossesUnlockedData.emplace_back(boss);
        }
    }

    tibia::game::processBosstiarySlots(data);
}

void ProtocolGame::parseBosstiaryCooldownTimer(const InputMessagePtr& msg)
{
    const uint16_t bossesOnTrackerSize = msg->getU16();
    std::vector<BossCooldownData> cooldownData;

    for (auto i = 0; i < bossesOnTrackerSize; ++i) {
        const uint32_t bossRaceId = msg->getU32(); // bossRaceId
        const uint64_t cooldownTime = msg->getU64(); // Boss cooldown in seconds

        cooldownData.emplace_back(bossRaceId, cooldownTime);
    }

    // Call the Lua callback with the cooldown data
    tibia::game::notify("onBossCooldown", cooldownData);
}

void ProtocolGame::parseBosstiaryEntryChanged(const InputMessagePtr& msg)
{
    const uint32_t bossId = msg->getU32();
    tibia::game::notify("onBosstiaryEntryChanged", bossId);
}

void ProtocolGame::parseClientEvent(const InputMessagePtr& msg)
{
    // Pre-15.21: opcode 0x75 was a simple screenshot trigger (single U8 screenshotType).
    // In 15.21+, it was repurposed as a full Client Events system with variable-length data.
    if (tibia::game::clientRevision() < 1521) {
        m_localPlayer->takeScreenshot(msg->getU8());
        return;
    }

    const auto type = static_cast<tibia::client::ClientEventType_t>(msg->getU8());
    switch (type) {
        case tibia::client::CLIENT_EVENT_TYPE_SIMPLE: {
            const auto eventType = static_cast<tibia::client::ClientEvent_t>(msg->getU8());
            if (eventType < tibia::client::CLIENT_EVENT_ATTACKSTOPPED) {
                m_localPlayer->takeScreenshot(eventType);
            } else {
                tibia::game::notify("onClientEvent", type, eventType);
            }
            break;
        }
        case tibia::client::CLIENT_EVENT_TYPE_ACHIEVEMENT:
        case tibia::client::CLIENT_EVENT_TYPE_TITLE: {
            const auto name = msg->getString();
            tibia::game::notify("onClientEvent", type, name);
            break;
        }
        case tibia::client::CLIENT_EVENT_TYPE_LEVEL: {
            const auto level = msg->getU16();
            tibia::game::notify("onClientEvent", type, level);
            break;
        }
        case tibia::client::CLIENT_EVENT_TYPE_SKILL: {
            const auto skillId = msg->getU8();
            const auto level = msg->getU16();
            tibia::game::notify("onClientEvent", type, skillId, level);
            break;
        }
        case tibia::client::CLIENT_EVENT_TYPE_BESTIARY:
        case tibia::client::CLIENT_EVENT_TYPE_BOSSTIARY: {
            const auto raceId = msg->getU16();
            const auto progressLevel = msg->getU8();
            tibia::game::notify("onClientEvent", type, raceId, progressLevel);
            break;
        }
        case tibia::client::CLIENT_EVENT_TYPE_QUEST: {
            const auto questName = msg->getString();
            const auto isCompleted = msg->getU8();
            tibia::game::notify("onClientEvent", type, questName, isCompleted);
            break;
        }
        case tibia::client::CLIENT_EVENT_TYPE_COSMETIC: {
            const auto lookType = msg->getU16();
            const auto skinName = msg->getString();
            const auto skinType = msg->getU8();
            tibia::game::notify("onClientEvent", type, lookType, skinName, skinType);
            break;
        }
        case tibia::client::CLIENT_EVENT_TYPE_PROFICIENCY: {
            const auto itemId = msg->getU16();
            const auto message = msg->getString();
            tibia::game::notify("onClientEvent", type, itemId, message);
            break;
        }
        case tibia::client::CLIENT_EVENT_TYPE_BOUNTY_TASK: {
            const auto taskId = msg->getU16();
            tibia::game::notify("onClientEvent", type, taskId);
            break;
        }
        case tibia::client::CLIENT_EVENT_TYPE_WEEKLY_TASK: {
            const auto raceId = msg->getU16();
            tibia::game::notify("onClientEvent", type, raceId);
            break;
        }
        case tibia::client::CLIENT_EVENT_TYPE_SPELL_UNLOCKED: {
            const auto spellId = msg->getU32();
            tibia::game::notify("onClientEvent", type, spellId);
            break;
        }
        case tibia::client::CLIENT_EVENT_TYPE_LEADER_MONSTER: {
            const auto raceId = msg->getU16();
            const auto charmPoints = msg->getU32();
            tibia::game::notify("onClientEvent", type, raceId, charmPoints);
            break;
        }
        case tibia::client::CLIENT_EVENT_TYPE_SUBAREA: {
            // Point of Interest / discovery viewpoint found (payload: uint16 areaId)
            const auto areaId = msg->getU16();
            tibia::game::notify("onClientEvent", type, areaId);
            break;
        }
        default:
            g_logger.warning(std::format("[ProtocolGame::parseClientEvent] Unknown event type {}", static_cast<uint8_t>(type)));
            break;
    }
}

void ProtocolGame::parseAttachedEffect(const InputMessagePtr& msg)
{
    const uint32_t creatureId = msg->getU32();
    const uint16_t attachedEffectId = msg->getU16();

    const auto& effect = g_attachedEffects.getById(attachedEffectId);
    if (!effect) {
        return;
    }

    tibia::creatures::protocolAttachEffect(creatureId, effect->clone());
}

void ProtocolGame::parseDetachEffect(const InputMessagePtr& msg)
{
    const uint32_t creatureId = msg->getU32();
    const uint16_t attachedEffectId = msg->getU16();
    tibia::creatures::protocolDetachEffect(creatureId, attachedEffectId);
}

void ProtocolGame::parseCreatureShader(const InputMessagePtr& msg)
{
    const uint32_t creatureId = msg->getU32();
    const auto& shaderName = msg->getString();
    tibia::creatures::protocolSetShader(creatureId, shaderName);
}

void ProtocolGame::parseMapShader(const InputMessagePtr& msg)
{
    msg->getString();
}

void ProtocolGame::parseCreatureTyping(const InputMessagePtr& msg)
{
    const uint32_t creatureId = msg->getU32();
    const bool typing = static_cast<bool>(msg->getU8());
    tibia::creatures::protocolSetTyping(creatureId, typing);
}

void ProtocolGame::parseFeatures(const InputMessagePtr& msg)
{
    const uint16_t features = msg->getU16();
    for (auto i = 0; i < features; ++i) {
        const auto feature = static_cast<tibia::client::GameFeature>(msg->getU8());
        const auto enabled = static_cast<bool>(msg->getU8());
        if (enabled) {
            tibia::game::enableFeature(feature);
        } else {
            tibia::game::disableFeature(feature);
        }
    }
}

void ProtocolGame::parseHighscores(const InputMessagePtr& msg)
{
    const bool isEmpty = static_cast<bool>(msg->getU8());
    if (isEmpty) {
        tibia::game::notify("onHighscoresNoData");
        return;
    }

    const uint8_t worldCount = msg->getU8();
    std::vector<std::string> worlds;
    for (auto i = 0; i < worldCount; ++i)
        worlds.emplace_back(msg->getString());
    const std::string serverName = worlds.empty() ? std::string() : worlds.front();
    const auto& world = msg->getString();
    const uint8_t worldType = msg->getU8();
    const uint8_t battlEye = msg->getU8();

    const uint8_t sizeVocation = msg->getU8();
    std::vector<std::tuple<uint32_t, std::string>> vocations;

    msg->getU32(); // skip 0xFFFFFFFF
    const auto& allVocationsName = msg->getString();

    for (auto i = 0; i < sizeVocation - 1; ++i) {
        const uint32_t vocationID = msg->getU32();
        const auto& vocationName = msg->getString();
        vocations.emplace_back(vocationID, vocationName);
    }

    const uint32_t selectedVocation = msg->getU32();

    const uint8_t sizeCategories = msg->getU8();
    std::vector<std::tuple<uint8_t, std::string>> categories;

    for (auto i = 0; i < sizeCategories; ++i) {
        const uint8_t id = msg->getU8();
        const auto& categoryName = msg->getString();
        categories.emplace_back(id, categoryName);
    }

    const uint8_t selectedCategory = msg->getU8();
    const uint16_t page = msg->getU16();
    const uint16_t totalPages = msg->getU16();

    const uint8_t sizeEntries = msg->getU8();
    std::vector<std::tuple<uint32_t, std::string, std::string, uint8_t, std::string, uint16_t, uint8_t, uint64_t>> highscores;

    for (auto i = 0; i < sizeEntries; ++i) {
        const uint32_t rank = msg->getU32();
        const auto& name = msg->getString();
        const auto& title = msg->getString();
        const uint8_t vocation = msg->getU8();
        const auto& world = msg->getString();
        const uint16_t level = msg->getU16();
        const uint8_t isPlayer = msg->getU8();
        const uint64_t points = msg->getU64();
        highscores.emplace_back(rank, name, title, vocation, world, level, isPlayer, points);
    }

    const uint8_t selectedIndex = msg->getU8(); // 0xFF: none
    const bool showLoyaltyTitle = msg->getU8() != 0;
    const uint8_t valueType = msg->getU8(); // HIGHSCORES_CATEGORIES[params.category].type
    const uint32_t entriesTs = msg->getU32(); // last update

    tibia::game::processHighscore(serverName, world, worldType, battlEye, vocations, categories, page, totalPages, highscores, entriesTs,
                            worlds, allVocationsName, selectedVocation, selectedCategory, selectedIndex, showLoyaltyTitle, valueType);
}

void ProtocolGame::parseWeaponProficiencyExperience(const InputMessagePtr& msg)
{
    const uint16_t itemId = msg->getU16();
    const uint32_t experience = msg->getU32();
    const uint8_t hasUnusedPerk = msg->getU8();
    tibia::game::notify("onWeaponProficiencyExperience", itemId, experience, hasUnusedPerk != 0);
}

void ProtocolGame::parseWeaponProficiencyInfo(const InputMessagePtr& msg)
{
    const uint16_t itemId = msg->getU16();
    const uint32_t experience = msg->getU32();
    const uint8_t perksCount = msg->getU8();

    std::map<uint8_t, uint8_t> perks;
    for (int i = 0; i < perksCount; ++i) {
        const uint8_t level = msg->getU8();
        const uint8_t perkPosition = msg->getU8();
        perks[level] = perkPosition;
    }

    std::vector<std::map<std::string, uint16_t>> modifiedSlots;
    if (tibia::game::clientRevision() >= 1530) {
        const uint8_t modifiedCount = msg->getU8();
        modifiedSlots.reserve(modifiedCount);
        for (int i = 0; i < modifiedCount; ++i) {
            const uint8_t level = msg->getU8();
            const uint8_t perkPosition = msg->getU8();
            const uint16_t perkType = msg->getU16();
            const uint8_t value = msg->getU8();
            modifiedSlots.push_back({
                { "grade", level },
                { "slot", perkPosition },
                { "modifierEnum", perkType },
                { "refineLevel", value }
            });
        }
    }

    constexpr uint16_t MarketCategoryWeaponsAll = 32;
    uint16_t marketCategory = MarketCategoryWeaponsAll;
    if (tibia::game::isValidDatId(itemId, ThingCategoryItem)) {
        const auto& itemType = tibia::game::thingType(itemId, ThingCategoryItem);
        if (itemType) {
            const auto& marketData = itemType->getMarketData();
            if (!marketData.name.empty()) {
                marketCategory = marketData.category;
            }
        }
    }
    tibia::game::notify("onWeaponProficiency", itemId, experience, perks, marketCategory, modifiedSlots);
}

void ProtocolGame::parseWeaponProficiencyReshapeOffers(const InputMessagePtr& msg)
{
    const uint16_t itemId = msg->getU16();
    const uint8_t currentLevel = msg->getU8();
    const uint8_t currentPosition = msg->getU8();

    const uint8_t offersCount = msg->getU8();
    std::vector<std::map<std::string, uint16_t>> offers;
    offers.reserve(offersCount);
    for (int i = 0; i < offersCount; ++i) {
        const uint16_t perkType = msg->getU16();
        const uint8_t value = msg->getU8();
        offers.push_back({ { "modifierEnum", perkType }, { "refineLevel", value } });
    }

    tibia::game::notify("onWeaponProficiencyReshape", itemId, currentLevel, currentPosition, offers);
}

void ProtocolGame::parseUnlockedSpells(const InputMessagePtr& msg)
{
    std::vector<uint16_t> spellIds;
    for (uint16_t i = 0, count = msg->getU16(); i < count; ++i)
        spellIds.push_back(msg->getU16());
    tibia::game::notify("onUnlockedSpells", spellIds);
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

void ProtocolGame::parseCharacterTradeConfiguration(const InputMessagePtr& msg)
{
    // Crystal charbazaar.lua S_Configuration (0x80): dataType byte, then typed body.
    const uint8_t dataType = msg->getU8();
    if (dataType == 0) {
        // CharacterTradeBaseData / conditions
        const auto& name = msg->getString();
        const uint16_t level = msg->getU16();
        const uint8_t vocationClientId = msg->getU8();
        const Outfit outfit = getOutfit(msg, false);
        const uint8_t worldType = msg->getU8();
        const uint32_t startTimestamp = msg->getU32();
        const uint32_t endTimestamp = msg->getU32();
        const uint16_t minimumBid = msg->getU16();
        std::vector<std::tuple<std::string, uint8_t>> conditions;
        for (uint8_t i = 0, n = msg->getU8(); i < n; ++i)
            conditions.emplace_back(msg->getString(), msg->getU8());
        tibia::game::notify("onCharacterTradeConditions", name, level, vocationClientId,
                                     outfit, worldType, startTimestamp, endTimestamp, minimumBid, conditions);
        return;
    }
    if (dataType == 1 || dataType == 2) {
        // CharacterTradeHighlightsItemsData
        std::vector<std::tuple<uint16_t, uint8_t, uint32_t>> items;
        for (uint16_t i = 0, n = msg->getU16(); i < n; ++i) {
            const uint16_t itemId = msg->getU16();
            uint8_t tier = 0;
            const auto& thingType = tibia::game::thingType(itemId, ThingCategoryItem);
            if (thingType && thingType->getClassification() > 0)
                tier = msg->getU8();
            const uint32_t count = msg->getU32();
            items.emplace_back(itemId, tier, count);
        }
        tibia::game::notify("onCharacterTradeItems", dataType, items);
        return;
    }
    if (dataType == 3) {
        // CharacterTradeHighlightsTextualData
        std::vector<std::tuple<uint8_t, std::string, std::vector<std::tuple<uint16_t, std::string>>>> groups;
        for (uint8_t g = 0, gn = msg->getU8(); g < gn; ++g) {
            const uint8_t category = msg->getU8();
            const auto& groupName = msg->getString();
            std::vector<std::tuple<uint16_t, std::string>> entries;
            for (uint16_t i = 0, n = msg->getU16(); i < n; ++i)
                entries.emplace_back(msg->getU16(), msg->getString());
            groups.emplace_back(category, groupName, entries);
        }
        tibia::game::notify("onCharacterTradeHighlights", groups);
        return;
    }
    if (dataType == 4) {
        // CharacterTradeSummaryData
        const uint16_t fee = msg->getU16();
        std::vector<std::string> problems;
        for (uint8_t i = 0, n = msg->getU8(); i < n; ++i)
            problems.push_back(msg->getString());
        tibia::game::notify("onCharacterTradeSummary", fee, problems);
        return;
    }
    g_logger.warning("CharacterTradeConfiguration unknown dataType {}", dataType);
}

void ProtocolGame::parseConfigureCreaturePodium(const InputMessagePtr& msg)
{
    // Crystal ProtocolGame::sendMonsterPodiumWindow (S 0xC2)
    const uint16_t lookType = msg->getU16();
    Outfit selected;
    if (lookType > 0) {
        selected.setCategory(ThingCategoryCreature);
        selected.setId(lookType);
        selected.setHead(msg->getU8());
        selected.setBody(msg->getU8());
        selected.setLegs(msg->getU8());
        selected.setFeet(msg->getU8());
        selected.setAddons(msg->getU8());
    } else {
        const uint16_t lookTypeEx = msg->getU16();
        if (lookTypeEx != 0) {
            selected.setCategory(ThingCategoryItem);
            selected.setAuxId(lookTypeEx);
        }
    }
    const uint16_t unknownListSize = msg->getU16();
    for (uint16_t i = 0; i < unknownListSize; ++i)
        msg->getU16(); // reserved

    const uint8_t isBossPodium = msg->getU8();
    std::vector<std::tuple<uint16_t, std::string, Outfit>> races;
    for (uint16_t i = 0, n = msg->getU16(); i < n; ++i) {
        const uint16_t raceId = msg->getU16();
        if (!isBossPodium) {
            races.emplace_back(raceId, std::string{}, Outfit{});
            continue;
        }
        const auto& name = msg->getString();
        Outfit outfit;
        const uint16_t raceLookType = msg->getU16();
        if (raceLookType != 0) {
            outfit.setId(raceLookType);
            outfit.setHead(msg->getU8());
            outfit.setBody(msg->getU8());
            outfit.setLegs(msg->getU8());
            outfit.setFeet(msg->getU8());
            outfit.setAddons(msg->getU8());
        } else {
            outfit.setCategory(ThingCategoryItem);
            outfit.setAuxId(msg->getU16());
        }
        races.emplace_back(raceId, name, outfit);
    }

    const Position pos = getPosition(msg);
    const uint16_t itemId = msg->getU16();
    const uint8_t stackpos = msg->getU8();
    const uint8_t podiumVisible = msg->getU8();
    const uint8_t monsterSelected = msg->getU8();
    const uint8_t direction = msg->getU8();
    tibia::game::notify("onConfigureCreaturePodium", selected, isBossPodium, races,
                                 pos, itemId, stackpos, podiumVisible, monsterSelected, direction);
}

void ProtocolGame::parseCloseRewardWall(const InputMessagePtr&)
{
    // CIP CloseRewardWall (S 0xE3) — no fields; Crystal daily_reward defines the opcode.
    tibia::game::notify("onCloseRewardWall");
}

void ProtocolGame::parseBossDifficultySelection(const InputMessagePtr& msg)
{
    const uint8_t disc = msg->getU8();
    if (disc == 1) {
        tibia::game::notify("onBossDifficultyClose");
        return;
    }

    if (disc == 2) {
        const uint16_t difficulty = msg->getU16();
        std::vector<std::string> red;
        for (int i = 0, n = msg->getU8(); i < n; ++i)
            red.push_back(msg->getString());
        std::vector<std::string> green;
        for (int i = 0, n = msg->getU8(); i < n; ++i)
            green.push_back(msg->getString());
        tibia::game::notify("onBossDifficultyUpdate", difficulty, red, green);
        return;
    }

    const uint32_t lowestDifficulty = msg->getU32();
    const uint8_t spinnerLocked = msg->getU8();
    const uint16_t raceId = msg->getU16();
    const uint16_t selectedDifficulty = msg->getU16();
    const uint16_t groupHighest = msg->getU16();
    const uint16_t personalHighest = msg->getU16();
    const uint32_t badLuck = msg->getU32();

    std::vector<std::string> banners;
    banners.reserve(5);
    for (int i = 0; i < 5; ++i)
        banners.push_back(msg->getString());

    msg->getU16();
    std::vector<std::string> red;
    for (int i = 0, n = msg->getU8(); i < n; ++i)
        red.push_back(msg->getString());
    std::vector<std::string> green;
    for (int i = 0, n = msg->getU8(); i < n; ++i)
        green.push_back(msg->getString());

    tibia::game::notify("onBossDifficultySelection", lowestDifficulty, spinnerLocked == 0, raceId,
                          selectedDifficulty, groupHighest, personalHighest, badLuck, banners, red, green);
}

// 0x5F - parse destiny wheel window
void ProtocolGame::parseOpenWheelWindow(const InputMessagePtr& msg)
{
    // Player ID
    uint32_t playerId = msg->getU32();
    g_logger.debug(std::format("[Wheel C++ Parse] parseOpenWheelWindow -> playerId={}", playerId));

    // CanView
    uint8_t canView = msg->getU8();
    g_logger.debug(std::format("[Wheel C++ Parse] canView={}", static_cast<int>(canView)));

    // Se não pode visualizar, encerra e dispara callback "vazio"
    if (!canView) {
        g_logger.debug("[Wheel C++ Parse] Player não pode abrir a Wheel of Destiny.");
        tibia::game::notify("onDestinyWheel",
            playerId, canView, 0, 0, 0, 0,
            std::vector<uint16_t>(), std::vector<uint16_t>(),
            std::vector<uint16_t>(), std::vector<GemData>(),
            std::map<uint8_t, uint8_t>(), std::map<uint8_t, uint8_t>(), 0,
            std::vector<uint16_t>(), 0, 0);
        return;
    }

    // Estado de mudança + vocation
    uint8_t changeState = msg->getU8();
    uint8_t vocationId = msg->getU8();
    g_logger.debug(std::format("[Wheel C++ Parse] changeState={} vocationId={}",
        static_cast<int>(changeState), static_cast<int>(vocationId)));

    // Pontos
    uint16_t points = msg->getU16();
    uint16_t extraPoints = msg->getU16();

    g_logger.debug(std::format("[Wheel C++ Parse] points={} extraPoints={}",
        static_cast<int>(points),
        static_cast<int>(extraPoints)));

    // Points por slot (37 slots fixos, igual ao servidor de 0 a 36)
    std::vector<uint16_t> pointInvested;
    pointInvested.reserve(36);

    for (int i = 0; i < 36; ++i) {
        uint16_t slotPoints = msg->getU16();
        pointInvested.push_back(slotPoints);
    }

    g_logger.debug(std::format("[Wheel C++ Parse] pointInvested ({} slots)", static_cast<int>(pointInvested.size())));

    // Log detalhado de cada slot
    for (int i = 0; i < static_cast<int>(pointInvested.size()); ++i) {
        g_logger.debug(std::format("  [Slot {:>2}] points={}", i, pointInvested[i]));
    }

    // Promotion scrolls
    std::vector<uint16_t> usedPromotionScrolls;
    std::vector<uint16_t> promotionScrollPoints;
    uint16_t scrollCount = msg->getU16();
    g_logger.debug(std::format("[Wheel C++ Parse] scrollCount={}", static_cast<int>(scrollCount)));

    for (uint16_t i = 0; i < scrollCount; ++i) {
        uint16_t itemId = msg->getU16();
        // Em protocolos 1500+ o servidor envia 1 byte extra (extraPoints)
        uint8_t extraPoints = 0;
        if (tibia::game::getProtocolVersion() >= 1500 && msg->getUnreadSize() > 0) {
            extraPoints = msg->getU8();
        }
        usedPromotionScrolls.push_back(itemId);
        promotionScrollPoints.push_back(extraPoints);

        g_logger.debug(std::format("  [Scroll {}] id={} extraPoints={}",
            static_cast<int>(i),
            static_cast<int>(itemId),
            static_cast<int>(extraPoints)));
    }

    uint8_t hasMonkQuest = 0;
    if (tibia::game::cipFeature(tibia::client::GameVocationMonk) && msg->getUnreadSize() > 0) {
        hasMonkQuest = msg->getU8();
        g_logger.debug(std::format("[Wheel C++ Parse] hasMonkQuest lido (valor={})", static_cast<int>(hasMonkQuest)));
    }
    uint16_t huntingTaskPoints = 0;
    if (tibia::game::cipFeature(tibia::client::GameTaskboard)) {
        huntingTaskPoints = msg->getU16(); // getExtraPointsFromHuntingTaskShop
    }
    // Gems ativas (equipadas)
    std::vector<uint16_t> equipedGems;
    uint8_t activeGemCount = msg->getU8();
    g_logger.debug(std::format("[Wheel C++ Parse] activeGemCount={}", static_cast<int>(activeGemCount)));
    for (uint8_t i = 0; i < activeGemCount; ++i) {
        uint16_t gemIndex = msg->getU16();
        equipedGems.push_back(gemIndex);
        g_logger.debug(std::format("  [ActiveGem {}] index={}", static_cast<int>(i), static_cast<int>(gemIndex)));
    }

    // Gems reveladas (atelier)
    std::vector<GemData> atelierGems;
    uint16_t revealedCount = msg->getU16();
    g_logger.debug(std::format("[Wheel C++ Parse] revealedGemCount={}", static_cast<int>(revealedCount)));

    for (uint16_t i = 0; i < revealedCount; ++i) {
        GemData gem;
        gem.gemID = msg->getU16();          // ID único da gema
        gem.locked = msg->getU8();          // Status bloqueada/desbloqueada
        gem.gemDomain = msg->getU8();       // Afinidade elemental
        gem.gemType = msg->getU8();         // Qualidade (Lesser, Regular, Greater, Supreme)
        gem.lesserBonus = msg->getU8();     // Primeiro modificador

        if (gem.gemType >= tibia::client::WheelGemQuality_Regular)
            gem.regularBonus = msg->getU8();
        if (gem.gemType >= tibia::client::WheelGemQuality_Greater)
            gem.supremeBonus = msg->getU8();

        atelierGems.push_back(gem);

        g_logger.debug(std::format(
            "  [RevealedGem {:02}] id={} locked={} domain={} type={} lesser={} regular={} supreme={}",
            static_cast<int>(i),
            static_cast<int>(gem.gemID),
            static_cast<int>(gem.locked),
            static_cast<int>(gem.gemDomain),
            static_cast<int>(gem.gemType),
            static_cast<int>(gem.lesserBonus),
            static_cast<int>(gem.regularBonus),
            static_cast<int>(gem.supremeBonus)
        ));
    }

    // Basic upgrades
    std::map<uint8_t, uint8_t> basicUpgraded;
    uint8_t basicCount = msg->getU8(); // geralmente 0x2E (46)
    g_logger.debug(std::format("[Wheel C++ Parse] basicUpgraded count={}", static_cast<int>(basicCount)));
    for (uint8_t i = 0; i < basicCount; ++i) {
        uint8_t pos = msg->getU8();
        uint8_t val = msg->getU8();
        basicUpgraded[pos] = val;
        g_logger.debug(std::format("  [BasicUpgrade {}] pos={} val={}",
            static_cast<int>(i), static_cast<int>(pos), static_cast<int>(val)));
    }

    // Supreme upgrades
    std::map<uint8_t, uint8_t> supremeUpgraded;
    uint8_t supCount = msg->getU8(); // geralmente 0x17 (23)
    g_logger.debug(std::format("[Wheel C++ Parse] supremeUpgraded count={}", static_cast<int>(supCount)));
    for (uint8_t i = 0; i < supCount; ++i) {
        uint8_t pos = msg->getU8();
        uint8_t val = msg->getU8();
        supremeUpgraded[pos] = val;
        g_logger.debug(std::format("  [SupremeUpgrade {}] pos={} val={}",
            static_cast<int>(i), static_cast<int>(pos), static_cast<int>(val)));
    }

    // Canary 15.10+ appends earnedFromAchievements; CrystalServer does not. Never drain the
    // unread bytes here: batched follow-up opcodes share this InputMessage.
    uint8_t earnedFromAchievements = 0;
    if (tibia::game::cipFeature(tibia::client::GameWheelAchievementPoints))
        earnedFromAchievements = msg->getU8();

    // Callback Lua
    tibia::game::notify("onDestinyWheel",
        playerId, canView, changeState, vocationId,
        points, extraPoints, pointInvested,
        usedPromotionScrolls, equipedGems, atelierGems,
        basicUpgraded, supremeUpgraded, earnedFromAchievements,
        promotionScrollPoints, huntingTaskPoints, hasMonkQuest
    );
}

void ProtocolGame::parseAttachedPaperdoll(const InputMessagePtr& msg) {
    const uint32_t id = msg->getU32();
    const auto& paperdoll = getPaperdoll(msg);
    tibia::creatures::protocolAttachPaperdoll(id, paperdoll);
}
void ProtocolGame::parseDetachPaperdoll(const InputMessagePtr& msg) {
    const uint32_t id = msg->getU32();
    const bool bySlot = msg->getU8();
    const uint16_t idOrSlot = msg->getU16();

    if (bySlot)
        tibia::creatures::protocolDetachPaperdollByPriority(id, static_cast<uint8_t>(idOrSlot));
    else
        tibia::creatures::protocolDetachPaperdollById(id, idOrSlot);
}

PaperdollPtr ProtocolGame::getPaperdoll(const InputMessagePtr& msg) const {
    uint16_t id = msg->getU16();
    uint8_t slot = msg->getU8();
    uint8_t color = msg->getU8();
    uint8_t head = msg->getU8();
    uint8_t body = msg->getU8();
    uint8_t legs = msg->getU8();
    uint8_t feet = msg->getU8();
    const auto& shader = msg->getString();

    auto paperdoll = g_paperdolls.getById(id);
    if (!paperdoll) return nullptr;

    paperdoll = paperdoll->clone();
    if (slot != UINT8_MAX)
        paperdoll->setPriority(slot);

    if (color != 0)
        paperdoll->setColor(color);

    if (head != 0)
        paperdoll->setHeadColor(head);

    if (body != 0)
        paperdoll->setBodyColor(body);

    if (legs != 0)
        paperdoll->setLegsColor(legs);

    if (feet != 0)
        paperdoll->setFeetColor(feet);

    if (!shader.empty())
        paperdoll->setShader(shader);

    return paperdoll;
}
