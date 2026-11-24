#pragma once

#include <cip_gen.h>

#include <QPointer>
#include <QQuickItem>

class QQmlEngine;

namespace tibia::config {
class TClientOptions;
}

namespace tibia::gamewindow {

/// The info banners over the map window (CIP: TInfoBannerController): makes
/// tutorial/InfoBannerWrapper.qml (its `controller` is this) under the parent
/// item; QML shows the banners of showInfoBanner. Turning the "show info
/// banner" option off clears the queued banners.
class TInfoBannerController final : public cip::tibia::gamewindow::TInfoBannerController
{
    Q_OBJECT
public:
    /// `ParentItem`: the wrapper item's parent; `MapWindowPane`: what QML asks
    /// mapWindowPane for. Reads the option at once (off clears the queue).
    TInfoBannerController(config::TClientOptions* ClientOptions, QQmlEngine* Engine, QQuickItem* ParentItem, QQuickItem* MapWindowPane,
                          QObject* parent = nullptr);

    Q_INVOKABLE QQuickItem* mapWindowPane() override;

public Q_SLOTS:
    /// Reads the option; off clears the queue.
    void onShowInfoBannerOptionChanged();

private:
    /// The wrapper item, made once.
    void createInfoBannerWrapper();

    config::TClientOptions* m_clientOptions;
    QQmlEngine* m_engine;
    QPointer<QQuickItem> m_parentItem;
    QPointer<QQuickItem> m_mapWindowPane;
    QPointer<QQuickItem> m_infoBannerWrapper;
    bool m_showInfoBanner = true;
};

}
