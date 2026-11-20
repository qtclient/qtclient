#include "sound/soundmetadatamanager.h"

#include "shared/tibiafilesystemhelper.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <stdexcept>

#ifdef FRAMEWORK_PROTOBUF
#include <sounds.pb.h>
#endif

namespace tibia::sound {

namespace {

constexpr int kPriorityMusic = 1;
constexpr int kPriorityAmbienceStream = 2;
constexpr int kPriorityDelayedEffect = 3;
constexpr int kPriorityObjectAmbience = 4;
constexpr int kPriorityCreatureNoise = 5;
constexpr int kPriorityEffect = 6;

constexpr int kNumericSoundTypeCreatureNoise = 5;
constexpr int kMusicTypeTitle = 3;

QString fromStd(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

}

bool TSoundMetadataManager::loadBank(const std::string& directory)
{
    const auto read = [](const std::string& path) -> std::optional<std::string> {
        try {
            return tibia::files::data().readFileContents(path);
        } catch (...) {
            return std::nullopt;
        }
    };
    const auto catalog = read(tibia::files::data().guessFilePath(directory + "catalog-sound", "json"));
    if (!catalog)
        return false;
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromStdString(*catalog));
    for (const QJsonValue& value : document.array()) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QLatin1String("type")).toString() != QLatin1String("sounds"))
            continue;
        const std::string file = entry.value(QLatin1String("file")).toString().toStdString();
        const auto bank = file.empty() ? std::nullopt : read(directory + file);
        if (!bank)
            return false;
        try {
            initialize(QByteArray::fromStdString(*bank));
        } catch (const std::exception&) {
            return false;
        }
        return !m_sounds.empty();
    }
    return false;
}

TSoundMetadataManager::TSoundMetadataManager(QObject* parent, const float delayFactor)
    : cip::tibia::sound::TSoundMetadataManager(parent)
    , m_delayFactor(delayFactor)
{
}

void TSoundMetadataManager::initialize(const QByteArray& bankData)
{
#ifndef FRAMEWORK_PROTOBUF
    Q_UNUSED(bankData);
    throw std::invalid_argument("TSoundMetadataManager::initialize malformed protobuf data");
#else
    tibia::protobuf::sound::Sounds bank;
    if (!bank.ParseFromArray(bankData.constData(), static_cast<int>(bankData.size())))
        throw std::invalid_argument("TSoundMetadataManager::initialize malformed protobuf data");

    m_sounds.clear();
    for (const auto& file : bank.sound()) {
        TSoundMetadata metadata;
        metadata.id = static_cast<uint16_t>(file.id());
        metadata.fileName = fromStd(file.filename());
        metadata.originalFileName = fromStd(file.original_filename());
        m_sounds[metadata.id] = std::move(metadata);
    }

    m_numericSoundEffects.clear();
    for (const auto& effect : bank.numeric_sound_effect()) {
        TNumericSoundEffectMetadata metadata;
        metadata.id = static_cast<uint16_t>(effect.id());
        if (effect.has_random_pitch()) {
            metadata.pitchMin = effect.random_pitch().min();
            metadata.pitchMax = effect.random_pitch().max();
        }
        if (effect.has_random_volume()) {
            metadata.volumeMin = effect.random_volume().min();
            metadata.volumeMax = effect.random_volume().max();
        }
        int priority = kPriorityEffect;
        if (effect.has_numeric_sound_type()) {
            metadata.numericSoundType = static_cast<int>(effect.numeric_sound_type());
            priority = metadata.numericSoundType == kNumericSoundTypeCreatureNoise ? kPriorityCreatureNoise : kPriorityEffect;
        }
        if (effect.has_random_sound_effect()) {
            TRandomSoundMetadata random;
            for (const uint32_t soundId : effect.random_sound_effect().random_sound_id()) {
                random.soundIds.push_back(static_cast<uint16_t>(soundId));
                setMemoryPriority(static_cast<uint16_t>(soundId), priority);
            }
            metadata.sound = std::move(random);
        } else if (effect.has_simple_sound_effect()) {
            const auto soundId = static_cast<uint16_t>(effect.simple_sound_effect().sound_id());
            metadata.sound = TSingleSoundEffectMetadata{ soundId };
            setMemoryPriority(soundId, priority);
        }
        m_numericSoundEffects[metadata.id] = std::move(metadata);
    }

    m_ambienceStreams.clear();
    for (const auto& stream : bank.ambience_stream()) {
        TAmbienceStreamMetadata metadata;
        metadata.id = static_cast<uint16_t>(stream.id());
        metadata.loopingSoundId = static_cast<uint16_t>(stream.looping_sound_id());
        setMemoryPriority(metadata.loopingSoundId, kPriorityAmbienceStream);
        for (const auto& delayed : stream.delayed_effects()) {
            const auto effectId = static_cast<uint16_t>(delayed.numeric_sound_effect_id());
            if (const auto effect = m_numericSoundEffects.find(effectId); effect != m_numericSoundEffects.end())
                setMemoryPriority(effect->second.sound, kPriorityDelayedEffect);
            const auto delayMs = static_cast<int64_t>(static_cast<float>(delayed.delay_seconds() * 1000) * m_delayFactor);
            metadata.delayedEffects.push_back({ effectId, delayMs });
        }
        m_ambienceStreams[metadata.id] = std::move(metadata);
    }

    m_musicTemplates.clear();
    for (const auto& music : bank.music_template()) {
        TMusicTemplateMetadata metadata;
        metadata.id = static_cast<uint16_t>(music.id());
        metadata.soundId = static_cast<uint16_t>(music.sound_id());
        metadata.musicType = static_cast<int>(music.music_type());
        setMemoryPriority(metadata.soundId, kPriorityMusic);
        m_musicTemplates[metadata.id] = metadata;
    }

    m_ambienceObjectStreams.clear();
    for (const auto& stream : bank.ambience_object_stream()) {
        TAmbienceObjectStreamMetadata metadata;
        metadata.id = static_cast<uint16_t>(stream.id());
        for (const uint32_t type : stream.counted_appearance_types())
            metadata.countedAppearanceTypes.push_back(static_cast<uint16_t>(type));
        for (const auto& effect : stream.sound_effects()) {
            const auto soundId = static_cast<uint16_t>(effect.looping_sound_id());
            setMemoryPriority(soundId, kPriorityObjectAmbience);
            metadata.loopingSoundIdByCount[effect.count()] = soundId;
        }
        if (stream.has_max_sound_distance())
            metadata.maxSoundDistance = stream.max_sound_distance();
        m_ambienceObjectStreams[metadata.id] = std::move(metadata);
    }

    assignLoadTypes();
    Q_EMIT metadataChanged();

    for (const auto& [id, sound] : m_sounds) {
        Q_UNUSED(id);
        if (sound.loadType != 1 && sound.loadType != 2)
            continue;
        const bool streamed = sound.memoryPriority == kPriorityEffect && sound.loadType == 2;
        Q_EMIT newAssetDataToLoad(sound.fileName, streamed, false);
    }
#endif
}

