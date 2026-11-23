#pragma once

#include <cip_gen.h>

#include <QJsonValue>
#include <QPointer>
#include <QQuickItem>

namespace tibia::gamewindow {

/// The info banners over the map window (tutorial/InfoBanner*.qml). Turning
/// the "show info banner" option off clears the queued banners.
class TInfoBannerController final : public cip::tibia::gamewindow::TInfoBannerController
{
    Q_OBJECT
public:
    explicit TInfoBannerController(QQuickItem* mapWindowPane = nullptr, QObject* parent = nullptr);

    Q_INVOKABLE QQuickItem* mapWindowPane() override;
    /// Shown only while the option is on.
    void enqueue(cip::tibia::qmlcomponents::enums::EInfoBannerType type, const QJsonValue& extra = {});
    void clearQueue();

public Q_SLOTS:
    /// Reads the option; off clears the queue.
    void onShowInfoBannerOptionChanged();

private:
    QPointer<QQuickItem> m_mapWindowPane;
    bool m_showInfoBanner = true;
};

}
