#include "effects/effectprotocolmessagehandler.h"

#include "appearances/appearanceinstance.h"
#include "client/magiceffectopacity.h"
#include "effects/effectstorage.h"
#include "clockservice.h"
#include "protocol/protocolciptypes.h"
#include "sound/soundstorage.h"
#include "worldmap/worldmapstorage.h"

#include <tibia/game/thingsession_forward.h>

#include <protocol.pb.h>

#include <stdexcept>

namespace tibia::effects {

namespace {

using protobuf::protocol::GameserverMessageGraphicalEffects;
using protobuf::protocol::GameserverMessageRemoveGraphicalEffect;

Position positionOf(const protobuf::protocol::ProtocolCoordinate& c)
{
    return Position(static_cast<int32_t>(c.x()), static_cast<int32_t>(c.y()), static_cast<uint8_t>(c.z()));
}

/// The protocol's effect source (0 to 4); any other value is an error.
uint32_t effectSource(const uint32_t source)
{
    if (source <= 4)
        return source;
    throw std::runtime_error("unhandled enum value");
}

/// An effect from a source shown at less than 1% is not created.
bool visible(const uint32_t source)
{
    return client::magicEffectOpacity(static_cast<client::MagicEffectSources>(effectSource(source))) >= 0.01f;
}

/// Effect `effectId` on `field` (a pack effect variant draws its base effect);
/// an effect with a delay is wrapped and draws once the delay has passed.
std::shared_ptr<appearances::TEffectInstance> makeEffect(const uint16_t effectId, const Position& field,
                                                         const uint8_t source, const qint64 now, const qint64 delay)
{
    const uint16_t drawId = game::resolveEffectId(effectId);
    if (!game::isValidDatId(drawId, ThingCategoryEffect))
        return nullptr;
    auto effect = appearances::TAppearanceInstanceFactory::createFieldEffect(drawId, field, source, now);
    if (const auto* variant = game::findEffectVariant(effectId)) {
        effect->setUnderCreatures(variant->underCreatures);
        effect->setVariantId(effectId);
    }
    if (delay == 0)
        return effect;
    return std::make_shared<appearances::TDelayedEffectInstance>(effect, now, delay);
}

}

TEffectProtocolMessageHandler::TEffectProtocolMessageHandler(std::shared_ptr<TEffectStorage> effectStorage,
                                                             std::shared_ptr<worldmap::TWorldMapStorage> worldMapStorage,
                                                             std::shared_ptr<sound::TSoundStorage> soundStorage,
                                                             std::shared_ptr<shared::IClockService> clockService)
    : m_effectStorage(std::move(effectStorage))
    , m_worldMapStorage(std::move(worldMapStorage))
    , m_soundStorage(std::move(soundStorage))
    , m_clockService(std::move(clockService))
{
}

void TEffectProtocolMessageHandler::handleGraphicalEffectsMessage(const GameserverMessageGraphicalEffects& message)
{
    // Host: effects start on the frame clock the renderer animates them with
    // (the session clock of a sessiondump runs on the dump's time base).
    const qint64 now = static_cast<qint64>(tibia::shared::TClockService::frameMillis());
    worldmap::TWorldMapStorage* map = m_worldMapStorage.get();
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
    if (!sounds.empty() && m_soundStorage)
        m_soundStorage->setSoundsToPlay(std::move(sounds), now);
}

void TEffectProtocolMessageHandler::handleRemoveGraphicalEffectMessage(const GameserverMessageRemoveGraphicalEffect& message)
{
    m_effectStorage->remove(game::resolveEffectId(static_cast<uint16_t>(message.appearance_id())),
                                      positionOf(message.coordinate()));
}

}
