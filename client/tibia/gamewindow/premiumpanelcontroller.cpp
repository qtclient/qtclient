#include "premiumpanelcontroller.h"

#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>

#include <QDateTime>
#include <QQuickItem>

#include <algorithm>
#include <iterator>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;

constexpr int RenewPremiumFeature = 0xd;
/// CIP: the timer's interval set in 0x100c482c0.
constexpr int HighlightMs = 600000;
/// CIP 0x100c48b40: a new trigger restarts a highlight with less left.
constexpr int RetriggerMs = 5000;

/// CIP 0x100b576c0, by feature id (13 is TPremiumMessageRenewPremium's).
constexpr const char* MessageIds[] = {
    "premium_feature_areas",         "premium_feature_offlinetraining", "premium_feature_spells",
    "premium_feature_mounts",        "premium_feature_outfits",         "premium_feature_xp_boost",
    "premium_feature_death_penalty", "premium_feature_market",          "premium_feature_house",
    "premium_feature_large_vip_list", "premium_feature_depot",          "premium_feature_private_chat",
    "premium_feature_promotion",     "dummy_unknown",                   "premium_feature_arena",
    "premium_feature_prey_slots",    "premium_feature_imbuing",         "premium_feature_daily_reward",
    "premium_feature_analysers",     "premium_all_quests",              "premium_no_login_queue",
    "premium_powerful_summons",      "premium_guild_management",        "premium_outfit_addons",
    "premium_premium_only_worlds",   "premium_regenerate_faster",       "premium_loyalty_system",
    "premium_track_more_items",      "premium_more_soul_points",        "premium_more_vip_groups",
    "premium_track_more_quests",     "premium_refined_quick_loot",      "premium_more_charms",
    "premium_secondary_battle_lists", "premium_use_team_finder",        "premium_use_skill_wheel",
};

/// CIP 0x1009272d0.
QString iconUrl(const int id)
{
    return QStringLiteral("image://premium-icons-20x20/%1").arg(id);
}

}

TPremiumMessage::TPremiumMessage(const int id, QObject* parent)
    : cip::tibia::game::TPremiumMessage(parent)
{
    m_message = qtTrId(MessageIds[id]);
    m_imageUrl = iconUrl(id);
}

TPremiumMessageRenewPremium::TPremiumMessageRenewPremium(QObject* parent)
    : cip::tibia::game::TPremiumMessageRenewPremium(parent)
{
    m_imageUrl = iconUrl(RenewPremiumFeature);
}

void TPremiumMessageRenewPremium::setDaysLeft(const int days) { m_daysLeft = days; }

QString TPremiumMessageRenewPremium::message() const
{
    return qtTrId(m_daysLeft == 1 ? "premium_time_expires" : "premium_time_expires_plural").arg(m_daysLeft);
}

TPremiumPanelController::TPremiumPanelController()
{
    for (int id = 0; id < static_cast<int>(std::size(MessageIds)); ++id) {
        if (id == RenewPremiumFeature) {
            m_renewPremium = new TPremiumMessageRenewPremium(this);
            m_messages[id] = m_renewPremium;
        } else {
            m_messages[id] = new TPremiumMessage(id, this);
        }
    }

    m_highlightTimer.setSingleShot(true);
    m_highlightTimer.setInterval(HighlightMs);
    connect(&m_highlightTimer, &QTimer::timeout, this, &TPremiumPanelController::stopHighlight);

    m_hooks.emplace_back("g_game", "onPremiumTrigger", [this](LuaObject*, const Args& args) {
        if (const auto* features = args.empty() ? nullptr : std::any_cast<std::vector<uint8_t>>(&args[0]))
            setTriggers(*features);
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onPremiumExpiration", [this](LuaObject*, const Args& args) {
        const auto* premium = args.size() < 2 ? nullptr : std::any_cast<bool>(&args[0]);
        const auto* expiration = args.size() < 2 ? nullptr : std::any_cast<uint32_t>(&args[1]);
        if (premium && expiration)
            setPremiumTime(*premium, *expiration);
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onStoreButtonDeeplink", [this](LuaObject*, const Args& args) {
        onStoreDeeplinkChanged(static_cast<int>(client::gamecore::argInt(args, 0)),
                               static_cast<int>(client::gamecore::argInt(args, 1)));
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        resetPremiumTime();
        applySessionVisibility();
        return std::any{};
    });

    input::setActionHandler(input::EGameAction::TOGGLE_PREMIUM_PANEL, [this](const input::TGameAction&) {
        togglePanel();
        return true;
    });
    input::setActionHandler(input::EGameAction::FORCE_HIDE_PREMIUM_PANEL, [this](const input::TGameAction&) {
        setPanelVisible(false);
        return true;
    });
    input::setActionHandler(input::EGameAction::CHANGE_TUTORIAL_MODE_ENABLED, [this](const input::TGameAction& action) {
        onTutorialModeChanged(action.enable);
        return false;
    });
}

