#pragma once

#include "teamfinder/teamfinderciptypes.h"
#include "teamfinder/teamsortproxymodels.h"

#include <cip_gen.h>

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>

#include <memory>

namespace tibia {
namespace cyclopedia {
class TCyclopediaMapStaticDataStorage;
}
namespace minimap {
class TMinimapMarkerStorage;
class TMinimapTileStorage;
}
namespace game {
class IGameSession;
class TPremiumTimeStorage;
}
namespace gamewindow {

/// TeamJoinPage.qml (CIP TTeamJoinPageController): the listed teams, apply / withdraw.
class TTeamJoinPageController final : public cip::tibia::gamewindow::TTeamJoinPageController
{
    Q_OBJECT
public:
    explicit TTeamJoinPageController(QObject* parent = nullptr);

    bool hasPremium() const override;
    QObject* teamsList() const override;

    /// Binds to the session's team storage, filters for the own character once, and asks for the list.
    void showDialogTab() override;
    void hideDialogTab() override;

    /// The account's premium state; premiumTimeChanged follows it.
    void setPremiumStorage(game::TPremiumTimeStorage* Storage);
    /// The map storages: the page keeps the Cyclopedia map's static data,
    /// which the team list's activities come from (the tile and marker storages are
    /// not used).
    void setMapStorages(minimap::TMinimapTileStorage* TileStorage, minimap::TMinimapMarkerStorage* MarkerStorage,
                        std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> StaticDataStorage);
    /// A new game session: the filters start over (and pick up the new character on the next show).
    /// CIP v12: the page follows the session's team finder; the filters start over.
    void setGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

public Q_SLOTS:
    void onJoinLeaveTeamClicked(uint TeamIDRaw, uint CurrentStatusRaw) override;
    void getPremiumClicked() override;
    void onTeamLisUpdateAvailable() override;
    void onTeamsChanged() override;

private:
    void disconnectAll();

    QPointer<game::TPremiumTimeStorage> m_premium;
    QPointer<teamfinder::TTeamFinderStorage> m_storage;
    std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> m_mapStaticDataStorage;
    std::shared_ptr<game::IGameSession> m_gameSession;
    QPointer<teamfinder::TTeamFinderProtocolMessageHandler> m_teamFinderProtocolMessageHandler;
    QList<QMetaObject::Connection> m_connections;
    teamfinder::TTeamfinderActivityNameResolver m_resolver;
    TTeamsListSortProxyModel* m_proxy = nullptr;
};

}
}
