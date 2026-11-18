#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

#include <QObject>

#include <cstddef>
#include <memory>
#include <set>

namespace tibia::game {

/// CIP game::TInternalGameActionRouter: the hub every game action goes through.
/// Controllers' publishGameAction signals are connected to
/// handleGameActionInternal; every handler's handleGameAction slot is connected
/// to publishGameActionInternal. Routers of several sessions forward actions to
/// each other through publishGameActionBetweenRouters, the ids of the routers
/// an action already went through keeping it from coming back.
class TInternalGameActionRouter final : public cip::tibia::game::TInternalGameActionRouter
{
    Q_OBJECT
public:
    explicit TInternalGameActionRouter(QObject* parent = nullptr);

    /// The process-unique id this router puts into IDsOfRouters.
    std::size_t id() const { return m_id; }

Q_SIGNALS:
    void publishGameActionInternal(tibia::input::IGameActionPtr pGameAction);
    void publishGameActionBetweenRouters(tibia::input::IGameActionPtr pGameAction, std::set<std::size_t>& IDsOfRouters);

public Q_SLOTS:
    void handleGameActionInternal(tibia::input::IGameActionPtr pGameAction);
    void handleGameActionFromOtherRouter(tibia::input::IGameActionPtr pGameAction, std::set<std::size_t>& IDsOfRouters);

private:
    std::size_t m_id;
};

/// CIP game::TGameActionRouter: the client's (and each session's) router; it
/// owns the internal router the game window's controllers and handlers are
/// connected to.
class TGameActionRouter final : public cip::tibia::game::TGameActionRouter
{
    Q_OBJECT
public:
    explicit TGameActionRouter(QObject* parent = nullptr);
    ~TGameActionRouter() override;

    TInternalGameActionRouter* internalRouter() const { return m_internalRouter.get(); }

public Q_SLOTS:
    /// Publishes the action to this router's handlers and to the other routers.
    void handleGameAction(tibia::input::IGameActionPtr pGameAction);

private:
    std::shared_ptr<TInternalGameActionRouter> m_internalRouter;
};

}
