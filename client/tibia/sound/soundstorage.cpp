#include "soundstorage.h"

#include "soundplayback.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <tibia/world/map.h>
#include <tibia/world/tile.h>
#include <framework/core/clock.h>
#include <framework/core/gameevents.h>
#include <framework/core/logger.h>
#include <framework/core/resourcemanager.h>

#ifdef FRAMEWORK_PROTOBUF
#include <sounds.pb.h>
#endif

#include <nlohmann/json.hpp>

#include <QByteArray>
#include <QEvent>
#include <QGuiApplication>
#include <QLoggingCategory>

#if defined(Q_OS_WASM)
#include <emscripten.h>
#endif

#include <algorithm>
#include <any>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <sstream>
#include <unordered_set>

#include "vendor/miniaudio.h"

Q_LOGGING_CATEGORY(lcSoundStorage, "tibia.sound.storage")

namespace tibia::sound {

namespace {

TSoundStorage* g_soundStorage = nullptr;

constexpr float kProtocolMaxDistance = 8.0f;
constexpr uint32_t kProtocolCooldownMs = 1000;
constexpr uint32_t kProtocolCooldownCleanupMs = 10000;
constexpr int kOneshotVoices = 16;
constexpr uint32_t kObjectDefaultDistance = 8;

struct Pcm {
    std::vector<float> samples;
    ma_uint32 channels = 0;
    ma_uint32 sampleRate = 0;
};

struct Voice {
    ma_sound sound{};
    ma_audio_buffer buf{};
    std::shared_ptr<Pcm> pcm;
    bool soundInit = false;
    bool bufInit = false;
    uint32_t audioFileId = 0;

    ~Voice() { release(); }

    void release()
    {
        if (soundInit) {
            ma_sound_uninit(&sound);
            soundInit = false;
        }
        if (bufInit) {
            ma_audio_buffer_uninit(&buf);
            bufInit = false;
        }
        pcm.reset();
        audioFileId = 0;
    }
};

QString deviceNameFromInfo(const ma_device_info& info)
{
    const QString utf8 = QString::fromUtf8(info.name);
    if (!utf8.isEmpty())
        return utf8;
    return QString::fromLocal8Bit(info.name);
}

template<typename Fn>
bool withPlaybackDevices(Fn&& fn)
{
    ma_context context;
    if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS)
        return false;
    ma_device_info* playback = nullptr;
    ma_uint32 playbackCount = 0;
    const ma_result result = ma_context_get_devices(&context, &playback, &playbackCount, nullptr, nullptr);
    if (result == MA_SUCCESS && playback != nullptr && playbackCount > 0)
        fn(playback, playbackCount);
    ma_context_uninit(&context);
    return result == MA_SUCCESS;
}

bool lookupPlaybackDeviceId(const QString& name, ma_device_id* outId)
{
    if (!outId || name.isEmpty())
        return false;
    bool found = false;
    withPlaybackDevices([&](const ma_device_info* playback, const ma_uint32 count) {
        for (ma_uint32 i = 0; i < count; ++i) {
            if (deviceNameFromInfo(playback[i]) != name)
                continue;
            *outId = playback[i].id;
            found = true;
            break;
        }
    });
    return found;
}

bool decodeOggBytes(const QByteArray& bytes, const std::shared_ptr<Pcm>& out)
{
    if (!out || bytes.isEmpty())
        return false;
    ma_decoder decoder;
    const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    if (ma_decoder_init_memory(bytes.constData(), static_cast<size_t>(bytes.size()), &config, &decoder) != MA_SUCCESS)
        return false;
    if (decoder.outputChannels == 0 || decoder.outputSampleRate == 0) {
        ma_decoder_uninit(&decoder);
        return false;
    }
    out->channels = decoder.outputChannels;
    out->sampleRate = decoder.outputSampleRate;
    out->samples.clear();

    ma_uint64 knownFrames = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &knownFrames) == MA_SUCCESS && knownFrames > 0)
        out->samples.reserve(static_cast<size_t>(knownFrames * out->channels));

    float chunk[4096];
    for (;;) {
        const ma_uint64 framesRequested =
            sizeof(chunk) / sizeof(float) / static_cast<ma_uint64>(std::max<ma_uint32>(1, out->channels));
        ma_uint64 framesRead = 0;
        if (ma_decoder_read_pcm_frames(&decoder, chunk, framesRequested, &framesRead) != MA_SUCCESS || framesRead == 0)
            break;
        const auto* begin = chunk;
        const auto* end = chunk + framesRead * out->channels;
        out->samples.insert(out->samples.end(), begin, end);
    }
    ma_decoder_uninit(&decoder);
    return !out->samples.empty();
}

