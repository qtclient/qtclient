#include "teamfinder/teamsortproxymodels.h"

namespace tibia {
namespace {

constexpr int DruidBit = 1;
constexpr int KnightBit = 2;
constexpr int PaladinBit = 4;
constexpr int SorcererBit = 8;
constexpr int MonkBit = 16;

constexpr int TeamTypeBoss = 1;
constexpr int TeamTypeHunt = 2;
constexpr int TeamTypeQuest = 3;
constexpr int TeamTypeOther = 4;

int roleOf(const QAbstractItemModel* model, const char* name)
{
    if (!model)
        return -1;
    const QHash<int, QByteArray> roles = model->roleNames();
    for (auto it = roles.constBegin(); it != roles.constEnd(); ++it) {
        if (it.value() == name)
            return it.key();
    }
    return -1;
}

QVariant sourceData(const QAbstractItemModel* model, const int row, const char* roleName)
{
    if (!model)
        return {};
    const int role = roleOf(model, roleName);
    if (role < 0)
        return {};
    return model->index(row, 0).data(role);
}

} // namespace

TTeamsListSortProxyModel::TTeamsListSortProxyModel(QAbstractItemModel* source, QObject* parent)
    : cip::TTeamsListSortProxyModel(parent)
    , m_time(new teamfinder::TTeamStartTimeManager(this))
{
    m_startTimeManager = m_time;
    m_druidWanted = true;
    m_knightWanted = true;
    m_paladinWanted = true;
    m_sorcererWanted = true;
    m_monkWanted = true;
    m_showBosses = true;
    m_showHunts = true;
    m_showQuests = true;
    m_showOther = true;
    setSourceModel(source);
    setDynamicSortFilter(true);
    // CIP private slot anouceFilterChanged: invalidate + refresh count.
    connect(this, &TTeamsListSortProxyModel::filterChanged, this, &TTeamsListSortProxyModel::anouceFilterChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &TTeamsListSortProxyModel::refreshNumberOfTeams);
    connect(this, &QAbstractItemModel::rowsInserted, this, &TTeamsListSortProxyModel::refreshNumberOfTeams);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &TTeamsListSortProxyModel::refreshNumberOfTeams);
    refreshNumberOfTeams();
}

int TTeamsListSortProxyModel::numberOfTeams() const
{
    return m_numberOfTeams;
}

QString TTeamsListSortProxyModel::filterLevel() const
{
    return m_filterLevel;
}

void TTeamsListSortProxyModel::setFilterLevel(const QString& v)
{
    if (v == m_filterLevel)
        return;
    m_filterLevel = v;
    Q_EMIT filterChanged();
}

QString TTeamsListSortProxyModel::minTeamSize() const
{
    return m_minTeamSize;
}

void TTeamsListSortProxyModel::setMinTeamSize(const QString& v)
{
    if (v == m_minTeamSize)
        return;
    m_minTeamSize = v;
    Q_EMIT filterChanged();
}

QString TTeamsListSortProxyModel::maxTeamSize() const
{
    return m_maxTeamSize;
}

void TTeamsListSortProxyModel::setMaxTeamSize(const QString& v)
{
    if (v == m_maxTeamSize)
        return;
    m_maxTeamSize = v;
    Q_EMIT filterChanged();
}

bool TTeamsListSortProxyModel::druidWanted() const
{
    return m_druidWanted;
}

void TTeamsListSortProxyModel::setDruidWanted(const bool v)
{
    if (v == m_druidWanted)
        return;
    m_druidWanted = v;
    Q_EMIT filterChanged();
}

bool TTeamsListSortProxyModel::knightWanted() const
{
    return m_knightWanted;
}

void TTeamsListSortProxyModel::setKnightWanted(const bool v)
{
    if (v == m_knightWanted)
        return;
    m_knightWanted = v;
    Q_EMIT filterChanged();
}

