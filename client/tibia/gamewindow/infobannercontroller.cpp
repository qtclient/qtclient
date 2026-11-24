#include "gamewindow/infobannercontroller.h"

#include "config/clientoptions.h"
#include "game/gamesessionbase.h"

#include <QVariantMap>

namespace tibia::gamewindow {

// dialogmanager.cpp: an item of `qmlFile` under `parentItem`, owned by `owner`.
QQuickItem* createItem(QQmlEngine* engine, const QString& qmlFile, QQuickItem* parentItem, QObject* owner, const QVariantMap& properties);

TInfoBannerController::TInfoBannerController(config::TClientOptions* ClientOptions, QQmlEngine* Engine, QQuickItem* ParentItem,
                                             QQuickItem* MapWindowPane, QObject* parent)
    : cip::tibia::gamewindow::TInfoBannerController(parent)
    , m_clientOptions(ClientOptions)
    , m_engine(Engine)
    , m_parentItem(ParentItem)
    , m_mapWindowPane(MapWindowPane)
{
    createInfoBannerWrapper();
    if (!m_clientOptions)
        return;
    connect(m_clientOptions, &config::TClientOptions::optionChanged, this, [this](const config::EClientOption id) {
        if (id == config::EClientOption::gameWindowShowInfoBanner)
            onShowInfoBannerOptionChanged();
    });
    onShowInfoBannerOptionChanged();
}

void TInfoBannerController::createInfoBannerWrapper()
{
    if (m_infoBannerWrapper)
        return;
    m_infoBannerWrapper =
        createItem(m_engine, QStringLiteral("tutorial/InfoBannerWrapper.qml"), m_parentItem, this,
                   { { QStringLiteral("controller"), QVariant::fromValue<QObject*>(this) } });
}

QQuickItem* TInfoBannerController::mapWindowPane()
{
    return m_mapWindowPane;
}

void TInfoBannerController::onShowInfoBannerOptionChanged()
{
    if (!m_clientOptions)
        return;
    m_showInfoBanner = m_clientOptions->boolOption(config::EClientOption::gameWindowShowInfoBanner);
    if (!m_showInfoBanner)
        Q_EMIT clearInfoBannerQueue();
}

void TInfoBannerController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSession = GameSession;
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get()))
        m_cyclopediaMapStorage = session->cyclopediaMapStorage();
}

}