bool startVoice(ma_engine* engine, Voice* voice, const std::shared_ptr<Pcm>& pcm, const bool loop, const float volume, const float pan)
{
    if (!engine || !voice || !pcm || pcm->samples.empty() || pcm->channels == 0)
        return false;
    voice->release();
    voice->pcm = pcm;
    ma_audio_buffer_config cfg = ma_audio_buffer_config_init(
        ma_format_f32, pcm->channels, pcm->samples.size() / pcm->channels, pcm->samples.data(), nullptr);
    cfg.sampleRate = pcm->sampleRate;
    if (ma_audio_buffer_init(&cfg, &voice->buf) != MA_SUCCESS) {
        voice->pcm.reset();
        return false;
    }
    voice->bufInit = true;
    if (ma_sound_init_from_data_source(engine, &voice->buf, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &voice->sound) != MA_SUCCESS) {
        voice->release();
        return false;
    }
    voice->soundInit = true;
    ma_sound_set_looping(&voice->sound, loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(&voice->sound, std::clamp(volume, 0.0f, 1.0f));
    ma_sound_set_pan(&voice->sound, std::clamp(pan, -1.0f, 1.0f));
    if (ma_sound_start(&voice->sound) != MA_SUCCESS)
        return false;
    return true;
}

void setVoiceMix(Voice* voice, const float volume, const float pan)
{
    if (!voice || !voice->soundInit)
        return;
    ma_sound_set_volume(&voice->sound, std::clamp(volume, 0.0f, 1.0f));
    ma_sound_set_pan(&voice->sound, std::clamp(pan, -1.0f, 1.0f));
}

int optionInt(const QJsonObject& options, const char* key, const int fallback)
{
    const QJsonValue value = options.value(QLatin1String(key));
    return value.isUndefined() ? fallback : value.toInt(fallback);
}

bool optionBool(const QJsonObject& options, const char* key, const bool fallback = true)
{
    const QJsonValue value = options.value(QLatin1String(key));
    return value.isUndefined() ? fallback : value.toBool(fallback);
}

std::optional<std::string> readResource(const std::string& virtualPath)
{
    if (virtualPath.empty())
        return std::nullopt;
    try {
        return tibia::files::data().readFileContents(virtualPath);
    } catch (const std::exception&) {
        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

#if defined(Q_OS_WASM)
void resumeWebAudio()
{
    EM_ASM({
        if (window.qtclientSoundMuted)
            return;
        var root = (typeof window !== 'undefined') ? window.miniaudio : null;
        if (!root || !root.devices)
            return;
        for (var i = 0; i < root.devices.length; i++) {
            var device = root.devices[i];
            if (device && device.webaudio && device.webaudio.state === 'suspended')
                device.webaudio.resume();
        }
    });
}

class WebAudioUnlock final : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::KeyPress:
        case QEvent::TouchBegin:
            resumeWebAudio();
            break;
        default:
            break;
        }
        return false;
    }
};
#endif

float distanceGain(const std::optional<Position>& listener, const Position& source, const float maxDistance)
{
    if (maxDistance <= 0.0f)
        return 0.0f;
    if (!listener || !listener->isMapPosition() || !source.isMapPosition())
        return 1.0f;
    if (listener->z != source.z)
        return 0.0f;
    const float distance = static_cast<float>(listener->distance(source));
    if (distance > maxDistance)
        return 0.0f;
    return std::max(0.0f, 1.0f - distance / maxDistance);
}

}

struct TSoundStorage::Engine {
    ma_engine engine{};
    Voice music;
    Voice ambience;
    Voice ui;
    std::array<Voice, kOneshotVoices> oneshots{};
    std::unordered_map<uint32_t, std::unique_ptr<Voice>> objects;
    std::unordered_map<std::string, std::shared_ptr<Pcm>> pcmByPath;
    int oneshotCursor = 0;

    void releaseAll()
    {
        music.release();
        ambience.release();
        ui.release();
        for (auto& voice : oneshots)
            voice.release();
        for (auto& [_, voice] : objects) {
            if (voice)
                voice->release();
        }
        objects.clear();
    }
};

void playProtocolSoundMain(const uint8_t soundSource, const uint16_t soundEffectId, const Position& pos)
{
    if (auto* storage = TSoundStorage::instance())
        storage->playProtocolSoundMain(soundSource, soundEffectId, pos);
}

void playProtocolSoundSecondary(const uint8_t soundEnum, const uint8_t soundSource, const uint16_t soundEffectId, const Position& pos)
{
    if (auto* storage = TSoundStorage::instance())
        storage->playProtocolSoundSecondary(soundEnum, soundSource, soundEffectId, pos);
}

TSoundStorage::TSoundStorage(QObject* parent)
    : cip::tibia::sound::TSoundStorage(parent)
    , m_activeSoundPlayer(this)
    , m_gameSessionSoundProvider(this)
    , m_objectSoundController(this)
    , m_soundConfiguration(this)
    , m_soundController(this)
    , m_soundControllerBase(this)
    , m_decompressorThreadpool(this)
    , m_soundEngine(this)
    , m_soundManager(this)
    , m_soundMetadataManager(this)
    , m_soundPlayer(this)
    , m_soundPlayerManager(this)
{
    g_soundStorage = this;
    ensureEngine();
    m_gameEndHook = g_gameEvents.subscribe("g_game", "onGameEnd", [this](LuaObject*, const GameEvents::Args&) {
        stopAmbience();
        stopObjectAmbience();
        stopOneShots();
        return std::any{};
    });
}

TSoundStorage::~TSoundStorage()
{
    if (m_gameEndHook)
        g_gameEvents.unsubscribe(m_gameEndHook);
    stopAll();
    if (m_engine) {
        m_engine->releaseAll();
        if (m_engineReady)
            ma_engine_uninit(&m_engine->engine);
    }
    if (g_soundStorage == this)
        g_soundStorage = nullptr;
}

TSoundStorage* TSoundStorage::instance()
{
    return g_soundStorage;
}

