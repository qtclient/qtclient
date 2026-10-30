#include "calendardialogcontroller.h"

#include "input/gameactionhandlers.h"
#include "news/eventqmlentry.h"

#include <QColor>
#include <QDate>
#include <QDateTime>
#include <QJsonObject>
#include <QTime>
#include <QVariantList>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

QColor parseColor(const QJsonObject& e, const char* key, const QColor& fallback)
{
    const QString raw = e.value(QLatin1String(key)).toString();
    if (raw.isEmpty())
        return fallback;
    const QColor color(raw.startsWith(QLatin1Char('#')) ? raw : QLatin1Char('#') + raw);
    return color.isValid() ? color : fallback;
}

bool isSeasonal(const QJsonObject& e)
{
    const QJsonValue v = e.value(QStringLiteral("isseasonal"));
    if (v.isBool())
        return v.toBool();
    return v.toInt() != 0;
}

QString dateKey(const QDate& day) { return day.toString(Qt::ISODate); }

}

TCalendarDialogController::TCalendarDialogController(QObject* parent)
    : TDialogController(QStringLiteral("CalendarDialog.qml"), parent)
{
    rebuild({});
    input::setActionHandler(A::OPEN_CALENDAR_DIALOG, [this](const input::TGameAction&) {
        showDialog();
        return true;
    });
}

TCalendarDialogController::~TCalendarDialogController()
{
    input::setActionHandler(A::OPEN_CALENDAR_DIALOG, nullptr);
    clearDayEntries();
}

void TCalendarDialogController::setEvents(const QJsonArray& events) { rebuild(events); }

void TCalendarDialogController::clearDayEntries()
{
    for (QObject* entry : std::as_const(m_ownedEntries))
        entry->deleteLater();
    m_ownedEntries.clear();
}

void TCalendarDialogController::rebuild(const QJsonArray& events)
{
    clearDayEntries();
    m_eventsMap.clear();
    m_tooltips.clear();
    m_seasonalTooltips.clear();

    const QDate today = QDate::currentDate();
    m_todayDateServer = today;
    m_minimumDate = today.addMonths(-1);
    m_maximumDate = today.addMonths(12);
    m_currentTimeServer = QTime::currentTime().toString(QStringLiteral("HH:mm"));

    QDate minDay = m_minimumDate;
    QDate maxDay = m_maximumDate;
    for (const auto& value : events) {
        const QJsonObject e = value.toObject();
        const QString name = e.value(QStringLiteral("name")).toString();
        if (name.isEmpty())
            continue;
        const qint64 startSec = e.value(QStringLiteral("startdate")).toInteger();
        const qint64 endSec = e.value(QStringLiteral("enddate")).toInteger();
        if (startSec <= 0 || endSec <= 0)
            continue;
        const QDate start = QDateTime::fromSecsSinceEpoch(startSec).date();
        const QDate end = QDateTime::fromSecsSinceEpoch(endSec).date();
        if (start.isValid() && start < minDay)
            minDay = start;
        if (end.isValid() && end > maxDay)
            maxDay = end;

        const QColor primary = parseColor(e, "colorlight", QColor(QStringLiteral("#c9a227")));
        const QColor secondary = parseColor(e, "colordark", QColor(QStringLiteral("#6b5a1e")));
        const bool seasonal = isSeasonal(e);

        for (QDate day = start; day.isValid() && day <= end; day = day.addDays(1)) {
            const QString key = dateKey(day);
            const bool edge = day == start || day == end;
            auto* entry = new news::TEventQMLEntry(name, primary, secondary, edge, this);
            m_ownedEntries.append(entry);
            QVariantList dayEvents = m_eventsMap.value(key).toList();
            dayEvents.append(QVariant::fromValue<QObject*>(entry));
            m_eventsMap.insert(key, dayEvents);

            const QString existing = m_tooltips.value(key).toString();
            m_tooltips.insert(key, existing.isEmpty() ? name : existing + QStringLiteral(", ") + name);
            if (seasonal) {
                const QString seasonalExisting = m_seasonalTooltips.value(key).toString();
                m_seasonalTooltips.insert(key, seasonalExisting.isEmpty() ? name : seasonalExisting + QStringLiteral(", ") + name);
            }
        }
    }
    m_minimumDate = minDay;
    m_maximumDate = maxDay;
}

void TCalendarDialogController::showDialog()
{
    dialog()->show();
}

void TCalendarDialogController::closeDialog() { dialog()->close(); }

void TCalendarDialogController::closeButtonClicked() { closeDialog(); }

QString TCalendarDialogController::getTooltipForDay(const QString& DateString)
{
    return m_tooltips.value(DateString).toString();
}

QString TCalendarDialogController::getSeasonalTooltipForDay(const QString& DateString)
{
    return m_seasonalTooltips.value(DateString).toString();
}

}
