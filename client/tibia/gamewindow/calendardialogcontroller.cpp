#include "calendardialogcontroller.h"

#include "input/gameactionhandlers.h"
#include "news/eventqmlentry.h"

#include <QJsonObject>
#include <QTimeZone>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

QColor parseColor(const QJsonObject& e, const char* key)
{
    const QString raw = e.value(QLatin1String(key)).toString();
    return QColor(raw.startsWith(QLatin1Char('#')) ? raw : QLatin1Char('#') + raw);
}

bool parseBool(const QJsonValue& v) { return v.isBool() ? v.toBool() : v.toInt() != 0; }

const QTimeZone& serverTimeZone()
{
    static const QTimeZone zone = [] {
#if QT_CONFIG(timezone)
        QTimeZone berlin(QByteArrayLiteral("Europe/Berlin"));
        if (berlin.isValid())
            return berlin;
#endif
        // qtclient: wasm Qt has no named zones; Berlin's standard time.
        return QTimeZone::fromSecondsAheadOfUtc(3600);
    }();
    return zone;
}

/// The server time as the game shows it: "2024-05-01, 10:00 CEST".
QString formatServerTime(const QDateTime& time)
{
    const QDateTime local = time.toTimeZone(serverTimeZone());
    return qtTrId("%1 %2")
        .arg(local.toString(QStringLiteral("yyyy-MM-dd, HH:mm")))
        .arg(local.isDaylightTime() ? QStringLiteral("CEST") : QStringLiteral("CET"));
}

QList<QObject*> entriesOf(const QVariant& value) { return value.value<QList<QObject*>>(); }

}

TCalendarDialogController::TCalendarDialogController(QObject* parent)
    : TDialogController(QStringLiteral("CalendarDialog.qml"), parent)
    , m_serverClock([] { return QDateTime::currentDateTimeUtc(); })
{
    input::setActionHandler(A::OPEN_CALENDAR_DIALOG, [this](const input::IGameAction& action) {
        handleGameAction(action);
        return true;
    });
}

TCalendarDialogController::~TCalendarDialogController()
{
    input::setActionHandler(A::OPEN_CALENDAR_DIALOG, nullptr);
    clearEvents();
}

void TCalendarDialogController::setEvents(const QJsonArray& events)
{
    m_events.clear();
    for (const auto& value : events) {
        const QJsonObject e = value.toObject();
        TEventInformation event;
        event.startDate = QDateTime::fromSecsSinceEpoch(e.value(QStringLiteral("startdate")).toInteger()).date();
        event.endDate = QDateTime::fromSecsSinceEpoch(e.value(QStringLiteral("enddate")).toInteger()).date();
        event.name = e.value(QStringLiteral("name")).toString();
        event.description = e.value(QStringLiteral("description")).toString();
        event.colorLight = parseColor(e, "colorlight");
        event.colorDark = parseColor(e, "colordark");
        event.displayPriority = static_cast<uint>(e.value(QStringLiteral("displaypriority")).toInteger());
        event.isSeasonal = parseBool(e.value(QStringLiteral("isseasonal")));
        m_events.push_back(std::move(event));
    }
}

QString TCalendarDialogController::currentTimeServer() const
{
    if (!m_serverClock)
        return {};
    return formatServerTime(m_serverClock());
}

QDate TCalendarDialogController::todayDateServer() const
{
    if (!m_serverClock)
        return QDate::currentDate();
    return m_serverClock().toTimeZone(serverTimeZone()).date();
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
