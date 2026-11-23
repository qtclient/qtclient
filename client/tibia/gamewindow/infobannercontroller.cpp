#include "gamewindow/infobannercontroller.h"

#include "app/clientoptions.h"

namespace tibia::gamewindow {

TInfoBannerController::TInfoBannerController(QQuickItem* mapWindowPane, QObject* parent)
    : cip::tibia::gamewindow::TInfoBannerController(parent)
    , m_mapWindowPane(mapWindowPane)
{
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this,
            &TInfoBannerController::onShowInfoBannerOptionChanged);
    onShowInfoBannerOptionChanged();
}

QQuickItem* TInfoBannerController::mapWindowPane()
{
    return m_mapWindowPane;
}

void TInfoBannerController::enqueue(const cip::tibia::qmlcomponents::enums::EInfoBannerType type, const QJsonValue& extra)
{
    if (!m_showInfoBanner)
        return;
    Q_EMIT showInfoBanner(type, extra);
    Q_EMIT displayDataChanged();
}

void TInfoBannerController::clearQueue()
{
    Q_EMIT clearInfoBannerQueue();
}

void TInfoBannerController::onShowInfoBannerOptionChanged()
{
    m_showInfoBanner = app::ClientOptions::load().options().value(QLatin1String("gameWindowShowInfoBanner")).toBool(true);
    if (!m_showInfoBanner)
        clearQueue();
}

}
