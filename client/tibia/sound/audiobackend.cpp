#include "sound/audiobackend.h"

#include "platform/platform.h"

#include "miniaudio_vorbis_backend.h"
#include "shared/tibiafilesystemhelper.h"

#include <QEvent>
#include <QGuiApplication>

#include <algorithm>
#include <set>

namespace tibia::sound {

struct TAudioSource::Impl {
    std::shared_ptr<const TRawSoundSamples> samples;
    QByteArray bytes;
    ma_audio_buffer buffer{};
    ma_decoder decoder{};
    ma_sound sound{};
    bool bufferInit = false;
    bool decoderInit = false;
    bool soundInit = false;
    bool paused = false;
    TAudioSource* owner = nullptr;

    ma_data_source* dataSource()
    {
        if (bufferInit)
            return &buffer;
        if (decoderInit)
            return &decoder;
        return nullptr;
    }
};

namespace {

struct TEngine {
    ma_engine engine{};
    bool initialized = false;
    std::optional<QString> device;
    std::set<TAudioSource::Impl*> sources;
};

TEngine& engine()
{
    static TEngine instance;
    return instance;
}

QString deviceName(const ma_device_info& info)
{
    const QString utf8 = QString::fromUtf8(info.name);
    return utf8.isEmpty() ? QString::fromLocal8Bit(info.name) : utf8;
}

template<typename Fn>
bool withPlaybackDevices(Fn&& fn)
{
    ma_context context;
    if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS)
        return false;
    ma_device_info* playback = nullptr;
    ma_uint32 count = 0;
    const ma_result result = ma_context_get_devices(&context, &playback, &count, nullptr, nullptr);
    if (result == MA_SUCCESS)
        fn(playback, count);
    ma_context_uninit(&context);
    return result == MA_SUCCESS;
}

bool initSound(TAudioSource::Impl& impl)
{
    TEngine& e = engine();
    ma_data_source* source = impl.dataSource();
    if (!e.initialized || !source)
        return false;
    if (ma_sound_init_from_data_source(&e.engine, source, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &impl.sound)
        != MA_SUCCESS)
        return false;
    impl.soundInit = true;
    return true;
}

void applyMix(TAudioSource::Impl& impl, const float volume, const float pitch, const bool looping)
{
    if (!impl.soundInit)
        return;
    ma_sound_set_volume(&impl.sound, std::clamp(volume, 0.0f, 100.0f) / 100.0f);
    ma_sound_set_pitch(&impl.sound, pitch);
    ma_sound_set_looping(&impl.sound, looping ? MA_TRUE : MA_FALSE);
}

bool initEngine(const std::optional<QString>& name)
{
    TEngine& e = engine();
    ma_engine_config config = ma_engine_config_init();
    ma_device_id id{};
    bool named = false;
    if (name) {
        withPlaybackDevices([&](const ma_device_info* playback, const ma_uint32 count) {
            for (ma_uint32 i = 0; i < count; ++i) {
                if (deviceName(playback[i]) == *name) {
                    id = playback[i].id;
                    named = true;
                    break;
                }
            }
        });
        if (!named)
            return false;
        config.pPlaybackDeviceID = &id;
    }
    if (ma_engine_init(&config, &e.engine) != MA_SUCCESS)
        return false;
    e.initialized = true;
    e.device = name;
    return true;
}

}

std::optional<TRawSoundSamples> decodeSoundData(const QByteArray& data)
{
    if (data.isEmpty())
        return std::nullopt;
    ma_decoder decoder;
    const ma_decoder_config config = maDecoderConfigForCipOgg();
    if (ma_decoder_init_memory(data.constData(), static_cast<size_t>(data.size()), &config, &decoder) != MA_SUCCESS)
        return std::nullopt;
    TRawSoundSamples out;
    out.channelCount = decoder.outputChannels;
    out.sampleRate = decoder.outputSampleRate;
    if (out.channelCount == 0 || out.sampleRate == 0) {
        ma_decoder_uninit(&decoder);
        return std::nullopt;
    }
    ma_uint64 frames = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &frames) == MA_SUCCESS && frames > 0)
        out.samples.reserve(static_cast<size_t>(frames * out.channelCount));
    float chunk[4096];
    const ma_uint64 framesPerChunk = sizeof(chunk) / sizeof(float) / out.channelCount;
    for (;;) {
        ma_uint64 read = 0;
        if (ma_decoder_read_pcm_frames(&decoder, chunk, framesPerChunk, &read) != MA_SUCCESS || read == 0)
            break;
        out.samples.insert(out.samples.end(), chunk, chunk + read * out.channelCount);
    }
    ma_decoder_uninit(&decoder);
    if (out.samples.empty())
        return std::nullopt;
    return out;
}

