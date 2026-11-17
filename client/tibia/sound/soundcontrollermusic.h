#pragma once

#include "sound/soundcontrollerbase.h"

#include <unordered_map>

namespace tibia {
namespace sound {

/// CIP sound::TSoundControllerMusic: picks, pauses and cross-fades music.
class TSoundControllerMusic final : public TSoundControllerBase
{
    Q_OBJECT
public:
    /// Music template kinds.
    enum EMusicType : int { Music = 1, ImmediateMusic = 2, TitleMusic = 3 };
    /// Pause before the next music.
    enum EPause : int { Immediate = 0, ShortPause = 1, LongPause = 2 };

    TSoundControllerMusic(std::shared_ptr<TSoundMetadataManager> metadata, std::shared_ptr<TSoundManager> soundManager,
                          std::weak_ptr<TSoundPlayerManager> playerManager,
                          std::shared_ptr<TSoundConfiguration> configuration);

    void update(int64_t nowMs) override;
    void setSuspended(bool suspended) override;
    /// The server's music template (0: none).
    void setCurrentMusicTemplate(uint16_t templateId);
    /// The player flag that limits plain music (CIP: player data).
    void setPlayerFlag(bool flag);

    const TActiveSoundPlayer& activePlayer() const { return m_activePlayer; }
    std::optional<TMusicTemplateMetadata> currentTemplate() const { return m_currentTemplate; }

protected:
    void onSoundConfigurationChanged() override;

private:
    void updateMusic();
    void onMusicFinished();
    void setPause(int pause);
    void startMusic(TSoundEffectID soundId);
    void stopMusic(bool immediately);

    std::optional<TMusicTemplateMetadata> m_currentTemplate;
    std::optional<TMusicTemplateMetadata> m_pendingTemplate;
    std::optional<TMusicTemplateMetadata> m_requestedTemplate;
    int64_t m_nowSeconds = 0;
    int64_t m_nextMusicSeconds = 0;
    std::unordered_map<uint16_t, int64_t> m_templateCooldownSeconds;
    TActiveSoundPlayer m_activePlayer;
    bool m_wasPlaying = false;
    bool m_playerFlag = false;
};

}
}
