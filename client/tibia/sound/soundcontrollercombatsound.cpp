#include "sound/soundcontrollercombatsound.h"

#include <QDebug>
#include <QList>

#include <algorithm>
#include <cstdlib>
#include <unordered_map>

namespace tibia::sound {

namespace {

uint8_t distanceModifier(const int max, const int step, const int distance)
{
    const int value = max - step * distance;
    if (value < 0)
        return 0;
    return static_cast<uint8_t>(std::min(value, 100));
}

}

void TSoundControllerCombatSound::playEffectSounds(const std::vector<TSoundToPlay>& sounds)
{
    const TSoundSettings& settings = configuration()->settings();
    // One player per sound: the loudest request wins.
    std::unordered_map<TSoundEffectID, TSoundEffectParameters> loudest;
    std::vector<TSoundEffectID> order;
    for (const TSoundToPlay& sound : sounds) {
        TSoundEffectRequest request;
        request.effectId = sound.effectId;
        request.source = sound.source;
        TSoundEffectParameters parameters = effectParameters(request);
        const int dx = std::abs(sound.position.x - m_playerPosition.x);
        const int dy = std::abs(sound.position.y - m_playerPosition.y);
        const int dz = std::abs(sound.position.z - m_playerPosition.z);
        const int distance = std::max(dx, dy) + dz * settings.floorDistance;
        parameters.volumeModifiers.distance =
            distanceModifier(settings.distanceVolumeMax, settings.distanceVolumeStep, distance);
        const auto it = loudest.find(sound.effectId);
        if (it == loudest.end()) {
            loudest.emplace(sound.effectId, parameters);
            order.push_back(sound.effectId);
        } else if (it->second.volumeModifiers.effectiveVolume() < parameters.volumeModifiers.effectiveVolume()) {
            it->second = parameters;
        }
    }

    QList<QString> skipped;
    for (const TSoundEffectID effectId : order) {
        const TSoundEffectParameters& parameters = loudest.at(effectId);
        if (freePlayerCount() == 0 && skipped.size() < 20) {
            skipped.append(QString::number(effectId));
            continue;
        }
        if (std::optional<TSoundPlayerToken> token = playSingleShot(parameters))
            keepSingleShot(std::move(*token));
    }
    if (!skipped.isEmpty())
        qDebug() << "Could not play" << skipped.size() << "numeric sound effects because no player was available"
                 << skipped;
}

void TSoundControllerCombatSound::stopAll()
{
    for (const TSoundPlayerToken& token : singleShots()) {
        if (token.player())
            token.player()->stop();
    }
}

}
