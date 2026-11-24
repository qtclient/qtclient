#pragma once

#include <cip_gen.h>

#include <QString>

#include <cstdint>
#include <memory>
#include <optional>

namespace tibia {
namespace chat {
struct TChatChannelIdentifier;
}
namespace config {
class TClientOptions;
}
namespace game {
class IGameSession;
}
namespace sound {

/// A range of seconds the music controller draws a pause or a cooldown from.
struct TSecondsRange
{
    uint16_t min = 0;
    uint16_t max = 0;
};

/// The sound system's fixed settings (CIP builds them in TGameClientBase).
struct TSoundSettings
{
    bool noSound = false;
    size_t soundPlayerCount = 64;
    /// Steps of every fade (100: a volume step per percent).
    uint8_t fadeSteps = 100;
    uint16_t ambienceStreamFadeMs = 1000;
    uint16_t ambienceObjectFadeMs = 500;
    /// Volume percent lost per field of distance, and the volume at distance 0.
    uint8_t distanceVolumeStep = 10;
    uint8_t distanceVolumeMax = 100;
    /// A floor counts as this many fields of distance.
    uint8_t floorDistance = 3;
    uint16_t musicFadeMs = 2000;
    /// Longest music play time in seconds; -1: the whole track.
    int32_t maxMusicDurationSeconds = -1;
    TSecondsRange shortMusicPause{ 10, 120 };
    TSecondsRange longMusicPause{ 120, 600 };
    TSecondsRange musicCooldown{ 600, 1200 };
    TSecondsRange immediateMusicCooldown{ 60, 180 };
    /// Most sounds a game session keeps (its TSoundStorage).
    uint16_t maxGameSessionSoundCount = 250;
    bool ambienceStreamsDisabled = false;
    bool ambienceDisabled = false;
    /// Distance modifier of plain music while the player flag is set.
    uint8_t musicVolumeWhileFlagged = 100;
    float ambienceDelayFactor = 2.0f;
    /// Seconds an unused decoded sound stays cached.
    int32_t soundCacheSeconds = 60;
};

/// Request context of a chat message sound (type 14): the channel the message
/// arrived in (the trigger's variant; empty for any other sound).
struct TChatSoundContext
{
    std::shared_ptr<const chat::TChatChannelIdentifier> channel;
};

/// CIP sound::TSoundConfiguration: the sound options and the settings.
class TSoundConfiguration final : public cip::tibia::sound::TSoundConfiguration
{
    Q_OBJECT
public:
    TSoundConfiguration(const TSoundSettings& settings, config::TClientOptions* options);

    const TSoundSettings& settings() const { return m_settings; }
    /// The session whose chat channels mute chat message sounds.
    void setGameSession(std::shared_ptr<game::IGameSession> GameSession);

    /// usedSoundDevice.
    QString usedSoundDevice() const;

    /// The engine is up and the master volume is not 0.
    bool isSoundEnabled() const;
    float itemsVolume() const;
    float eventsVolume() const;
    float uiVolume() const;
    float ambienceVolume() const;
    float musicVolume() const;
    /// The engine is up and master × ambience volume is audible.
    bool isAmbienceAudible() const;
    /// isAmbienceAudible and ambience streams not disabled.
    bool isAmbienceStreamEnabled() const;
    /// isAmbienceAudible and ambience not disabled.
    bool isAmbienceEnabled() const;
    /// The soundMasterVolume option, 0..100.
    uint8_t masterVolume() const;
    bool isMusicEnabled() const;
    /// isMusicEnabled and the anthem option.
    bool isTitleMusicEnabled() const;
    bool uiInteractionSoundsEnabled() const;
    bool privateMessagesWithoutTabSoundEnabled() const;

    bool creatureAttackSoundsEnabled() const;
    bool ownWeaponSoundsEnabled() const;
    bool othersWeaponSoundsEnabled() const;
    bool creatureNoiseSoundsEnabled() const;
    bool creatureDeathSoundsEnabled() const;
    bool eatingSoundsEnabled() const;
    bool moveItemSoundsEnabled() const;
    bool partySoundsEnabled() const;
    bool vipSoundsEnabled() const;
    bool raidMessageSoundsEnabled() const;
    bool systemMessageSoundsEnabled() const;
    bool chatSoundsEnabled() const;
    bool globalMessageSoundsEnabled() const;
    bool guildMessageSoundsEnabled() const;
    bool npcMessageSoundsEnabled() const;
    bool partyMessageSoundsEnabled() const;
    bool privateMessageSoundsEnabled() const;
    bool teamFinderMessageSoundsEnabled() const;
    bool ownSpellSoundsEnabled() const;
    bool othersSpellSoundsEnabled() const;

    struct TEffectSettings
    {
        bool enabled = false;
        float volume = 0.0f;
    };
    /// Whether a numeric sound effect of `type` from `source` (1 own, 2
    /// other players, 3 creatures) plays, and its volume in percent.
    TEffectSettings effectSettings(int type, const std::optional<TChatSoundContext>& chat, int source) const;

public Q_SLOTS:
    void onSoundOptionsChanged();

private:
    bool boolOption(int id) const;
    int intOption(int id) const;
    /// master / 100 × option / 100 × 100.
    float volume(int id) const;
    bool ownAttackSpellSoundsEnabled() const;
    bool ownHealingSpellSoundsEnabled() const;
    bool ownSupportSpellSoundsEnabled() const;
    bool othersAttackSpellSoundsEnabled() const;
    bool othersHealingSpellSoundsEnabled() const;
    bool othersSupportSpellSoundsEnabled() const;
    float ownBattleVolume() const;
    float othersBattleVolume() const;
    float creatureBattleVolume() const;
    /// Chat sounds on, a channel given, the session's channel of it not
    /// muted, then the option of the channel's kind.
    bool chatMessageSoundEnabled(const std::optional<TChatSoundContext>& chat) const;

    TSoundSettings m_settings;
    config::TClientOptions* m_options = nullptr;
    std::shared_ptr<game::IGameSession> m_gameSession;
};

}
}
