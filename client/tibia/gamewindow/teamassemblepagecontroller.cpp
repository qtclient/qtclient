#include "teamassemblepagecontroller.h"
#include "game/gamesessionbase.h"
#include "cyclopedia/staticcontentinformation.h"

#include "assets/things.h"
#include "creatures/creaturestorage.h"
#include "cyclopedia/cyclopediamapstaticdatastorage.h"
#include "game/gameciptypes.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <tibia/game/gamesession.h>

#include <QKeySequence>

#include <algorithm>

namespace tibia::gamewindow {

using teamfinder::TTeamFinderProtocolMessageHandler;

TTeamAssemblePageController::TTeamAssemblePageController(QObject* parent)
    : cip::tibia::gamewindow::TTeamAssemblePageController(parent)
    , m_errorMessage(new TMessageDialogController(this))
    , m_settings(new teamfinder::TTeamSettings(this))
    , m_activities(new TActivitiesSortProxyModel(this))
    , m_members(new TTeamMembersSortProxyModel(this))
{
    m_teamSettings = m_settings;
    m_activityList = m_activities;
    m_teamMembers = m_members;
    m_errorMessage->setCloseOnButtonClicked(true);
}

bool TTeamAssemblePageController::hasPremium() const
{
    return m_premium ? m_premium->isPremium() : false;
}

bool TTeamAssemblePageController::assemblyStarted() const
{
    if (!m_storage)
        return false;
    return m_storage->teamParameters().activity.kind != teamfinder::TTeamActivity::None;
}

QString TTeamAssemblePageController::activityName() const
{
    if (!m_storage)
        return {};
    return activityDisplayName(m_storage->teamParameters().activity, m_resolver);
}

bool TTeamAssemblePageController::isInParty() const
{
    if (!m_storage)
        return false;
    auto* creatures = creatures::TCreatureStorage::instance();
    const auto player = creatures ? creatures->player() : nullptr;
    return player ? player->isPartyMember() : false;
}

void TTeamAssemblePageController::loadActivities()
{
    using teamfinder::TTeamActivity;
    std::vector<TActivity> activities;
    const auto addHunt = [&](const int area, const int subarea) {
        activities.push_back(makeActivity({ .kind = TTeamActivity::Hunt, .huntType = static_cast<uint16_t>(area),
                                            .huntArea = static_cast<uint16_t>(subarea) },
                                          m_resolver));
    };
    const auto addBoss = [&](const uint16_t boss) {
        activities.push_back(makeActivity({ .kind = TTeamActivity::Boss, .bossId = boss }, m_resolver));
    };
    const auto addQuest = [&](const uint32_t quest) {
        activities.push_back(makeActivity({ .kind = TTeamActivity::Quest, .questId = quest }, m_resolver));
    };
    addHunt(0, 0);
    addBoss(0);
    addQuest(0);
    activities.push_back(makeActivity({ .kind = TTeamActivity::Other }, m_resolver));

    // Hunting places: each map area (with an "any" entry when it has several subareas) and its subareas.
    for (const auto& [areaId, area] : cyclopedia::TCyclopediaMapStaticDataStorage::instance().areas()) {
        if (area.subareaIds.size() > 1)
            addHunt(areaId, 0);
        for (const quint16 subarea : area.subareaIds)
            addHunt(areaId, subarea);
    }

    std::vector<uint16_t> bosses;
    for (const auto& race : appearances::TAppearancesManager::instance().getAllRaces()) {
        if (race.boss && !race.notInTeamFinder)
            bosses.push_back(static_cast<uint16_t>(race.raceId));
    }
    std::sort(bosses.begin(), bosses.end());
    for (const uint16_t boss : bosses)
        addBoss(boss);

    for (const cyclopedia::TStaticQuest& quest : cyclopedia::staticQuests()) {
        if (quest.id != 0 && !quest.name.isEmpty())
            addQuest(static_cast<uint16_t>(quest.id));
    }

    m_activities->setActivities(activities);
}

QObject* TTeamAssemblePageController::teamSettings() const
{
    return m_settings;
}

QObject* TTeamAssemblePageController::activityList() const
{
    return m_activities;
}

QObject* TTeamAssemblePageController::teamMembers() const
{
    return m_members;
}

void TTeamAssemblePageController::setPremiumStorage(game::TPremiumTimeStorage* Storage)
{
    if (m_premium)
        disconnect(m_premium, &game::TPremiumTimeStorage::premiumTimeChanged, this, &TTeamAssemblePageController::premiumTimeChanged);
    m_premium = Storage;
    if (m_premium)
        connect(m_premium, &game::TPremiumTimeStorage::premiumTimeChanged, this, &TTeamAssemblePageController::premiumTimeChanged);
    Q_EMIT premiumTimeChanged();
}

void TTeamAssemblePageController::disconnectAll()
{
    for (const QMetaObject::Connection& connection : m_connections)
        disconnect(connection);
    m_connections.clear();
}

void TTeamAssemblePageController::showDialogTab()
{
    if (m_activities->rowCount() == 0)
        loadActivities();
    disconnectAll();
    m_storage = nullptr;
    if (game::TPremiumTimeStorage* premium = game::TPremiumTimeStorage::liveInstance(); premium != m_premium)
        setPremiumStorage(premium);
    TTeamFinderProtocolMessageHandler* handler = m_teamFinderProtocolMessageHandler;
    if (!handler)
        return;
    m_storage = handler->storage();
    m_connections.push_back(connect(m_storage, &teamfinder::TTeamFinderStorage::teamParametersChanged, this,
                                    &TTeamAssemblePageController::onTeamParametersChanged));
    m_connections.push_back(connect(m_storage, &teamfinder::TTeamFinderStorage::membersChanged, this,
                                    &TTeamAssemblePageController::onTeamMembersChanged));
    m_connections.push_back(connect(handler, &TTeamFinderProtocolMessageHandler::ownTeamUpdateAvailable, this,
                                    &TTeamAssemblePageController::onOwnTeamUpdateAvailable));
    const QString characterName = tibia::game::hasLocalPlayer() ? QString::fromStdString(tibia::game::localPlayerName()) : QString();
    onTeamParametersChanged();
    onTeamMembersChanged();
    handler->requestOwnTeam();
    m_members->model()->setCharacterName(characterName);
}

void TTeamAssemblePageController::hideDialogTab()
{
    disconnectAll();
}

void TTeamAssemblePageController::showErrors(const QStringList& Errors)
{
    m_errorMessage->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK,
                                                          QKeySequence(QStringLiteral("Return"))) });
    m_errorMessage->showMessage(qtTrId("error"), Errors.join(QStringLiteral("<br>\n")));
}

