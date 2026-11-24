#pragma once

// CIP-named client surfaces (promoted from cipwire).

#include "client/gameapplication.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia {
namespace client {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

/// CIP `client::IGamesessionManager` moc surface. Signal
/// `changePrimaryGameSession(shared_ptr<IGameSession>)` is cip_skipped — qtclient
/// redeclares name-only floor. Does **not** own login / swap GameClient
/// sessions; fans `TGameNotify` game start/end only.
class IGamesessionManager : public cip::tibia::client::IGamesessionManager
{
    Q_OBJECT
public:
    explicit IGamesessionManager(QObject* parent = nullptr);
    ~IGamesessionManager() override;

    /// Process-lifetime qtclient twin (moc/RTTI keep). Not GameClient's live session owner.
    static IGamesessionManager* instance();

Q_SIGNALS:
    /// CIP moc signal (shared_ptr<IGameSession> skipped from cip_gen); qtclient name parity.
    void changePrimaryGameSession();

private:
    void bindLiveNotifyFans();

    struct LiveFans;
    LiveFans* m_liveFans = nullptr;
};

class TAntiCheatController final : public cip::tibia::client::TAntiCheatController
{
    Q_OBJECT
public:
    using cip::tibia::client::TAntiCheatController::TAntiCheatController;
};

}
}
