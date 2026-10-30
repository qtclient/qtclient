#include "playerstatestracker.h"

#include "app/clientoptions.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>

namespace tibia::game {

namespace {

using Args = std::vector<std::any>;

/// CIP reads the creature's emblem: green, red, blue and "other" guilds are at war.
bool emblemAtWar(const uint8_t emblem) { return emblem < 6 && ((0x2eu >> emblem) & 1u) != 0; }

}

TPlayerStatesTracker::TPlayerStatesTracker(const config::SpecialConditionsFilter filter, QObject* parent)
    : QObject(parent)
    , m_filter(filter)
    , m_options(config::SpecialConditionsOptions::load())
{
    const auto onPlayer = [this](LuaObject* self, const Args&) {
        if (const LocalPlayerPtr& player = g_game.getLocalPlayer(); player && self == player.get())
            refresh();
        return std::any{};
    };
    for (const char* event : { "onStatesChange", "onRegenerationChange", "onSkullChange", "onEmblemChange" })
        m_hooks.emplace_back("object", event, onPlayer);
    m_hooks.emplace_back("g_game", "onRestingAreaState", [this](LuaObject*, const Args& args) {
        const bool inRestingArea = client::gamecore::argBool(args, 0);
        const bool hasBonus = client::gamecore::argBool(args, 1);
        const QString text = client::gamecore::argString(args, 2);
        if (inRestingArea != m_inRestingArea || hasBonus != m_restingAreaBonus || text != m_restingAreaText) {
            m_inRestingArea = inRestingArea;
            m_restingAreaBonus = hasBonus;
            m_restingAreaText = text;
            Q_EMIT restingAreaChanged(inRestingArea, hasBonus, text);
            refresh();
        }
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        m_inRestingArea = m_restingAreaBonus = false;
        m_restingAreaText.clear();
        m_model.setEntries({});
        return std::any{};
    });
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name != QLatin1String("specialConditionsOptions"))
            return;
        reloadOptions();
        Q_EMIT optionsChanged();
    });
    refresh();
}

void TPlayerStatesTracker::reloadOptions()
{
    m_options = config::SpecialConditionsOptions::load();
    refresh();
}

void TPlayerStatesTracker::refresh()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    if (!player) {
        m_model.setEntries({});
        return;
    }
    PlayerStatesInput input;
    input.states = static_cast<quint32>(player->getStates());
    input.rottenBlood = player->getRottenBloodState();
    input.inRestingArea = m_inRestingArea;
    input.restingAreaBonus = m_restingAreaBonus;
    input.restingAreaText = m_restingAreaText;
    input.playerKillerFlag = player->getSkull();
    input.guildWar = emblemAtWar(player->getEmblem());
    input.hungry = player->getRegenerationTime() == 0;
    m_model.setEntries(buildPlayerStates(input, m_options, m_filter));
}

}
