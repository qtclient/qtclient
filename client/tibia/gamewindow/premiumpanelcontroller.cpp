#include "premiumpanelcontroller.h"

#include "game/currentgamesession.h"
#include "game/gameciptypes.h"
#include "game/gamesessionbase.h"
#include "store/storestorage.h"

#include "input/gameactionhandlers.h"


#include <QDateTime>
#include <QQuickItem>

#include <algorithm>
#include <iterator>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {


constexpr int RenewPremiumFeature = 0xd;
/// CIP: the timer's interval set in .
constexpr int HighlightMs = 600000;
/// CIP : a new trigger restarts a highlight with less left.
constexpr int RetriggerMs = 5000;

}

TPremiumPanelController::TPremiumPanelController()
{
    m_messages = createPremiumMessages(false, this);
    // The renew line is the panel's message for its feature.
    m_renewPremium = new game::TPremiumMessageRenewPremium(this);
    m_messages[RenewPremiumFeature] = m_renewPremium;

    m_highlightTimer.setSingleShot(true);
    m_highlightTimer.setInterval(HighlightMs);
    connect(&m_highlightTimer, &QTimer::timeout, this, &TPremiumPanelController::stopHighlight);

    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() { applySessionVisibility(); });

    m_gameActions.on(input::EGameAction::TOGGLE_PREMIUM_PANEL, [this](const input::IGameAction&) {
        togglePanel();
        return true;
    });
    m_gameActions.on(input::EGameAction::FORCE_HIDE_PREMIUM_PANEL, [this](const input::IGameAction&) {
        hidePanel();
        return true;
    });
    m_gameActions.on(input::EGameAction::CHANGE_TUTORIAL_MODE_ENABLED, [this](const input::TGameActionChangeTutorialModeEnabled& action) {
        onTutorialModeChanged(action.enabled());
        return false;
    });
}

TPremiumPanelController::~TPremiumPanelController()
{
    // Host: the item stops reaching this controller before the base deletes it.
    releasePanelItem();
}

std::map<int, cip::tibia::game::TPremiumMessage*> TPremiumPanelController::createPremiumMessages(const bool largeIcon, QObject* parent)
{
    std::map<int, cip::tibia::game::TPremiumMessage*> messages;
    for (int id = 0; id <= 0x23; ++id)
        if (id != RenewPremiumFeature)
            messages[id] = new game::TPremiumMessage(static_cast<uint8_t>(id), largeIcon, parent);
    return messages;
}

bool TPremiumPanelController::isPremium() const { return m_premiumTimeStorage && m_premiumTimeStorage->isPremium(); }
bool TPremiumPanelController::hasUnlimitedPremium() const { return m_premiumTimeStorage && m_premiumTimeStorage->hasUnlimitedPremium(); }
bool TPremiumPanelController::hasPremiumTime() const { return m_premiumTimeStorage && m_premiumTimeStorage->hasPremiumRecord(); }
bool TPremiumPanelController::isFreeAccount() const { return !m_premiumTimeStorage || m_premiumTimeStorage->isFreeAccount(); }
int TPremiumPanelController::premiumDaysLeft() const { return m_premiumTimeStorage ? m_premiumTimeStorage->remainingDays() : 0; }

void TPremiumPanelController::setPremiumTimeStorage(game::TPremiumTimeStorage* PremiumTimeStorage)
{
    // Host: the old storage's connection goes with it.
    if (m_premiumTimeStorage)
        disconnect(m_premiumTimeStorage, &game::TPremiumTimeStorage::premiumTimeChanged, this, &TPremiumPanelController::onPremiumTimeChanged);
    m_premiumTimeStorage = PremiumTimeStorage;
    if (!m_premiumTimeStorage)
        return;
    onPremiumTimeChanged();
    connect(m_premiumTimeStorage, &game::TPremiumTimeStorage::premiumTimeChanged, this, &TPremiumPanelController::onPremiumTimeChanged);
}

