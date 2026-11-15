#include "teamfinder/teamsortproxymodels.h"

#include <QDateTime>
#include <QLocale>

#include <compare>
#include <map>
#include <tuple>

namespace tibia {
namespace {

using teamfinder::TTeamActivity;

/// "ddd HH:mm" in the system locale, local time.
QString weekdayTimeString(const uint32_t Timestamp)
{
    return QLocale().toString(QDateTime::fromSecsSinceEpoch(Timestamp), QStringLiteral("ddd HH:mm"));
}

/// Every word of `Filter` (split at spaces) occurs in `Text`, ignoring case.
bool containsAllWords(const QString& Text, const QString& Filter)
{
    const QStringList words = Filter.split(QLatin1Char(' '));
    for (const QString& word : words) {
        if (Text.indexOf(word, 0, Qt::CaseInsensitive) == -1)
            return false;
    }
    return true;
}

/// Sort rank of a member / application status: pending, invited, accepted, rejected, none.
int statusRank(const uint Status)
{
    static const std::map<uint, int> ranks = { { 1, 0 }, { 2, 1 }, { 3, 2 }, { 4, 3 }, { 0, 4 } };
    const auto it = ranks.find(Status);
    return it != ranks.end() ? it->second : 4;
}

/// Orders activities by type name, then by name (case-sensitive).
std::strong_ordering compareTypeAndName(const QString& LeftType, const QString& LeftName, const QString& RightType,
                                        const QString& RightName)
{
    if (QString::compare(LeftType, RightType, Qt::CaseSensitive) < 0)
        return std::strong_ordering::less;
    if (QString::compare(RightType, LeftType, Qt::CaseSensitive) < 0)
        return std::strong_ordering::greater;
    if (QString::compare(LeftName, RightName, Qt::CaseSensitive) < 0)
        return std::strong_ordering::less;
    if (QString::compare(RightName, LeftName, Qt::CaseSensitive) < 0)
        return std::strong_ordering::greater;
    return std::strong_ordering::equal;
}

} // namespace

QString activityTypeName(const int Kind)
{
    switch (Kind) {
        case TTeamActivity::Boss: return qtTrId("teamfinder_activty_boss");
        case TTeamActivity::Hunt: return qtTrId("teamfinder_activty_hunt");
        case TTeamActivity::Quest: return qtTrId("teamfinder_activty_quest");
        case TTeamActivity::Other: return qtTrId("teamfinder_activty_other");
        default: return qtTrId("dummy_unknown");
    }
}

TActivity makeActivity(const TTeamActivity& Type, const teamfinder::TTeamfinderActivityNameResolver& Resolver)
{
    return { .type = Type, .typeName = activityTypeName(Type.kind), .name = Resolver.name(Type), .alias = Resolver.alias(Type) };
}

QString activityDisplayName(const TTeamActivity& Activity, const teamfinder::TTeamfinderActivityNameResolver& Resolver)
{
    return QStringLiteral("%1: %2").arg(activityTypeName(Activity.kind)).arg(Resolver.name(Activity).trimmed());
}

// ---------------------------------------------------------------------------
// TTeamMembersModel

QHash<int, QByteArray> TTeamMembersModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[CharacterIdRole] = "characterId";
    roles[NameRole] = "name";
    roles[LevelRole] = "level";
    roles[VocationRole] = "vocation";
    roles[VocationStringRole] = "vocationString";
    roles[StatusRole] = "status";
    roles[IsLeaderRole] = "isLeader";
    return roles;
}

void TTeamMembersModel::setCharacterName(const QString& Name)
{
    if (m_characterName == Name)
        return;
    m_characterName = Name;
    if (rowCount() > 0)
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0), { IsLeaderRole });
}

QVariant TTeamMembersModel::itemData(const teamfinder::TTeamMember& Member, const int Role) const
{
    switch (Role) {
        case CharacterIdRole: return Member.characterId;
        case NameRole: return Member.name;
        case LevelRole: return uint(Member.level);
        case VocationRole: return uint(Member.vocation);
        case VocationStringRole: return teamfinder::vocationName(Member.vocation, false);
        case StatusRole: return Member.status;
        case IsLeaderRole: return Member.name == m_characterName;
        default: return {};
    }
}

