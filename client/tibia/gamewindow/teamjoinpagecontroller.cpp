#include "teamjoinpagecontroller.h"
#include "game/gamesessionbase.h"

#include "game/gameciptypes.h"
#include "gamewindow/contextmenucontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include "game/playerdata.h"

namespace tibia::gamewindow {

TTeamJoinPageController::TTeamJoinPageController(QObject* parent)
    : cip::tibia::gamewindow::TTeamJoinPageController(parent)
    , m_proxy(new TTeamsListSortProxyModel(this))
{
    m_teamsList = m_proxy;
}

void TTeamJoinPageController::setGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    m_teamFinderProtocolMessageHandler = session ? session->teamFinderProtocolMessageHandler().get() : nullptr;
    m_proxy->resetFilters();
}

bool TTeamJoinPageController::hasPremium() const
{
    return m_premium ? m_premium->isPremium() : false;
}

QObject* TTeamJoinPageController::teamsList() const
{
    return m_proxy;
}

void TTeamJoinPageController::setPremiumStorage(game::TPremiumTimeStorage* Storage)
{
    if (m_premium)
        disconnect(m_premium, &game::TPremiumTimeStorage::premiumTimeChanged, this, &TTeamJoinPageController::premiumTimeChanged);
    m_premium = Storage;
    if (m_premium)
        connect(m_premium, &game::TPremiumTimeStorage::premiumTimeChanged, this, &TTeamJoinPageController::premiumTimeChanged);
    Q_EMIT premiumTimeChanged();
}

void TTeamJoinPageController::disconnectAll()
{
    for (const QMetaObject::Connection& connection : m_connections)
        disconnect(connection);
    m_connections.clear();
}

void TTeamJoinPageController::showDialogTab()
{
    disconnectAll();
    m_storage = nullptr;
    teamfinder::TTeamFinderProtocolMessageHandler* handler = m_teamFinderProtocolMessageHandler;
    if (!handler)
        return;
    m_storage = handler->storage();
    m_connections.push_back(connect(m_storage, &teamfinder::TTeamFinderStorage::teamsChanged, this, &TTeamJoinPageController::onTeamsChanged));
    m_connections.push_back(connect(handler, &teamfinder::TTeamFinderProtocolMessageHandler::teamLisUpdateAvailable, this,
                                    &TTeamJoinPageController::onTeamLisUpdateAvailable));
    // The first time: filter for the own level and vocation; the own character's team shows as led.
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    const game::TPlayerData* player = session && session->hasLocalPlayer() ? session->playerData().get() : nullptr;
    if (!m_proxy->characterFiltersSet() && player) {
        m_proxy->model()->setCharacterName(player->characterNameQString());
        m_proxy->setCharacterFilters(static_cast<uint32_t>(player->level()), static_cast<uint8_t>(player->vocation() % 10));
    }
    m_proxy->model()->setResolver(&m_resolver);
    onTeamsChanged();
    handler->requestTeamList();
}

void TTeamJoinPageController::hideDialogTab()
{
    disconnectAll();
}

void TTeamJoinPageController::onJoinLeaveTeamClicked(const uint TeamIDRaw, const uint CurrentStatusRaw)
{
    if (!m_storage)
        return;
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    const auto teamId = static_cast<uint16_t>(TeamIDRaw);
    if (CurrentStatusRaw - 1u < 3u) {
        connect(menu->addCustomEntry(qtTrId("temfinder_leave")), &TContextMenuEntryItem::triggered, this,
                [this, teamId] {
                    if (m_teamFinderProtocolMessageHandler)
                        m_teamFinderProtocolMessageHandler->leaveTeam(teamId);
                });
    } else if (CurrentStatusRaw == 0) {
        connect(menu->addCustomEntry(qtTrId("temfinder_join")), &TContextMenuEntryItem::triggered, this,
                [this, teamId] {
                    if (m_teamFinderProtocolMessageHandler)
                        m_teamFinderProtocolMessageHandler->joinTeam(teamId);
                });
    }
    menu->showCustomMenu();
}

void TTeamJoinPageController::getPremiumClicked()
{
    auto action = std::make_shared<input::TGameActionOpenStore>(store::TStoreOpenParameters{.openAction = store::OPEN_PREMIUM_OR_BOOSTS, .premiumDetail = 0});
    Q_EMIT publishGameAction(action);
}

void TTeamJoinPageController::onTeamLisUpdateAvailable()
{
    if (m_teamFinderProtocolMessageHandler)
        m_teamFinderProtocolMessageHandler->requestTeamList();
}

void TTeamJoinPageController::onTeamsChanged()
{
    if (!m_storage)
        return;
    m_proxy->setTeams(m_storage->teams());
}

void TTeamJoinPageController::setMapStorages(minimap::TMinimapTileStorage*, minimap::TMinimapMarkerStorage*,
                                             std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> StaticDataStorage)
{
    m_mapStaticDataStorage = std::move(StaticDataStorage);
}

}