TAudioSource::TAudioSource()
    : m_impl(std::make_unique<Impl>())
{
    m_impl->owner = this;
    engine().sources.insert(m_impl.get());
}

TAudioSource::~TAudioSource()
{
    close();
    engine().sources.erase(m_impl.get());
}

void TAudioSource::close()
{
    Impl& impl = *m_impl;
    if (impl.soundInit) {
        ma_sound_uninit(&impl.sound);
        impl.soundInit = false;
    }
    if (impl.bufferInit) {
        ma_audio_buffer_uninit(&impl.buffer);
        impl.bufferInit = false;
    }
    if (impl.decoderInit) {
        ma_decoder_uninit(&impl.decoder);
        impl.decoderInit = false;
    }
    impl.samples.reset();
    impl.bytes.clear();
    impl.paused = false;
}

bool TAudioSource::openBuffer(const TSoundBuffer& buffer)
{
    close();
    if (buffer.empty())
        return false;
    Impl& impl = *m_impl;
    impl.samples = buffer.samples;
    const TRawSoundSamples& raw = *impl.samples;
    ma_audio_buffer_config config = ma_audio_buffer_config_init(
        ma_format_f32, raw.channelCount, raw.samples.size() / raw.channelCount, raw.samples.data(), nullptr);
    config.sampleRate = raw.sampleRate;
    if (ma_audio_buffer_init(&config, &impl.buffer) != MA_SUCCESS) {
        impl.samples.reset();
        return false;
    }
    impl.bufferInit = true;
    initSound(impl);
    applyMix(impl, m_volume, m_pitch, m_looping);
    return true;
}

bool TAudioSource::openMemory(const QByteArray& data)
{
    close();
    if (data.isEmpty())
        return false;
    Impl& impl = *m_impl;
    impl.bytes = data;
    const ma_decoder_config config = maDecoderConfigForCipOgg();
    if (ma_decoder_init_memory(impl.bytes.constData(), static_cast<size_t>(impl.bytes.size()), &config, &impl.decoder)
        != MA_SUCCESS) {
        impl.bytes.clear();
        return false;
    }
    impl.decoderInit = true;
    initSound(impl);
    applyMix(impl, m_volume, m_pitch, m_looping);
    return true;
}

bool TAudioSource::openFile(const std::string& virtualPath)
{
    try {
        const std::string bytes = tibia::files::data().readFileContents(virtualPath);
        return openMemory(QByteArray::fromStdString(bytes));
    } catch (...) {
        close();
        return false;
    }
}

bool TAudioSource::isOpen() const
{
    return m_impl->dataSource() != nullptr;
}

void TAudioSource::play()
{
    Impl& impl = *m_impl;
    if (!impl.soundInit && !initSound(impl))
        return;
    applyMix(impl, m_volume, m_pitch, m_looping);
    if (ma_sound_at_end(&impl.sound))
        ma_sound_seek_to_pcm_frame(&impl.sound, 0);
    ma_sound_start(&impl.sound);
    impl.paused = false;
}

void TAudioSource::pause()
{
    Impl& impl = *m_impl;
    if (!impl.soundInit || !ma_sound_is_playing(&impl.sound))
        return;
    ma_sound_stop(&impl.sound);
    impl.paused = true;
}

void TAudioSource::stop()
{
    Impl& impl = *m_impl;
    impl.paused = false;
    if (!impl.soundInit)
        return;
    ma_sound_stop(&impl.sound);
    ma_sound_seek_to_pcm_frame(&impl.sound, 0);
}

TAudioSource::EStatus TAudioSource::status() const
{
    Impl& impl = *m_impl;
    if (!impl.soundInit)
        return EStatus::Stopped;
    if (ma_sound_is_playing(&impl.sound))
        return EStatus::Playing;
    return impl.paused ? EStatus::Paused : EStatus::Stopped;
}

void TAudioSource::setVolume(const float volume)
{
    m_volume = volume;
    if (m_impl->soundInit)
        ma_sound_set_volume(&m_impl->sound, std::clamp(volume, 0.0f, 100.0f) / 100.0f);
}