// ---------------------------------------------------------------------------
// TActivitiesModel

QHash<int, QByteArray> TActivitiesModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TypeRole] = "type";
    roles[TypeNameRole] = "typeName";
    roles[NameRole] = "name";
    roles[AliasRole] = "alias";
    return roles;
}

QVariant TActivitiesModel::itemData(const TActivity& Activity, const int Role) const
{
    switch (Role) {
        case TypeRole: return int(Activity.type.kind);
        case TypeNameRole: return QStringLiteral("%1:").arg(Activity.typeName);
        case NameRole: return Activity.name;
        case AliasRole: return Activity.alias;
        default: return {};
    }
}

// ---------------------------------------------------------------------------
// TTeamsListModel

QHash<int, QByteArray> TTeamsListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TeamIdRole] = "teamId";
    roles[ActivityTypeRole] = "activityType";
    roles[TypeNameRole] = "typeName";
    roles[NameRole] = "name";
    roles[AliasRole] = "alias";
    roles[MinLevelRole] = "minLevel";
    roles[MaxLevelRole] = "maxLevel";
    roles[LevelRangeRole] = "levelRange";
    roles[TeamSizeRole] = "teamSize";
    roles[TakenSlotsRole] = "takenSlots";
    roles[TeamSizeStringRole] = "teamSizeString";
    roles[VocationStringRole] = "vocationString";
    roles[StartTimeRole] = "startTime";
    roles[StartTimeStringRole] = "startTimeString";
    roles[StartTimeDetailStringRole] = "startTimeDetailString";
    roles[LeaderNameRole] = "leaderName";
    roles[StatusRole] = "status";
    roles[IsLeaderRole] = "isLeader";
    return roles;
}

void TTeamsListModel::setCharacterName(const QString& Name)
{
    if (m_characterName == Name)
        return;
    m_characterName = Name;
    if (rowCount() > 0)
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0), { IsLeaderRole });
}

QVariant TTeamsListModel::itemData(const teamfinder::TTeamData& Team, const int Role) const
{
    if (!m_resolver)
        return {};
    const teamfinder::TTeamParameters& parameters = Team.parameters;
    switch (Role) {
        case TeamIdRole: return uint(Team.teamId);
        case ActivityTypeRole: return int(parameters.activity.kind);
        case TypeNameRole: return QStringLiteral("%1:").arg(activityTypeName(parameters.activity.kind));
        case NameRole: return m_resolver->name(parameters.activity);
        case AliasRole: return m_resolver->alias(parameters.activity);
        case MinLevelRole: return uint(parameters.minLevel);
        case MaxLevelRole: return uint(parameters.maxLevel);
        case LevelRangeRole: {
            const uint minLevel = parameters.minLevel;
            const uint maxLevel = parameters.maxLevel;
            if (minLevel != 0 && maxLevel != 0)
                return QStringLiteral("%1-%2").arg(minLevel).arg(maxLevel);
            if (minLevel != 0)
                return QStringLiteral("> %1").arg(minLevel);
            if (maxLevel != 0)
                return QStringLiteral("< %1").arg(maxLevel);
            return qtTrId("any");
        }
        case TeamSizeRole: return uint(parameters.teamSlots);
        case TakenSlotsRole: return uint(parameters.freeSlots);
        case TeamSizeStringRole:
            if (parameters.teamSlots != 0xffff)
                return QStringLiteral("%1/%2").arg(uint(parameters.freeSlots)).arg(uint(parameters.teamSlots));
            return QString::number(parameters.freeSlots);
        case VocationStringRole: {
            QStringList names;
            for (const uint8_t vocation : { 4, 1, 2, 3, 5 }) {
                if (parameters.vocations.contains(vocation))
                    names.append(teamfinder::vocationName(vocation, false));
            }
            if (!names.isEmpty() && names.size() != 5)
                return names.join(QStringLiteral(", "));
            return qtTrId("spelllist_all_vocations");
        }
        case StartTimeRole: return parameters.startTime;
        case StartTimeStringRole:
            if (parameters.startTime != 1)
                return weekdayTimeString(parameters.startTime);
            return qtTrId("teamfinder_start_time_immediately");
        case StartTimeDetailStringRole:
            if (parameters.startTime != 1)
                return qtTrId("teamfinder_date_time_local_sever")
                    .arg(teamfinder::localDateTimeString(parameters.startTime))
                    .arg(teamfinder::serverDateTimeString(parameters.startTime));
            return QString();
        case LeaderNameRole: return Team.leaderName;
        case StatusRole: return Team.status;
        case IsLeaderRole: return Team.leaderName == m_characterName;
        default: return {};
    }
}