TPremiumPanelController::~TPremiumPanelController()
{
    input::setActionHandler(input::EGameAction::TOGGLE_PREMIUM_PANEL, nullptr);
    input::setActionHandler(input::EGameAction::FORCE_HIDE_PREMIUM_PANEL, nullptr);
    input::setActionHandler(input::EGameAction::CHANGE_TUTORIAL_MODE_ENABLED, nullptr);
    setPanelItem(nullptr);
}

int TPremiumPanelController::premiumDaysLeft() const
{
    if (m_premiumTimeSetAt > m_premiumExpiration || m_premiumExpiration == m_premiumTimeSetAt)
        return 0;
    return static_cast<int>((m_premiumExpiration - m_premiumTimeSetAt) / 86400);
}

void TPremiumPanelController::setPremiumTime(const bool premium, const uint32_t expiration)
{
    m_premium = premium;
    m_premiumExpiration = expiration;
    m_premiumTimeSetAt = static_cast<uint32_t>(QDateTime::currentSecsSinceEpoch());
    onPremiumTimeChanged();
}

void TPremiumPanelController::onSessionStarted(const bool premium, const qint64 premiumUntil)
{
    setPremiumTime(premium, static_cast<uint32_t>(std::max<qint64>(premiumUntil, 0)));
    applySessionVisibility();
}

void TPremiumPanelController::resetPremiumTime()
{
    m_premium = false;
    m_premiumExpiration = 0;
    m_premiumTimeSetAt = 0;
    onPremiumTimeChanged();
}

bool TPremiumPanelController::advertisePremium() const
{
    const bool neverPremium = isFreeAccount() && !hasPremiumTime();
    const bool endingSoon = !hasUnlimitedPremium() && isPremium() && premiumDaysLeft() < 4;
    return neverPremium || endingSoon;
}

void TPremiumPanelController::applySessionVisibility()
{
    if (!g_game.isOnline()) {
        setPanelVisible(false);
        return;
    }
    if (advertisePremium())
        showCollapsed();
}

void TPremiumPanelController::togglePanel()
{
    stopHighlight();
    if (!isPremium() && !hasUnlimitedPremium() && !hasPremiumTime())
        return;
    if (m_item && m_item->isVisible())
        setPanelVisible(false);
    else
        showCollapsed();
}

void TPremiumPanelController::onTutorialModeChanged(const bool enabled)
{
    if (enabled)
        setPanelVisible(false);
    else if (advertisePremium())
        showCollapsed();
}

void TPremiumPanelController::showCollapsed()
{
    setMaximized(false);
    setPanelVisible(true);
}

void TPremiumPanelController::setPanelVisible(const bool visible)
{
    if (m_item)
        m_item->setVisible(visible);
}

void TPremiumPanelController::setPanelItem(QQuickItem* item)
{
    if (m_item)
        disconnect(m_item, nullptr, this, nullptr);
    m_item = item;
    if (!m_item)
        return;
    connect(m_item, &QQuickItem::visibleChanged, this, &TPremiumPanelController::onIsVisibleChanged);
    applySessionVisibility();
}

void TPremiumPanelController::setTriggers(const std::vector<uint8_t>& features)
{
    m_premiumMessages.clear();
    for (const uint8_t feature : features) {
        if (const auto it = m_messages.find(feature); it != m_messages.end())
            m_premiumMessages.append(it->second);
    }
    Q_EMIT premiumMessagesChanged();
    startHighlight(RetriggerMs);
}

void TPremiumPanelController::onStoreDeeplinkChanged(const int openAction, const int detail)
{
    if (openAction == 0 || detail == 1)
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
    Q_EMIT isVisibleChanged(m_item && m_item->isVisible());
}

}