void TAudioSource::setPitch(const float pitch)
{
    m_pitch = pitch;
    if (m_impl->soundInit)
        ma_sound_set_pitch(&m_impl->sound, pitch);
}

void TAudioSource::setLooping(const bool looping)
{
    m_looping = looping;
    if (m_impl->soundInit)
        ma_sound_set_looping(&m_impl->sound, looping ? MA_TRUE : MA_FALSE);
}

int64_t TAudioSource::playingOffsetMs() const
{
    Impl& impl = *m_impl;
    if (!impl.soundInit)
        return 0;
    float seconds = 0.0f;
    if (ma_sound_get_cursor_in_seconds(&impl.sound, &seconds) != MA_SUCCESS)
        return 0;
    return static_cast<int64_t>(seconds * 1000.0f);
}

void TAudioSource::setPlayingOffsetMs(const int64_t offsetMs)
{
    Impl& impl = *m_impl;
    if (!impl.soundInit)
        return;
    ma_uint32 sampleRate = 0;
    if (ma_sound_get_data_format(&impl.sound, nullptr, nullptr, &sampleRate, nullptr, 0) != MA_SUCCESS || sampleRate == 0)
        return;
    ma_sound_seek_to_pcm_frame(&impl.sound, static_cast<ma_uint64>(std::max<int64_t>(0, offsetMs)) * sampleRate / 1000);
}

int64_t TAudioSource::durationMs() const
{
    Impl& impl = *m_impl;
    if (!impl.soundInit)
        return 0;
    float seconds = 0.0f;
    if (ma_sound_get_length_in_seconds(&impl.sound, &seconds) != MA_SUCCESS)
        return 0;
    return static_cast<int64_t>(seconds * 1000.0f);
}

namespace playbackdevice {

bool initialize()
{
    TEngine& e = engine();
    if (e.initialized)
        return true;
    if (!initEngine(std::nullopt))
        return false;
    e.device = defaultDevice();
    tibia::platform::installAudioUnlock();
    return true;
}

bool isInitialized()
{
    return engine().initialized;
}

QStringList availableDevices()
{
    QStringList names;
    withPlaybackDevices([&](const ma_device_info* playback, const ma_uint32 count) {
        for (ma_uint32 i = 0; i < count; ++i) {
            const QString name = deviceName(playback[i]);
            if (!name.isEmpty() && !names.contains(name))
                names.append(name);
        }
    });
    return names;
}

std::optional<QString> defaultDevice()
{
    std::optional<QString> result;
    withPlaybackDevices([&](const ma_device_info* playback, const ma_uint32 count) {
        for (ma_uint32 i = 0; i < count; ++i) {
            if (playback[i].isDefault) {
                result = deviceName(playback[i]);
                return;
            }
        }
        if (count > 0)
            result = deviceName(playback[0]);
    });
    return result;
}

std::optional<QString> device()
{
    return engine().device;
}

bool setDevice(const QString& name)
{
    TEngine& e = engine();
    struct TSaved {
        TAudioSource::Impl* impl;
        ma_uint64 cursor;
        bool playing;
    };
    std::vector<TSaved> saved;
    for (TAudioSource::Impl* impl : e.sources) {
        if (!impl->soundInit)
            continue;
        ma_uint64 cursor = 0;
        ma_sound_get_cursor_in_pcm_frames(&impl->sound, &cursor);
        saved.push_back({ impl, cursor, ma_sound_is_playing(&impl->sound) == MA_TRUE });
        ma_sound_uninit(&impl->sound);
        impl->soundInit = false;
    }
    const std::optional<QString> previous = e.device;
    if (e.initialized) {
        ma_engine_uninit(&e.engine);
        e.initialized = false;
    }
    bool ok = initEngine(name);
    if (!ok)
        initEngine(previous);
    for (const TSaved& entry : saved) {
        if (!initSound(*entry.impl))
            continue;
        TAudioSource* owner = entry.impl->owner;
        applyMix(*entry.impl, owner->volume(), owner->pitch(), owner->looping());
        ma_sound_seek_to_pcm_frame(&entry.impl->sound, entry.cursor);
        if (entry.playing)
            ma_sound_start(&entry.impl->sound);
    }
    return ok;
}

void setSuspended(const bool suspended)
{
    tibia::platform::setAudioSuspended(suspended);
    TEngine& e = engine();
    if (!e.initialized)
        return;
    if (suspended)
        ma_engine_stop(&e.engine);
    else
        ma_engine_start(&e.engine);
}

}

}
