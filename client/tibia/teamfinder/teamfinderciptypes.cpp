#include "teamfinder/teamfinderciptypes.h"
#include <vector>
#include <protocol.pb.h>
#include <tibia/game/gamesession.h>
#include "teamfinder/teamfinder_types.h"
#include "protocol/protocolciptypes.h"

#include <QTime>

namespace tibia::teamfinder {

namespace {
} // namespace

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

TTeamStartTimeManager::TTeamStartTimeManager(QObject* parent)
    : cip::tibia::teamfinder::TTeamStartTimeManager(parent)
    , m_when(QDateTime::currentDateTime())
{
    m_startImmediately = true;
    m_hour = m_when.time().hour();
    m_minute = m_when.time().minute();
    refreshStrings();
}

QString TTeamStartTimeManager::localDateString() const
{
    return m_localDateString;
}

QString TTeamStartTimeManager::localTimeString() const
{
    return m_localTimeString;
}

QString TTeamStartTimeManager::serverTimeString() const
{
    return m_serverTimeString;
}

int TTeamStartTimeManager::hour() const
{
    return m_hour;
}

int TTeamStartTimeManager::minute() const
{
    return m_minute;
}

bool TTeamStartTimeManager::startImmediately() const
{
    return m_startImmediately;
}

void TTeamStartTimeManager::setStartImmediately(const bool v)
{
    if (::cip::unchanged(m_startImmediately, v))
        return;
    m_startImmediately = v;
    if (v) {
        m_when = QDateTime::currentDateTime();
        m_hour = m_when.time().hour();
        m_minute = m_when.time().minute();
    }
    refreshStrings();
    Q_EMIT timeChanged();
}

void TTeamStartTimeManager::setTime(const int Hour, const int Minute)
{
    m_startImmediately = false;
    m_hour = Hour;
    m_minute = Minute;
    m_when.setTime(QTime(qBound(0, Hour, 23), qBound(0, Minute, 59)));
    if (!m_when.isValid())
        m_when = QDateTime::currentDateTime();
    refreshStrings();
    Q_EMIT timeChanged();
}

uint32_t TTeamStartTimeManager::unixTime() const
{
    if (m_startImmediately)
        return 0;
    return static_cast<uint32_t>(m_when.toSecsSinceEpoch());
}

void TTeamStartTimeManager::refreshStrings()
{
    m_localDateString = m_when.date().toString(Qt::ISODate);
    m_localTimeString = m_when.time().toString(QStringLiteral("HH:mm"));
    // Stock qtclient: server clock mirrors local until a live offset is wired.
    m_serverTimeString = m_localTimeString;
}

TTeamSettings::TTeamSettings(QObject* parent)
    : cip::tibia::teamfinder::TTeamSettings(parent)
    , m_time(new TTeamStartTimeManager(this))
{
    m_startTimeManager = m_time;
    m_minLevel = QStringLiteral("1");
    m_maxLevel = QStringLiteral("9999");
    m_teamSize = QStringLiteral("5");
    m_freeSlots = QStringLiteral("4");
    m_druidNeeded = true;
    m_knightNeeded = true;
    m_paladinNeeded = true;
    m_sorcererNeeded = true;
    m_monkNeeded = true;
}

QString TTeamSettings::minLevel() const
{
    return m_minLevel;
}

void TTeamSettings::setMinLevel(const QString& v)
{
    cip::tibia::teamfinder::TTeamSettings::setMinLevel(v);
}

QString TTeamSettings::maxLevel() const
{
    return m_maxLevel;
}

void TTeamSettings::setMaxLevel(const QString& v)
{
    cip::tibia::teamfinder::TTeamSettings::setMaxLevel(v);
}

QString TTeamSettings::teamSize() const
{
    return m_teamSize;
}

void TTeamSettings::setTeamSize(const QString& v)
{
    cip::tibia::teamfinder::TTeamSettings::setTeamSize(v);
}

QString TTeamSettings::freeSlots() const
{
    return m_freeSlots;
}

void TTeamSettings::setFreeSlots(const QString& v)
{
    cip::tibia::teamfinder::TTeamSettings::setFreeSlots(v);
}

bool TTeamSettings::druidNeeded() const
{
    return m_druidNeeded;
}

void TTeamSettings::setDruidNeeded(const bool v)
{
    cip::tibia::teamfinder::TTeamSettings::setDruidNeeded(v);
}

bool TTeamSettings::knightNeeded() const
{
    return m_knightNeeded;
}

void TTeamSettings::setKnightNeeded(const bool v)
{
    cip::tibia::teamfinder::TTeamSettings::setKnightNeeded(v);
}

bool TTeamSettings::paladinNeeded() const
{
    return m_paladinNeeded;
}

void TTeamSettings::setPaladinNeeded(const bool v)
{
    cip::tibia::teamfinder::TTeamSettings::setPaladinNeeded(v);
}

bool TTeamSettings::sorcererNeeded() const
{
    return m_sorcererNeeded;
}

void TTeamSettings::setSorcererNeeded(const bool v)
{
    cip::tibia::teamfinder::TTeamSettings::setSorcererNeeded(v);
}

bool TTeamSettings::monkNeeded() const
{
    return m_monkNeeded;
}

void TTeamSettings::setMonkNeeded(const bool v)
{
    cip::tibia::teamfinder::TTeamSettings::setMonkNeeded(v);
}

bool TTeamSettings::includeParty() const
{
    return m_includeParty;
}

void TTeamSettings::setIncludeParty(const bool v)
{
    cip::tibia::teamfinder::TTeamSettings::setIncludeParty(v);
}

QObject* TTeamSettings::startTimeManager() const
{
    return m_startTimeManager;
}

namespace {

/// The settings a team listing and the leader's own team share.
template<typename Data>
void applyTeamSettings(const ::tibia::protobuf::protocol::ProtocolTeamSettings& settings, Data& data)
{
    data.minLevel = static_cast<uint16_t>(settings.min_level());
    data.maxLevel = static_cast<uint16_t>(settings.max_level());
    data.vocationIDs = static_cast<uint8_t>(settings.vocations());
    data.teamSlots = static_cast<uint16_t>(settings.team_slots());
    data.timestamp = settings.timestamp();
    data.teamType = static_cast<uint8_t>(settings.type().team_type());
    data.bossID = static_cast<uint16_t>(settings.type().boss_id());
    data.huntType = static_cast<uint16_t>(settings.type().hunt_type());
    data.huntArea = static_cast<uint16_t>(settings.type().hunt_area());
    data.questID = static_cast<uint16_t>(settings.type().quest_id());
}

}

void TTeamFinderProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/,
                                                                const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow)
        return;
    if (opcode == 44 && shadow->HasExtension(GameserverMessageTeamFinderTeamLeader::gameserver_message_team_finder_team_leader)) {
        const auto& m = shadow->GetExtension(GameserverMessageTeamFinderTeamLeader::gameserver_message_team_finder_team_leader);
        TeamFinderLeaderData data;
        data.reset = m.reset() != 0;
        if (!data.reset) {
            applyTeamSettings(m.settings(), data);
            data.freeSlots = static_cast<uint16_t>(m.settings().free_slots());
            for (const auto& member : m.members())
                data.members.push_back({ .guid = member.guid(), .name = member.name(), .level = static_cast<uint16_t>(member.level()),
                                         .vocation = static_cast<uint8_t>(member.vocation()),
                                         .status = static_cast<uint8_t>(member.status()) });
        }
        tibia::game::processTeamFinderLeader(data);
    } else if (opcode == 45 && shadow->HasExtension(GameserverMessageTeamFinderTeamMember::gameserver_message_team_finder_team_member)) {
        const auto& m = shadow->GetExtension(GameserverMessageTeamFinderTeamMember::gameserver_message_team_finder_team_member);
        std::vector<TeamFinderListEntry> entries;
        for (const auto& team : m.teams()) {
            TeamFinderListEntry& entry = entries.emplace_back();
            entry.leaderGuid = team.leader_guid();
            entry.leaderName = team.leader_name();
            applyTeamSettings(team.settings(), entry);
            entry.membersCount = static_cast<uint16_t>(team.settings().free_slots());
            entry.status = static_cast<uint8_t>(team.status());
        }
        tibia::game::processTeamFinderList(m.rate_limited() != 0, entries);
    }
}

} // namespace tibia::teamfinder