bool TSoundStorage::ensureEngine()
{
    if (m_engineReady)
        return true;
    if (!m_engine)
        m_engine = std::make_unique<Engine>();

    ma_engine_config config = ma_engine_config_init();
    ma_device_id deviceId{};
    const bool wantNamed = !m_usedPlaybackDevice.isEmpty()
        && m_usedPlaybackDevice != QLatin1String("auto");
    if (wantNamed) {
        if (lookupPlaybackDeviceId(m_usedPlaybackDevice, &deviceId))
            config.pPlaybackDeviceID = &deviceId;
        else
            qCWarning(lcSoundStorage) << "sound device not found, using default"
                                      << m_usedPlaybackDevice;
    }

    if (ma_engine_init(&config, &m_engine->engine) != MA_SUCCESS) {
        if (config.pPlaybackDeviceID != nullptr) {
            qCWarning(lcSoundStorage) << "miniaudio init failed for device"
                                      << m_usedPlaybackDevice << "- retrying default";
            config.pPlaybackDeviceID = nullptr;
            if (ma_engine_init(&config, &m_engine->engine) != MA_SUCCESS) {
                qCWarning(lcSoundStorage) << "miniaudio engine init failed";
                return false;
            }
        } else {
            qCWarning(lcSoundStorage) << "miniaudio engine init failed";
            return false;
        }
    }
#if defined(Q_OS_WASM)
    if (auto* app = QGuiApplication::instance())
        app->installEventFilter(new WebAudioUnlock(app));
#endif
    m_engineReady = true;
    qCInfo(lcSoundStorage) << "miniaudio engine ready"
                           << (config.pPlaybackDeviceID ? m_usedPlaybackDevice : QStringLiteral("auto"));
    return true;
}

bool TSoundStorage::reinitEngine()
{
    if (m_engine) {
        stopAll();
        m_engine->releaseAll();
        if (m_engineReady) {
            ma_engine_uninit(&m_engine->engine);
            m_engineReady = false;
        }
    }
    return ensureEngine();
}

QStringList TSoundStorage::playbackDeviceNames()
{
    QStringList names;
    withPlaybackDevices([&](const ma_device_info* playback, const ma_uint32 count) {
        names.reserve(static_cast<qsizetype>(count));
        for (ma_uint32 i = 0; i < count; ++i) {
            const QString name = deviceNameFromInfo(playback[i]);
            if (!name.isEmpty() && !names.contains(name))
                names.append(name);
        }
    });
    m_soundEngine.notifyAvailableSoundDevicesChanged();
    return names;
}

bool TSoundStorage::setUsedPlaybackDevice(const QString& deviceOrAuto)
{
    const QString normalized = deviceOrAuto.isEmpty() ? QStringLiteral("auto") : deviceOrAuto;
    if (m_usedPlaybackDevice == normalized && m_engineReady)
        return true;
    m_usedPlaybackDevice = normalized;
    if (!m_engine && !m_engineReady)
        return ensureEngine();
    return reinitEngine();
}

bool TSoundStorage::loadBank(const std::string& virtualDirectory)
{
    if (virtualDirectory.empty())
        return false;
    if (m_bankLoaded && m_bankDir == virtualDirectory)
        return true;
    m_bankLoaded = loadCatalog(virtualDirectory);
    if (m_bankLoaded) {
        m_bankDir = virtualDirectory;
        ensureEngine();
        qCInfo(lcSoundStorage) << "CIP sound bank loaded from" << QString::fromStdString(virtualDirectory)
                               << "files" << static_cast<int>(m_files.size());
    }
    return m_bankLoaded;
}

bool TSoundStorage::loadCatalog(const std::string& directory)
{
#ifndef FRAMEWORK_PROTOBUF
    Q_UNUSED(directory);
    g_logger.error("Protobuf not supported in this build. Enable FRAMEWORK_PROTOBUF");
    return false;
#else
    // The bank is optional. readFileContents throws when the catalog is absent, and
    // that exception escapes the wasm catch and aborts the page. Open it here.
    const auto catalogBytes = readResource(
        tibia::files::data().guessFilePath(directory + "catalog-sound", "json"));
    if (!catalogBytes)
        return false;
    try {
        using json = nlohmann::json;
        const json document = json::parse(*catalogBytes);

        m_files.clear();
        m_effects.clear();
        m_ambients.clear();
        m_objects.clear();
        m_objectsByClientId.clear();
        m_music.clear();
        m_maxObjectDistance = kObjectDefaultDistance;
        stopObjectAmbience();

        bool loaded = false;
        for (const auto& obj : document) {
            if (obj.value("type", "") != "sounds")
                continue;
            const std::string fileName = obj.value("file", "");
            if (fileName.empty())
                continue;

            const auto bankBytes = readResource(directory + fileName);
            if (!bankBytes)
                return false;
            std::stringstream fileInputStream;
            fileInputStream.write(bankBytes->data(), static_cast<std::streamsize>(bankBytes->size()));
            tibia::protobuf::sound::Sounds protobufSounds;
            if (!protobufSounds.ParseFromIstream(&fileInputStream))
                return false;

            for (const auto& audioFile : protobufSounds.sound())
                m_files[audioFile.id()] = audioFile.filename();

            for (const auto& protobufEffect : protobufSounds.numeric_sound_effect()) {
                CatalogEffect effect;
                effect.type = static_cast<int>(protobufEffect.numeric_sound_type());
                if (protobufEffect.has_simple_sound_effect())
                    effect.soundId = protobufEffect.simple_sound_effect().sound_id();
                if (protobufEffect.has_random_sound_effect()) {
                    for (const uint32_t audioFileId : protobufEffect.random_sound_effect().random_sound_id())
                        effect.randomSoundId.push_back(audioFileId);
                }
                m_effects.emplace(protobufEffect.id(), std::move(effect));
            }

            for (const auto& ambient : protobufSounds.ambience_stream()) {
                m_ambients.emplace(ambient.id(), CatalogAmbient{ ambient.looping_sound_id() });
            }

            for (const auto& objectAmbient : protobufSounds.ambience_object_stream()) {
                CatalogObject object;
                object.maxDistance = objectAmbient.max_sound_distance() > 0
                    ? objectAmbient.max_sound_distance()
                    : kObjectDefaultDistance;
                for (const auto& soundEffect : objectAmbient.sound_effects())
                    object.countToFile.emplace_back(soundEffect.looping_sound_id(), soundEffect.count());
                const uint32_t effectId = objectAmbient.id();
                m_maxObjectDistance = std::max(m_maxObjectDistance, object.maxDistance);
                for (const auto clientId : objectAmbient.counted_appearance_types())
                    m_objectsByClientId[clientId].push_back(effectId);
                m_objects.emplace(effectId, std::move(object));
            }

            for (const auto& music : protobufSounds.music_template())
                m_music.emplace(music.id(), CatalogMusic{ music.sound_id() });

            loaded = !m_files.empty();
        }
        return loaded;
    } catch (const std::exception& e) {
        g_logger.debug("Failed to load '{}' (Sounds): {}", directory, e.what());
        return false;
    } catch (...) {
        g_logger.debug("Failed to load '{}' (Sounds)", directory);
        return false;
    }
#endif
}