void TSoundMetadataManager::assignLoadTypes()
{
    for (auto& [id, sound] : m_sounds) {
        switch (sound.memoryPriority) {
        case 0:
            qCDebug(lcSound) << "sound" << sound.fileName << "has no memory priority";
            break;
        case kPriorityMusic:
        case kPriorityAmbienceStream:
            sound.loadType = 1;
            sound.cacheType = 1;
            break;
        case kPriorityDelayedEffect:
        case kPriorityObjectAmbience:
        case kPriorityCreatureNoise:
            sound.loadType = 1;
            sound.cacheType = 3;
            break;
        case kPriorityEffect:
            sound.loadType = 2;
            sound.cacheType = 3;
            break;
        default:
            break;
        }
        if (sound.cacheType == 3 && sound.loadType != 2)
            sound.keepLoaded = static_cast<unsigned>(sound.memoryPriority - 3) < 0xfffffffeU ? 1 : 0;
        else if (sound.cacheType != 0 || sound.loadType == 0)
            sound.keepLoaded = 1;
    }
}

void TSoundMetadataManager::setMemoryPriority(const uint16_t soundId, const int priority)
{
    const auto it = m_sounds.find(soundId);
    if (it == m_sounds.end())
        return;
    const int current = it->second.memoryPriority;
    if (current != priority && current != 0 && current != kPriorityEffect)
        qCDebug(lcSound) << "Sound" << soundId << "has already sound memory priority of" << current << ". It will be reset to"
                 << priority;
    it->second.memoryPriority = priority;
}

void TSoundMetadataManager::setMemoryPriority(const TSoundEffectSound& sound, const int priority)
{
    if (const auto* single = std::get_if<TSingleSoundEffectMetadata>(&sound)) {
        setMemoryPriority(single->soundId, priority);
    } else if (const auto* random = std::get_if<TRandomSoundMetadata>(&sound)) {
        for (const uint16_t soundId : random->soundIds)
            setMemoryPriority(soundId, priority);
    }
}

uint16_t TSoundMetadataManager::soundIdByFileName(const QString& fileName) const
{
    const auto found = soundByFileName(fileName);
    return found ? found->id : 0;
}

QString TSoundMetadataManager::originalFileName(const QString& fileName) const
{
    const auto found = soundByFileName(fileName);
    return found ? found->originalFileName : QString();
}

std::optional<TSoundMetadata> TSoundMetadataManager::soundByFileName(const QString& fileName) const
{
    for (const auto& [id, sound] : m_sounds) {
        Q_UNUSED(id);
        if (sound.fileName == fileName)
            return sound;
    }
    return std::nullopt;
}

std::optional<TSoundMetadata> TSoundMetadataManager::sound(const uint16_t id) const
{
    const auto it = m_sounds.find(id);
    if (it == m_sounds.end())
        return std::nullopt;
    return it->second;
}

std::optional<TNumericSoundEffectMetadata> TSoundMetadataManager::numericSoundEffect(const uint16_t id) const
{
    const auto it = m_numericSoundEffects.find(id);
    if (it == m_numericSoundEffects.end())
        return std::nullopt;
    return it->second;
}

std::optional<TAmbienceStreamMetadata> TSoundMetadataManager::ambienceStream(const uint16_t id) const
{
    const auto it = m_ambienceStreams.find(id);
    if (it == m_ambienceStreams.end())
        return std::nullopt;
    return it->second;
}

std::optional<TMusicTemplateMetadata> TSoundMetadataManager::musicTemplate(const uint16_t id) const
{
    const auto it = m_musicTemplates.find(id);
    if (it == m_musicTemplates.end())
        return std::nullopt;
    return it->second;
}

std::optional<TMusicTemplateMetadata> TSoundMetadataManager::titleMusicTemplate() const
{
    for (const auto& [id, music] : m_musicTemplates) {
        Q_UNUSED(id);
        if (music.musicType == kMusicTypeTitle)
            return music;
    }
    return std::nullopt;
}

std::optional<TAmbienceObjectStreamMetadata> TSoundMetadataManager::ambienceObjectStream(const uint16_t& id) const
{
    const auto it = m_ambienceObjectStreams.find(id);
    if (it == m_ambienceObjectStreams.end())
        return std::nullopt;
    return it->second;
}

}
