#include "calendardialogcontroller.h"

#include "input/gameactionhandlers.h"
#include "news/eventqmlentry.h"
#include "qmlcomponents/texthelper.h"

#include <QJsonObject>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

QList<QObject*> entriesOf(const QVariant& value) { return value.value<QList<QObject*>>(); }

}

TCalendarDialogController::TCalendarDialogController(QObject* parent)
    : TDialogController(QStringLiteral("CalendarDialog.qml"), parent)
    , m_serverClock([] { return QDateTime::currentDateTimeUtc(); })
{
    m_gameActions.on(A::OPEN_CALENDAR_DIALOG, [this](const input::IGameAction& action) {
        handleGameAction(action);
        return true;
    });
}

TCalendarDialogController::~TCalendarDialogController()
{
    clearEvents();
}

void TCalendarDialogController::setEvents(const std::vector<news::TEventInformation>& events)
{
    m_events.clear();
    for (const news::TEventInformation& e : events) {
        TEventInformation event;
        event.startDate = e.startDate;
        event.endDate = e.endDate;
        event.name = e.name;
        event.description = e.description;
        event.colorLight = e.colorLight;
        event.colorDark = e.colorDark;
        event.displayPriority = static_cast<uint>(e.displayPriority);
        event.isSeasonal = e.isSeasonal;
        m_events.push_back(std::move(event));
    }
}

QString TCalendarDialogController::currentTimeServer() const
{
    if (!m_serverClock)
        return {};
    return qmlcomponents::formatServerDateTime(m_serverClock());
}

QDate TCalendarDialogController::todayDateServer() const
{
    if (!m_serverClock)
        return QDate::currentDate();
    return qmlcomponents::toServerTime(m_serverClock()).date();
}

QDate TCalendarDialogController::minimumDate() const
{
    const QDate today = todayDateServer();
    return QDate(today.year(), today.month(), 1);
}

QDate TCalendarDialogController::maximumDate() const
{
    const QDate last = todayDateServer().addMonths(11);
    return QDate(last.year(), last.month(), last.daysInMonth());
}

QObject* TCalendarDialogController::makeEntry(const TEventInformation& event, const bool startsOrEndsThatDay)
{
    auto* entry = new news::TEventQMLEntry(event.name, event.colorLight, event.colorDark, startsOrEndsThatDay);
    m_entryDetails.insert(entry, { event.description, event.displayPriority });
    return entry;
}

bool TCalendarDialogController::entryLessThan(const QObject* a, const QObject* b) const
{
    const uint priorityA = m_entryDetails.value(a).displayPriority;
    const uint priorityB = m_entryDetails.value(b).displayPriority;
    if (priorityA != priorityB)
        return priorityA < priorityB;
    return a->property("title").toString() < b->property("title").toString();
}

/// Rebuilds both maps from the schedule: every day of an event gets an entry
/// in its list, keyed "yyyy-MM-dd"; each day's list is sorted by priority,
/// then title.
void TCalendarDialogController::updateEvents()
{
    clearEvents();
    for (const TEventInformation& event : m_events) {
        QVariantMap& map = event.isSeasonal ? m_seasonalEventsMap : m_eventsMap;
        for (QDate day = event.startDate; day <= event.endDate; day = day.addDays(1)) {
            const QString key = day.toString(QStringLiteral("yyyy-MM-dd"));
            if (!map.contains(key))
                map.insert(key, QVariant::fromValue(QList<QObject*>()));
            QList<QObject*> entries = entriesOf(map.value(key));
            const bool edge = !event.isSeasonal && (day == event.startDate || day == event.endDate);
            entries.append(makeEntry(event, edge));
            map.insert(key, QVariant::fromValue(entries));
        }
    }
    for (QVariantMap* map : { &m_eventsMap, &m_seasonalEventsMap }) {
        for (auto it = map->begin(); it != map->end(); ++it) {
            QList<QObject*> entries = entriesOf(it.value());
            std::sort(entries.begin(), entries.end(),
                      [this](const QObject* a, const QObject* b) { return entryLessThan(a, b); });
            it.value() = QVariant::fromValue(entries);
        }
    }
}

/// Deletes every day's entries and empties both maps.
void TCalendarDialogController::clearEvents()
{
    for (QVariantMap* map : { &m_eventsMap, &m_seasonalEventsMap }) {
        for (auto it = map->begin(); it != map->end(); ++it) {
            const QList<QObject*> entries = entriesOf(it.value());
            it.value() = QVariant();
            for (QObject* entry : entries)
                delete entry;
        }
        map->clear();
    }
    m_entryDetails.clear();
}

void TCalendarDialogController::showDialog()
{
    updateEvents();
    dialog()->create();
    dialog()->show();
}

void TCalendarDialogController::closeDialog()
{
    dialog()->close();
    clearEvents();
}

void TCalendarDialogController::closeButtonClicked() { closeDialog(); }

/// "<u>title:</u><br>description" for every entry of the day, blank-line separated.
QString TCalendarDialogController::tooltipFor(const QVariantMap& events, const QString& dateString) const
{
    QString tooltip;
    for (QObject* object : entriesOf(events.value(dateString))) {
        auto* entry = qobject_cast<news::TEventQMLEntry*>(object);
        if (!entry)
            continue;
        if (!tooltip.isEmpty())
            tooltip.append(QLatin1String("<br><br>\n"));
        tooltip.append(QStringLiteral("<u>%1:</u><br>\n%2")
                           .arg(entry->title())
                           .arg(m_entryDetails.value(entry).description));
    }
    return tooltip;
}

QString TCalendarDialogController::getTooltipForDay(const QString& DateString)
{
    return tooltipFor(m_eventsMap, DateString);
}

QString TCalendarDialogController::getSeasonalTooltipForDay(const QString& DateString)
{
    return tooltipFor(m_seasonalEventsMap, DateString);
}

void TCalendarDialogController::handleGameAction(const input::IGameAction& action)
{
    if (action.gameActionType() != A::OPEN_CALENDAR_DIALOG)
        return;
    showDialog();
}

}