bool TTeamsListSortProxyModel::paladinWanted() const
{
    return m_paladinWanted;
}

void TTeamsListSortProxyModel::setPaladinWanted(const bool v)
{
    if (v == m_paladinWanted)
        return;
    m_paladinWanted = v;
    Q_EMIT filterChanged();
}

bool TTeamsListSortProxyModel::sorcererWanted() const
{
    return m_sorcererWanted;
}

void TTeamsListSortProxyModel::setSorcererWanted(const bool v)
{
    if (v == m_sorcererWanted)
        return;
    m_sorcererWanted = v;
    Q_EMIT filterChanged();
}

bool TTeamsListSortProxyModel::monkWanted() const
{
    return m_monkWanted;
}

void TTeamsListSortProxyModel::setMonkWanted(const bool v)
{
    if (v == m_monkWanted)
        return;
    m_monkWanted = v;
    Q_EMIT filterChanged();
}

QString TTeamsListSortProxyModel::nameFilter() const
{
    return m_nameFilter;
}

void TTeamsListSortProxyModel::setNameFilter(const QString& v)
{
    if (v == m_nameFilter)
        return;
    m_nameFilter = v;
    Q_EMIT filterChanged();
}

bool TTeamsListSortProxyModel::showBosses() const
{
    return m_showBosses;
}

void TTeamsListSortProxyModel::setShowBosses(const bool v)
{
    if (v == m_showBosses)
        return;
    m_showBosses = v;
    Q_EMIT filterChanged();
}

bool TTeamsListSortProxyModel::showHunts() const
{
    return m_showHunts;
}

void TTeamsListSortProxyModel::setShowHunts(const bool v)
{
    if (v == m_showHunts)
        return;
    m_showHunts = v;
    Q_EMIT filterChanged();
}

bool TTeamsListSortProxyModel::showQuests() const
{
    return m_showQuests;
}

void TTeamsListSortProxyModel::setShowQuests(const bool v)
{
    if (v == m_showQuests)
        return;
    m_showQuests = v;
    Q_EMIT filterChanged();
}

bool TTeamsListSortProxyModel::showOther() const
{
    return m_showOther;
}

void TTeamsListSortProxyModel::setShowOther(const bool v)
{
    if (v == m_showOther)
        return;
    m_showOther = v;
    Q_EMIT filterChanged();
}

uint TTeamsListSortProxyModel::jitterTime() const
{
    return m_jitterTime;
}

void TTeamsListSortProxyModel::setJitterTime(const uint v)
{
    if (v == m_jitterTime)
        return;
    m_jitterTime = v;
    Q_EMIT filterChanged();
}

QObject* TTeamsListSortProxyModel::startTimeManager() const
{
    return m_startTimeManager;
}

void TTeamsListSortProxyModel::resetFilters()
{
    const bool changed = !m_druidWanted || !m_knightWanted || !m_paladinWanted || !m_sorcererWanted
        || !m_monkWanted || !m_showBosses || !m_showHunts || !m_showQuests || !m_showOther
        || !m_filterLevel.isEmpty() || !m_minTeamSize.isEmpty() || !m_maxTeamSize.isEmpty()
        || !m_nameFilter.isEmpty() || m_jitterTime != 0;
    m_druidWanted = true;
    m_knightWanted = true;
    m_paladinWanted = true;
    m_sorcererWanted = true;
    m_monkWanted = true;
    m_showBosses = true;
    m_showHunts = true;
    m_showQuests = true;
    m_showOther = true;
    m_filterLevel.clear();
    m_minTeamSize.clear();
    m_maxTeamSize.clear();
    m_nameFilter.clear();
    m_jitterTime = 0;
    if (m_time)
        m_time->setStartImmediately(true);
    if (changed)
        Q_EMIT filterChanged();
}

void TTeamsListSortProxyModel::anouceFilterChanged()
{
    invalidate();
    refreshNumberOfTeams();
}