void TSoundStorage::setMasterGain(const float gain)
{
    m_masterGain = std::clamp(gain, 0.0f, 1.0f);
    refreshLoopVolumes();
    syncPlaybackDevice();
}

void TSoundStorage::setChannelGain(const Channel channel, const float gain)
{
    const float g = std::clamp(gain, 0.0f, 1.0f);
    switch (channel) {
    case Channel::Music: m_musicGain = g; break;
    case Channel::Ambience: m_ambienceGain = g; break;
    case Channel::Ui: m_uiGain = g; break;
    }
    refreshLoopVolumes();
}

void TSoundStorage::applyClientOptions(const QJsonObject& options)
{
    const QString device = options.value(QLatin1String("usedSoundDevice")).toString();
    setUsedPlaybackDevice(device.isEmpty() ? QStringLiteral("auto") : device);

    const int master = std::clamp(optionInt(options, "soundMasterVolume", 100), 0, 100);
    m_masterGain = master / 100.0f;
    m_musicGain = std::clamp(optionInt(options, "soundMusicVolume", 50), 0, 100) / 100.0f;
    m_ambienceGain = std::clamp(optionInt(options, "soundAmbienceVolume", 100), 0, 100) / 100.0f;
    m_uiGain = std::clamp(optionInt(options, "soundUiVolume", 100), 0, 100) / 100.0f;

    m_protocol.ownBattleVolume = std::clamp(optionInt(options, "soundBattleOwnVolume", 100), 0, 100);
    m_protocol.otherPlayersVolume = std::clamp(optionInt(options, "soundBattleOthersVolume", 100), 0, 100);
    m_protocol.creatureVolume = std::clamp(optionInt(options, "soundBattleCreaturesVolume", 100), 0, 100);
    m_protocol.itemsVolume = std::clamp(optionInt(options, "soundItemsVolume", 100), 0, 100);
    m_protocol.eventVolume = std::clamp(optionInt(options, "soundEventsVolume", 100), 0, 100);
    m_protocol.uiVolume = std::clamp(optionInt(options, "soundUiVolume", 100), 0, 100);
    m_protocol.ambienceVolume = std::clamp(optionInt(options, "soundAmbienceVolume", 100), 0, 100);

    m_protocol.ownAttack = optionBool(options, "soundBattleOwnAttackSpellsEnabled");
    m_protocol.ownHealing = optionBool(options, "soundBattleOwnHealingSpellsEnabled");
    m_protocol.ownSupport = optionBool(options, "soundBattleOwnSupportSpellsEnabled");
    m_protocol.ownWeapons = optionBool(options, "soundBattleOwnWeaponsEnabled");
    m_protocol.othersAttack = optionBool(options, "soundBattleOthersAttackSpellsEnabled");
    m_protocol.othersHealing = optionBool(options, "soundBattleOthersHealingSpellsEnabled");
    m_protocol.othersSupport = optionBool(options, "soundBattleOthersSupportSpellsEnabled");
    m_protocol.othersWeapons = optionBool(options, "soundBattleOthersWeaponsEnabled");
    m_protocol.creatureNoises = optionBool(options, "soundBattleCreaturesNoisesEnabled");
    m_protocol.creatureNoisesDeath = optionBool(options, "soundBattleCreaturesDeathEnabled");
    m_protocol.creatureAttacksAndSpells = optionBool(options, "soundBattleCreaturesAttacksEnabled");
    m_protocol.foodAndBeverages = optionBool(options, "soundEatingEnabled");
    m_protocol.moveItem = optionBool(options, "soundMoveItemEnabled");
    m_protocol.uiInteractions = optionBool(options, "soundUiInteractionsEnabled");
    m_protocol.uiJoinLeaveParty = optionBool(options, "soundPartyEnabled");
    m_protocol.uiVipLoginLogout = optionBool(options, "soundVipEnabled");
    m_protocol.notificationInteractions = optionBool(options, "soundNotificationConsoleMessages");
    m_protocol.notificationParty = optionBool(options, "soundNotificationsubChannelsParty");
    m_protocol.notificationLocalChat = optionBool(options, "soundNotificationsubChannelsLocalChat");
    m_protocol.notificationPrivateMessages = optionBool(options, "soundNotificationsubChannelsPrivateMessages");
    m_protocol.notificationRaidAnnouncements = optionBool(options, "soundNotificationsubChannelsRaidAnnouncements");
    m_protocol.notificationSystemAnnouncements = optionBool(options, "soundNotificationsubChannelsSystemAnnouncements");
    refreshLoopVolumes();
    syncPlaybackDevice();
}