// ---------------------------------------------------------------------------
// TTeamsListSortProxyModel

TTeamsListSortProxyModel::TTeamsListSortProxyModel(QObject* parent)
    : cip::TTeamsListSortProxyModel(parent)
    , m_model(new TTeamsListModel(this))
    , m_time(new teamfinder::TTeamStartTimeManager(this))
{
    m_startTimeManager = m_time;
    m_time->setStartImmediately(true);
    setDynamicSortFilter(true);
    sort(0, Qt::AscendingOrder);
    setSourceModel(m_model);
    connect(m_time, &teamfinder::TTeamStartTimeManager::timeChanged, this, &TTeamsListSortProxyModel::anouceFilterChanged);
    resetFilters();
}

int TTeamsListSortProxyModel::numberOfTeams() const
{
    return rowCount();
}

QString TTeamsListSortProxyModel::filterLevel() const
{
    return teamfinder::numberOrEmpty(m_filterLevel);
}

void TTeamsListSortProxyModel::setFilterLevel(const QString& v)
{
    setUIntFilter(m_filterLevel, v);
}

QString TTeamsListSortProxyModel::minTeamSize() const
{
    return teamfinder::numberOrEmpty(m_minSize);
}

void TTeamsListSortProxyModel::setMinTeamSize(const QString& v)
{
    setUIntFilter(m_minSize, v);
}

QString TTeamsListSortProxyModel::maxTeamSize() const
{
    return teamfinder::numberOrEmpty(m_maxSize);
}

void TTeamsListSortProxyModel::setMaxTeamSize(const QString& v)
{
    setUIntFilter(m_maxSize, v);
}

void TTeamsListSortProxyModel::setDruidWanted(const bool v)
{
    setFlag(m_druid, v);
}

void TTeamsListSortProxyModel::setKnightWanted(const bool v)
{
    setFlag(m_knight, v);
}

void TTeamsListSortProxyModel::setPaladinWanted(const bool v)
{
    setFlag(m_paladin, v);
}

void TTeamsListSortProxyModel::setSorcererWanted(const bool v)
{
    setFlag(m_sorcerer, v);
}

void TTeamsListSortProxyModel::setMonkWanted(const bool v)
{
    setFlag(m_monk, v);
}

QString TTeamsListSortProxyModel::nameFilter() const
{
    return m_name;
}

void TTeamsListSortProxyModel::setNameFilter(const QString& v)
{
    if (m_name == v)
        return;
    m_name = v;
    anouceFilterChangedAndEmitfilterchanged();
}

void TTeamsListSortProxyModel::setShowBosses(const bool v)
{
    setFlag(m_showBosses, v);
}

void TTeamsListSortProxyModel::setShowHunts(const bool v)
{
    setFlag(m_showHunts, v);
}

void TTeamsListSortProxyModel::setShowQuests(const bool v)
{
    setFlag(m_showQuests, v);
}

void TTeamsListSortProxyModel::setShowOther(const bool v)
{
    setFlag(m_showOther, v);
}

void TTeamsListSortProxyModel::setJitterTime(const uint v)
{
    if (m_jitterTime == v)
        return;
    m_jitterTime = v;
    anouceFilterChangedAndEmitfilterchanged();
}