void TTeamsListSortProxyModel::anouceFilterChangedAndEmitfilterchanged()
{
    anouceFilterChanged();
    Q_EMIT filterChanged();
}

QString TTeamsListSortProxyModel::getUIntFilterAsString(const uint UIntMember) const
{
    return UIntMember == 0 ? QString() : QString::number(UIntMember);
}

void TTeamsListSortProxyModel::setStringAsUIntFilter(QString NumberText, uint& UIntMember)
{
    const QString trimmed = NumberText.trimmed();
    uint value = 0;
    if (!trimmed.isEmpty()) {
        bool ok = false;
        value = trimmed.toUInt(&ok);
        if (!ok)
            return;
    }
    if (UIntMember == value)
        return;
    UIntMember = value;
    Q_EMIT filterChanged();
}

void TTeamsListSortProxyModel::refreshNumberOfTeams()
{
    const int count = rowCount();
    if (count == m_numberOfTeams)
        return;
    m_numberOfTeams = count;
    Q_EMIT numberOfTeamsChagned();
}

bool TTeamsListSortProxyModel::vocationWanted(const int bits) const
{
    if (bits == 0)
        return true;
    if ((bits & DruidBit) && m_druidWanted)
        return true;
    if ((bits & KnightBit) && m_knightWanted)
        return true;
    if ((bits & PaladinBit) && m_paladinWanted)
        return true;
    if ((bits & SorcererBit) && m_sorcererWanted)
        return true;
    if ((bits & MonkBit) && m_monkWanted)
        return true;
    return false;
}

bool TTeamsListSortProxyModel::typeShown(const int teamType) const
{
    switch (teamType) {
        case TeamTypeBoss: return m_showBosses;
        case TeamTypeHunt: return m_showHunts;
        case TeamTypeQuest: return m_showQuests;
        case TeamTypeOther: return m_showOther;
        default: return m_showOther;
    }
}

bool TTeamsListSortProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    Q_UNUSED(sourceParent);
    const QAbstractItemModel* source = sourceModel();
    if (!source)
        return false;

    if (!m_nameFilter.isEmpty()) {
        const QString name = sourceData(source, sourceRow, "name").toString();
        if (!name.contains(m_nameFilter, Qt::CaseInsensitive))
            return false;
    }

    const int teamType = sourceData(source, sourceRow, "teamType").toInt();
    if (teamType != 0 && !typeShown(teamType))
        return false;

    const int vocations = sourceData(source, sourceRow, "vocations").toInt();
    if (!vocationWanted(vocations))
        return false;

    if (!m_filterLevel.isEmpty()) {
        bool ok = false;
        const uint level = m_filterLevel.toUInt(&ok);
        if (ok) {
            const uint minLevel = sourceData(source, sourceRow, "minLevel").toUInt();
            const uint maxLevel = sourceData(source, sourceRow, "maxLevel").toUInt();
            if ((minLevel != 0 && level < minLevel) || (maxLevel != 0 && level > maxLevel))
                return false;
        }
    }

    if (!m_minTeamSize.isEmpty()) {
        bool ok = false;
        const uint minSize = m_minTeamSize.toUInt(&ok);
        if (ok) {
            const uint teamSize = sourceData(source, sourceRow, "teamSize").toUInt();
            if (teamSize != 0 && teamSize < minSize)
                return false;
        }
    }
    if (!m_maxTeamSize.isEmpty()) {
        bool ok = false;
        const uint maxSize = m_maxTeamSize.toUInt(&ok);
        if (ok) {
            const uint teamSize = sourceData(source, sourceRow, "teamSize").toUInt();
            if (teamSize != 0 && teamSize > maxSize)
                return false;
        }
    }

    return true;
}

bool TTeamsListSortProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString leftName = left.data(roleOf(sourceModel(), "name")).toString();
    const QString rightName = right.data(roleOf(sourceModel(), "name")).toString();
    return QString::localeAwareCompare(leftName, rightName) < 0;
}