void TSoundStorage::syncPlaybackDevice()
{
#if defined(Q_OS_WASM)
    EM_ASM({ window.qtclientSoundMuted = $0 ? true : false; }, m_masterGain <= 0.0f);
#endif
    if (!m_engineReady || !m_engine)
        return;
    if (m_masterGain <= 0.0f)
        ma_engine_stop(&m_engine->engine);
    else
        ma_engine_start(&m_engine->engine);
}

void TSoundStorage::refreshLoopVolumes()
{
    if (!m_engine)
        return;
    if (m_engine->music.soundInit)
        ma_sound_set_volume(&m_engine->music.sound, m_masterGain * m_musicGain);
    if (m_engine->ambience.soundInit)
        ma_sound_set_volume(&m_engine->ambience.sound,
                            m_masterGain * m_ambienceGain * m_ambienceVoiceGain);
    if (m_engine->ui.soundInit)
        ma_sound_set_volume(&m_engine->ui.sound, m_masterGain * m_uiGain);
}

std::shared_ptr<Pcm> cachedPcm(std::unordered_map<std::string, std::shared_ptr<Pcm>>& cache, const std::string& virtualPath)
{
    if (virtualPath.empty())
        return {};
    if (const auto it = cache.find(virtualPath); it != cache.end())
        return it->second;
    const auto bytes = readResource(tibia::files::data().guessFilePath(virtualPath, "ogg"));
    if (!bytes)
        return {};
    auto pcm = std::make_shared<Pcm>();
    const QByteArray view = QByteArray::fromRawData(bytes->data(), static_cast<qsizetype>(bytes->size()));
    if (!decodeOggBytes(view, pcm)) {
        qCWarning(lcSoundStorage) << "ogg decode failed" << QString::fromStdString(virtualPath);
        return {};
    }
    cache.emplace(virtualPath, pcm);
    return pcm;
}

bool TSoundStorage::playResolvedPath(const std::string& virtualPath, const Channel channel, const bool loop, const float gain)
{
    if (virtualPath.empty() || m_masterGain <= 0.0f || !ensureEngine())
        return false;
    const auto pcm = cachedPcm(m_engine->pcmByPath, virtualPath);
    if (!pcm)
        return false;

    Voice* slot = nullptr;
    float channelGain = 1.0f;
    switch (channel) {
    case Channel::Music:
        slot = &m_engine->music;
        channelGain = m_musicGain;
        break;
    case Channel::Ambience:
        slot = &m_engine->ambience;
        channelGain = m_ambienceGain * m_ambienceVoiceGain;
        break;
    case Channel::Ui:
        slot = &m_engine->ui;
        channelGain = m_uiGain;
        break;
    }
    if (!startVoice(&m_engine->engine, slot, pcm, loop, m_masterGain * channelGain * std::clamp(gain, 0.0f, 1.0f), 0.0f))
        return false;
    Q_EMIT soundsToPlayChanged();
    return true;
}

std::string TSoundStorage::pathForAudioFile(const uint32_t audioFileId) const
{
    const auto it = m_files.find(audioFileId);
    if (it == m_files.end() || it->second.empty())
        return {};
    // CipSoft: Resources search root + short name `sounds/<basename>`.
    return std::string("sounds/") + it->second;
}

const TSoundStorage::CatalogEffect* TSoundStorage::effect(const uint32_t id) const
{
    const auto it = m_effects.find(id);
    return it == m_effects.end() ? nullptr : &it->second;
}

std::optional<uint32_t> TSoundStorage::chooseAudioFileId(const uint16_t soundEffectId)
{
    const auto* soundEffect = effect(soundEffectId);
    if (!soundEffect)
        return std::nullopt;
    if (soundEffect->soundId != 0 && soundEffect->randomSoundId.empty())
        return soundEffect->soundId;
    if (soundEffect->randomSoundId.empty())
        return std::nullopt;
    if (soundEffect->randomSoundId.size() == 1)
        return soundEffect->randomSoundId.front();
    std::uniform_int_distribution<size_t> randomIndex(0, soundEffect->randomSoundId.size() - 1);
    return soundEffect->randomSoundId[randomIndex(m_rng)];
}

bool TSoundStorage::playMusic(const uint16_t musicTemplateId, const bool loop)
{
    if (musicTemplateId == 0) {
        stopMusic();
        return true;
    }
    const auto it = m_music.find(musicTemplateId);
    if (it == m_music.end())
        return false;
    if (!playResolvedPath(pathForAudioFile(it->second.audioFileId), Channel::Music, loop, 1.0f))
        return false;
    m_playingLoginAnthem = false;
    return true;
}

bool TSoundStorage::playAmbienceInternal(const uint16_t ambientId, const bool loop)
{
    if (ambientId == 0) {
        m_currentAmbienceId = 0;
        stopAmbience();
        return true;
    }
    if (ambientId == m_currentAmbienceId)
        return true;
    const auto it = m_ambients.find(ambientId);
    if (it == m_ambients.end())
        return false;
    if (!playResolvedPath(pathForAudioFile(it->second.loopedAudioFileId), Channel::Ambience, loop, 1.0f))
        return false;
    m_currentAmbienceId = ambientId;
    return true;
}

