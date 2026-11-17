#pragma once

#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"
#include "news/eventsuploader.h"

#include <cip_gen.h>

#include <QColor>
#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QVariantMap>

#include <functional>
#include <vector>

namespace tibia {
namespace gamewindow {

/// One `eventlist` row of the portal `eventschedule` reply (CIP: news's
/// TEventInformation, what TEventsStorage keeps).
struct TEventInformation
{
    QDate startDate;
    QDate endDate;
    QString name;
    QString description;
    QColor colorLight;
    QColor colorDark;
    uint displayPriority = 0;
    bool isSeasonal = false;
};

/// CalendarDialog.qml (CIP: TCalendarDialogController): the event schedule,
/// one list of TEventQMLEntry per day. Seasonal events only show in the
/// seasonal tooltip.
class TCalendarDialogController final : public TDialogController<cip::tibia::gamewindow::TCalendarDialogController>
{
    Q_OBJECT
public:
    explicit TCalendarDialogController(QObject* parent = nullptr);
    ~TCalendarDialogController() override;

    /// The events storage's schedule, from the portal reply's `eventlist`.
    void setEvents(const std::vector<news::TEventInformation>& events);
    /// The game server's clock (CIP: the time provider passed at creation).
    void setServerClock(std::function<QDateTime()> clock) { m_serverClock = std::move(clock); }

    QString currentTimeServer() const override;
    QDate todayDateServer() const override;
    QDate minimumDate() const override;
    QDate maximumDate() const override;

    void showDialog() override;
    void closeDialog() override;
    void closeButtonClicked() override;
    QString getTooltipForDay(const QString& DateString) override;
    QString getSeasonalTooltipForDay(const QString& DateString) override;
    void handleGameAction(const input::IGameAction& action);

private:
    struct EntryDetails
    {
        QString description;
        uint displayPriority = 0;
    };

    void updateEvents();
    void clearEvents();
    QObject* makeEntry(const TEventInformation& event, bool startsOrEndsThatDay);
    bool entryLessThan(const QObject* a, const QObject* b) const;
    QString tooltipFor(const QVariantMap& events, const QString& dateString) const;

    std::function<QDateTime()> m_serverClock;
    std::vector<TEventInformation> m_events;
    QVariantMap m_seasonalEventsMap;
    /// What TEventQMLEntry carries in CIP but the qtclient entry does not.
    QHash<const QObject*, EntryDetails> m_entryDetails;
};

}
}
