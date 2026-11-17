#include "analysers/analyserstorages.h"
#include "analysers/itempricestorage.h"
#include "cyclopedia/itemtrackingstorage.h"
#include "quickloot/quicklootblackwhiteliststorage.h"

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
{}

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
    Q_EMIT experienceChanged();
    Q_EMIT sessionLengthChanged();
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

void TPartyHuntStorage::setSessionStart(const quint32 elapsedSeconds)
{
    const qint64 start = QDateTime::currentSecsSinceEpoch() - elapsedSeconds;
    if (m_sessionStart == start)
        return;
    m_sessionStart = start;
    Q_EMIT sessionStartChanged();
}

void TPartyHuntStorage::setLootType(const int lootType)
{
    if (m_lootType == lootType)
        return;
    m_lootType = lootType;
    Q_EMIT lootTypeChanged();
}

void TPartyHuntStorage::setLeaderId(const quint32 leaderId)
{
    if (m_leaderId == leaderId)
        return;
    m_leaderId = leaderId;
    recompute();
    Q_EMIT partyLeaderIDChanged();
}

void TPartyHuntStorage::setMembers(const std::vector<Member>& members, const std::unordered_map<quint32, QString>& names)
{
    if (!names.empty() && names != m_names)
        m_names = names;
    if (members.empty() && !m_names.empty())
        m_names.clear();
    if (members == m_members)
        return;
    m_members = members;
    recompute();
    Q_EMIT changed();
    Q_EMIT partyHuntDataChanged();
}

void TPartyHuntStorage::resolveName(Member& member) const
{
    if (const auto it = m_names.find(member.id); it != m_names.end())
        member.name = it->second;
    else
        member.name = qtTrId("dummy_unknown");
}

void TPartyHuntStorage::recompute()
{
    m_totalLoot = 0;
    m_totalSupplies = 0;
    m_totalBalance = 0;
    for (Member& member : m_members) {
        resolveName(member);
        member.balance = member.loot - member.supplies;
        m_totalLoot += member.loot;
        m_totalSupplies += member.supplies;
        m_totalBalance += member.balance;
    }
}

quint32 TPartyHuntStorage::sessionSeconds() const
{
    return m_sessionStart == 0 ? 0 : static_cast<quint32>(QDateTime::currentSecsSinceEpoch() - m_sessionStart);
}

void TPartyHuntStorage::clear()
{
    const bool hadSession = m_sessionStart != 0;
    const bool hadLeader = m_leaderId != 0;
    const bool hadLootType = m_lootType != 0;
    m_sessionStart = 0;
    m_leaderId = 0;
    m_lootType = 0;
    m_members.clear();
    m_names.clear();
    m_totalLoot = m_totalSupplies = m_totalBalance = 0;
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
    , m_gainWaste(TItemPriceStorage::instance())
    , m_huntingSession(&m_progress)
    , m_itemTracking(std::make_unique<cyclopedia::TItemTrackingStorage>(TItemPriceStorage::instance()))
    , m_lootBlackWhitelist(std::make_unique<quickloot::TQuickLootBlackWhitelistStorage>())
{
    s_instance = this;
    // CIP TCharacterConfigurationManager: a configuration changed by the user is saved.
    connect(TItemPriceStorage::instance(), &TItemPriceStorage::configurationChangedFromUserInput, this, &TAnalyserStorages::onConfigurationChanged);
    connect(m_itemTracking.get(), &cyclopedia::TItemTrackingStorage::configurationChangedFromUserInput, this,
            &TAnalyserStorages::onConfigurationChanged);
    connect(m_lootBlackWhitelist.get(), &quickloot::TQuickLootBlackWhitelistStorage::configurationChangedFromUserInput, this,
            &TAnalyserStorages::onConfigurationChanged);
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPremiumChange", [this](GameObject*, const Args&) {
        Q_EMIT premiumTimeChanged();
        return std::any{};
    });

    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameStarted, this, [this]() {
        resetAll();
        m_partyHunt.clear();
        if (tibia::game::hasLocalPlayer())
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
            m_damageInput.addHit({ static_cast<int>(amount), element, argString(args, 3), nowMs() });
        return std::any{};
    });
    m_timer.setInterval(TickIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &TAnalyserStorages::tick);
    if (tibia::game::isOnline()) {
        if (tibia::game::hasLocalPlayer())
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

TItemPriceStorage* TAnalyserStorages::prices() { return TItemPriceStorage::instance(); }

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
    m_damageInput.update(now);
    m_itemTracking->update(now);
    if (std::exchange(m_configurationDirty, false))
        saveConfiguration();
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

std::vector<TCharacterConfiguration*> TAnalyserStorages::configurations()
{
    return { &m_progress, &m_gainWaste, &m_impact, &m_damageInput, &m_huntingSession, TItemPriceStorage::instance(),
             m_itemTracking.get(), m_lootBlackWhitelist.get() };
}

void TAnalyserStorages::setCharacter(const quint32 characterId)
{
    if (characterId == m_characterId)
        return;
    saveConfiguration();
    m_characterId = characterId;
    // CIP makes the item storages per game session; the configurations start empty.
    TItemPriceStorage::instance()->fromJson(QJsonObject{});
    m_itemTracking->clear();
    m_lootBlackWhitelist->clear();
    if (m_characterId != 0) {
        for (TCharacterConfiguration* configuration : configurations()) {
            QFile file(configurationPath(*configuration));
            if (!file.open(QIODevice::ReadOnly))
                continue;
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
            if (document.isObject())
                configuration->fromJson(document.object());
        }
    }
    m_configurationDirty = false;
    Q_EMIT configurationLoadedForCharacter();
}

void TAnalyserStorages::saveConfiguration()
{
    if (m_characterId == 0)
        return;
    for (const TCharacterConfiguration* configuration : configurations()) {
        const QString path = configurationPath(*configuration);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            file.write(QJsonDocument(configuration->toJson()).toJson(QJsonDocument::Indented));
    }
}

}