bool TSoundStorage::playAmbience(const uint16_t ambientId, const bool loop)
{
    if (m_ambienceHeld) {
        m_deferredAmbienceId = ambientId;
        m_hasDeferredAmbience = true;
        return true;
    }
    return playAmbienceInternal(ambientId, loop);
}

void TSoundStorage::beginAmbienceHold()
{
    m_ambienceHeld = true;
}

void TSoundStorage::endAmbienceHold(const uint16_t fallbackRestoreId)
{
    m_ambienceHeld = false;
    const uint16_t restore = m_hasDeferredAmbience ? m_deferredAmbienceId : fallbackRestoreId;
    m_hasDeferredAmbience = false;
    m_deferredAmbienceId = 0;
    m_ambienceVoiceGain = 1.0f;
    playAmbienceInternal(restore, true);
    refreshLoopVolumes();
}

bool TSoundStorage::playHeldAmbience(const uint16_t ambientId, const bool loop)
{
    return playAmbienceInternal(ambientId, loop);
}

bool TSoundStorage::playHeldAmbiencePath(const std::string& virtualPath, const bool loop)
{
    if (virtualPath.empty())
        return false;
    if (!playResolvedPath(virtualPath, Channel::Ambience, loop, 1.0f))
        return false;
    // Path beds are not catalog ambient ids — keep place restore via saved/deferred id.
    m_currentAmbienceId = 0;
    return true;
}

void TSoundStorage::setAmbienceVoiceGain(const float gain)
{
    const float g = std::clamp(gain, 0.0f, 1.0f);
    if (std::abs(m_ambienceVoiceGain - g) < 0.001f)
        return;
    m_ambienceVoiceGain = g;
    refreshLoopVolumes();
}

bool TSoundStorage::playUiEffect(const uint16_t soundEffectId, const float gain)
{
    if (soundEffectId == 0 || gain <= 0.0f || m_uiGain <= 0.0f)
        return false;
    const auto audioFileId = chooseAudioFileId(soundEffectId);
    if (!audioFileId)
        return false;
    return playResolvedPath(pathForAudioFile(*audioFileId), Channel::Ui, false, gain);
}

bool TSoundStorage::playOneshotPath(const std::string& virtualPath, const float gain, const float pan)
{
    if (virtualPath.empty() || m_masterGain <= 0.0f || m_ambienceGain <= 0.0f || !ensureEngine())
        return false;
    const auto pcm = cachedPcm(m_engine->pcmByPath, virtualPath);
    if (!pcm)
        return false;
    Voice* slot = nullptr;
    for (int i = 0; i < kOneshotVoices; ++i) {
        Voice& candidate = m_engine->oneshots[static_cast<size_t>((m_engine->oneshotCursor + i) % kOneshotVoices)];
        if (!candidate.soundInit || !ma_sound_is_playing(&candidate.sound)) {
            slot = &candidate;
            m_engine->oneshotCursor = (m_engine->oneshotCursor + i + 1) % kOneshotVoices;
            break;
        }
    }
    if (!slot) {
        slot = &m_engine->oneshots[static_cast<size_t>(m_engine->oneshotCursor)];
        m_engine->oneshotCursor = (m_engine->oneshotCursor + 1) % kOneshotVoices;
    }
    const float volume = m_masterGain * m_ambienceGain * std::clamp(gain, 0.0f, 1.0f);
    if (!startVoice(&m_engine->engine, slot, pcm, false, volume, pan))
        return false;
    Q_EMIT soundsToPlayChanged();
    return true;
}

bool TSoundStorage::playEffect(const uint16_t soundEffectId, const float gain, const float pan)
{
    if (soundEffectId == 0 || gain <= 0.0f || m_ambienceGain <= 0.0f)
        return false;
    const auto audioFileId = chooseAudioFileId(soundEffectId);
    if (!audioFileId)
        return false;
    return playOneshotPath(pathForAudioFile(*audioFileId), gain, pan);
}

bool TSoundStorage::playAmbienceOneshot(const std::string& virtualPath, const float gain, const float pan)
{
    return playOneshotPath(virtualPath, gain, pan);
}

bool TSoundStorage::playFile(const std::string& virtualOrRelativePath, const Channel channel, const bool loop)
{
    if (!playResolvedPath(virtualOrRelativePath, channel, loop, 1.0f))
        return false;
    if (channel == Channel::Music)
        m_playingLoginAnthem = true;
    return true;
}

void TSoundStorage::stopMusic()
{
    m_playingLoginAnthem = false;
    if (m_engine && m_engine->music.soundInit)
        ma_sound_stop(&m_engine->music.sound);
}

void TSoundStorage::stopLoginAnthem()
{
    if (!m_playingLoginAnthem)
        return;
    stopMusic();
}

void TSoundStorage::stopAmbience()
{
    m_currentAmbienceId = 0;
    if (m_engine && m_engine->ambience.soundInit)
        ma_sound_stop(&m_engine->ambience.sound);
}

void TSoundStorage::stopUi()
{
    if (m_engine && m_engine->ui.soundInit)
        ma_sound_stop(&m_engine->ui.sound);
}

void TSoundStorage::stopOneShots()
{
    if (!m_engine)
        return;
    for (auto& voice : m_engine->oneshots) {
        if (voice.soundInit)
            ma_sound_stop(&voice.sound);
    }
}

void TSoundStorage::stopObjectAmbience()
{
    if (!m_engine)
        return;
    for (auto& [_, voice] : m_engine->objects) {
        if (voice)
            voice->release();
    }
    m_engine->objects.clear();
}

