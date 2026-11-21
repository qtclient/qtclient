#include "effects/effectprotocolmessagehandler.h"

#include "appearances/appearanceinstance.h"
#include "client/magiceffectopacity.h"
#include "effects/effectstorage.h"
#include "clockservice.h"
#include "protocol/protocolciptypes.h"
#include "game/gamesessionbase.h"
#include "game/currentgamesession.h"
#include "sound/soundstorage.h"
#include "worldmap/worldmapstorage.h"

#include <tibia/game/thingsession_forward.h>

#include <protocol.pb.h>

namespace tibia::effects {

namespace {

using protobuf::protocol::GameserverMessageGraphicalEffects;
using protobuf::protocol::GameserverMessageRemoveGraphicalEffect;

Position positionOf(const protobuf::protocol::ProtocolCoordinate& c)
{
    return Position(static_cast<int32_t>(c.x()), static_cast<int32_t>(c.y()), static_cast<uint8_t>(c.z()));
}

/// An effect from a source shown at less than 1% is not created.
bool visible(const uint32_t source)
{
    return client::magicEffectOpacity(static_cast<client::MagicEffectSources>(source)) >= 0.01f;
}

/// Effect `effectId` on `field` (a pack effect variant draws its base effect).
std::shared_ptr<appearances::TFieldEffectInstance> makeEffect(const uint16_t effectId, const Position& field,
                                                              const uint8_t source, const qint64 now, const qint64 delay)
{
    const uint16_t drawId = game::resolveEffectId(effectId);
    if (!game::isValidDatId(drawId, ThingCategoryEffect))
        return nullptr;
    auto effect = appearances::TAppearanceInstanceFactory::createFieldEffect(drawId, field, source, now, delay);
    if (const auto* variant = game::findEffectVariant(effectId)) {
        effect->setUnderCreatures(variant->underCreatures);
        effect->setVariantId(effectId);
    }
    return effect;
}

}

void TEffectProtocolMessageHandler::handleGraphicalEffectsMessage(const GameserverMessageGraphicalEffects& message)
{
    const qint64 now = static_cast<qint64>(tibia::shared::TClockService::frameMillis());
    worldmap::TWorldMapStorage* map = worldmap::TWorldMapStorage::ensureStorage();
    std::vector<sound::TSoundToPlay> sounds;
    for (const auto& entry : message.entries()) {
        const Position at = positionOf(entry.coordinate());
        switch (entry.kind()) {
        case 0: {
            if (!visible(entry.source()))
                break;
            if (const auto effect = makeEffect(static_cast<uint16_t>(entry.appearance_id()), at,
                                               static_cast<uint8_t>(entry.source()), now, entry.delay()))
                map->addEffectAppearance(effect, at);
            break;
        }
        case 1: {
            const auto id = static_cast<uint16_t>(entry.appearance_id());
            if (!visible(entry.source()) || !game::isValidDatId(id, ThingCategoryMissile))
                break;
            map->addMissileAppearance(appearances::TAppearanceInstanceFactory::createMissile(
                id, at, positionOf(entry.destination()), static_cast<uint8_t>(entry.source()), now));
            break;
        }
        case 2:
        case 3:
            sounds.push_back({ static_cast<uint16_t>(entry.sound_id()), at, static_cast<int>(entry.sound_source()), now });
            break;
        default:
            break;
        }
    }
    // The message's sound effects play together (CIP: TSoundStorage).
    if (!sounds.empty()) {
        if (game::TGameSessionBase* session = game::currentGameSession(); session && session->soundStorage())
            session->soundStorage()->setSoundsToPlay(std::move(sounds), now);
    }
}

void TEffectProtocolMessageHandler::handleRemoveGraphicalEffectMessage(const GameserverMessageRemoveGraphicalEffect& message)
{
    TEffectStorage::instance().remove(game::resolveEffectId(static_cast<uint16_t>(message.appearance_id())),
                                      positionOf(message.coordinate()));
}

}
