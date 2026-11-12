#include "teamfinder/teamfinderciptypes.h"

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

} // namespace tibia::teamfinder
