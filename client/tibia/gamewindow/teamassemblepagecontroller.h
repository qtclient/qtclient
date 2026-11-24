#pragma once

#include "teamfinder/teamfinderciptypes.h"
#include "teamfinder/teamsortproxymodels.h"

#include <cip_gen.h>

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QStringList>

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

class TMessageDialogController;

/// TeamAssemblePage.qml (CIP TTeamAssemblePageController): list the own team and manage its members.
class TTeamAssemblePageController final : public cip::tibia::gamewindow::TTeamAssemblePageController
{
    Q_OBJECT
public:
    explicit TTeamAssemblePageController(QObject* parent = nullptr);
    /// CIP v12: the page talks to the session's team finder while shown.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    bool hasPremium() const override;
    /// The own team is listed (it has an activity).
    bool assemblyStarted() const override;
    QString activityName() const override;
    bool isInParty() const override;
    QObject* teamSettings() const override;
    QObject* activityList() const override;
    QObject* teamMembers() const override;

    /// Binds to the session's team storage, shows the own team and asks the server for it.
    void showDialogTab() override;
    void hideDialogTab() override;

    /// The account's premium state; premiumTimeChanged follows it.
    void setPremiumStorage(game::TPremiumTimeStorage* Storage);
    /// The map storages: the page keeps the Cyclopedia map's static data,
    /// which the hunting places come from (the tile and marker storages are
    /// not used).
    void setMapStorages(minimap::TMinimapTileStorage* TileStorage, minimap::TMinimapMarkerStorage* MarkerStorage,
                        std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> StaticDataStorage);

public Q_SLOTS:
    void onStartAssembleTeamClicked(int ActivityRowIndex) override;
    void onUpdateTeamSettingsClicked() override;
    void onUnlistTeamClicked() override;
    void onEditMemberStatusClicked(uint CharacterIDRaw, uint CurrentStatusRaw) override;
    void getPremiumClicked() override;
    uint getLeaderlevel() override;
    void onTeamParametersChanged() override;
    void onTeamMembersChanged() override;
    void onOwnTeamUpdateAvailable() override;

private:
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> m_mapStaticDataStorage;
    QPointer<teamfinder::TTeamFinderProtocolMessageHandler> m_teamFinderProtocolMessageHandler;
    /// Fills the activity list: unspecified entries of each kind, the hunting places, the bosses and the quests.
    void loadActivities();
    void showErrors(const QStringList& Errors);
    void disconnectAll();

    TMessageDialogController* m_errorMessage = nullptr;
    QPointer<game::TPremiumTimeStorage> m_premium;
    QPointer<teamfinder::TTeamFinderStorage> m_storage;
    QList<QMetaObject::Connection> m_connections;
    teamfinder::TTeamfinderActivityNameResolver m_resolver;
    teamfinder::TTeamSettings* m_settings = nullptr;
    TActivitiesSortProxyModel* m_activities = nullptr;
    TTeamMembersSortProxyModel* m_members = nullptr;
};

}
}