void TPremiumPanelController::onSessionStarted()
{
    // CIP: the session's premium protocol message handler's feature list.
    if (const game::TGameSessionBase* session = game::currentGameSession())
        if (const auto& handler = session->premiumProtocolMessageHandler())
            connect(handler.get(), &game::TPremiumProtocolMessageHandler::newPremiumFeatureList, this,
                    &TPremiumPanelController::onNewPremiumFeatureList, Qt::UniqueConnection);
    applySessionVisibility();
}

bool TPremiumPanelController::advertisePremium() const
{
    const bool neverPremium = isFreeAccount() && !hasPremiumTime();
    const bool endingSoon = !hasUnlimitedPremium() && isPremium() && premiumDaysLeft() < 4;
    return neverPremium || endingSoon;
}

void TPremiumPanelController::applySessionVisibility()
{
    if (const game::TGameSessionBase* session = tibia::game::currentGameSession(); !session || !session->isOnline()) {
        hidePanel();
        return;
    }
    if (advertisePremium())
        showCollapsed();
}

bool TPremiumPanelController::canTogglePanel() const
{
    return isPremium() || hasUnlimitedPremium() || hasPremiumTime();
}

void TPremiumPanelController::togglePanel()
{
    stopHighlight();
    if (!canTogglePanel())
        return;
    if (panelItem() && panelItem()->isVisible())
        hidePanel();
    else
        showCollapsed();
}

void TPremiumPanelController::onTutorialModeChanged(const bool enabled)
{
    if (enabled)
        hidePanel();
    else if (advertisePremium())
        showCollapsed();
}

void TPremiumPanelController::showCollapsed()
{
    setMaximized(false);
    showPanel();
}

void TPremiumPanelController::setPanelItem(QQuickItem* PanelItem)
{
    TSidebarPanelController::setPanelItem(PanelItem);
    connect(PanelItem, &QQuickItem::visibleChanged, this, &TPremiumPanelController::onIsVisibleChanged);
    hidePanel();
}

void TPremiumPanelController::releasePanelItem()
{
    if (QQuickItem* const item = panelItem())
        disconnect(item, &QQuickItem::visibleChanged, this, &TPremiumPanelController::onIsVisibleChanged);
    TSidebarPanelController::releasePanelItem();
}

void TPremiumPanelController::onNewPremiumFeatureList(const std::vector<uint8_t>& features)
{
    m_premiumMessages.clear();
    // An unknown feature adds an empty line, as the table lookup inserts one.
    for (const uint8_t feature : features)
        m_premiumMessages.append(m_messages[feature]);
    Q_EMIT premiumMessagesChanged();
    startHighlight(RetriggerMs);
}

void TPremiumPanelController::onStoreDeeplinkChanged(const store::TStoreOpenParameters& StoreDeeplink)
{
    if (StoreDeeplink.openAction == 0 || StoreDeeplink.premiumDetail == 1)
        return;
    startHighlight(HighlightMs);
}

void TPremiumPanelController::startHighlight(const int minimumMs)
{
    if (m_isMaximized)
        return;
    if (m_highlightTimer.isActive() && m_highlightTimer.remainingTime() >= minimumMs)
        return;
    m_highlightTimer.start();
    setHighlight(true);
}

void TPremiumPanelController::setHighlight(const bool highlight)
{
    if (highlight == m_highlightMaximizeButton)
        return;
    m_highlightMaximizeButton = highlight;
    Q_EMIT highlightMaximizeButtonChanged();
}

void TPremiumPanelController::stopHighlight()
{
    m_highlightTimer.stop();
    setHighlight(false);
}

void TPremiumPanelController::setMaximized(const bool maximized)
{
    if (maximized == m_isMaximized)
        return;
    m_isMaximized = maximized;
    Q_EMIT maximziedChanged();
    stopHighlight();
}

void TPremiumPanelController::maximizePanel() { setMaximized(true); }

void TPremiumPanelController::minimizePanel() { setMaximized(false); }

void TPremiumPanelController::onPremiumTimeChanged()
{
    if (m_renewPremium)
        m_renewPremium->setDaysLeft(premiumDaysLeft());
}

void TPremiumPanelController::onIsVisibleChanged()
{
    Q_EMIT isVisibleChanged(panelItem() && panelItem()->isVisible());
}

}
