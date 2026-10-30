#include "analysers/analyserstorages.h"

#include <tibia/world/game.h>
#include <client/item.h>
#include <tibia/world/localplayer.h>
#include <client/staticdata.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>

#include <chrono>

namespace tibia::analysers {

namespace {

using Args = std::vector<std::any>;
using client::gamecore::argInt;
using client::gamecore::argString;

/// onImpactTracker analyzer types (Crystal ANALYZER_*; CIP handler 0x100a4e9b0 types 0 / 1 / 2).
constexpr qint64 AnalyzerHeal = 0;
constexpr qint64 AnalyzerDamageDealt = 1;
constexpr qint64 AnalyzerDamageReceived = 2;

constexpr auto KeyShowBaseXp = "showBaseXp";

TAnalyserStorages* s_instance = nullptr;

}

THuntingSessionStorage::THuntingSessionStorage(TProgressStorage* progress, QObject* parent)
    : QObject(parent)
    , m_progress(progress)
{
    connect(progress, &TProgressStorage::dataChanged, this, &THuntingSessionStorage::experienceChanged);
}

QJsonObject THuntingSessionStorage::toJson() const { return { { QLatin1String(KeyShowBaseXp), m_showBaseXp } }; }

void THuntingSessionStorage::fromJson(const QJsonObject& json)
{
    m_showBaseXp = json.value(QLatin1String(KeyShowBaseXp)).toBool(false);
    Q_EMIT optionsChanged();
}

void THuntingSessionStorage::tick(const qint64 nowMs)
{
    if (m_start == 0) {
        m_start = nowMs;
        m_startEpochMs = QDateTime::currentMSecsSinceEpoch();
    }
    if (nowMs <= m_nextTick)
        return;
    m_nextTick = nowMs + 1000;
    Q_EMIT sessionLengthChanged();
    Q_EMIT experienceChanged();
}

void THuntingSessionStorage::reset()
{
    m_start = m_nextTick;
    m_startEpochMs = QDateTime::currentMSecsSinceEpoch();
    if (m_progress)
        m_progress->reset();
    m_kills.clear();
    Q_EMIT sessionLengthChanged();
    Q_EMIT experienceChanged();
    Q_EMIT killsChanged();
}

void THuntingSessionStorage::addKill(const QString& name)
{
    ++m_kills[name];
    Q_EMIT killsChanged();
}

void THuntingSessionStorage::setShowBaseXp(const bool show)
{
    if (m_showBaseXp == show)
        return;
    m_showBaseXp = show;
    Q_EMIT optionsChanged();
}

TPartyHuntStorage::TPartyHuntStorage(QObject* parent)
    : QObject(parent)
{}

void TPartyHuntStorage::update(const quint32 sessionSeconds, const quint32 leaderId, const int priceType, std::vector<Member> members)
{
    for (Member& member : members) {
        if (!member.name.isEmpty())
            m_names[member.id] = member.name;
        else if (const auto it = m_names.find(member.id); it != m_names.end())
            member.name = it->second;
    }
    m_sessionSeconds = sessionSeconds;
    m_leaderId = leaderId;
    m_priceType = priceType;
    m_members = std::move(members);
    Q_EMIT changed();
}

void TPartyHuntStorage::clear()
{
    m_sessionSeconds = 0;
    m_leaderId = 0;
    m_priceType = 0;
    m_members.clear();
    m_names.clear();
    Q_EMIT changed();
}

TAnalyserStorages::TAnalyserStorages(QObject* parent)
    : QObject(parent)
    , m_gainWaste(&m_prices)
    , m_huntingSession(&m_progress)
{
    s_instance = this;

    m_hooks.emplace_back("g_game", "onGameStart", [this](LuaObject*, const Args&) {
        resetAll();
        m_partyHunt.clear();
        if (const LocalPlayerPtr& player = g_game.getLocalPlayer())
            setCharacter(player->getId());
        m_timer.start();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        m_timer.stop();
        setCharacter(0);
        return std::any{};
    });
    // CIP XpChanged handler 0x100a55670: (raw, final).
    m_hooks.emplace_back("g_game", "onUpdateExperience", [this](LuaObject*, const Args& args) {
        m_progress.onExperienceGained(argInt(args, 0), argInt(args, 1), nowMs());
        return std::any{};
    });
    // CIP ItemLooted handler 0x100a4f0b0.
    m_hooks.emplace_back("g_game", "onLootStats", [this](LuaObject*, const Args& args) {
        if (args.empty())
            return std::any{};
        const auto* item = std::any_cast<ItemPtr>(&args[0]);
        if (!item || !*item)
            return std::any{};
        const quint32 count = static_cast<quint32>(std::max(1, static_cast<int>((*item)->getCount())));
        m_gainWaste.addLoot(static_cast<uint16_t>((*item)->getId()), count, argString(args, 1), nowMs());
        return std::any{};
    });
    // CIP ItemWasted handler 0x100a4ee10: one item per message.
    m_hooks.emplace_back("g_game", "onSupplyTracker", [this](LuaObject*, const Args& args) {
        m_gainWaste.addWaste(static_cast<uint16_t>(argInt(args, 0)), 1, nowMs());
        return std::any{};
    });
    // CIP ImpactTracking handler 0x100a4e9b0. Crystal sends one amount and its element for
    // damage dealt, which CIP takes as (primary 0, secondary amount, element).
    m_hooks.emplace_back("g_game", "onImpactTracker", [this](LuaObject*, const Args& args) {
        const qint64 type = argInt(args, 0);
        const qint64 amount = argInt(args, 1);
        const int element = static_cast<int>(argInt(args, 2));
        if (type == AnalyzerHeal)
            m_impact.addHealing(amount, nowMs());
        else if (type == AnalyzerDamageDealt)
            m_impact.addDamage(0, amount, element, nowMs());
        else if (type == AnalyzerDamageReceived)
            m_damageInput.addDamage(amount, element, argString(args, 3), nowMs());
        return std::any{};
    });
    // CIP KillTracking handler 0x100a4e110.
    m_hooks.emplace_back("g_game", "onKillTracker", [this](LuaObject*, const Args& args) {
        m_huntingSession.addKill(argString(args, 0));
        return std::any{};
    });
    // CIP PartyHuntAnalyser handler 0x100a550e0 (every message; the Lua hook is throttled).
    const auto applyPartyAnalyzer = [this](LuaObject*, const Args& args) {
        if (args.size() < 5)
            return std::any{};
        const auto* data = std::any_cast<std::vector<PartyMemberData>>(&args[3]);
        const auto* names = std::any_cast<std::vector<PartyMemberName>>(&args[4]);
        if (!data)
            return std::any{};
        std::vector<TPartyHuntStorage::Member> members;
        members.reserve(data->size());
        for (const PartyMemberData& d : *data) {
            TPartyHuntStorage::Member member;
            member.id = d.memberID;
            member.active = d.highlight != 0;
            member.loot = static_cast<qint64>(d.loot);
            member.supplies = static_cast<qint64>(d.supply);
            member.damage = static_cast<qint64>(d.damage);
            member.healing = static_cast<qint64>(d.healing);
            if (names) {
                for (const PartyMemberName& n : *names) {
                    if (n.memberID == d.memberID)
                        member.name = QString::fromStdString(n.memberName);
                }
            }
            members.push_back(std::move(member));
        }
        m_partyHunt.update(static_cast<quint32>(argInt(args, 0)), static_cast<quint32>(argInt(args, 1)), static_cast<int>(argInt(args, 2)),
                           std::move(members));
        return std::any{};
    };
    m_hooks.emplace_back("g_game", "onPartyAnalyzerUpdate", applyPartyAnalyzer);
    // Throttled CipSoft publish (same payload as Update); keep both subscribed.
    m_hooks.emplace_back("g_game", "onPartyAnalyzer", applyPartyAnalyzer);
    // CIP market statistics (TItemPriceStorage +0x60).
    m_hooks.emplace_back("g_game", "onItemsPrice", [this](LuaObject*, const Args& args) {
        if (args.empty())
            return std::any{};
        const auto* prices = std::any_cast<std::map<uint16_t, uint64_t>>(&args[0]);
        if (!prices)
            return std::any{};
        std::unordered_map<uint16_t, qint64> market;
        market.reserve(prices->size());
        for (const auto& [itemId, price] : *prices)
            market.emplace(itemId, static_cast<qint64>(price));
        m_prices.setMarketPrices(std::move(market));
        return std::any{};
    });

    m_timer.setInterval(TickIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &TAnalyserStorages::tick);
    if (g_game.isOnline()) {
        if (const LocalPlayerPtr& player = g_game.getLocalPlayer())
            setCharacter(player->getId());
        m_timer.start();
    }
}

TAnalyserStorages::~TAnalyserStorages()
{
    saveConfiguration();
    if (s_instance == this)
        s_instance = nullptr;
}

TAnalyserStorages* TAnalyserStorages::instance() { return s_instance; }

qint64 TAnalyserStorages::nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void TAnalyserStorages::tick()
{
    const qint64 now = nowMs();
    m_huntingSession.tick(now);
    m_progress.tick(now);
    m_gainWaste.tick(now);
    m_impact.tick(now);
    m_damageInput.tick(now);
}

void TAnalyserStorages::resetAll()
{
    m_gainWaste.reset(TGainWasteStorage::Loot);
    m_gainWaste.reset(TGainWasteStorage::Waste);
    m_impact.reset();
    m_damageInput.reset();
    m_huntingSession.reset();
    m_progress.reset();
    Q_EMIT sessionReset();
}

QString TAnalyserStorages::configurationPath(const TCharacterConfiguration& configuration) const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/characterdata/%1/%2.json").arg(m_characterId).arg(configuration.configurationName());
}

void TAnalyserStorages::setCharacter(const quint32 characterId)
{
    if (characterId == m_characterId)
        return;
    saveConfiguration();
    m_characterId = characterId;
    if (m_characterId == 0)
        return;
    TCharacterConfiguration* const configurations[] = { &m_progress, &m_gainWaste, &m_impact, &m_damageInput, &m_huntingSession };
    for (TCharacterConfiguration* configuration : configurations) {
        QFile file(configurationPath(*configuration));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        if (document.isObject())
            configuration->fromJson(document.object());
    }
}

void TAnalyserStorages::saveConfiguration()
{
    if (m_characterId == 0)
        return;
    const TCharacterConfiguration* const configurations[] = { &m_progress, &m_gainWaste, &m_impact, &m_damageInput, &m_huntingSession };
    for (const TCharacterConfiguration* configuration : configurations) {
        const QString path = configurationPath(*configuration);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            file.write(QJsonDocument(configuration->toJson()).toJson(QJsonDocument::Indented));
    }
}

}
