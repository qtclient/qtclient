#include "teamfinder/teamfinderciptypes.h"
#include "cyclopedia/staticcontentinformation.h"
#include <vector>
#include <protocol.pb.h>
#include <tibia/game/gamesession.h>
#include "chat/chatprotocolmessagehandler.h"
#include "teamfinder/teamfinder_types.h"
#include "protocol/protocolciptypes.h"
#include "assets/things.h"
#include "cyclopedia/cyclopediamapstaticdatastorage.h"

#include <QLocale>
#include <QTime>
#include <QTimeZone>

namespace tibia::teamfinder {

namespace {

/// "ddd yyyy-MM-dd" in the system locale, local time.
QString localDateString(const uint32_t Timestamp)
{
    return QLocale().toString(QDateTime::fromSecsSinceEpoch(Timestamp), QStringLiteral("ddd yyyy-MM-dd"));
}

/// `Time` in the server's time zone (Europe/Berlin).
QDateTime toServerTime(const QDateTime& Time)
{
#if QT_CONFIG(timezone)
    return Time.toTimeZone(QTimeZone("Europe/Berlin"));
#else
    // Qt for WebAssembly has no tz database: EU rule, CEST from 01:00 UTC on the last Sunday of
    // March to 01:00 UTC on the last Sunday of October.
    const QDateTime utc = Time.toUTC();
    const int year = utc.date().year();
    const auto switchAt = [year](const int month) {
        const QDate last(year, month, QDate(year, month, 1).daysInMonth());
        return QDateTime(last.addDays(-(last.dayOfWeek() % 7)), QTime(1, 0), QTimeZone::UTC);
    };
    const bool summer = utc >= switchAt(3) && utc < switchAt(10);
    return utc.toOffsetFromUtc(summer ? 7200 : 3600);
#endif
}

bool isServerSummerTime(const QDateTime& ServerTime)
{
#if QT_CONFIG(timezone)
    return ServerTime.isDaylightTime();
#else
    return ServerTime.offsetFromUtc() == 7200;
#endif
}

/// Team listing parameters from the server's settings message.
TTeamParameters parametersFromSettings(const ::tibia::protobuf::protocol::ProtocolTeamSettings& Settings)
{
    TTeamParameters parameters;
    parameters.minLevel = static_cast<uint16_t>(Settings.min_level());
    parameters.maxLevel = static_cast<uint16_t>(Settings.max_level());
    // One bit per vocation id (1 << vocation).
    const uint32_t vocations = Settings.vocations();
    if (vocations & 0x10)
        parameters.vocations.insert(4);
    if (vocations & 0x02)
        parameters.vocations.insert(1);
    if (vocations & 0x04)
        parameters.vocations.insert(2);
    if (vocations & 0x08)
        parameters.vocations.insert(3);
    if (vocations & 0x20)
        parameters.vocations.insert(5);
    parameters.teamSlots = static_cast<uint16_t>(Settings.team_slots());
    parameters.freeSlots = static_cast<uint16_t>(Settings.free_slots());
    parameters.startTime = Settings.timestamp();
    // The host protocol schema carries no "include party" flag for listings.
    parameters.includeParty = false;
    const auto& type = Settings.type();
    switch (type.team_type()) {
        case 0: parameters.activity = {}; break;
        case 1: parameters.activity = { .kind = TTeamActivity::Boss, .bossId = static_cast<uint16_t>(type.boss_id()) }; break;
        case 2:
            parameters.activity = { .kind = TTeamActivity::Hunt, .huntType = static_cast<uint16_t>(type.hunt_type()),
                                    .huntArea = static_cast<uint16_t>(type.hunt_area()) };
            break;
        case 3: parameters.activity = { .kind = TTeamActivity::Quest, .questId = type.quest_id() }; break;
        case 4: parameters.activity = { .kind = TTeamActivity::Other }; break;
        default: break;
    }
    return parameters;
}

/// The assemble / update request body for `Parameters`.
void fillAssembly(::tibia::protobuf::protocol::ProtocolTeamFinderAssembly* Assembly, const TTeamParameters& Parameters)
{
    Assembly->set_min_level(Parameters.minLevel);
    Assembly->set_max_level(Parameters.maxLevel);
    uint8_t vocations = 0;
    for (const uint8_t vocation : Parameters.vocations)
        vocations |= static_cast<uint8_t>(1u << (vocation & 0x1f));
    Assembly->set_vocations(vocations);
    Assembly->set_team_slots(Parameters.teamSlots);
    Assembly->set_free_slots(Parameters.freeSlots);
    Assembly->set_party_only(Parameters.includeParty);
    Assembly->set_timestamp(Parameters.startTime);
    // The host schema flattens the activity into a type and up to two ids.
    const TTeamActivity& activity = Parameters.activity;
    Assembly->set_team_type(static_cast<uint32_t>(activity.kind));
    switch (activity.kind) {
        case TTeamActivity::Boss: Assembly->set_first_id(activity.bossId); break;
        case TTeamActivity::Hunt:
            Assembly->set_first_id(activity.huntType);
            Assembly->set_second_id(activity.huntArea);
            break;
        case TTeamActivity::Quest: Assembly->set_first_id(activity.questId); break;
        default: break;
    }
}

std::unique_ptr<::tibia::protobuf::protocol::GameclientMessage> newAssembleTeamMessage(
    ::tibia::protobuf::protocol::GameclientMessageTeamFinderAssembleTeam*& Message)
{
    using namespace ::tibia::protobuf::protocol;
    auto message = protocol::newGameclientMessage(44);
    Message = message->MutableExtension(GameclientMessageTeamFinderAssembleTeam::gameclient_message_team_finder_assemble_team);
    return message;
}

std::unique_ptr<::tibia::protobuf::protocol::GameclientMessage> newJoinTeamMessage(
    ::tibia::protobuf::protocol::GameclientMessageTeamFinderJoinTeam*& Message)
{
    using namespace ::tibia::protobuf::protocol;
    auto message = protocol::newGameclientMessage(45);
    Message = message->MutableExtension(GameclientMessageTeamFinderJoinTeam::gameclient_message_team_finder_join_team);
    return message;
}

} // namespace

QString localDateTimeString(const uint32_t Timestamp)
{
    return QLocale().toString(QDateTime::fromSecsSinceEpoch(Timestamp), QStringLiteral("ddd yyyy-MM-dd, HH:mm"));
}

QString serverDateTimeString(const uint32_t Timestamp)
{
    const QDateTime serverTime = toServerTime(QDateTime::fromSecsSinceEpoch(Timestamp));
    return qtTrId("%1 %2")
        .arg(QLocale().toString(serverTime, QStringLiteral("ddd yyyy-MM-dd, HH:mm")))
        .arg(isServerSummerTime(serverTime) ? QStringLiteral("CEST") : QStringLiteral("CET"));
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

QString vocationName(const uint8_t Vocation, const bool Promoted)
{
    if (!Promoted) {
        switch (Vocation) {
            case 1: return qtTrId("knight");
            case 2: return qtTrId("paladin");
            case 3: return qtTrId("sorcerer");
            case 4: return qtTrId("druid");
            case 5: return qtTrId("monk");
            default: break;
        }
    } else {
        switch (Vocation) {
            case 1: return qtTrId("knight_promoted");
            case 2: return qtTrId("paladin_promoted");
            case 3: return qtTrId("sorcerer_promoted");
            case 4: return qtTrId("druid_promoted");
            case 5: return qtTrId("monk_promoted");
            default: break;
        }
    }
    return qtTrId("none");
}

// ---------------------------------------------------------------------------
// TTeamActivity / TTeamParameters

bool TTeamActivity::operator==(const TTeamActivity& other) const
{
    if (kind != other.kind)
        return false;
    switch (kind) {
        case Boss: return bossId == other.bossId;
        case Hunt: return huntType == other.huntType && huntArea == other.huntArea;
        case Quest: return questId == other.questId;
        default: return true;
    }
}

bool TTeamParameters::operator==(const TTeamParameters& other) const
{
    return minLevel == other.minLevel && maxLevel == other.maxLevel && vocations == other.vocations
        && teamSlots == other.teamSlots && freeSlots == other.freeSlots && startTime == other.startTime
        && activity == other.activity && includeParty == other.includeParty;
}

bool TTeamParameters::operator!=(const TTeamParameters& other) const
{
    return !(*this == other);
}

// ---------------------------------------------------------------------------
// TTeamFinderStorage

void TTeamFinderStorage::reset()
{
    setTeamParameters(TTeamParameters{});
    setMembers({});
}

void TTeamFinderStorage::setTeamParameters(const TTeamParameters& Parameters)
{
    m_teamParameters = Parameters;
    Q_EMIT teamParametersChanged();
}

void TTeamFinderStorage::setMembers(const std::vector<TTeamMember>& Members)
{
    if (m_members == Members)
        return;
    m_members = Members;
    Q_EMIT membersChanged();
}

void TTeamFinderStorage::setTeams(const std::vector<TTeamData>& Teams)
{
    if (m_teams == Teams)
        return;
    m_teams = Teams;
    Q_EMIT teamsChanged();
}

void TTeamFinderStorage::setMemberStatus(const uint32_t CharacterID, const uint32_t Status)
{
    for (TTeamMember& member : m_members) {
        if (member.characterId != CharacterID)
            continue;
        member.status = Status;
        Q_EMIT membersChanged();
        return;
    }
}

void TTeamFinderStorage::setTeamStatus(const uint16_t TeamID, const uint32_t Status)
{
    for (TTeamData& team : m_teams) {
        if (team.teamId != TeamID)
            continue;
        team.status = Status;
        Q_EMIT teamsChanged();
        return;
    }
}

// ---------------------------------------------------------------------------
// TTeamFinderProtocolMessageHandler

TTeamFinderProtocolMessageHandler::TTeamFinderProtocolMessageHandler(TTeamFinderStorage* Storage, QObject* parent)
    : cip::tibia::teamfinder::TTeamFinderProtocolMessageHandler(parent)
    , m_storage(Storage)
{
}

void TTeamFinderProtocolMessageHandler::handleTeamFinderTeamLeaderMessage(
    const ::tibia::protobuf::protocol::GameserverMessageTeamFinderTeamLeader& Message)
{
    if (Message.reset() == 0) {
        m_storage->setTeamParameters(parametersFromSettings(Message.settings()));
        std::vector<TTeamMember> members;
        for (const auto& member : Message.members())
            members.push_back({ .characterId = member.guid(),
                                .name = QString::fromStdString(member.name()),
                                .level = static_cast<uint16_t>(member.level()),
                                .vocation = static_cast<uint8_t>(member.vocation()),
                                .status = member.status() });
        // Applicants the leader ignores are turned down straight away.
        for (TTeamMember& member : members) {
            if (member.status != TeamMemberPending || !m_ignoreList || !m_ignoreList->isBlacklisted(member.name))
                continue;
            ::tibia::protobuf::protocol::GameclientMessageTeamFinderAssembleTeam* request = nullptr;
            auto message = newAssembleTeamMessage(request);
            request->set_action(2);
            request->set_member_id(member.characterId);
            request->set_member_status(TeamMemberRejected);
            m_storage->setMemberStatus(member.characterId, TeamMemberRejected);
            protocol::sendGameclientMessage(std::move(message));
            member.status = TeamMemberRejected;
        }
        m_storage->setMembers(members);
    } else if (Message.reset() == 1) {
        Q_EMIT ownTeamUpdateAvailable();
    }

    const auto& m = Message;
    // Product hook: the same message as plain data for packs.
    TeamFinderLeaderData data;
    data.reset = m.reset() != 0;
    if (!data.reset) {
        const auto& s = m.settings();
        data.minLevel = static_cast<uint16_t>(s.min_level());
        data.maxLevel = static_cast<uint16_t>(s.max_level());
        data.vocationIDs = static_cast<uint8_t>(s.vocations());
        data.teamSlots = static_cast<uint16_t>(s.team_slots());
        data.freeSlots = static_cast<uint16_t>(s.free_slots());
        data.timestamp = s.timestamp();
        data.teamType = static_cast<uint8_t>(s.type().team_type());
        data.bossID = static_cast<uint16_t>(s.type().boss_id());
        data.huntType = static_cast<uint16_t>(s.type().hunt_type());
        data.huntArea = static_cast<uint16_t>(s.type().hunt_area());
        data.questID = static_cast<uint16_t>(s.type().quest_id());
        for (const auto& member : m.members())
            data.members.push_back({ .guid = member.guid(), .name = member.name(), .level = static_cast<uint16_t>(member.level()),
                                     .vocation = static_cast<uint8_t>(member.vocation()),
                                     .status = static_cast<uint8_t>(member.status()) });
    }
    tibia::game::processTeamFinderLeader(data);
}

void TTeamFinderProtocolMessageHandler::handleTeamFinderTeamMemberMessage(
    const ::tibia::protobuf::protocol::GameserverMessageTeamFinderTeamMember& Message)
{
    if (Message.rate_limited() == 0) {
        std::vector<TTeamData> teams;
        for (const auto& team : Message.teams())
            teams.push_back({ .teamId = static_cast<uint16_t>(team.leader_guid()),
                              .leaderName = QString::fromStdString(team.leader_name()),
                              .parameters = parametersFromSettings(team.settings()),
                              .status = team.status() });
        m_storage->setTeams(teams);
    } else if (Message.rate_limited() == 1) {
        Q_EMIT teamLisUpdateAvailable();
    }

    const auto& m = Message;
    std::vector<TeamFinderListEntry> entries;
    for (const auto& team : m.teams()) {
        const auto& s = team.settings();
        TeamFinderListEntry& entry = entries.emplace_back();
        entry.leaderGuid = team.leader_guid();
        entry.leaderName = team.leader_name();
        entry.minLevel = static_cast<uint16_t>(s.min_level());
        entry.maxLevel = static_cast<uint16_t>(s.max_level());
        entry.vocationIDs = static_cast<uint8_t>(s.vocations());
        entry.teamSlots = static_cast<uint16_t>(s.team_slots());
        entry.membersCount = static_cast<uint16_t>(s.free_slots());
        entry.timestamp = s.timestamp();
        entry.teamType = static_cast<uint8_t>(s.type().team_type());
        entry.bossID = static_cast<uint16_t>(s.type().boss_id());
        entry.huntType = static_cast<uint16_t>(s.type().hunt_type());
        entry.huntArea = static_cast<uint16_t>(s.type().hunt_area());
        entry.questID = static_cast<uint16_t>(s.type().quest_id());
        entry.status = static_cast<uint8_t>(team.status());
    }
    tibia::game::processTeamFinderList(m.rate_limited() != 0, entries);
}

void TTeamFinderProtocolMessageHandler::requestOwnTeam()
{
    ::tibia::protobuf::protocol::GameclientMessageTeamFinderAssembleTeam* request = nullptr;
    auto message = newAssembleTeamMessage(request);
    request->set_action(0);
    protocol::sendGameclientMessage(std::move(message));
}

void TTeamFinderProtocolMessageHandler::unlistTeam()
{
    m_storage->reset();
    ::tibia::protobuf::protocol::GameclientMessageTeamFinderAssembleTeam* request = nullptr;
    auto message = newAssembleTeamMessage(request);
    request->set_action(1);
    protocol::sendGameclientMessage(std::move(message));
}

void TTeamFinderProtocolMessageHandler::assembleTeam(TTeamParameters Parameters)
{
    // Only one of team size / free slots given: derive the other (the leader takes one slot).
    if (Parameters.freeSlots == 0xffff && Parameters.teamSlots > 1 && Parameters.teamSlots != 0xffff)
        Parameters.freeSlots = static_cast<uint16_t>(Parameters.teamSlots - 1);
    else if (Parameters.teamSlots == 0xffff && Parameters.freeSlots != 0xffff)
        Parameters.teamSlots = static_cast<uint16_t>(Parameters.freeSlots + 1);
    m_storage->setTeamParameters(Parameters);
    updateTeam(Parameters);
}

void TTeamFinderProtocolMessageHandler::updateTeam(const TTeamParameters& Parameters)
{
    ::tibia::protobuf::protocol::GameclientMessageTeamFinderAssembleTeam* request = nullptr;
    auto message = newAssembleTeamMessage(request);
    request->set_action(3);
    fillAssembly(request->mutable_assembly(), Parameters);
    protocol::sendGameclientMessage(std::move(message));
}

void TTeamFinderProtocolMessageHandler::setMemberStatus(const uint32_t CharacterID, const uint32_t Status)
{
    ::tibia::protobuf::protocol::GameclientMessageTeamFinderAssembleTeam* request = nullptr;
    auto message = newAssembleTeamMessage(request);
    request->set_action(2);
    request->set_member_id(CharacterID);
    request->set_member_status(Status);
    m_storage->setMemberStatus(CharacterID, Status);
    protocol::sendGameclientMessage(std::move(message));
}

void TTeamFinderProtocolMessageHandler::requestTeamList()
{
    ::tibia::protobuf::protocol::GameclientMessageTeamFinderJoinTeam* request = nullptr;
    auto message = newJoinTeamMessage(request);
    request->set_action(0);
    protocol::sendGameclientMessage(std::move(message));
}

void TTeamFinderProtocolMessageHandler::joinTeam(const uint16_t TeamID)
{
    ::tibia::protobuf::protocol::GameclientMessageTeamFinderJoinTeam* request = nullptr;
    auto message = newJoinTeamMessage(request);
    request->set_action(1);
    request->set_leader_id(TeamID);
    m_storage->setTeamStatus(TeamID, 1);
    protocol::sendGameclientMessage(std::move(message));
}

void TTeamFinderProtocolMessageHandler::leaveTeam(const uint16_t TeamID)
{
    ::tibia::protobuf::protocol::GameclientMessageTeamFinderJoinTeam* request = nullptr;
    auto message = newJoinTeamMessage(request);
    request->set_action(2);
    request->set_leader_id(TeamID);
    m_storage->setTeamStatus(TeamID, 0);
    protocol::sendGameclientMessage(std::move(message));
}

// ---------------------------------------------------------------------------
// TTeamfinderActivityNameResolver

namespace {

/// Every space-separated word starts upper case ("the pale count" -> "The Pale Count").
QString capitalizeWords(const QString& Text)
{
    if (Text.isEmpty())
        return QString();
    QStringList words = Text.split(QStringLiteral(" "));
    for (QString& word : words) {
        if (!word.isEmpty())
            word = QString(word.at(0).toUpper()) + word.mid(1);
    }
    return words.join(QStringLiteral(" "));
}

QString bossName(const uint16_t BossID)
{
    for (const auto& race : tibia::appearances::TAppearancesManager::instance().getAllRaces()) {
        if (race.boss && race.raceId == BossID)
            return QString::fromStdString(race.name);
    }
    return QString();
}

QString questName(const uint32_t QuestID)
{
    const int id = static_cast<int>(QuestID);
    return cyclopedia::hasQuest(id) ? cyclopedia::questName(id) : QString();
}

} // namespace

QString TTeamfinderActivityNameResolver::name(const TTeamActivity& Activity) const
{
    const auto& map = cyclopedia::TCyclopediaMapStaticDataStorage::instance();
    switch (Activity.kind) {
        case TTeamActivity::Boss:
            if (Activity.bossId == 0)
                return qtTrId("teamfinder_unspecified_activity");
            return capitalizeWords(bossName(Activity.bossId));
        case TTeamActivity::Hunt: {
            if (Activity.huntType == 0)
                return qtTrId("teamfinder_unspecified_activity");
            return Activity.huntArea != 0 ? map.subAreaName(static_cast<quint16>(Activity.huntArea))
                                          : map.areaName(static_cast<quint16>(Activity.huntType));
        }
        case TTeamActivity::Quest:
            if (Activity.questId == 0)
                return qtTrId("teamfinder_unspecified_activity");
            return questName(Activity.questId);
        case TTeamActivity::Other: return qtTrId("teamfinder_unspecified_activity");
        default: return QString();
    }
}

QString TTeamfinderActivityNameResolver::alias(const TTeamActivity& Activity) const
{
    if (Activity.kind != TTeamActivity::Hunt)
        return QString();
    const auto& map = cyclopedia::TCyclopediaMapStaticDataStorage::instance();
    return Activity.huntArea != 0 ? map.subAreaAlias(static_cast<quint16>(Activity.huntArea))
                                  : map.areaAlias(static_cast<quint16>(Activity.huntType));
}

// ---------------------------------------------------------------------------
// TTeamStartTimeManager

QString TTeamStartTimeManager::localDateString() const
{
    if (m_startTime == 0)
        return qtTrId("teamfinder_not_time_selected");
    return teamfinder::localDateString(m_startTime);
}

QString TTeamStartTimeManager::localTimeString() const
{
    if (m_startTime == 0)
        return qtTrId("teamfinder_not_time_selected");
    return localDateTimeString(m_startTime);
}

QString TTeamStartTimeManager::serverTimeString() const
{
    if (m_startTime == 0)
        return qtTrId("teamfinder_not_time_selected");
    return serverDateTimeString(m_startTime);
}

int TTeamStartTimeManager::hour() const
{
    if (m_startTime == 0)
        return -1;
    return QDateTime::fromSecsSinceEpoch(m_startTime).time().hour();
}

int TTeamStartTimeManager::minute() const
{
    if (m_startTime == 0)
        return -1;
    return QDateTime::fromSecsSinceEpoch(m_startTime).time().minute();
}

bool TTeamStartTimeManager::startImmediately() const
{
    return m_startImmediatelyFlag;
}

void TTeamStartTimeManager::setStartImmediately(const bool StartImmediately)
{
    if (m_startImmediatelyFlag == StartImmediately)
        return;
    m_startImmediatelyFlag = StartImmediately;
    Q_EMIT timeChanged();
}

void TTeamStartTimeManager::setTime(const int Hour, const int Minute)
{
    if ((Hour | Minute) < 0) {
        m_startTime = 0;
    } else {
        // The next occurrence of Hour:Minute: today, or tomorrow if that has passed.
        const QTime time(Hour, Minute, 0, 0);
        QDateTime when;
        when.setDate(QDate::currentDate());
        if (time < QTime::currentTime())
            when = when.addDays(1);
        when.setTime(time);
        m_startTime = static_cast<uint32_t>(when.toSecsSinceEpoch());
    }
    Q_EMIT timeChanged();
}

uint32_t TTeamStartTimeManager::getStartTime() const
{
    return m_startImmediatelyFlag ? 1u : m_startTime;
}

void TTeamStartTimeManager::setStartTime(const uint32_t StartTime)
{
    const uint32_t startTime = StartTime == 1 ? 0u : StartTime;
    if (m_startTime == startTime)
        return;
    m_startTime = startTime;
    Q_EMIT timeChanged();
}

// ---------------------------------------------------------------------------
// TTeamSettings

QString numberOrEmpty(const uint32_t Value)
{
    return Value == 0 ? QString() : QString::number(Value);
}

QString slotsOrEmpty(const uint32_t Value)
{
    return Value == 0xffff ? QString() : QString::number(Value);
}

TTeamSettings::TTeamSettings(QObject* parent)
    : cip::tibia::teamfinder::TTeamSettings(parent)
    , m_druid(true)
    , m_knight(true)
    , m_paladin(true)
    , m_sorcerer(true)
    , m_monk(true)
    , m_party(true)
    , m_time(new TTeamStartTimeManager(this))
{
    m_startTimeManager = m_time;
    m_time->setStartImmediately(true);
}

QString TTeamSettings::minLevel() const
{
    return numberOrEmpty(m_min);
}

void TTeamSettings::setMinLevel(const QString& MinLevel)
{
    setNumber(m_min, MinLevel);
}

QString TTeamSettings::maxLevel() const
{
    return numberOrEmpty(m_max);
}

void TTeamSettings::setMaxLevel(const QString& MaxLevel)
{
    setNumber(m_max, MaxLevel);
}

QString TTeamSettings::teamSize() const
{
    return slotsOrEmpty(m_size);
}

void TTeamSettings::setTeamSize(const QString& TeamSize)
{
    setSlots(m_size, TeamSize);
}

QString TTeamSettings::freeSlots() const
{
    return slotsOrEmpty(m_free);
}

void TTeamSettings::setFreeSlots(const QString& FreeSlots)
{
    setSlots(m_free, FreeSlots);
}

void TTeamSettings::setNumber(uint32_t& Field, const QString& Text)
{
    const uint32_t value = Text.toUInt();
    if (Field == value)
        return;
    Field = value;
    Q_EMIT changed();
}

void TTeamSettings::setSlots(uint32_t& Field, const QString& Text)
{
    const uint32_t value = Text.isEmpty() ? 0xffffu : Text.toUInt();
    if (Field == value)
        return;
    Field = value;
    Q_EMIT changed();
}

void TTeamSettings::setFlag(bool& Flag, const bool Value)
{
    if (Flag == Value)
        return;
    Flag = Value;
    Q_EMIT changed();
}

void TTeamSettings::setDruidNeeded(const bool v)
{
    setFlag(m_druid, v);
}

void TTeamSettings::setKnightNeeded(const bool v)
{
    setFlag(m_knight, v);
}

void TTeamSettings::setPaladinNeeded(const bool v)
{
    setFlag(m_paladin, v);
}

void TTeamSettings::setSorcererNeeded(const bool v)
{
    setFlag(m_sorcerer, v);
}

void TTeamSettings::setMonkNeeded(const bool v)
{
    setFlag(m_monk, v);
}

void TTeamSettings::setIncludeParty(const bool v)
{
    setFlag(m_party, v);
}

QObject* TTeamSettings::startTimeManager() const
{
    return m_time;
}

void TTeamSettings::setFromParameters(const TTeamParameters& Parameters)
{
    m_min = Parameters.minLevel;
    m_max = Parameters.maxLevel;
    m_druid = Parameters.vocations.contains(4);
    m_knight = Parameters.vocations.contains(1);
    m_paladin = Parameters.vocations.contains(2);
    m_sorcerer = Parameters.vocations.contains(3);
    m_monk = Parameters.vocations.contains(5);
    m_size = Parameters.teamSlots;
    m_free = Parameters.freeSlots;
    m_time->setStartTime(Parameters.startTime);
    m_party = false;
    Q_EMIT changed();
}

void TTeamSettings::toParameters(TTeamParameters& Parameters) const
{
    Parameters.minLevel = static_cast<uint16_t>(m_min);
    Parameters.maxLevel = static_cast<uint16_t>(m_max);
    Parameters.vocations.clear();
    if (m_druid)
        Parameters.vocations.insert(4);
    if (m_knight)
        Parameters.vocations.insert(1);
    if (m_paladin)
        Parameters.vocations.insert(2);
    if (m_sorcerer)
        Parameters.vocations.insert(3);
    if (m_monk)
        Parameters.vocations.insert(5);
    Parameters.teamSlots = static_cast<uint16_t>(m_size);
    Parameters.freeSlots = static_cast<uint16_t>(m_free);
    Parameters.startTime = m_time->getStartTime();
    Parameters.includeParty = m_party;
}

QStringList TTeamSettings::validate(const uint32_t AcceptedMembers) const
{
    QStringList errors;
    if (m_max != 0 && m_max < m_min)
        errors.append(qtTrId("teamfinder_levelrange_error"));
    // The free slots plus the accepted members (at least the leader) must fit the team size.
    if (m_free != 0xffff && m_size != 0xffff && m_size < m_free + AcceptedMembers + (AcceptedMembers == 0 ? 1u : 0u))
        errors.append(qtTrId("teamfinder_freespots_error"));
    if (!m_time->startImmediately() && m_time->getStartTime() == 0)
        errors.append(qtTrId("teamfinder_starttime_error"));
    if (m_size != 0xffff && m_size < AcceptedMembers)
        errors.append(qtTrId("teamfinder_teamsize_error"));
    return errors;
}

// ---------------------------------------------------------------------------
// Ingress

} // namespace tibia::teamfinder
