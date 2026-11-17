#pragma once

#include "sound/soundmanager.h"

#include <cip_gen.h>

namespace tibia {
namespace sound {

/// CIP sound::TSoundDecompressorThreadpool: converts sound data on the global
/// thread pool and reports the result on the owner's thread.
class TSoundDecompressorThreadpool final : public cip::tibia::sound::TSoundDecompressorThreadpool
{
    Q_OBJECT
public:
    explicit TSoundDecompressorThreadpool(QObject* parent = nullptr);

    /// v12: `soundData` converted to `target` as playable data.
    void convertToPlayableSoundData(TSoundEffectID soundId, const TSoundData& soundData, ESoundLoadTarget target);
    /// v13: `soundData` converted to `target` as source data.
    void convertSoundData(TSoundEffectID soundId, const TSoundData& soundData, ESoundLoadTarget target);

    static TPlayableSoundData toPlayableSoundData(const TSoundData& soundData, ESoundLoadTarget target);
    static TSoundData toSoundData(const TSoundData& soundData, ESoundLoadTarget target);

Q_SIGNALS:
    void convertingToPlayableSoundDataFinished(tibia::sound::TSoundEffectID soundId,
                                               const tibia::sound::TPlayableSoundData& playableSoundData);
    void convertingSoundDataFinished(tibia::sound::TSoundEffectID soundId, const tibia::sound::TSoundData& soundData);
};

}
}