void TSoundStorage::stopAll()
{
    stopMusic();
    stopAmbience();
    stopUi();
    stopOneShots();
    stopObjectAmbience();
}

void TSoundStorage::enqueueEffect(const QueuedEffect& effect)
{
    if (!m_bankLoaded)
        return;
    std::lock_guard lock(m_queueMutex);
    if (m_queue.size() < 256)
        m_queue.push_back(effect);
}

void TSoundStorage::playProtocolSoundMain(const uint8_t soundSource, const uint16_t soundEffectId, const Position& pos)
{
    enqueueEffect(QueuedEffect{ false, 0, soundSource, soundEffectId, pos });
}

void TSoundStorage::playProtocolSoundSecondary(const uint8_t soundEnum, const uint8_t soundSource, const uint16_t soundEffectId, const Position& pos)
{
    enqueueEffect(QueuedEffect{ true, soundEnum, soundSource, soundEffectId, pos });
}

int TSoundStorage::protocolVolume(const uint8_t soundSource, const int soundType) const
{
    // tibia.protobuf.sound.ENumericSoundType
    switch (soundType) {
    case 9:  // food and drink
    case 10: // item movement
        return m_protocol.itemsVolume;
    case 11: // event
        return m_protocol.eventVolume;
    case 8: // ambience stream
        return m_protocol.ambienceVolume;
    case 12: // ui
    case 13: // whisper without open chat
    case 14: // chat message
    case 15: // party
    case 16: // vip list
    case 17: // raid announcement
    case 18: // server message
        return m_protocol.uiVolume;
    default:
        break;
    }
    switch (soundSource) {
    case 0:
    case 1:
        return m_protocol.ownBattleVolume;
    case 2:
        return m_protocol.otherPlayersVolume;
    case 3:
        return m_protocol.creatureVolume;
    default:
        return 100;
    }
}

bool TSoundStorage::protocolEnabled(const uint8_t soundSource, const int soundType) const
{
    // tibia.protobuf.sound.ENumericSoundType
    if (soundType == 9)
        return m_protocol.foodAndBeverages;
    if (soundType == 10)
        return m_protocol.moveItem;
    if (soundType == 12)
        return m_protocol.uiInteractions;
    if (soundType == 15)
        return m_protocol.notificationParty && m_protocol.notificationInteractions;
    if (soundType == 16)
        return m_protocol.uiVipLoginLogout;
    if (soundType == 17)
        return m_protocol.notificationRaidAnnouncements && m_protocol.notificationInteractions;
    if (soundType == 18)
        return m_protocol.notificationSystemAnnouncements && m_protocol.notificationInteractions;
    if (soundType == 14)
        return m_protocol.notificationLocalChat && m_protocol.notificationInteractions;
    if (soundType == 13)
        return m_protocol.notificationPrivateMessages && m_protocol.notificationInteractions;
    if (soundType == 11 || soundType == 8 || soundType == 0 || soundType == 19)
        return true;

    switch (soundSource) {
    case 0:
        return true;
    case 1:
        switch (soundType) {
        case 1: return m_protocol.ownAttack;
        case 2: return m_protocol.ownHealing;
        case 3: return m_protocol.ownSupport;
        case 4:
        case 7:
            return m_protocol.ownWeapons;
        default:
            return true;
        }
    case 2:
        switch (soundType) {
        case 1: return m_protocol.othersAttack;
        case 2: return m_protocol.othersHealing;
        case 3: return m_protocol.othersSupport;
        case 4:
        case 7:
            return m_protocol.othersWeapons;
        case 5:
            return m_protocol.creatureNoises;
        case 6:
            return m_protocol.creatureNoisesDeath;
        default:
            return true;
        }
    case 3:
        switch (soundType) {
        case 5:
            return m_protocol.creatureNoises;
        case 6:
            return m_protocol.creatureNoisesDeath;
        case 7:
        case 1:
        case 2:
        case 3:
        case 4:
            return m_protocol.creatureAttacksAndSpells;
        default:
            return true;
        }
    default:
        return true;
    }
}

bool TSoundStorage::recentlyPlayed(const uint16_t soundEffectId)
{
    const uint64_t now = static_cast<uint64_t>(g_clock.millis());
    if (now - m_lastCooldownCleanup > kProtocolCooldownCleanupMs) {
        for (auto it = m_lastPlayedAt.begin(); it != m_lastPlayedAt.end();) {
            if (now - it->second > kProtocolCooldownCleanupMs)
                it = m_lastPlayedAt.erase(it);
            else
                ++it;
        }
        m_lastCooldownCleanup = now;
    }
    const auto it = m_lastPlayedAt.find(soundEffectId);
    return it != m_lastPlayedAt.end() && now - it->second < kProtocolCooldownMs;
}

std::optional<Position> TSoundStorage::listenerPosition() const
{
    if (!g_game.isOnline())
        return std::nullopt;
    const auto& player = g_game.getLocalPlayer();
    if (!player)
        return std::nullopt;
    const Position pos = player->getPosition();
    if (!pos.isMapPosition())
        return std::nullopt;
    return pos;
}

