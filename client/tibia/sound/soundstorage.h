#pragma once

#include "sound/soundciptypes.h"

#include <cip_gen.h>

#include <client/position.h>

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tibia::sound {

/**
 * CIP sound bank (catalog-sound.json + sounds-*.dat) and miniaudio playback.
 * Music, ambience, UI, combat / secondary effects, and object ambience all
 * play here. There is no OpenAL path.
 */
class TSoundStorage final : public cip::tibia::sound::TSoundStorage
{
    Q_OBJECT
public:
    enum class Channel : uint8_t { Music = 0, Ambience = 1, Ui = 2 };

    explicit TSoundStorage(QObject* parent = nullptr);
    ~TSoundStorage() override;

    static TSoundStorage* instance();

    /// Load catalog-sound.json + sounds-*.dat from CIP `sounds/` (Resources
    /// sibling of `assets/`). Legacy extract paths are still accepted.
    bool loadBank(const std::string& virtualDirectory);
    bool isBankLoaded() const { return m_bankLoaded; }
    bool isEngineReady() const { return m_engineReady; }

    /// Playback device names from miniaudio (excludes CIP auto-select).
    QStringList playbackDeviceNames();
    /// CIP `usedSoundDevice`: `"auto"` or a name from playbackDeviceNames().
    QString usedPlaybackDevice() const { return m_usedPlaybackDevice; }
    /// Select output device; reinits the engine when the selection changes.
    bool setUsedPlaybackDevice(const QString& deviceOrAuto);

    void setChannelGain(Channel channel, float gain);
    void setMasterGain(float gain);
    /// CIP ClientOptions: master, channel gains, protocol volumes, usedSoundDevice.
    void applyClientOptions(const QJsonObject& options);

    bool playMusic(uint16_t musicTemplateId, bool loop = true);
    bool playAmbience(uint16_t ambientId, bool loop = true);
    /// Last ambient id passed to playAmbience (0 after stop / id 0). Used to restore place beds.
    uint16_t currentAmbienceId() const { return m_currentAmbienceId; }
    /// Weather (or other exclusive owner) holds the ambience channel: PlaceSound
    /// `0x85` ambient triggers queue in deferredAmbienceId() instead of stomping.
    bool ambienceHeld() const { return m_ambienceHeld; }
    void beginAmbienceHold();
    /// Release hold and play deferred PlaceSound id if any, else `fallbackRestoreId`.
    void endAmbienceHold(uint16_t fallbackRestoreId);
    /// Play/replace the loop while the channel is held (weather precip bed).
    bool playHeldAmbience(uint16_t ambientId, bool loop = true);
    /// Held ambience from a product path (`/data/sounds/mod/weather/...`), not a CIP ambient id.
    bool playHeldAmbiencePath(const std::string& virtualPath, bool loop = true);
    /// Extra multiplier on the ambience loop voice (intensity swell). Resets to 1 on hold end.
    void setAmbienceVoiceGain(float gain);
    float ambienceVoiceGain() const { return m_ambienceVoiceGain; }
    bool playUiEffect(uint16_t soundEffectId, float gain = 1.0f);
    /// One-shot numeric_sound_effect on the ambience bus (weather thunder, etc.).
    bool playEffect(uint16_t soundEffectId, float gain = 1.0f, float pan = 0.0f);
    /// One-shot from a product path on the ambience bus (weather thunder clap/rumble).
    bool playAmbienceOneshot(const std::string& virtualPath, float gain = 1.0f, float pan = 0.0f);
    /// Product login anthem (`/data/sounds/startup.ogg`).
    bool playFile(const std::string& virtualOrRelativePath, Channel channel, bool loop);

    /// Queued and mixed on poll() (protocol parse may run off the GUI thread).
    void playProtocolSoundMain(uint8_t soundSource, uint16_t soundEffectId, const Position& pos);
    void playProtocolSoundSecondary(uint8_t soundEnum, uint8_t soundSource, uint16_t soundEffectId, const Position& pos);

    /// Drain queued effects and refresh object-ambience loops.
    void poll();

    void stopMusic();
    /// Stops the login anthem only. World music on the same channel is left playing.
    void stopLoginAnthem();
    void stopAmbience();
    void stopUi();
    void stopAll();

private:
    // CIP-named engine surfaces (live playback is miniaudio via Engine).
    TActiveSoundPlayer m_activeSoundPlayer;
    TGameSessionSoundProvider m_gameSessionSoundProvider;
    TObjectSoundController m_objectSoundController;
    TSoundConcurrentException m_soundConcurrentException;
    TSoundConfiguration m_soundConfiguration;
    TSoundController m_soundController;
    TSoundControllerAmbienceObjectSound m_ambienceObjectSound;
    TSoundControllerAmbienceStream m_ambienceStream;
    TSoundControllerBase m_soundControllerBase;
    TSoundControllerCombatSound m_combatSound;
    TSoundControllerMusic m_musicController;
    TSoundDecompressorThreadpool m_decompressorThreadpool;
    TSoundEngine m_soundEngine;
    TSoundManager m_soundManager;
    TSoundMetadataManager m_soundMetadataManager;
    TSoundPlayer m_soundPlayer;
    TSoundPlayerManager m_soundPlayerManager;

    struct Engine;
    struct CatalogEffect {
        int type = 0;
        uint32_t soundId = 0;
        std::vector<uint32_t> randomSoundId;
    };
    struct CatalogAmbient {
        uint32_t loopedAudioFileId = 0;
    };
    struct CatalogObject {
        uint32_t maxDistance = 8;
        std::vector<std::pair<uint32_t, uint32_t>> countToFile;
    };
    struct CatalogMusic {
        uint32_t audioFileId = 0;
    };
    struct ProtocolSettings {
        int ownBattleVolume = 100;
        int otherPlayersVolume = 100;
        int creatureVolume = 100;
        int itemsVolume = 100;
        int eventVolume = 100;
        int uiVolume = 100;
        int ambienceVolume = 100;
        bool ownAttack = true;
        bool ownHealing = true;
        bool ownSupport = true;
        bool ownWeapons = true;
        bool othersAttack = true;
        bool othersHealing = true;
        bool othersSupport = true;
        bool othersWeapons = true;
        bool creatureNoises = true;
        bool creatureNoisesDeath = true;
        bool creatureAttacksAndSpells = true;
        bool foodAndBeverages = true;
        bool moveItem = true;
        bool uiInteractions = true;
        bool uiJoinLeaveParty = true;
        bool uiVipLoginLogout = true;
        bool notificationInteractions = true;
        bool notificationParty = true;
        bool notificationLocalChat = true;
        bool notificationPrivateMessages = true;
        bool notificationRaidAnnouncements = true;
        bool notificationSystemAnnouncements = true;
    };
    struct QueuedEffect {
        bool secondary = false;
        uint8_t soundEnum = 0;
        uint8_t source = 0;
        uint16_t id = 0;
        Position pos;
    };

    bool ensureEngine();
    bool reinitEngine();
    bool loadCatalog(const std::string& directory);
    bool playAmbienceInternal(uint16_t ambientId, bool loop);
    bool playResolvedPath(const std::string& virtualPath, Channel channel, bool loop, float gain);
    bool playOneshotPath(const std::string& virtualPath, float gain, float pan);
    void enqueueEffect(const QueuedEffect& effect);
    void playQueuedEffect(const QueuedEffect& effect);
    void pollObjectAmbience();
    void stopObjectAmbience();
    void stopOneShots();
    void refreshLoopVolumes();
    /// A running WebAudio context shows the tab speaker icon even at gain 0.
    void syncPlaybackDevice();
    std::string pathForAudioFile(uint32_t audioFileId) const;
    std::optional<uint32_t> chooseAudioFileId(uint16_t soundEffectId);
    const CatalogEffect* effect(uint32_t id) const;
    int protocolVolume(uint8_t soundSource, int soundType) const;
    bool protocolEnabled(uint8_t soundSource, int soundType) const;
    bool recentlyPlayed(uint16_t soundEffectId);
    std::optional<Position> listenerPosition() const;

    std::unique_ptr<Engine> m_engine;
    std::mutex m_queueMutex;
    std::vector<QueuedEffect> m_queue;
    bool m_bankLoaded = false;
    bool m_engineReady = false;
    bool m_playingLoginAnthem = false;
    QString m_usedPlaybackDevice = QStringLiteral("auto");
    bool m_ambienceHeld = false;
    bool m_hasDeferredAmbience = false;
    uint16_t m_currentAmbienceId = 0;
    uint16_t m_deferredAmbienceId = 0;
    std::string m_bankDir;
    float m_masterGain = 1.0f;
    float m_musicGain = 1.0f;
    float m_ambienceGain = 1.0f;
    float m_ambienceVoiceGain = 1.0f;
    float m_uiGain = 1.0f;
    int m_gameEndHook = 0;
    uint32_t m_maxObjectDistance = 8;
    ProtocolSettings m_protocol;
    std::unordered_map<uint32_t, std::string> m_files;
    std::unordered_map<uint32_t, CatalogEffect> m_effects;
    std::unordered_map<uint32_t, CatalogAmbient> m_ambients;
    std::unordered_map<uint32_t, CatalogObject> m_objects;
    std::unordered_map<uint32_t, std::vector<uint32_t>> m_objectsByClientId;
    std::unordered_map<uint32_t, CatalogMusic> m_music;
    std::unordered_map<uint16_t, uint64_t> m_lastPlayedAt;
    uint64_t m_lastCooldownCleanup = 0;
    std::mt19937 m_rng{ std::random_device{}() };
};

}
