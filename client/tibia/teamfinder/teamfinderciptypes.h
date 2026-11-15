#pragma once

// Team Finder: the leader's own team, the team listings, and the settings and
// start-time objects the Join / Assemble pages bind to.

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <set>
#include <vector>

namespace tibia::protobuf::protocol {
class GameserverMessageTeamFinderTeamLeader;
class GameserverMessageTeamFinderTeamMember;
class ProtocolTeamFinderAssembly;
class ProtocolTeamSettings;
}

namespace tibia::chat {
class TWhiteBlacklist;
}

namespace tibia {
namespace teamfinder {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

/// What a team assembles for: nothing yet, a boss, a hunting place, a quest, or something else.
struct TTeamActivity
{
    enum Kind : int
    {
        None = 0,
        Boss = 1,
        Hunt = 2,
        Quest = 3,
        Other = 4,
    };

    Kind kind = None;
    uint16_t bossId = 0;
    uint16_t huntType = 0;
    uint16_t huntArea = 0;
    uint32_t questId = 0;

    /// Same kind and the same ids for that kind.
    bool operator==(const TTeamActivity& other) const;
};

/// A team's listing parameters. 0xffff team slots / free slots means "not set".
struct TTeamParameters
{
    uint16_t minLevel = 0;
    uint16_t maxLevel = 0;
    std::set<uint8_t> vocations;
    uint16_t teamSlots = 0xffff;
    uint16_t freeSlots = 0xffff;
    uint32_t startTime = 0;
    TTeamActivity activity;
    bool includeParty = false;

    bool operator==(const TTeamParameters& other) const;
};

/// Member status: applied and waiting, invited by the leader, accepted, rejected.
enum ETeamMemberStatus : uint32_t
{
    TeamMemberPending = 1,
    TeamMemberInvited = 2,
    TeamMemberAccepted = 3,
    TeamMemberRejected = 4,
};

struct TTeamMember
{
    uint32_t characterId = 0;
    QString name;
    uint16_t level = 0;
    uint8_t vocation = 0;
    uint32_t status = 0;

    bool operator==(const TTeamMember& other) const = default;
};

/// One listed team as the Join page sees it; `status` is the own application state.
struct TTeamData
{
    uint16_t teamId = 0;
    QString leaderName;
    TTeamParameters parameters;
    uint32_t status = 0;

    bool operator==(const TTeamData& other) const = default;
};

/// The own team (as leader) and the listed teams (as applicant).
class TTeamFinderStorage final : public cip::tibia::teamfinder::TTeamFinderStorage
{
    Q_OBJECT
public:
    using cip::tibia::teamfinder::TTeamFinderStorage::TTeamFinderStorage;

    /// Back to no own team: default parameters and no members.
    void reset();
    TTeamParameters teamParameters() const { return m_teamParameters; }
    void setTeamParameters(const TTeamParameters& Parameters);
    std::vector<TTeamMember> members() const { return m_members; }
    void setMembers(const std::vector<TTeamMember>& Members);
    std::vector<TTeamData> teams() const { return m_teams; }
    void setTeams(const std::vector<TTeamData>& Teams);
    void setMemberStatus(uint32_t CharacterID, uint32_t Status);
    void setTeamStatus(uint16_t TeamID, uint32_t Status);

private:
    TTeamParameters m_teamParameters;
    std::vector<TTeamMember> m_members;
    std::vector<TTeamData> m_teams;
};

/// Applies the team finder server messages to the storage and sends the team finder requests.
class TTeamFinderProtocolMessageHandler final : public cip::tibia::teamfinder::TTeamFinderProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TTeamFinderProtocolMessageHandler(TTeamFinderStorage* Storage, QObject* parent = nullptr);

    /// The session's handler (and its storage), created on first use.
    static TTeamFinderProtocolMessageHandler* instance();
    TTeamFinderStorage* storage() const { return m_storage; }

    /// Applicants on this list are rejected as soon as they apply.
    void setIgnoreList(chat::TWhiteBlacklist* IgnoreList) { m_ignoreList = IgnoreList; }

    /// Ingress: applies the protobuf message the queue built for `opcode`.
    static void onServerMessageEnqueued(int opcode);

    void handleTeamFinderTeamLeaderMessage(const ::tibia::protobuf::protocol::GameserverMessageTeamFinderTeamLeader& Message);
    void handleTeamFinderTeamMemberMessage(const ::tibia::protobuf::protocol::GameserverMessageTeamFinderTeamMember& Message);

