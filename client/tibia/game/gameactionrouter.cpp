#include "game/gameactionrouter.h"

namespace tibia::game {

namespace {
std::size_t nextRouterId = 0;
}

TInternalGameActionRouter::TInternalGameActionRouter(QObject* parent)
    : cip::tibia::game::TInternalGameActionRouter(parent)
    , m_id(nextRouterId++)
{
}

void TInternalGameActionRouter::handleGameActionInternal(tibia::input::IGameActionPtr pGameAction)
{
    Q_EMIT publishGameActionInternal(pGameAction);
    std::set<std::size_t> IDsOfRouters{ m_id };
    Q_EMIT publishGameActionBetweenRouters(pGameAction, IDsOfRouters);
}

void TInternalGameActionRouter::handleGameActionFromOtherRouter(tibia::input::IGameActionPtr pGameAction,
                                                                std::set<std::size_t>& IDsOfRouters)
{
    if (!IDsOfRouters.insert(m_id).second)
        return;
    Q_EMIT publishGameActionInternal(pGameAction);
    Q_EMIT publishGameActionBetweenRouters(pGameAction, IDsOfRouters);
}

TGameActionRouter::TGameActionRouter(QObject* parent)
    : cip::tibia::game::TGameActionRouter(parent)
    , m_internalRouter(std::make_shared<TInternalGameActionRouter>())
{
}

TGameActionRouter::~TGameActionRouter() = default;

void TGameActionRouter::handleGameAction(tibia::input::IGameActionPtr pGameAction)
{
    m_internalRouter->handleGameActionInternal(std::move(pGameAction));
}

}