QObject* TTeamsListSortProxyModel::startTimeManager() const
{
    return m_time;
}

void TTeamsListSortProxyModel::resetFilters()
{
    m_filterLevel = 0;
    m_minSize = 0;
    m_maxSize = 0;
    m_druid = false;
    m_knight = false;
    m_paladin = false;
    m_sorcerer = false;
    m_monk = false;
    m_name.clear();
    m_jitterTime = 0;
    m_showBosses = true;
    m_showHunts = true;
    m_showQuests = true;
    m_showOther = true;
    m_time->setStartTime(0);
    m_time->setStartImmediately(false);
    m_characterFiltersSet = false;
    anouceFilterChangedAndEmitfilterchanged();
}

void TTeamsListSortProxyModel::setCharacterFilters(const uint32_t Level, const uint8_t Vocation)
{
    m_characterFiltersSet = true;
    m_filterLevel = Level & 0xffff;
    m_druid = Vocation == 4;
    m_knight = Vocation == 1;
    m_paladin = Vocation == 2;
    m_sorcerer = Vocation == 3;
    m_monk = Vocation == 5;
    anouceFilterChangedAndEmitfilterchanged();
}

void TTeamsListSortProxyModel::setTeams(const std::vector<teamfinder::TTeamData>& Teams)
{
    beginFilterChange();
    m_model->setItems(Teams);
    endFilterChange(QSortFilterProxyModel::Direction::Both);
    invalidate();
    Q_EMIT numberOfTeamsChagned();
}

void TTeamsListSortProxyModel::anouceFilterChanged()
{
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Both);
    invalidate();
}

void TTeamsListSortProxyModel::anouceFilterChangedAndEmitfilterchanged()
{
    anouceFilterChanged();
    Q_EMIT filterChanged();
}

void TTeamsListSortProxyModel::setFlag(bool& Flag, const bool Value)
{
    if (Flag == Value)
        return;
    Flag = Value;
    anouceFilterChangedAndEmitfilterchanged();
}

void TTeamsListSortProxyModel::setUIntFilter(uint32_t& Field, const QString& Text)
{
    const uint32_t value = Text.toUInt();
    if (Field == value)
        return;
    Field = value;
    anouceFilterChangedAndEmitfilterchanged();
}

