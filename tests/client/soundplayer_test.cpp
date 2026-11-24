#include <gtest/gtest.h>

#include "sound/activesoundplayer.h"
#include "sound/sounddecompressorthreadpool.h"
#include "sound/soundplayermanager.h"

#include "readonlymemoryblob.h"
#include "shared/tibiafilesystemhelper.h"

#include <cmath>

// The test builds the players alone: a silent audio source and no decoder.
namespace tibia::sound {

struct TAudioSource::Impl {
    bool open = false;
    EStatus status = EStatus::Stopped;
    int64_t offset = 0;
};

TAudioSource::TAudioSource() : m_impl(std::make_unique<Impl>()) {}
TAudioSource::~TAudioSource() = default;
void TAudioSource::close() { *m_impl = Impl{}; }
bool TAudioSource::openBuffer(const TSoundBuffer& buffer) { m_impl->open = !buffer.empty(); return m_impl->open; }
bool TAudioSource::openMemory(const QByteArray& data) { m_impl->open = !data.isEmpty(); return m_impl->open; }
bool TAudioSource::openFile(const std::string&) { return false; }
bool TAudioSource::isOpen() const { return m_impl->open; }
void TAudioSource::play() { if (m_impl->open) m_impl->status = EStatus::Playing; }
void TAudioSource::pause() { m_impl->status = EStatus::Paused; }
void TAudioSource::stop() { m_impl->status = EStatus::Stopped; m_impl->offset = 0; }
TAudioSource::EStatus TAudioSource::status() const { return m_impl->status; }
void TAudioSource::setVolume(const float volume) { m_volume = volume; }
void TAudioSource::setPitch(const float pitch) { m_pitch = pitch; }
void TAudioSource::setLooping(const bool looping) { m_looping = looping; }
int64_t TAudioSource::playingOffsetMs() const { return m_impl->offset; }
void TAudioSource::setPlayingOffsetMs(const int64_t offsetMs) { m_impl->offset = offsetMs; }
int64_t TAudioSource::durationMs() const { return 1000; }

TSoundDecompressorThreadpool::TSoundDecompressorThreadpool(QObject* parent)
    : cip::tibia::sound::TSoundDecompressorThreadpool(parent) {}
void TSoundDecompressorThreadpool::convertToPlayableSoundData(TSoundEffectID, const TSoundData&, ESoundLoadTarget) {}
void TSoundDecompressorThreadpool::convertSoundData(TSoundEffectID, const TSoundData&, ESoundLoadTarget) {}
TPlayableSoundData TSoundDecompressorThreadpool::toPlayableSoundData(const TSoundData&, ESoundLoadTarget)
{
    return ESoundDataState::Unavailable;
}
TSoundData TSoundDecompressorThreadpool::toSoundData(const TSoundData& data, ESoundLoadTarget) { return data; }
TPlayableSoundData TSoundDecompressorThreadpool::playableSoundData(TSoundEffectID, const TSoundData&, ESoundLoadTarget)
{
    return ESoundDataState::Unavailable;
}
TSoundData TSoundDecompressorThreadpool::soundData(TSoundEffectID, const TSoundData& data, ESoundLoadTarget) { return data; }

TSoundMetadataManager::TSoundMetadataManager(QObject* parent, float)
    : cip::tibia::sound::TSoundMetadataManager(parent) {}
std::optional<TSoundMetadata> TSoundMetadataManager::sound(uint16_t) const { return std::nullopt; }
std::optional<TSoundMetadata> TSoundMetadataManager::soundByFileName(const QString&) const { return std::nullopt; }
QString soundDisplayName(const TSoundMetadata& sound) { return sound.fileName; }

}

namespace tibia::shared {
TTibiaFileSystemHelper::TTibiaFileSystemHelper() = default;
TTibiaFileSystemHelper::~TTibiaFileSystemHelper() = default;
bool TTibiaFileSystemHelper::fileExists(const std::string& fileName) const { return fileName == "/sounds/present.ogg"; }
}

namespace tibia::files {
shared::TTibiaFileSystemHelper& data()
{
    static shared::TTibiaFileSystemHelper helper;
    return helper;
}
}

namespace tibia::shared {
QByteArray TReadOnlyMemoryBlob::getData() const { return m_data; }
QString TReadOnlyMemoryBlob::getFilePath() const { return m_filePath; }
}

using namespace tibia::sound;

namespace {

TSoundCacheEntry readyEntry(const TSoundEffectID id)
{
    TSoundCacheEntry entry;
    entry.soundId = id;
    auto samples = std::make_shared<TRawSoundSamples>();
    samples->samples.assign(100, 0.0f);
    samples->channelCount = 1;
    samples->sampleRate = 100;
    entry.playableData = TSoundBuffer{ samples };
    return entry;
}

TPlayableSound playable(TSoundCacheEntry& entry)
{
    TSoundMetadata metadata;
    metadata.id = entry.soundId;
    metadata.fileName = QStringLiteral("sound.ogg");
    return TPlayableSound(&entry, metadata);
}

}