void TSoundStorage::playQueuedEffect(const QueuedEffect& effect)
{
    (void)effect.soundEnum;
    if (m_masterGain <= 0.0f || !m_bankLoaded || !ensureEngine())
        return;
    const auto* soundEffect = this->effect(effect.id);
    if (!soundEffect)
        return;
    const int volume = protocolVolume(effect.source, soundEffect->type);
    if (volume <= 1 || !protocolEnabled(effect.source, soundEffect->type))
        return;
    if (recentlyPlayed(effect.id))
        return;
    const auto audioFileId = chooseAudioFileId(effect.id);
    if (!audioFileId)
        return;
    const std::string path = pathForAudioFile(*audioFileId);
    const auto pcm = cachedPcm(m_engine->pcmByPath, path);
    if (!pcm)
        return;

    const auto listener = listenerPosition();
    const float falloff = distanceGain(listener, effect.pos, kProtocolMaxDistance);
    if (falloff <= 0.0f)
        return;
    const float gain = m_masterGain * std::clamp(volume / 100.0f, 0.0f, 1.0f) * falloff;
    float pan = 0.0f;
    if (listener && listener->isMapPosition() && effect.pos.isMapPosition())
        pan = std::clamp(static_cast<float>(effect.pos.x - listener->x) / kProtocolMaxDistance, -1.0f, 1.0f);

    Voice* slot = nullptr;
    for (int i = 0; i < kOneshotVoices; ++i) {
        Voice& candidate = m_engine->oneshots[static_cast<size_t>((m_engine->oneshotCursor + i) % kOneshotVoices)];
        if (!candidate.soundInit || !ma_sound_is_playing(&candidate.sound)) {
            slot = &candidate;
            m_engine->oneshotCursor = (m_engine->oneshotCursor + i + 1) % kOneshotVoices;
            break;
        }
    }
    if (!slot) {
        slot = &m_engine->oneshots[static_cast<size_t>(m_engine->oneshotCursor)];
        m_engine->oneshotCursor = (m_engine->oneshotCursor + 1) % kOneshotVoices;
    }
    if (!startVoice(&m_engine->engine, slot, pcm, false, gain, pan))
        return;
    slot->audioFileId = *audioFileId;
    m_lastPlayedAt[effect.id] = static_cast<uint64_t>(g_clock.millis());
}

void TSoundStorage::poll()
{
    std::vector<QueuedEffect> batch;
    {
        std::lock_guard lock(m_queueMutex);
        batch.swap(m_queue);
    }
    for (const auto& effect : batch)
        playQueuedEffect(effect);
    pollObjectAmbience();
}

void TSoundStorage::pollObjectAmbience()
{
    if (!m_bankLoaded || m_objects.empty() || !ensureEngine())
        return;
    const float channelGain = m_masterGain * m_ambienceGain;
    const auto listener = listenerPosition();
    if (channelGain <= 0.0f || !listener) {
        stopObjectAmbience();
        return;
    }

    struct Accumulator {
        uint32_t count = 0;
        double nearest = std::numeric_limits<double>::max();
        Position pos;
    };
    std::unordered_map<uint32_t, Accumulator> live;
    const int radius = static_cast<int>(m_maxObjectDistance);
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dy = -radius; dy <= radius; ++dy) {
            const Position tilePos(listener->x + dx, listener->y + dy, listener->z);
            if (listener->distance(tilePos) > static_cast<double>(radius))
                continue;
            const auto& tile = g_map.getTile(tilePos);
            if (!tile)
                continue;
            const double distance = listener->distance(tilePos);
            for (const auto& thing : tile->getThings()) {
                if (!thing || !thing->isItem())
                    continue;
                const auto effects = m_objectsByClientId.find(thing->getId());
                if (effects == m_objectsByClientId.end())
                    continue;
                for (const uint32_t effectId : effects->second) {
                    const auto objectIt = m_objects.find(effectId);
                    if (objectIt == m_objects.end() || distance > objectIt->second.maxDistance)
                        continue;
                    auto& acc = live[effectId];
                    ++acc.count;
                    if (distance < acc.nearest) {
                        acc.nearest = distance;
                        acc.pos = tilePos;
                    }
                }
            }
        }
    }

    std::unordered_set<uint32_t> active;
    for (const auto& [effectId, acc] : live) {
        const auto objectIt = m_objects.find(effectId);
        if (objectIt == m_objects.end() || acc.count == 0)
            continue;
        uint32_t audioFileId = 0;
        uint32_t selectedCount = 0;
        for (const auto& [fileId, requiredCount] : objectIt->second.countToFile) {
            if (acc.count >= requiredCount && requiredCount >= selectedCount) {
                audioFileId = fileId;
                selectedCount = requiredCount;
            }
        }
        if (audioFileId == 0)
            continue;
        const std::string path = pathForAudioFile(audioFileId);
        const auto pcm = cachedPcm(m_engine->pcmByPath, path);
        if (!pcm)
            continue;
        const float maxDistance = static_cast<float>(objectIt->second.maxDistance > 0 ? objectIt->second.maxDistance : kObjectDefaultDistance);
        const float gain = std::max(0.1f, 1.0f - static_cast<float>(acc.nearest / maxDistance)) * channelGain;
        const float pan = std::clamp(static_cast<float>(acc.pos.x - listener->x) / maxDistance, -1.0f, 1.0f);
        auto& slot = m_engine->objects[effectId];
        if (!slot)
            slot = std::make_unique<Voice>();
        if (slot->audioFileId == audioFileId && slot->soundInit && ma_sound_is_playing(&slot->sound))
            setVoiceMix(slot.get(), gain, pan);
        else if (!startVoice(&m_engine->engine, slot.get(), pcm, true, gain, pan))
            continue;
        slot->audioFileId = audioFileId;
        active.insert(effectId);
    }

    for (auto it = m_engine->objects.begin(); it != m_engine->objects.end();) {
        if (active.contains(it->first)) {
            ++it;
            continue;
        }
        if (it->second)
            it->second->release();
        it = m_engine->objects.erase(it);
    }
}

}
