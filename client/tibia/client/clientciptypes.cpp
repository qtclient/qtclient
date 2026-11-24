#include "client/clientciptypes.h"

#include <tibia/game/tgamenotify.h>

#include <vector>

namespace tibia::client {

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

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    Q_UNUSED(parent);
    // Process-lifetime qtclient twin (linked below). Not GameClient session owner.
    (void)IGamesessionManager::instance();
}

namespace {
const bool kQtclientIGamesessionManagerLinked = (IGamesessionManager::instance() != nullptr);
}

} // namespace tibia::client