TTeamMembersSortProxyModel::TTeamMembersSortProxyModel(QAbstractItemModel* source, QObject* parent)
    : cip::TTeamMembersSortProxyModel(parent)
{
    setSourceModel(source);
    setDynamicSortFilter(true);
    connect(this, &QAbstractItemModel::modelReset, this, &TTeamMembersSortProxyModel::onSourceChanged);
    connect(this, &QAbstractItemModel::rowsInserted, this, &TTeamMembersSortProxyModel::onSourceChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &TTeamMembersSortProxyModel::onSourceChanged);
    connect(this, &QAbstractItemModel::dataChanged, this, &TTeamMembersSortProxyModel::onSourceChanged);
    refreshCounts();
}

uint TTeamMembersSortProxyModel::countAccepted() const
{
    return m_countAccepted;
}

uint TTeamMembersSortProxyModel::countInvited() const
{
    return m_countInvited;
}

uint TTeamMembersSortProxyModel::countRejected() const
{
    return m_countRejected;
}

uint TTeamMembersSortProxyModel::countPending() const
{
    return m_countPending;
}

uint TTeamMembersSortProxyModel::countDruid() const
{
    return m_countDruid;
}

uint TTeamMembersSortProxyModel::countKnight() const
{
    return m_countKnight;
}

uint TTeamMembersSortProxyModel::countPaladin() const
{
    return m_countPaladin;
}

uint TTeamMembersSortProxyModel::countSorcerer() const
{
    return m_countSorcerer;
}

uint TTeamMembersSortProxyModel::countMonk() const
{
    return m_countMonk;
}

void TTeamMembersSortProxyModel::onSourceChanged()
{
    refreshCounts();
}

void TTeamMembersSortProxyModel::refreshCounts()
{
    uint accepted = 0;
    uint invited = 0;
    uint rejected = 0;
    uint pending = 0;
    uint druid = 0;
    uint knight = 0;
    uint paladin = 0;
    uint sorcerer = 0;
    uint monk = 0;

    const QAbstractItemModel* source = sourceModel();
    const int rows = source ? source->rowCount() : 0;
    for (int row = 0; row < rows; ++row) {
        switch (sourceData(source, row, "status").toUInt()) {
            case 1: ++accepted; break;
            case 2: ++invited; break;
            case 3: ++pending; break;
            case 4: ++rejected; break;
            default: break;
        }
        switch (sourceData(source, row, "vocationBits").toInt()) {
            case DruidBit: ++druid; break;
            case KnightBit: ++knight; break;
            case PaladinBit: ++paladin; break;
            case SorcererBit: ++sorcerer; break;
            case MonkBit: ++monk; break;
            default: break;
        }
    }

    const bool changed = accepted != m_countAccepted || invited != m_countInvited || rejected != m_countRejected
        || pending != m_countPending || druid != m_countDruid || knight != m_countKnight || paladin != m_countPaladin
        || sorcerer != m_countSorcerer || monk != m_countMonk;
    m_countAccepted = accepted;
    m_countInvited = invited;
    m_countRejected = rejected;
    m_countPending = pending;
    m_countDruid = druid;
    m_countKnight = knight;
    m_countPaladin = paladin;
    m_countSorcerer = sorcerer;
    m_countMonk = monk;
    if (changed)
        Q_EMIT countChanged();
}

bool TTeamMembersSortProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const bool leftLeader = left.data(roleOf(sourceModel(), "isLeader")).toBool();
    const bool rightLeader = right.data(roleOf(sourceModel(), "isLeader")).toBool();
    if (leftLeader != rightLeader)
        return leftLeader;
    const QString leftName = left.data(roleOf(sourceModel(), "name")).toString();
    const QString rightName = right.data(roleOf(sourceModel(), "name")).toString();
    return QString::localeAwareCompare(leftName, rightName) < 0;
}