void TTeamAssemblePageController::onStartAssembleTeamClicked(const int ActivityRowIndex)
{
    if (!m_storage)
        return;
    const QStringList errors = m_settings->validate(m_members->countAccepted());
    if (!errors.isEmpty()) {
        showErrors(errors);
        return;
    }
    teamfinder::TTeamParameters parameters;
    m_settings->toParameters(parameters);
    const QModelIndex row = m_activities->mapToSource(m_activities->index(ActivityRowIndex, 0));
    const std::vector<TActivity>& activities = m_activities->model()->items();
    if (row.row() < 0 || row.row() >= static_cast<int>(activities.size()))
        return;
    parameters.activity = activities[static_cast<size_t>(row.row())].type;
    if (m_teamFinderProtocolMessageHandler)
        m_teamFinderProtocolMessageHandler->assembleTeam(parameters);
}

void TTeamAssemblePageController::onUpdateTeamSettingsClicked()
{
    if (!m_storage)
        return;
    const QStringList errors = m_settings->validate(m_members->countAccepted());
    if (!errors.isEmpty()) {
        showErrors(errors);
        return;
    }
    teamfinder::TTeamParameters parameters = m_storage->teamParameters();
    m_settings->toParameters(parameters);
    if (m_teamFinderProtocolMessageHandler)
        m_teamFinderProtocolMessageHandler->updateTeam(parameters);
}

void TTeamAssemblePageController::onUnlistTeamClicked()
{
    if (!m_storage)
        return;
    if (m_teamFinderProtocolMessageHandler)
        m_teamFinderProtocolMessageHandler->unlistTeam();
}

void TTeamAssemblePageController::onEditMemberStatusClicked(const uint CharacterIDRaw, const uint CurrentStatusRaw)
{
    if (!m_storage)
        return;
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    const auto setStatus = [this, menu, CharacterIDRaw](const char* label, const uint32_t status) {
        connect(menu->addCustomEntry(qtTrId(label)), &TContextMenuEntryItem::triggered, this, [this, CharacterIDRaw, status] {
            if (m_teamFinderProtocolMessageHandler)
                m_teamFinderProtocolMessageHandler->setMemberStatus(CharacterIDRaw, status);
        });
    };
    // Every status but the current one.
    if (CurrentStatusRaw != teamfinder::TeamMemberAccepted)
        setStatus("accept", teamfinder::TeamMemberAccepted);
    if (CurrentStatusRaw != teamfinder::TeamMemberInvited)
        setStatus("temfinder_invite", teamfinder::TeamMemberInvited);
    if (CurrentStatusRaw != teamfinder::TeamMemberRejected)
        setStatus("temfinder_reject", teamfinder::TeamMemberRejected);
    menu->showCustomMenu();
}

void TTeamAssemblePageController::getPremiumClicked()
{
    auto action = std::make_shared<input::TGameActionOpenStore>(store::TStoreOpenParameters{.openAction = store::OPEN_PREMIUM_OR_BOOSTS, .premiumDetail = 0});
    Q_EMIT publishGameAction(action);
}

uint TTeamAssemblePageController::getLeaderlevel()
{
    if (!tibia::game::hasLocalPlayer())
        return 2;
    return static_cast<uint16_t>(tibia::game::localPlayerLevel());
}

void TTeamAssemblePageController::onTeamParametersChanged()
{
    m_settings->setFromParameters(m_storage ? m_storage->teamParameters() : teamfinder::TTeamParameters{});
    Q_EMIT assemblyStartedChanged();
}

void TTeamAssemblePageController::onTeamMembersChanged()
{
    if (!m_storage)
        return;
    m_members->setMembers(m_storage->members());
}

void TTeamAssemblePageController::onOwnTeamUpdateAvailable()
{
    if (m_teamFinderProtocolMessageHandler)
        m_teamFinderProtocolMessageHandler->requestOwnTeam();
}

void TTeamAssemblePageController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    m_teamFinderProtocolMessageHandler = session ? session->teamFinderProtocolMessageHandler().get() : nullptr;
}

}
