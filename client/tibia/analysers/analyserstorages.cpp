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

QJsonObject THuntingSessionStorage::configurationToJson() const { return { { QLatin1String(KeyShowBaseXp), m_showBaseXp } }; }

void THuntingSessionStorage::configurationFromJson(const QJsonObject& json)
{
    m_showBaseXp = json.value(QLatin1String(KeyShowBaseXp)).toBool(false);
    Q_EMIT optionsChanged();
}

void THuntingSessionStorage::resetConfiguration() { configurationFromJson({}); }

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
    // No members first; then each value that was set falls back, with its signal.
    setMembers({}, {});
    if (m_lootType != 0) {
        m_lootType = 0;
        Q_EMIT lootTypeChanged();
    }
    if (m_sessionStart != 0) {
        m_sessionStart = 0;
        Q_EMIT sessionStartChanged();
    }
    if (m_leaderId != 0) {
        m_leaderId = 0;
        recompute();
        Q_EMIT partyLeaderIDChanged();
    }
}

TAnalyserStorages::TAnalyserStorages(QObject* parent)
    : QObject(parent)
    , m_gainWaste(TItemPriceStorage::instance())
    , m_huntingSession(&m_progress)
    , m_itemTracking(std::make_unique<cyclopedia::TItemTrackingStorage>(TItemPriceStorage::instance()))
    , m_lootBlackWhitelist(std::make_unique<quickloot::TQuickLootBlackWhitelistStorage>())
{
    s_instance = this;
    connect(&m_impact, &TImpactStorage::publishGameAction, this, &TAnalyserStorages::publishGameAction);
    connect(m_itemTracking.get(), &cyclopedia::TItemTrackingStorage::publishGameAction, this, &TAnalyserStorages::publishGameAction);
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPremiumChange", [this](GameObject*, const Args&) {
        Q_EMIT premiumTimeChanged();
        return std::any{};
    });

    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameStarted, this, [this]() {
        resetAll();
        m_partyHunt.clear();
        m_timer.start();
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        m_timer.stop();
        m_hasCharacter = false;
        Q_EMIT configurationLoadedForCharacter();
    });
    m_timer.setInterval(TickIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &TAnalyserStorages::tick);
    if (tibia::game::isOnline())
        m_timer.start();
}

TAnalyserStorages::~TAnalyserStorages()
{
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
}

void TAnalyserStorages::onCharacterConfigurationLoaded()
{
    m_hasCharacter = true;
    Q_EMIT configurationLoadedForCharacter();
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


}
