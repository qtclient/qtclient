#pragma once

#include "sound/soundmanager.h"

#include <cip_gen.h>

#include <optional>
#include <variant>

namespace tibia {
namespace sound {

/// A sound's data at a step of loading: a file, its bytes, the decoded
/// samples, or the buffer that plays them.
using TLoadingSoundData = std::variant<TSoundFile, QByteArray, TRawSoundSamples, TSoundBuffer>;

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

    /// `data` as playable data of `target`: Unavailable without data; the
    /// data itself when it already is in that form (raw samples as a buffer);
    /// else Loading, the conversion runs (convertToPlayableSoundData).
    TPlayableSoundData playableSoundData(TSoundEffectID soundId, const TSoundData& data, ESoundLoadTarget target);
    /// `data` as source data of `target`: Unavailable without data; the
    /// data itself when it already is in that form (raw samples for a buffer);
    /// else Loading, the conversion runs (convertSoundData).
    TSoundData soundData(TSoundEffectID soundId, const TSoundData& data, ESoundLoadTarget target);

    /// `soundData` loaded up to `target` as playable data: a file, bytes or
    /// a buffer; Unavailable without data or when it stays raw samples.
    static TPlayableSoundData toPlayableSoundData(const TSoundData& soundData, ESoundLoadTarget target);
    /// `soundData` loaded up to `target` as source data: a file, bytes or raw
    /// samples; Unavailable without data or when it became a buffer.
    static TSoundData toSoundData(const TSoundData& soundData, ESoundLoadTarget target);

    /// `soundData` loaded up to `target` (loadUpTo); nothing for a data state.
    static std::optional<TLoadingSoundData> loadSoundData(const TSoundData& soundData, ESoundLoadTarget target);
    /// `data` stepped forward until it has the form of `target`. The data
    /// itself when `target` is not a later form, or when a step fails.
    static TLoadingSoundData loadUpTo(const TLoadingSoundData& data, ESoundLoadTarget target);
    /// One loading step: a file is read, bytes are decoded, samples become a
    /// buffer. Nothing when it fails, or for a buffer (the last form).
    static std::optional<TLoadingSoundData> loadStep(const TLoadingSoundData& data);

Q_SIGNALS:
    void convertingToPlayableSoundDataFinished(tibia::sound::TSoundEffectID soundId,
                                               const tibia::sound::TPlayableSoundData& playableSoundData);
    void convertingSoundDataFinished(tibia::sound::TSoundEffectID soundId, const tibia::sound::TSoundData& soundData);
};

}
}
