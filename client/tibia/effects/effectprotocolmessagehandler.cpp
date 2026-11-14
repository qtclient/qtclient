#include "effects/effectprotocolmessagehandler.h"

#include "appearances/appearanceinstance.h"
#include "client/magiceffectopacity.h"
#include "effects/effectstorage.h"
#include "framework/core/clock.h"
#include "protocol/protocolciptypes.h"
#include "sound/soundplayback.h"
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

appearances::TAppearanceInstancePtr makeEffect(const uint16_t effectId)
{
    const uint16_t drawId = game::resolveEffectId(effectId);
    if (!game::isValidDatId(drawId, ThingCategoryEffect))
        return nullptr;
    const auto appearance = appearances::TAppearanceInstanceFactory::fromThingType(game::rawThingType(drawId, ThingCategoryEffect));
    auto* effect = dynamic_cast<appearances::TEffectInstance*>(appearance.get());
    if (!effect)
        return nullptr;
    if (const auto* variant = game::findEffectVariant(effectId)) {
        effect->setUnderCreatures(variant->underCreatures);
        effect->setVariantId(effectId);
    }
    return appearance;
}

}

void TEffectProtocolMessageHandler::handleGraphicalEffectsMessage(const GameserverMessageGraphicalEffects& message)
{
    const qint64 now = static_cast<qint64>(g_clock.millis());
    worldmap::TWorldMapStorage* map = worldmap::TWorldMapStorage::ensureStorage();
    for (const auto& entry : message.entries()) {
        const Position at = positionOf(entry.coordinate());
        switch (entry.kind()) {
        case 0: {
            if (!visible(entry.source()))
                break;
            const auto appearance = makeEffect(static_cast<uint16_t>(entry.appearance_id()));
            auto* effect = dynamic_cast<appearances::TEffectInstance*>(appearance.get());
            if (!effect)
                break;
            effect->setSource(static_cast<client::MagicEffectSources>(entry.source()));
            effect->start(at, now, entry.delay());
            map->addEffectAppearance(appearance, at);
            break;
        }
        case 1: {
            const auto id = static_cast<uint16_t>(entry.appearance_id());
            if (!visible(entry.source()) || !game::isValidDatId(id, ThingCategoryMissile))
                break;
            const auto appearance = appearances::TAppearanceInstanceFactory::fromThingType(game::rawThingType(id, ThingCategoryMissile));
            auto* missile = dynamic_cast<appearances::TMissileEffectInstance*>(appearance.get());
            if (!missile)
                break;
            missile->setSource(static_cast<client::MagicEffectSources>(entry.source()));
            missile->setPath(at, positionOf(entry.destination()), now);
            map->addMissileAppearance(appearance);
            break;
        }
        case 2:
            sound::playProtocolSoundMain(static_cast<uint8_t>(entry.sound_source()), static_cast<uint16_t>(entry.sound_id()), at);
            break;
        case 3:
            sound::playProtocolSoundSecondary(static_cast<uint8_t>(entry.sound_enum()), static_cast<uint8_t>(entry.sound_source()),
                                              static_cast<uint16_t>(entry.sound_id()), at);
            break;
        default:
            break;
        }
    }
}

void TEffectProtocolMessageHandler::handleRemoveGraphicalEffectMessage(const GameserverMessageRemoveGraphicalEffect& message)
{
    TEffectStorage::instance().remove(game::resolveEffectId(static_cast<uint16_t>(message.appearance_id())),
                                      positionOf(message.coordinate()));
}

void TEffectProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray&, const bool applyClassic)
{
    if (applyClassic || !protocol::nativeApplySkipsClassic(opcode))
        return;
    const auto* shadow = protocol::lastShadowServerMessage();
    if (!shadow)
        return;
    if (opcode == 131 && shadow->HasExtension(GameserverMessageGraphicalEffects::gameserver_message_graphical_effects))
        handleGraphicalEffectsMessage(shadow->GetExtension(GameserverMessageGraphicalEffects::gameserver_message_graphical_effects));
    else if (opcode == 132 && shadow->HasExtension(GameserverMessageRemoveGraphicalEffect::gameserver_message_remove_graphical_effect))
        handleRemoveGraphicalEffectMessage(
            shadow->GetExtension(GameserverMessageRemoveGraphicalEffect::gameserver_message_remove_graphical_effect));
}

}