bool TTeamsListSortProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    const QModelIndex row = m_model->index(sourceRow, 0);
    if (!row.isValid())
        return false;

    bool accepted = true;
    const QString filter = m_name.trimmed();
    if (!filter.isEmpty()) {
        const QString text = row.data(TTeamsListModel::TypeNameRole).toString() + QLatin1Char(' ')
            + row.data(TTeamsListModel::NameRole).toString() + QLatin1Char(' ')
            + row.data(TTeamsListModel::AliasRole).toString() + QLatin1Char(' ')
            + row.data(TTeamsListModel::LeaderNameRole).toString();
        accepted = containsAllWords(text, filter);
    }

    if (m_filterLevel != 0) {
        const uint minLevel = row.data(TTeamsListModel::MinLevelRole).toUInt();
        const uint maxLevelValue = row.data(TTeamsListModel::MaxLevelRole).toUInt();
        const uint maxLevel = maxLevelValue != 0 ? maxLevelValue : 99999;
        accepted = accepted && m_filterLevel <= maxLevel && minLevel <= m_filterLevel;
    }

    // The listing's vocations, read back from its display text.
    const QString vocationText = row.data(TTeamsListModel::VocationStringRole).toString();
    std::set<uint8_t> vocations;
    if (vocationText.indexOf(qtTrId("spelllist_all_vocations"), 0, Qt::CaseSensitive) != -1)
        vocations = { 4, 1, 2, 3, 5 };
    for (const uint8_t vocation : { 4, 1, 2, 3, 5 }) {
        if (vocationText.indexOf(teamfinder::vocationName(vocation, false), 0, Qt::CaseSensitive) != -1)
            vocations.insert(vocation);
    }
    const bool anyVocationWanted = m_druid || m_knight || m_paladin || m_sorcerer || m_monk;
    if (!vocations.empty() && anyVocationWanted) {
        const bool match = (m_druid && vocations.contains(4)) || (m_knight && vocations.contains(1))
            || (m_paladin && vocations.contains(2)) || (m_sorcerer && vocations.contains(3))
            || (m_monk && vocations.contains(5));
        accepted = accepted && match;
    }

    // Team size within [min, max]; teams without a size are judged by their taken slots.
    const uint teamSize = row.data(TTeamsListModel::TeamSizeRole).toUInt();
    const uint takenSlots = row.data(TTeamsListModel::TakenSlotsRole).toUInt();
    const uint maxSize = m_maxSize != 0 ? m_maxSize : 99999;
    const bool takenFits = accepted && m_minSize <= takenSlots && takenSlots <= maxSize;
    bool sizeFits = accepted;
    if (teamSize < m_minSize || maxSize < teamSize || teamSize == 0xffff)
        sizeFits = takenFits;

    // Start time: any, "immediately", or within the jitter window (minutes) after the chosen time.
    bool result = sizeFits;
    const uint32_t start = m_time->getStartTime();
    const uint teamStart = row.data(TTeamsListModel::StartTimeRole).toUInt();
    if (start == 1)
        result = teamStart == 1 && sizeFits;
    else if (start != 0)
        result = start <= teamStart && teamStart <= m_jitterTime * 60 + start && sizeFits;

    const int type = row.data(TTeamsListModel::ActivityTypeRole).toInt();
    return result && (type != TTeamActivity::Hunt || m_showHunts) && (type != TTeamActivity::Boss || m_showBosses)
        && (type != TTeamActivity::Quest || m_showQuests) && (type != TTeamActivity::Other || m_showOther);
}

bool TTeamsListSortProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString leftType = left.data(TTeamsListModel::TypeNameRole).toString();
    const QString rightType = right.data(TTeamsListModel::TypeNameRole).toString();
    const QString leftName = left.data(TTeamsListModel::NameRole).toString();
    const QString rightName = right.data(TTeamsListModel::NameRole).toString();
    const QString leftLeader = left.data(TTeamsListModel::LeaderNameRole).toString();
    const QString rightLeader = right.data(TTeamsListModel::LeaderNameRole).toString();
    const int leftRank = statusRank(left.data(TTeamsListModel::StatusRole).toUInt());
    const int rightRank = statusRank(right.data(TTeamsListModel::StatusRole).toUInt());
    const uint leftStart = left.data(TTeamsListModel::StartTimeRole).toUInt();
    const uint rightStart = right.data(TTeamsListModel::StartTimeRole).toUInt();
    return std::tie(leftRank, leftType, leftName, leftStart, leftLeader)
        < std::tie(rightRank, rightType, rightName, rightStart, rightLeader);
}

// ---------------------------------------------------------------------------
// TTeamMembersSortProxyModel

TTeamMembersSortProxyModel::TTeamMembersSortProxyModel(QObject* parent)
    : cip::TTeamMembersSortProxyModel(parent)
    , m_model(new TTeamMembersModel(this))
{
    setDynamicSortFilter(true);
    sort(0, Qt::AscendingOrder);
    setSourceModel(m_model);
}

void TTeamMembersSortProxyModel::setMembers(const std::vector<teamfinder::TTeamMember>& Members)
{
    beginFilterChange();
    m_model->setItems(Members);
    m_countAccepted = 0;
    m_countInvited = 0;
    m_countRejected = 0;
    m_countPending = 0;
    m_countDruid = 0;
    m_countKnight = 0;
    m_countPaladin = 0;
    m_countSorcerer = 0;
    m_countMonk = 0;
    for (const teamfinder::TTeamMember& member : m_model->items()) {
        switch (member.status) {
            case teamfinder::TeamMemberPending: ++m_countPending; break;
            case teamfinder::TeamMemberInvited: ++m_countInvited; break;
            case teamfinder::TeamMemberAccepted:
                ++m_countAccepted;
                switch (member.vocation) {
                    case 1: ++m_countKnight; break;
                    case 2: ++m_countPaladin; break;
                    case 3: ++m_countSorcerer; break;
                    case 4: ++m_countDruid; break;
                    case 5: ++m_countMonk; break;
                    default: break;
                }
                break;
            case teamfinder::TeamMemberRejected: ++m_countRejected; break;
            default: break;
        }
    }
    endFilterChange(QSortFilterProxyModel::Direction::Both);
    invalidate();
    Q_EMIT countChanged();
}

