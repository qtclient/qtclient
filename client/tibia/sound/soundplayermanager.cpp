#include "sound/soundplayermanager.h"

#include <algorithm>

namespace tibia::sound {

TSoundPlayerManager::TSoundPlayerToken::TSoundPlayerToken(TEntry* entry)
    : m_entry(entry)
{
    if (m_entry)
        m_entry->inUse = true;
}

TSoundPlayerManager::TSoundPlayerToken::TSoundPlayerToken(TSoundPlayerToken&& other) noexcept
    : m_entry(other.m_entry)
{
    other.m_entry = nullptr;
}

TSoundPlayerManager::TSoundPlayerToken& TSoundPlayerManager::TSoundPlayerToken::operator=(TSoundPlayerToken&& other) noexcept
{
    if (this != &other) {
        release();
        m_entry = other.m_entry;
        other.m_entry = nullptr;
    }
    return *this;
}

TSoundPlayerManager::TSoundPlayerToken::~TSoundPlayerToken()
{
    release();
}

void TSoundPlayerManager::TSoundPlayerToken::release()
{
    if (!m_entry)
        return;
    if (m_entry->player)
        m_entry->player->reset();
    m_entry->inUse = false;
    m_entry = nullptr;
}

void releaseFinishedPlayers(std::vector<TSoundPlayerToken>& tokens)
{
    tokens.erase(std::remove_if(tokens.begin(), tokens.end(),
                                [](const TSoundPlayerToken& token) {
                                    return !token.player() || !token.player()->isPlaying();
                                }),
                 tokens.end());
}

TSoundPlayerManager::TSoundPlayerManager(const size_t playerCount)
{
    m_players.reserve(playerCount);
    for (size_t index = 1; index <= playerCount; ++index)
        m_players.push_back(TEntry{ index, false, std::make_unique<TSoundPlayer>() });
    m_checkTimer.setInterval(100);
    m_checkTimer.setSingleShot(false);
    connect(&m_checkTimer, &QTimer::timeout, this, &TSoundPlayerManager::onCheckPlayers);
    m_checkTimer.start();
}

TSoundPlayerManager::~TSoundPlayerManager()
{
    for (TEntry& entry : m_players)
        entry.player.reset();
    m_finishingPlayers.clear();
}

std::optional<TSoundPlayerToken> TSoundPlayerManager::acquirePlayer()
{
    for (TEntry& entry : m_players) {
        if (!entry.inUse)
            return TSoundPlayerToken(&entry);
    }
    return std::nullopt;
}

size_t TSoundPlayerManager::freePlayerCount() const
{
    return static_cast<size_t>(std::count_if(m_players.begin(), m_players.end(), [](const TEntry& entry) {
        return !entry.inUse;
    }));
}

void TSoundPlayerManager::keepUntilFinished(TSoundPlayerToken token)
{
    m_finishingPlayers.push_back(std::move(token));
}

void TSoundPlayerManager::onCheckPlayers()
{
    releaseFinishedPlayers(m_finishingPlayers);
}

void TSoundPlayerManager::onSoundManagerPlayableSoundsChanged()
{
    for (TEntry& entry : m_players) {
        if (entry.inUse && entry.player->isWaitingForData())
            entry.player->applySound();
    }
}

}
