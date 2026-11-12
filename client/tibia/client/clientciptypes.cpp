#include "client/clientciptypes.h"

#include <tibia/game/tgamenotify.h>

#include <QEvent>
#include <QLoggingCategory>

#include <vector>

namespace tibia::client {

Q_LOGGING_CATEGORY(lcGameApplication, "tibia.client.gameapplication")

struct IGamesessionManager::LiveFans {
    std::vector<QMetaObject::Connection> typed;
};

IGamesessionManager::IGamesessionManager(QObject* parent)
    : cip::tibia::client::IGamesessionManager(parent)
    , m_liveFans(new LiveFans)
{
    bindLiveNotifyFans();
}

IGamesessionManager::~IGamesessionManager()
{
    if (m_liveFans) {
        for (const QMetaObject::Connection& c : m_liveFans->typed)
            QObject::disconnect(c);
        delete m_liveFans;
        m_liveFans = nullptr;
    }
}

void IGamesessionManager::bindLiveNotifyFans()
{
    // Name-only floor: CIP passes shared_ptr<IGameSession>. Tip does not take
    // session ownership or call loginWorld / forceLogout.
    auto* fans = m_liveFans;
    if (!fans)
        return;
    game::TGameNotify* notify = game::TGameNotify::instance();
    fans->typed.push_back(connect(notify, &game::TGameNotify::gameStarted, this, [this] {
        Q_EMIT changePrimaryGameSession();
    }));
    fans->typed.push_back(connect(notify, &game::TGameNotify::gameEnded, this, [this] {
        Q_EMIT changePrimaryGameSession();
    }));
}

IGamesessionManager* IGamesessionManager::instance()
{
    static IGamesessionManager keep;
    return &keep;
}

TGameApplicationBase::TGameApplicationBase(QObject* parent)
    : QObject(parent)
{
}

TGameApplication::TGameApplication(QObject* parent)
    : TGameApplicationBase(parent)
{
}

void TGameApplication::showErrorAsPopupAndCriticalLog(QString ErrorMessage)
{
    // CIP slot: critical log + error popup. Tip logs only — opening a blocking
    // dialog from the keep-alive twin can race the login/character shell.
    qCCritical(lcGameApplication).noquote() << ErrorMessage;
}

void TGameApplication::onClientPluginReadyToClose()
{
}

void TGameApplication::onRequestClose()
{
    // Live close/confirm stays on TGameClient / host; do not quit from here.
}

void TGameApplication::onAboutToQuit()
{
}

void TGameApplication::startClientWindowChangedReactionTimer()
{
}

void TGameApplication::screenGeometryChanged()
{
}

void TGameApplication::onDelayedCliendWindowChanged()
{
}

void TGameApplication::onNumberOfSidebarsChanged(const std::size_t NewNumber)
{
    (void)NewNumber;
}

void TGameApplication::onLeftAndRightStatusActionBarWidthChanged(const int NewWidth)
{
    (void)NewWidth;
}

void TGameApplication::windowSizeChanged()
{
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    // CIP TGameApplication keep-alive under the session/feature parent.
    // Not the process QApplication — moc slot floor only.
    if (parent)
        new TGameApplication(parent);
    else
        new TGameApplication(nullptr);
    // Process-lifetime tip twin (linked below). Not GameClient session owner.
    (void)IGamesessionManager::instance();
}

// b1/95257f35e515e8a9
bool TPreventCloseEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    return cip::tibia::client::TPreventCloseEventFilter::eventFilter(watched, event);
}

namespace {
const bool kQtclientIGamesessionManagerLinked = (IGamesessionManager::instance() != nullptr);
}

} // namespace tibia::client
