#pragma once

#include <cip_gen.h>

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>
#include <variant>
#include <string>
#include <vector>

namespace tibia {
namespace sound {

/// One audio file of the sound bank (CIP: protobuf Sound).
struct TSoundMetadata
{
    uint16_t id = 0;
    /// Highest-priority use of the file: 1 music, 2 ambience stream, 3 delayed
    /// ambience effect, 4 object ambience, 5 creature noise, 6 other effect.
    int memoryPriority = 0;
    /// From the priority (initialize): 1 decoded in memory, 2 streamed.
    int loadType = 0;
    int cacheType = 0;
    int keepLoaded = 0;
    QString fileName;
    QString originalFileName;
};

struct TSingleSoundEffectMetadata
{
    uint16_t soundId = 0;
};

struct TRandomSoundMetadata
{
    std::vector<uint16_t> soundIds;
};

using TSoundEffectSound = std::variant<std::monostate, TSingleSoundEffectMetadata, TRandomSoundMetadata>;

/// CIP: protobuf NumericSoundEffect.
struct TNumericSoundEffectMetadata
{
    uint16_t id = 0;
    float pitchMin = 1.0f;
    float pitchMax = 1.0f;
    float volumeMin = 1.0f;
    float volumeMax = 1.0f;
    TSoundEffectSound sound;
    int numericSoundType = 0;
};

struct TDelayedSoundEffectMetadata
{
    uint16_t numericSoundEffectId = 0;
    int64_t delayMs = 0;
};

/// CIP: protobuf AmbienceStream.
struct TAmbienceStreamMetadata
{
    uint16_t id = 0;
    uint16_t loopingSoundId = 0;
    std::vector<TDelayedSoundEffectMetadata> delayedEffects;
};

/// CIP: protobuf AmbienceObjectStream.
struct TAmbienceObjectStreamMetadata
{
    uint16_t id = 0;
    std::vector<uint16_t> countedAppearanceTypes;
    /// Object count -> looping sound.
    std::map<uint32_t, uint16_t> loopingSoundIdByCount;
    uint32_t maxSoundDistance = 10;
};

/// CIP: protobuf MusicTemplate.
struct TMusicTemplateMetadata
{
    uint16_t id = 0;
    uint16_t soundId = 0;
    int musicType = 0;
};

/// The parsed sound bank (CIP: sound::TSoundMetadataManager).
class TSoundMetadataManager final : public cip::tibia::sound::TSoundMetadataManager
{
    Q_OBJECT
public:
    /// delayFactor scales the ambience stream effect delays.
    explicit TSoundMetadataManager(QObject* parent = nullptr, float delayFactor = 1.0f);

    /// v12: replaces all metadata from a sounds-*.dat bank. Throws
    /// std::invalid_argument on malformed data.
    void initialize(const QByteArray& bankData);
    /// Host: reads catalog-sound.json in `directory` and initialises from its
    /// sounds bank (CipSoft's asset catalog does this). False without a bank.
    bool loadBank(const std::string& directory);
    /// v13: derives each sound's load type from its memory priority.
    void assignLoadTypes();

    /// v14: the id of the sound stored as fileName, 0 if none.
    uint16_t soundIdByFileName(const QString& fileName) const;
    /// v15: the original file name of the sound stored as fileName.
    QString originalFileName(const QString& fileName) const;
    /// v16
    std::optional<TSoundMetadata> soundByFileName(const QString& fileName) const;
    /// v17
    std::optional<TSoundMetadata> sound(uint16_t id) const;
    /// v18
    std::optional<TNumericSoundEffectMetadata> numericSoundEffect(uint16_t id) const;
    /// v19
    std::optional<TAmbienceStreamMetadata> ambienceStream(uint16_t id) const;
    /// v20
    std::optional<TMusicTemplateMetadata> musicTemplate(uint16_t id) const;
    /// v21: the title music.
    std::optional<TMusicTemplateMetadata> titleMusicTemplate() const;
    /// v22
    std::optional<TAmbienceObjectStreamMetadata> ambienceObjectStream(const uint16_t& id) const;

    const std::unordered_map<uint16_t, TSoundMetadata>& sounds() const { return m_sounds; }
    const std::unordered_map<uint16_t, TNumericSoundEffectMetadata>& numericSoundEffects() const { return m_numericSoundEffects; }
    const std::unordered_map<uint16_t, TAmbienceStreamMetadata>& ambienceStreams() const { return m_ambienceStreams; }
    const std::unordered_map<uint16_t, TMusicTemplateMetadata>& musicTemplates() const { return m_musicTemplates; }
    const std::unordered_map<uint16_t, TAmbienceObjectStreamMetadata>& ambienceObjectStreams() const { return m_ambienceObjectStreams; }

Q_SIGNALS:
    /// A sound file to load ahead of use (CIP passes a TAsset).
    void newAssetDataToLoad(const QString& fileName, bool streamed, bool prioritized);

private:
    /// Raises the memory priority of a known sound.
    void setMemoryPriority(uint16_t soundId, int priority);
    void setMemoryPriority(const TSoundEffectSound& sound, int priority);

    std::unordered_map<uint16_t, TSoundMetadata> m_sounds;
    std::unordered_map<uint16_t, TNumericSoundEffectMetadata> m_numericSoundEffects;
    std::unordered_map<uint16_t, TAmbienceStreamMetadata> m_ambienceStreams;
    std::unordered_map<uint16_t, TMusicTemplateMetadata> m_musicTemplates;
    std::unordered_map<uint16_t, TAmbienceObjectStreamMetadata> m_ambienceObjectStreams;
    float m_delayFactor = 1.0f;
};

}
}
