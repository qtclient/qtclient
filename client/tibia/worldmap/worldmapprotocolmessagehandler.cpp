#include "worldmap/worldmapprotocolmessagehandler.h"

#include <tibia/world/localplayer.h>
#include "protocol/protocolciptypes.h"
#include "worldmap/worldmapstorage.h"

#include <tibia/game/gamenotify.h>
#include <tibia/world/map.h>
#include <tibia/world/world.h>

#include <protocol.pb.h>

namespace tibia::worldmap {

namespace {

TWorldmapProtocolMessageHandler* g_liveWorldmapProtocolHandler = nullptr;

using ::tibia::protobuf::protocol::GameserverMessageAmbientLight;

} // namespace

TWorldmapProtocolMessageHandler* TWorldmapProtocolMessageHandler::liveInstance()
{
    return g_liveWorldmapProtocolHandler;
}

void TWorldmapProtocolMessageHandler::setLiveInstance(TWorldmapProtocolMessageHandler* handler)
{
    g_liveWorldmapProtocolHandler = handler;
}

void TWorldmapProtocolMessageHandler::clearField(const Position& position)
{
    tibia::world::fields().cleanTile(position);
}

void TWorldmapProtocolMessageHandler::applyThingToField(const ThingPtr& thing, const Position& position, const int stackPos)
{
    // Phase 9B.1: play-path field mutate is appearance + creatureId only.
    // Item decode sets lastParsedItemAppearance and returns nullptr from getThing;
    // the only remaining ThingPtr is LocalPlayer session keep-alive.
    if (!thing)
        return;
    if (thing->isLocalPlayer())
        thing->static_self_cast<LocalPlayer>()->resetPreWalk();
    if (thing->isCreature()) {
        tibia::world::fields().placeCreatureId(thing->getId(), position, stackPos);
        return;
    }
    // Non-creature ThingPtr must not land on the field stack — use applyAppearanceToField.
}

void TWorldmapProtocolMessageHandler::applyAppearanceToField(const appearances::TAppearanceInstancePtr& appearance,
                                                             const Position& position, const int stackPos)
{
    tibia::world::fields().placeAppearance(appearance, position, stackPos);
}

void TWorldmapProtocolMessageHandler::applyCreatureIdToField(const uint32_t creatureId, const Position& position,
                                                             const int stackPos)
{
    tibia::world::fields().placeCreatureId(creatureId, position, stackPos);
}

void TWorldmapProtocolMessageHandler::handleAmbientLightMessage(const GameserverMessageAmbientLight& AmbientLightMessage)
{
    const auto& oldLight = g_map.getLight();
    const auto intensity = static_cast<uint8_t>(AmbientLightMessage.intensity());
    const auto color = static_cast<uint8_t>(AmbientLightMessage.color());

    g_map.setLight({ intensity, color });

    if (oldLight.color != color || oldLight.intensity != intensity)
        tibia::game::notify("onWorldLightChange", g_map.getLight(), oldLight);
}

void TWorldmapProtocolMessageHandler::onServerMessageEnqueued(int opcode, const QByteArray& /*classicFrame*/,
                                                              bool applyClassic)
{
    // Peeled opcodes: native H applies when the classic bridge will not
    // (applyClassic=false on the structured TR→Q path). Measure-fail still
    // classic-applies first then enqueues with applyClassic=false — re-applying
    // the same AmbientLight values here is idempotent (no spurious notify).
    if (applyClassic || !protocol::nativeApplySkipsClassic(opcode))
        return;

    switch (opcode) {
    case 130: {
        const auto* shadow = protocol::lastShadowServerMessage();
        if (!shadow || !shadow->HasExtension(GameserverMessageAmbientLight::gameserver_message_ambient_light))
            return;
        handleAmbientLightMessage(
            shadow->GetExtension(GameserverMessageAmbientLight::gameserver_message_ambient_light));
        break;
    }
    default:
        break;
    }
}

TWorldmapProtocolMessageHandler::TWorldmapProtocolMessageHandler(TWorldMapStorage* storage, QObject* parent)
    : cip::tibia::worldmap::TWorldmapProtocolMessageHandler(parent)
    , m_storage(storage)
{
}

void TWorldmapProtocolMessageHandler::onWindowChanged(const bool fullDescription)
{
    if (m_storage && fullDescription)
        m_storage->notifyStoredVolumeChanged();
}

void TWorldmapProtocolMessageHandler::onTileChanged(const Position& position)
{
    if (m_storage)
        m_storage->notifyCoordinateChanged(position);
}

}
