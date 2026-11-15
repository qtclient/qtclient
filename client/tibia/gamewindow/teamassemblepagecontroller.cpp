#include "teamassemblepagecontroller.h"

#include "game/gameciptypes.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <tibia/game/gamesession.h>

#include <QKeySequence>

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
    // The listed activity's display name comes from the activity name resolver (static data), not wired yet.
    return {};
}

bool TTeamAssemblePageController::isInParty() const
{
    return m_isInParty;
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
    disconnectAll();
    m_storage = nullptr;
    if (game::TPremiumTimeStorage* premium = game::TPremiumTimeStorage::liveInstance(); premium != m_premium)
        setPremiumStorage(premium);
    auto* handler = TTeamFinderProtocolMessageHandler::instance();
    m_storage = handler->storage();
    m_connections.push_back(connect(m_storage, &teamfinder::TTeamFinderStorage::teamParametersChanged, this,
                                    &TTeamAssemblePageController::onTeamParametersChanged));
    m_connections.push_back(connect(m_storage, &teamfinder::TTeamFinderStorage::membersChanged, this,
                                    &TTeamAssemblePageController::onTeamMembersChanged));
    m_connections.push_back(connect(handler, &TTeamFinderProtocolMessageHandler::ownTeamUpdateAvailable, this,
                                    &TTeamAssemblePageController::onOwnTeamUpdateAvailable));
    const QString characterName = tibia::game::localPlayer() ? QString::fromStdString(tibia::game::localPlayerName()) : QString();
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
    TTeamFinderProtocolMessageHandler::instance()->assembleTeam(parameters);
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
    TTeamFinderProtocolMessageHandler::instance()->updateTeam(parameters);
}

void TTeamAssemblePageController::onUnlistTeamClicked()
{
    if (!m_storage)
        return;
    TTeamFinderProtocolMessageHandler::instance()->unlistTeam();
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
        connect(menu->addCustomEntry(qtTrId(label)), &TContextMenuEntryItem::triggered, this, [CharacterIDRaw, status] {
            TTeamFinderProtocolMessageHandler::instance()->setMemberStatus(CharacterIDRaw, status);
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
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_STORE));
    action->parameter = 1;
    input::publish(action);
}

uint TTeamAssemblePageController::getLeaderlevel()
{
    if (!tibia::game::localPlayer())
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
    TTeamFinderProtocolMessageHandler::instance()->requestOwnTeam();
}

}
