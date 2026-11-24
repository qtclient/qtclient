#include "playerstatestracker.h"

#include "config/clientoptions.h"
#include "dailyreward/dailyrewardciptypes.h"
#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"
#include "game/playerdata.h"

#include <tibia/creatures/creaturestorage.h>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::game {

TPlayerStatesTracker::TPlayerStatesTracker(const config::SpecialConditionsFilter filter, QObject* parent)
    : QObject(parent)
    , m_filter(filter)
    , m_options(config::SpecialConditionsOptions::load())
{
    // CIP: the session's player data, own creature and daily reward signals.
    m_sessionListener = subscribeCurrentGameSessionChanged([this] { connectToCurrentGameSession(); });
    connectToCurrentGameSession();
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        m_inRestingArea = m_restingAreaBonus = false;
        m_rewardStreak = 0;
        m_restingAreaText.clear();
        m_model.setEntries({});
    });
    connect(config::TClientOptions::instance(), &config::TClientOptions::specialConditionsOptionsChanged, this, [this] {
        reloadOptions();
        Q_EMIT optionsChanged();
    });
    refresh();
}

TPlayerStatesTracker::~TPlayerStatesTracker()
{
    unsubscribeCurrentGameSessionChanged(m_sessionListener);
}

void TPlayerStatesTracker::connectToCurrentGameSession()
{
    for (const QMetaObject::Connection& connection : m_sessionConnections)
        disconnect(connection);
    m_sessionConnections.clear();
    const TGameSessionBase* session = currentGameSession();
    if (!session) {
        connectToPlayerCreature();
        return;
    }
    // The states, the regeneration and the hunger are the player data's.
    if (const std::shared_ptr<TPlayerData>& player = session->playerData())
        m_sessionConnections.push_back(connect(player.get(), &TPlayerData::playerDataChanged, this, &TPlayerStatesTracker::refresh));
    // The skull and the guild war emblem are the own creature's.
    if (const std::shared_ptr<creatures::TCreatureStorage>& storage = session->creatureStorage())
        m_sessionConnections.push_back(connect(storage.get(), &creatures::TCreatureStorage::playerAdded, this,
                                               &TPlayerStatesTracker::connectToPlayerCreature));
    // CipSoft: the server only says whether the player is in a resting area;
    // the bonus follows the daily reward streak (at least 2).
    if (const auto& handler = session->dailyRewardProtocolMessageHandler())
        m_sessionConnections.push_back(connect(handler.get(), &dailyreward::TDailyRewardProtocolMessageHandler::restingAreaStateChanged,
                                               this, [this](const bool inRestingArea) {
                                                   m_inRestingArea = inRestingArea;
                                                   updateRestingArea();
                                               }));
    if (const std::shared_ptr<dailyreward::TDailyRewardStorage>& rewards = session->dailyRewardStorage()) {
        dailyreward::TDailyRewardStorage* const storage = rewards.get();
        m_sessionConnections.push_back(connect(storage, &dailyreward::TDailyRewardStorage::dailyRewardStreakStateChanged, this,
                                               [this, storage] {
                                                   m_rewardStreak = storage->rewardStreakLength();
                                                   updateRestingArea();
                                               }));
    }
    connectToPlayerCreature();
}

void TPlayerStatesTracker::connectToPlayerCreature()
{
    disconnect(m_playerKillerMarkConnection);
    disconnect(m_playerGuildFlagConnection);
    m_playerKillerMarkConnection = {};
    m_playerGuildFlagConnection = {};
    creatures::TCreatureStorage* const storage = currentCreatureStorage();
    const std::shared_ptr<creatures::TCreature> player = storage ? storage->player() : nullptr;
    if (player) {
        m_playerKillerMarkConnection =
            connect(player.get(), &creatures::TCreature::playerKillerMarkChanged, this, &TPlayerStatesTracker::refresh);
        m_playerGuildFlagConnection =
            connect(player.get(), &creatures::TCreature::playerGuildFlagChanged, this, &TPlayerStatesTracker::refresh);
    }
    refresh();
}

void TPlayerStatesTracker::updateRestingArea()
{
    const bool hasBonus = m_inRestingArea && m_rewardStreak >= 2;
    const QString text = m_inRestingArea ? qtTrId("playerstatus_client_restingarea_group_name") : QString();
    if (hasBonus == m_restingAreaBonus && text == m_restingAreaText && m_inRestingArea == m_lastInRestingArea)
        return;
    m_lastInRestingArea = m_inRestingArea;
    m_restingAreaBonus = hasBonus;
    m_restingAreaText = text;
    Q_EMIT restingAreaChanged(m_inRestingArea, hasBonus, text);
    refresh();
}

void TPlayerStatesTracker::reloadOptions()
{
    m_options = config::SpecialConditionsOptions::load();
    refresh();
}

void TPlayerStatesTracker::refresh()
{
    const TGameSessionBase* session = currentGameSession();
    tibia::creatures::TCreatureStorage* storage = session ? session->creatureStorage().get() : nullptr;
    const TPlayerData* data = session ? session->playerData().get() : nullptr;
    const std::shared_ptr<tibia::creatures::TCreature> player = storage ? storage->player() : nullptr;
    if (!session || !session->hasLocalPlayer() || !storage || storage->playerID() == 0 || !data) {
        m_model.setEntries({});
        return;
    }
    PlayerStatesInput input;
    input.states = static_cast<quint32>(data->states().flags.to_ullong());
    input.inRestingArea = m_inRestingArea;
    input.restingAreaText = m_restingAreaText;
    input.playerKillerFlag = player ? player->skull() : 0;
    input.guildWar = player && player->isInGuildWar();
    input.rottenBlood = data->rottenBloodState();
    input.hungry = data->regenerationTime() == 0;
    m_model.setEntries(buildPlayerStates(input, m_options, m_filter));
}

}
