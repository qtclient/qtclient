#include "premiumpanelcontroller.h"

#include "game/gameciptypes.h"
#include "store/storestorage.h"

#include "input/gameactionhandlers.h"


#include <QDateTime>
#include <QQuickItem>

#include <algorithm>
#include <iterator>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;

constexpr int RenewPremiumFeature = 0xd;
/// CIP: the timer's interval set in .
constexpr int HighlightMs = 600000;
/// CIP : a new trigger restarts a highlight with less left.
constexpr int RetriggerMs = 5000;

}

TPremiumPanelController::TPremiumPanelController()
{
    createPremiumMessages();
    // The renew line replaces the table's plain message for its feature.
    delete m_messages[RenewPremiumFeature];
    m_renewPremium = new game::TPremiumMessageRenewPremium(this);
    m_messages[RenewPremiumFeature] = m_renewPremium;

    m_highlightTimer.setSingleShot(true);
    m_highlightTimer.setInterval(HighlightMs);
    connect(&m_highlightTimer, &QTimer::timeout, this, &TPremiumPanelController::stopHighlight);

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPremiumTrigger", [this](GameObject*, const Args& args) {
        if (const auto* features = args.empty() ? nullptr : std::any_cast<std::vector<uint8_t>>(&args[0]))
            onNewPremiumFeatureList(*features);
        return std::any{};
    });
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

void TPremiumPanelController::createPremiumMessages()
{
    for (int id = 0; id <= 0x23; ++id)
        m_messages[id] = new game::TPremiumMessage(static_cast<uint8_t>(id), false, this);
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
    if (!tibia::game::isOnline()) {
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
