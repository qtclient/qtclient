#include "playerstatestracker.h"

#include "app/clientoptions.h"

#include <tibia/creatures/creaturestorage.h>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::game {

namespace {

using Args = std::vector<std::any>;


}

TPlayerStatesTracker::TPlayerStatesTracker(const config::SpecialConditionsFilter filter, QObject* parent)
    : QObject(parent)
    , m_filter(filter)
    , m_options(config::SpecialConditionsOptions::load())
{
    const auto onPlayer = [this](GameObject*, const Args&) {
        refresh();
        return std::any{};
    };
    for (const char* event : { "onStatesChange", "onRegenerationChange", "onSkullChange", "onEmblemChange" })
        m_hooks.emplace_back(tibia::game::kNotifyScope, event, onPlayer);
    // CipSoft: the server only says whether the player is in a resting area;
    // the bonus follows the daily reward streak (at least 2).
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onRestingAreaState", [this](GameObject*, const Args& args) {
        m_inRestingArea = client::gamecore::argBool(args, 0);
        updateRestingArea();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onOpenRewardWall", [this](GameObject*, const Args& args) {
        m_rewardStreak = static_cast<uint>(client::gamecore::argInt(args, 7));
        updateRestingArea();
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        m_inRestingArea = m_restingAreaBonus = false;
        m_rewardStreak = 0;
        m_restingAreaText.clear();
        m_model.setEntries({});
    });
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name != QLatin1String("specialConditionsOptions"))
            return;
        reloadOptions();
        Q_EMIT optionsChanged();
    });
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
    if (tibia::game::localPlayerId() == 0) {
        m_model.setEntries({});
        return;
    }
    PlayerStatesInput input;
    input.states = static_cast<quint32>(tibia::game::localPlayerStates());
    input.inRestingArea = m_inRestingArea;
    input.restingAreaText = m_restingAreaText;
    input.playerKillerFlag = tibia::game::localPlayerSkull();
    bool guildWar = false;
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (const auto row = storage->creatureAt(tibia::game::localPlayerId()))
            guildWar = row->isInGuildWar();
    }
    input.guildWar = guildWar;
    input.rottenBlood = tibia::game::localPlayerRottenBloodState();
    input.hungry = tibia::game::localPlayerRegenerationTime() == 0;
    m_model.setEntries(buildPlayerStates(input, m_options, m_filter));
}

}
