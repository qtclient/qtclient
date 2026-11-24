#pragma once

#include "input/gameactiondispatch.h"
#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <QDate>
#include <QVariantMap>

#include <memory>

class QQmlEngine;
class QQuickItem;

namespace tibia {
namespace news {
class TEventsStorage;
}
namespace shared {
class IClockService;
}
namespace gamewindow {

/// CalendarDialog.qml (CIP: TCalendarDialogController): the event schedule of
/// the client's events storage, one list of TEventQMLEntry per day. Seasonal
/// events only show in the seasonal tooltip.
class TCalendarDialogController final : public TDialogController<cip::tibia::gamewindow::TCalendarDialogController>
{
    Q_OBJECT
public:
    /// CIP: made by the client with its clock, its events storage (the events
    /// uploader fills it), the engine and the dialog parent item.
    TCalendarDialogController(std::shared_ptr<shared::IClockService> ClockService,
                              std::shared_ptr<news::TEventsStorage> EventsStorage, QQmlEngine* Engine,
                              QQuickItem* DialogParentItem, QObject* parent = nullptr);
    ~TCalendarDialogController() override;

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
    void updateEvents();
    void clearEvents();
    QString tooltipFor(const QVariantMap& events, const QString& dateString) const;

    std::shared_ptr<shared::IClockService> m_pClockService;
    std::shared_ptr<news::TEventsStorage> m_pEventsStorage;
    QVariantMap m_seasonalEventsMap;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
