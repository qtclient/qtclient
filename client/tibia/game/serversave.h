#pragma once

#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QTimeZone>

namespace tibia {
namespace game {

/// 10:00 Berlin time on `date`: the server save.
inline QDateTime serverSave(const QDate& date)
{
#if QT_CONFIG(timezone)
    return QDateTime(date, QTime(10, 0), QTimeZone("Europe/Berlin"));
#else
    const QDate lastMarch(date.year(), 3, 31);
    const QDate lastOctober(date.year(), 10, 31);
    const bool summer = date >= lastMarch.addDays(-(lastMarch.dayOfWeek() % 7)) && date < lastOctober.addDays(-(lastOctober.dayOfWeek() % 7));
    return QDateTime(date, QTime(10, 0), QTimeZone::fromSecondsAheadOfUtc(summer ? 7200 : 3600));
#endif
}

/// Today's server save, or yesterday's before it (CIP: a classless helper of the
/// character selection's server save check and the houses dialog).
inline QDateTime lastServerSave()
{
    const QDateTime now = QDateTime::fromSecsSinceEpoch(QDateTime::currentSecsSinceEpoch());
    const QDateTime today = serverSave(now.date());
    return now < today ? today.addDays(-1) : today;
}

}
}
