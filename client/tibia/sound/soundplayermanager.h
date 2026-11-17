#pragma once

#include "sound/soundplayer.h"

#include <cip_gen.h>

#include <QTimer>

#include <memory>
#include <optional>
#include <vector>

namespace tibia {
namespace sound {

/// CIP sound::TSoundPlayerManager: the pool of sound players.
class TSoundPlayerManager final : public cip::tibia::sound::TSoundPlayerManager
{
    Q_OBJECT
public:
    struct TEntry
    {
        size_t index = 0;
        bool inUse = false;
        std::unique_ptr<TSoundPlayer> player;
    };

    /// A pool player in use; releasing it resets the player and frees it
    /// (CIP: TSoundPlayerManager::TSoundPlayerToken).
    class TSoundPlayerToken
    {
    public:
        TSoundPlayerToken() = default;
        explicit TSoundPlayerToken(TEntry* entry);
        TSoundPlayerToken(TSoundPlayerToken&& other) noexcept;
        TSoundPlayerToken& operator=(TSoundPlayerToken&& other) noexcept;
        TSoundPlayerToken(const TSoundPlayerToken&) = delete;
        TSoundPlayerToken& operator=(const TSoundPlayerToken&) = delete;
        ~TSoundPlayerToken();

        TSoundPlayer* player() const { return m_entry ? m_entry->player.get() : nullptr; }
        explicit operator bool() const { return m_entry != nullptr; }
        void release();

    private:
        TEntry* m_entry = nullptr;
    };

    explicit TSoundPlayerManager(size_t playerCount);
    ~TSoundPlayerManager() override;

    /// A free player, marked in use.
    std::optional<TSoundPlayerToken> acquirePlayer();
    size_t freePlayerCount() const;
    /// Keeps a player until its sound finished.
    void keepUntilFinished(TSoundPlayerToken token);

public Q_SLOTS:
    void onCheckPlayers();
    void onSoundManagerPlayableSoundsChanged() override;

private:
    std::vector<TEntry> m_players;
    QTimer m_checkTimer;
    std::vector<TSoundPlayerToken> m_finishingPlayers;
};

using TSoundPlayerToken = TSoundPlayerManager::TSoundPlayerToken;

/// Drops every token whose player stopped.
void releaseFinishedPlayers(std::vector<TSoundPlayerToken>& tokens);

}
}
