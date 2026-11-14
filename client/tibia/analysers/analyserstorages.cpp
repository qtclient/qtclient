#include "analysers/analyserstorages.h"

#include <client/item.h>
#include <client/staticdata.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>

#include <chrono>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::analysers {

namespace {

using Args = std::vector<std::any>;
using client::gamecore::argInt;
using client::gamecore::argString;

/// onImpactTracker analyzer types (Crystal ANALYZER_*; CIP handler types 0 / 1 / 2).
constexpr qint64 AnalyzerHeal = 0;
constexpr qint64 AnalyzerDamageDealt = 1;
constexpr qint64 AnalyzerDamageReceived = 2;

constexpr auto KeyShowBaseXp = "showBaseXp";

TAnalyserStorages* s_instance = nullptr;

}

THuntingSessionStorage::THuntingSessionStorage(TProgressStorage* progress, QObject* parent)
    : cip::tibia::cyclopedia::THuntingSessionStorage(parent)
    , m_progress(progress)
{
    connect(progress, &TProgressStorage::dataChanged, this, &THuntingSessionStorage::experienceChanged);
}

void THuntingSessionStorage::emitKillsChanged()
{
    Q_EMIT killsChanged();
    Q_EMIT killedMonstersChanged();
}

void THuntingSessionStorage::emitOptionsChanged()
{
    Q_EMIT optionsChanged();
    Q_EMIT configurationChangedFromUserInput();
}

QJsonObject THuntingSessionStorage::toJson() const { return { { QLatin1String(KeyShowBaseXp), m_showBaseXp } }; }

void THuntingSessionStorage::fromJson(const QJsonObject& json)
{
    m_showBaseXp = json.value(QLatin1String(KeyShowBaseXp)).toBool(false);
    emitOptionsChanged();
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
    emitKillsChanged();
}

void THuntingSessionStorage::addKill(const QString& name)
{
    ++m_kills[name];
    emitKillsChanged();
}

void THuntingSessionStorage::setShowBaseXp(const bool show)
{
    if (m_showBaseXp == show)
        return;
    m_showBaseXp = show;
    emitOptionsChanged();
}

TPartyHuntStorage::TPartyHuntStorage(QObject* parent)
    : cip::tibia::cyclopedia::TPartyHuntStorage(parent)
{}

void TPartyHuntStorage::update(const quint32 sessionSeconds, const quint32 leaderId, const int priceType, std::vector<Member> members)
{
    for (Member& member : members) {
        if (!member.name.isEmpty())
            m_names[member.id] = member.name;
        else if (const auto it = m_names.find(member.id); it != m_names.end())
            member.name = it->second;
    }
    const bool sessionChanged = sessionSeconds != m_sessionSeconds;
    const bool leaderChanged = leaderId != m_leaderId;
    const bool priceTypeChanged = priceType != m_priceType;
    m_sessionSeconds = sessionSeconds;
    m_leaderId = leaderId;
    m_priceType = priceType;
    m_members = std::move(members);
    Q_EMIT changed();
    Q_EMIT partyHuntDataChanged();
    if (priceTypeChanged)
        Q_EMIT lootTypeChanged();
    if (sessionChanged)
        Q_EMIT sessionStartChanged();
    if (leaderChanged)
        Q_EMIT partyLeaderIDChanged();
}

void TPartyHuntStorage::clear()
{
    const bool hadSession = m_sessionSeconds != 0;
    const bool hadLeader = m_leaderId != 0;
    const bool hadLootType = m_priceType != 0;
    m_sessionSeconds = 0;
    m_leaderId = 0;
    m_priceType = 0;
    m_members.clear();
    m_names.clear();
    Q_EMIT changed();
    Q_EMIT partyHuntDataChanged();
    if (hadLootType)
        Q_EMIT lootTypeChanged();
    if (hadSession)
        Q_EMIT sessionStartChanged();
    if (hadLeader)
        Q_EMIT partyLeaderIDChanged();
}

TAnalyserStorages::TAnalyserStorages(QObject* parent)
    : QObject(parent)
    , m_gainWaste(&m_prices)
    , m_huntingSession(&m_progress)
{
    s_instance = this;

    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameStarted, this, [this]() {
        resetAll();
        m_partyHunt.clear();
        if (tibia::game::localPlayer())
            setCharacter(tibia::game::localPlayerId());
        m_timer.start();
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        m_timer.stop();
        setCharacter(0);
    });
    // CIP XpChanged handler : (raw, final).
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onUpdateExperience", [this](GameObject*, const Args& args) {
        m_progress.onExperienceGained(argInt(args, 0), argInt(args, 1), nowMs());
        return std::any{};
    });
    // CIP ItemLooted handler .
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onLootStats", [this](GameObject*, const Args& args) {
        if (args.size() < 2)
            return std::any{};
        const uint16_t typeId = static_cast<uint16_t>(argInt(args, 0));
        const quint32 count = static_cast<quint32>(std::max<qint64>(1, argInt(args, 1)));
        m_gainWaste.addLoot(typeId, count, argString(args, 2), nowMs());
        return std::any{};
    });
    // CIP ItemWasted handler : one item per message.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onSupplyTracker", [this](GameObject*, const Args& args) {
        m_gainWaste.addWaste(static_cast<uint16_t>(argInt(args, 0)), 1, nowMs());
        return std::any{};
    });
    // CIP ImpactTracking handler: damage dealt is (physical amount, elemental
    // amount, element).
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onImpactTracker", [this](GameObject*, const Args& args) {
        const qint64 type = argInt(args, 0);
        const qint64 amount = argInt(args, 1);
        const int element = static_cast<int>(argInt(args, 2));
        if (type == AnalyzerHeal)
            m_impact.addHealing(amount, nowMs());
        else if (type == AnalyzerDamageDealt)
            m_impact.addDamage(amount, argInt(args, 4), element, nowMs());
        else if (type == AnalyzerDamageReceived)
            m_damageInput.addDamage(amount, element, argString(args, 3), nowMs());
        return std::any{};
    });
    // CIP KillTracking handler .
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onKillTracker", [this](GameObject*, const Args& args) {
        m_huntingSession.addKill(argString(args, 0));
        return std::any{};
    });
    // CIP PartyHuntAnalyser handler (every message; the Lua hook is throttled).
    const auto applyPartyAnalyzer = [this](GameObject*, const Args& args) {
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
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPartyAnalyzerUpdate", applyPartyAnalyzer);
    // CIP market statistics (TItemPriceStorage +0x60).
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onItemsPrice", [this](GameObject*, const Args& args) {
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
    if (tibia::game::isOnline()) {
        if (tibia::game::localPlayer())
            setCharacter(tibia::game::localPlayerId());
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