TEST(TVolumeModifiers, MultipliesFactorsAndClampsFadeAndDistance)
{
    TVolumeModifiers modifiers;
    EXPECT_FLOAT_EQ(modifiers.effectiveVolume(), 100.0f);
    modifiers.fade = 50;
    modifiers.distance = 200;
    modifiers.volume = 80.0f;
    modifiers.random = 120;
    // fade 0.5 × distance 1 (clamped) × volume 0.8 × random 1.2 (not clamped)
    EXPECT_NEAR(modifiers.effectiveVolume(), 48.0f, 1e-4f);
}

TEST(TPlayableSound, RegistersAndReleasesItsUse)
{
    TSoundCacheEntry entry = readyEntry(7);
    {
        TPlayableSound first = playable(entry);
        TPlayableSound copy = first;
        EXPECT_EQ(entry.users.size(), 2u);
        EXPECT_TRUE(first.isReady());
    }
    EXPECT_TRUE(entry.users.empty());
    // CipSoft counts a sound whose data is still loading as ready; the player waits for it.
    entry.playableData = ESoundDataState::Loading;
    TPlayableSound loading = playable(entry);
    EXPECT_TRUE(loading.isReady());
    EXPECT_TRUE(loading.isLoading());
}

TEST(TPlayableSound, IsReadyVisitsThePlayableData)
{
    TSoundCacheEntry entry = readyEntry(9);
    const auto ready = [&entry](TPlayableSoundData data) {
        entry.playableData = std::move(data);
        return playable(entry).isReady();
    };
    EXPECT_FALSE(ready(ESoundDataState::Unloaded));
    EXPECT_FALSE(ready(ESoundDataState::Unavailable));
    EXPECT_TRUE(ready(ESoundDataState::Loading));
    EXPECT_TRUE(ready(TSoundFile{ "/sounds/present.ogg" }));
    EXPECT_FALSE(ready(TSoundFile{ "/sounds/missing.ogg" }));
    EXPECT_TRUE(ready(QByteArray("ogg")));
    EXPECT_FALSE(ready(QByteArray()));
    EXPECT_FALSE(ready(TSoundBuffer{}));
    EXPECT_FALSE(ready(TSoundBuffer{ std::make_shared<TRawSoundSamples>() }));

    // A sound without an id is never ready.
    entry.playableData = ESoundDataState::Loading;
    EXPECT_FALSE(TPlayableSound(&entry, TSoundMetadata{}).isReady());
}

TEST(TSoundPlayerManager, HandsOutFreePlayersAndTakesThemBack)
{
    TSoundPlayerManager manager(2);
    EXPECT_EQ(manager.freePlayerCount(), 2u);
    std::optional<TSoundPlayerToken> a = manager.acquirePlayer();
    std::optional<TSoundPlayerToken> b = manager.acquirePlayer();
    ASSERT_TRUE(a && b);
    EXPECT_FALSE(manager.acquirePlayer());
    a.reset();
    EXPECT_EQ(manager.freePlayerCount(), 1u);

    // A kept player is released once its sound stopped.
    TSoundCacheEntry entry = readyEntry(3);
    b->player()->setSound(playable(entry));
    b->player()->play();
    manager.keepUntilFinished(std::move(*b));
    manager.onCheckPlayers();
    EXPECT_EQ(manager.freePlayerCount(), 1u);
    // reset() through the token stops the player; here the sound ends.
    std::optional<TSoundPlayerToken> c = manager.acquirePlayer();
    ASSERT_TRUE(c);
    EXPECT_EQ(manager.freePlayerCount(), 0u);
}

TEST(TSoundPlayer, WaitsForLoadingDataThenPlays)
{
    TSoundCacheEntry entry = readyEntry(5);
    entry.playableData = ESoundDataState::Loading;
    TSoundPlayer player;
    player.setSound(playable(entry));
    player.play();
    EXPECT_TRUE(player.isWaitingForData());
    EXPECT_TRUE(player.isPlaying());
    entry = readyEntry(5);
    entry.users.push_back(0);
    player.applySound();
    EXPECT_FALSE(player.isWaitingForData());
    EXPECT_TRUE(player.isPlaying());
    player.reset();
    EXPECT_FALSE(player.isPlaying());
    EXPECT_FALSE(player.sound().has_value());
}

TEST(TActiveSoundPlayer, FadesInInStepsAndCrossFades)
{
    auto manager = std::make_shared<TSoundPlayerManager>(4);
    TActiveSoundPlayer active(manager);
    active.setFade(1, 1000, 10);
    TSoundCacheEntry first = readyEntry(1);
    TSoundCacheEntry second = readyEntry(2);

    active.update(0);
    active.play(playable(first));
    EXPECT_EQ(active.currentSoundId(), 1);
    EXPECT_EQ(active.currentFadeLevel(), 0);
    active.update(250);
    // round(0.25 × 10) / 10 → 30% (round half away from zero)
    EXPECT_EQ(active.currentFadeLevel(), 30);
    active.update(1001);
    EXPECT_EQ(active.currentFadeLevel(), 100);

    active.play(playable(second));
    EXPECT_EQ(active.currentSoundId(), 2);
    EXPECT_EQ(active.lastFadingSoundId(), 1);
    active.update(1500);
    EXPECT_EQ(active.lastFadingLevel(), 50);
    active.update(2100);
    EXPECT_EQ(active.lastFadingSoundId(), 0);
    EXPECT_EQ(manager->freePlayerCount(), 3u);
}
