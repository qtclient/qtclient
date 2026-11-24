#include "sound/sounddecompressorthreadpool.h"

#include "sound/soundconcurrentexception.h"

#include "shared/tibiafilesystemhelper.h"

#include <QFutureWatcher>
#include <QPromise>
#include <QThreadPool>

#include <functional>

namespace tibia::sound {

namespace {

std::optional<QByteArray> readFile(const TSoundFile& file)
{
    try {
        QByteArray bytes = QByteArray::fromStdString(tibia::files::data().readFileContents(file.virtualPath));
        if (bytes.isEmpty())
            return std::nullopt;
        return bytes;
    } catch (...) {
        return std::nullopt;
    }
}

/// Runs `work` on the thread pool; once it finished, `done` gets its result
/// on the owner's thread and the watcher goes. A conversion that threw holds a
/// TSoundConcurrentException instead, which reading the result throws again.
template<typename T>
void runAsync(QObject* owner, std::function<T()> work, std::function<void(const T&)> done)
{
    auto* watcher = new QFutureWatcher<T>(owner);
    QObject::connect(watcher, &QFutureWatcher<T>::finished, owner, [watcher, done = std::move(done)] {
        done(watcher->future().result());
        watcher->deleteLater();
    });
    auto promise = std::make_shared<QPromise<T>>();
    watcher->setFuture(promise->future());
    auto task = [promise, work = std::move(work)] {
        promise->start();
        try {
            promise->addResult(work());
        } catch (...) {
            promise->setException(TSoundConcurrentException());
        }
        promise->finish();
    };
#if QT_CONFIG(thread)
    QThreadPool::globalInstance()->start(task);
#else
    task();
#endif
}

}

TSoundDecompressorThreadpool::TSoundDecompressorThreadpool(QObject* parent)
    : cip::tibia::sound::TSoundDecompressorThreadpool(parent)
{}

std::optional<TLoadingSoundData> TSoundDecompressorThreadpool::loadStep(const TLoadingSoundData& data)
{
    if (const auto* file = std::get_if<TSoundFile>(&data)) {
        if (auto bytes = readFile(*file))
            return TLoadingSoundData{ std::move(*bytes) };
        return std::nullopt;
    }
    if (const auto* bytes = std::get_if<QByteArray>(&data)) {
        if (auto samples = decodeSoundData(*bytes))
            return TLoadingSoundData{ std::move(*samples) };
        return std::nullopt;
    }
    if (const auto* samples = std::get_if<TRawSoundSamples>(&data)) {
        if (samples->empty())
            return std::nullopt;
        return TLoadingSoundData{ TSoundBuffer{ std::make_shared<const TRawSoundSamples>(*samples) } };
    }
    return std::nullopt;
}

TLoadingSoundData TSoundDecompressorThreadpool::loadUpTo(const TLoadingSoundData& data, const ESoundLoadTarget target)
{
    // The steps run file, bytes, samples, buffer: only a later form is reachable.
    const auto wanted = static_cast<std::size_t>(target);
    if (wanted <= data.index())
        return data;
    TLoadingSoundData loaded = data;
    while (loaded.index() != wanted) {
        std::optional<TLoadingSoundData> next = loadStep(loaded);
        if (!next)
            return data;
        loaded = std::move(*next);
    }
    return loaded;
}

std::optional<TLoadingSoundData> TSoundDecompressorThreadpool::loadSoundData(const TSoundData& soundData,
                                                                            const ESoundLoadTarget target)
{
    if (const auto* file = std::get_if<TSoundFile>(&soundData))
        return loadUpTo(TLoadingSoundData{ *file }, target);
    if (const auto* bytes = std::get_if<QByteArray>(&soundData))
        return loadUpTo(TLoadingSoundData{ *bytes }, target);
    if (const auto* samples = std::get_if<TRawSoundSamples>(&soundData))
        return loadUpTo(TLoadingSoundData{ *samples }, target);
    return std::nullopt;
}

TSoundData TSoundDecompressorThreadpool::toSoundData(const TSoundData& soundData, const ESoundLoadTarget target)
{
    std::optional<TLoadingSoundData> loaded = loadSoundData(soundData, target);
    if (!loaded)
        return ESoundDataState::Unavailable;
    if (auto* file = std::get_if<TSoundFile>(&*loaded))
        return std::move(*file);
    if (auto* bytes = std::get_if<QByteArray>(&*loaded))
        return std::move(*bytes);
    if (auto* samples = std::get_if<TRawSoundSamples>(&*loaded))
        return std::move(*samples);
    // A buffer is no source data.
    return ESoundDataState::Unavailable;
}

TPlayableSoundData TSoundDecompressorThreadpool::toPlayableSoundData(const TSoundData& soundData,
                                                                     const ESoundLoadTarget target)
{
    std::optional<TLoadingSoundData> loaded = loadSoundData(soundData, target);
    if (!loaded)
        return ESoundDataState::Unavailable;
    if (auto* file = std::get_if<TSoundFile>(&*loaded))
        return std::move(*file);
    if (auto* bytes = std::get_if<QByteArray>(&*loaded))
        return std::move(*bytes);
    if (auto* buffer = std::get_if<TSoundBuffer>(&*loaded))
        return std::move(*buffer);
    // Raw samples do not play: only their buffer does.
    return ESoundDataState::Unavailable;
}

TPlayableSoundData TSoundDecompressorThreadpool::playableSoundData(const TSoundEffectID soundId, const TSoundData& data,
                                                                   const ESoundLoadTarget target)
{
    if (std::holds_alternative<ESoundDataState>(data))
        return ESoundDataState::Unavailable;
    const int kind = static_cast<int>(data.index());
    if (kind == static_cast<int>(target) || (target == ESoundLoadTarget::Buffer && kind == static_cast<int>(ESoundLoadTarget::RawSamples))) {
        if (const auto* file = std::get_if<TSoundFile>(&data))
            return *file;
        if (const auto* bytes = std::get_if<QByteArray>(&data))
            return *bytes;
        return TSoundBuffer{ std::make_shared<const TRawSoundSamples>(std::get<TRawSoundSamples>(data)) };
    }
    convertToPlayableSoundData(soundId, data, target);
    return ESoundDataState::Loading;
}

TSoundData TSoundDecompressorThreadpool::soundData(const TSoundEffectID soundId, const TSoundData& data,
                                                   const ESoundLoadTarget target)
{
    if (std::holds_alternative<ESoundDataState>(data))
        return ESoundDataState::Unavailable;
    const int kind = static_cast<int>(data.index());
    if (kind == static_cast<int>(target) || (target == ESoundLoadTarget::Buffer && kind == static_cast<int>(ESoundLoadTarget::RawSamples)))
        return data;
    convertSoundData(soundId, data, target);
    return ESoundDataState::Loading;
}

void TSoundDecompressorThreadpool::convertToPlayableSoundData(const TSoundEffectID soundId, const TSoundData& soundData,
                                                              const ESoundLoadTarget target)
{
    runAsync<TPlayableSoundData>(
        this, [soundData, target] { return toPlayableSoundData(soundData, target); },
        [this, soundId](const TPlayableSoundData& result) { Q_EMIT convertingToPlayableSoundDataFinished(soundId, result); });
}

void TSoundDecompressorThreadpool::convertSoundData(const TSoundEffectID soundId, const TSoundData& soundData,
                                                    const ESoundLoadTarget target)
{
    runAsync<TSoundData>(
        this, [soundData, target] { return toSoundData(soundData, target); },
        [this, soundId](const TSoundData& result) { Q_EMIT convertingSoundDataFinished(soundId, result); });
}

}
