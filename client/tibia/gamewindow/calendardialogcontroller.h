#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QJsonArray>
#include <QVariantMap>

namespace tibia::gamewindow {

/// CalendarDialog.qml (CIP: TCalendarDialogController): event-schedule days
/// from the portal `eventschedule` multiplex (TEventsUploader).
class TCalendarDialogController final : public TDialogController<cip::tibia::gamewindow::TCalendarDialogController>
{
    Q_OBJECT
public:
    explicit TCalendarDialogController(QObject* parent = nullptr);
    ~TCalendarDialogController() override;

    void setEvents(const QJsonArray& events);

    void showDialog() override;
    void closeDialog() override;
    void closeButtonClicked() override;
    QString getTooltipForDay(const QString& DateString) override;
    QString getSeasonalTooltipForDay(const QString& DateString) override;

private:
    void rebuild(const QJsonArray& events);
    void clearDayEntries();

    QVariantMap m_tooltips;
    QVariantMap m_seasonalTooltips;
    QList<QObject*> m_ownedEntries;
};

}