bool TTeamMembersSortProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString leftName = left.data(TTeamMembersModel::NameRole).toString();
    const QString rightName = right.data(TTeamMembersModel::NameRole).toString();
    const int leftRank = statusRank(left.data(TTeamMembersModel::StatusRole).toUInt());
    const int rightRank = statusRank(right.data(TTeamMembersModel::StatusRole).toUInt());
    if (leftRank != rightRank)
        return leftRank < rightRank;
    return QString::compare(leftName, rightName, Qt::CaseSensitive) < 0;
}

// ---------------------------------------------------------------------------
// TActivitiesSortProxyModel

TActivitiesSortProxyModel::TActivitiesSortProxyModel(QObject* parent)
    : cip::TActivitiesSortProxyModel(parent)
    , m_model(new TActivitiesModel(this))
{
    setDynamicSortFilter(true);
    sort(0, Qt::AscendingOrder);
    setSourceModel(m_model);
}

void TActivitiesSortProxyModel::setActivities(const std::vector<TActivity>& Activities)
{
    beginFilterChange();
    m_model->setItems(Activities);
    endFilterChange(QSortFilterProxyModel::Direction::Both);
    invalidate();
}

QString TActivitiesSortProxyModel::nameFilter() const
{
    return m_name;
}

void TActivitiesSortProxyModel::setNameFilter(const QString& v)
{
    if (m_name == v)
        return;
    m_name = v;
    filterUpdated();
}

void TActivitiesSortProxyModel::setShowBosses(const bool v)
{
    setFlag(m_showBosses, v);
}

void TActivitiesSortProxyModel::setShowHunts(const bool v)
{
    setFlag(m_showHunts, v);
}

void TActivitiesSortProxyModel::setShowQuests(const bool v)
{
    setFlag(m_showQuests, v);
}

void TActivitiesSortProxyModel::setShowOther(const bool v)
{
    setFlag(m_showOther, v);
}

void TActivitiesSortProxyModel::setFlag(bool& Flag, const bool Value)
{
    if (Flag == Value)
        return;
    Flag = Value;
    filterUpdated();
}

void TActivitiesSortProxyModel::filterUpdated()
{
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Both);
    invalidate();
    Q_EMIT filterChanged();
}

bool TActivitiesSortProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    const QModelIndex row = m_model->index(sourceRow, 0);
    if (!row.isValid())
        return false;
    bool accepted = true;
    const QString filter = m_name.trimmed();
    if (!filter.isEmpty()) {
        const QString text = row.data(TActivitiesModel::TypeNameRole).toString() + QLatin1Char(' ')
            + row.data(TActivitiesModel::NameRole).toString() + QLatin1Char(' ')
            + row.data(TActivitiesModel::AliasRole).toString();
        accepted = containsAllWords(text, filter);
    }
    const int type = row.data(TActivitiesModel::TypeRole).toInt();
    return accepted && (type != TTeamActivity::Boss || m_showBosses) && (type != TTeamActivity::Hunt || m_showHunts)
        && (type != TTeamActivity::Other || m_showOther) && (type != TTeamActivity::Quest || m_showQuests);
}

bool TActivitiesSortProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    return compareTypeAndName(left.data(TActivitiesModel::TypeNameRole).toString(),
                              left.data(TActivitiesModel::NameRole).toString(),
                              right.data(TActivitiesModel::TypeNameRole).toString(),
                              right.data(TActivitiesModel::NameRole).toString())
        < 0;
}

}