TActivitiesSortProxyModel::TActivitiesSortProxyModel(QAbstractItemModel* source, QObject* parent)
    : cip::TActivitiesSortProxyModel(parent)
{
    m_showBosses = true;
    m_showHunts = true;
    m_showQuests = true;
    m_showOther = true;
    setSourceModel(source);
    setDynamicSortFilter(true);
    connect(this, &TActivitiesSortProxyModel::filterChanged, this, &TActivitiesSortProxyModel::onFilterAnnounced);
}

QString TActivitiesSortProxyModel::nameFilter() const
{
    return m_nameFilter;
}

void TActivitiesSortProxyModel::setNameFilter(const QString& v)
{
    if (v == m_nameFilter)
        return;
    m_nameFilter = v;
    Q_EMIT filterChanged();
}

bool TActivitiesSortProxyModel::showBosses() const
{
    return m_showBosses;
}

void TActivitiesSortProxyModel::setShowBosses(const bool v)
{
    if (v == m_showBosses)
        return;
    m_showBosses = v;
    Q_EMIT filterChanged();
}

bool TActivitiesSortProxyModel::showHunts() const
{
    return m_showHunts;
}

void TActivitiesSortProxyModel::setShowHunts(const bool v)
{
    if (v == m_showHunts)
        return;
    m_showHunts = v;
    Q_EMIT filterChanged();
}

bool TActivitiesSortProxyModel::showQuests() const
{
    return m_showQuests;
}

void TActivitiesSortProxyModel::setShowQuests(const bool v)
{
    if (v == m_showQuests)
        return;
    m_showQuests = v;
    Q_EMIT filterChanged();
}

bool TActivitiesSortProxyModel::showOther() const
{
    return m_showOther;
}

void TActivitiesSortProxyModel::setShowOther(const bool v)
{
    if (v == m_showOther)
        return;
    m_showOther = v;
    Q_EMIT filterChanged();
}

void TActivitiesSortProxyModel::resetFilters()
{
    const bool changed = !m_showBosses || !m_showHunts || !m_showQuests || !m_showOther || !m_nameFilter.isEmpty();
    m_showBosses = true;
    m_showHunts = true;
    m_showQuests = true;
    m_showOther = true;
    m_nameFilter.clear();
    if (changed)
        Q_EMIT filterChanged();
}

void TActivitiesSortProxyModel::onFilterAnnounced()
{
    invalidate();
}

bool TActivitiesSortProxyModel::typeShown(const int teamType) const
{
    switch (teamType) {
        case TeamTypeBoss: return m_showBosses;
        case TeamTypeHunt: return m_showHunts;
        case TeamTypeQuest: return m_showQuests;
        case TeamTypeOther: return m_showOther;
        default: return m_showOther;
    }
}

bool TActivitiesSortProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    Q_UNUSED(sourceParent);
    const QAbstractItemModel* source = sourceModel();
    if (!source)
        return false;
    if (!m_nameFilter.isEmpty()) {
        const QString name = sourceData(source, sourceRow, "name").toString();
        if (!name.contains(m_nameFilter, Qt::CaseInsensitive))
            return false;
    }
    const int teamType = sourceData(source, sourceRow, "teamType").toInt();
    if (teamType != 0 && !typeShown(teamType))
        return false;
    return true;
}

bool TActivitiesSortProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const int leftType = left.data(roleOf(sourceModel(), "teamType")).toInt();
    const int rightType = right.data(roleOf(sourceModel(), "teamType")).toInt();
    if (leftType != rightType)
        return leftType < rightType;
    const QString leftName = left.data(roleOf(sourceModel(), "name")).toString();
    const QString rightName = right.data(roleOf(sourceModel(), "name")).toString();
    return QString::localeAwareCompare(leftName, rightName) < 0;
}

}