    /// Leader side.
    void requestOwnTeam();
    void unlistTeam();
    void assembleTeam(TTeamParameters Parameters);
    void updateTeam(const TTeamParameters& Parameters);
    void setMemberStatus(uint32_t CharacterID, uint32_t Status);

    /// Applicant side.
    void requestTeamList();
    void joinTeam(uint16_t TeamID);
    void leaveTeam(uint16_t TeamID);

private:
    TTeamFinderStorage* m_storage = nullptr;
    chat::TWhiteBlacklist* m_ignoreList = nullptr;
};

/// Start time of a team listing or filter; 0 = none chosen, startImmediately = "now".
class TTeamStartTimeManager final : public cip::tibia::teamfinder::TTeamStartTimeManager
{
    Q_OBJECT
public:
    using cip::tibia::teamfinder::TTeamStartTimeManager::TTeamStartTimeManager;

    QString localDateString() const override;
    QString localTimeString() const override;
    QString serverTimeString() const override;
    int hour() const override;
    int minute() const override;
    bool startImmediately() const override;
    void setStartImmediately(bool StartImmediately) override;

    Q_INVOKABLE void setTime(int Hour, int Minute) override;

    /// 1 while "start immediately" is set, else the chosen time (0 = none).
    uint32_t getStartTime() const;
    /// 1 (immediately) is stored as "no time chosen".
    void setStartTime(uint32_t StartTime);

private:
    uint32_t m_startTime = 0;
    bool m_startImmediatelyFlag = false;
};

/// Assemble Team settings pane. Levels and sizes are numbers; the QML edits them as text.
class TTeamSettings final : public cip::tibia::teamfinder::TTeamSettings
{
    Q_OBJECT

public:
    explicit TTeamSettings(QObject* parent = nullptr);

    QString minLevel() const override;
    void setMinLevel(const QString& MinLevel) override;
    QString maxLevel() const override;
    void setMaxLevel(const QString& MaxLevel) override;
    QString teamSize() const override;
    void setTeamSize(const QString& TeamSize) override;
    QString freeSlots() const override;
    void setFreeSlots(const QString& FreeSlots) override;
    bool druidNeeded() const override { return m_druid; }
    void setDruidNeeded(bool v) override;
    bool knightNeeded() const override { return m_knight; }
    void setKnightNeeded(bool v) override;
    bool paladinNeeded() const override { return m_paladin; }
    void setPaladinNeeded(bool v) override;
    bool sorcererNeeded() const override { return m_sorcerer; }
    void setSorcererNeeded(bool v) override;
    bool monkNeeded() const override { return m_monk; }
    void setMonkNeeded(bool v) override;
    bool includeParty() const override { return m_party; }
    void setIncludeParty(bool v) override;
    QObject* startTimeManager() const override;

    TTeamStartTimeManager* startTime() const { return m_time; }

    /// Shows the storage's own-team parameters in the pane.
    void setFromParameters(const TTeamParameters& Parameters);
    /// Writes the pane into `Parameters` (levels, vocations, sizes, start time, party); the activity is kept.
    void toParameters(TTeamParameters& Parameters) const;
    /// Why the pane cannot be listed with `AcceptedMembers` members already accepted; empty when it can.
    QStringList validate(uint32_t AcceptedMembers) const;

private:
    /// Text to number (invalid text is 0); emits `changed` when the value differs.
    void setNumber(uint32_t& Field, const QString& Text);
    /// As setNumber, but empty text means "not set" (0xffff).
    void setSlots(uint32_t& Field, const QString& Text);
    void setFlag(bool& Flag, bool Value);

    uint32_t m_min = 0;
    uint32_t m_max = 0;
    bool m_druid = false;
    bool m_knight = false;
    bool m_paladin = false;
    bool m_sorcerer = false;
    bool m_monk = false;
    uint32_t m_size = 0xffff;
    uint32_t m_free = 0xffff;
    bool m_party = false;
    TTeamStartTimeManager* m_time = nullptr;
};

/// Empty for 0 ("no filter / not set"), else the number.
QString numberOrEmpty(uint32_t Value);
/// Empty for 0xffff ("not set"), else the number.
QString slotsOrEmpty(uint32_t Value);

/// "ddd yyyy-MM-dd, HH:mm" in the system locale, local time.
QString localDateTimeString(uint32_t Timestamp);
/// "ddd yyyy-MM-dd, HH:mm CET" (or CEST) in the server's time zone.
QString serverDateTimeString(uint32_t Timestamp);

/// Vocation display name ("knight", ... or the promoted form); "none" for anything else.
QString vocationName(uint8_t Vocation, bool Promoted);

}
}
