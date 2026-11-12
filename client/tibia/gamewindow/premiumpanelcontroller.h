#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

#include <map>
#include <vector>

class QQuickItem;

namespace tibia {
namespace gamewindow {

/// One premium feature line (CIP: game::TPremiumMessage, ctor):
/// "premium_feature_*" / "premium_*" text and its premium-icons-20x20 cell.
class TPremiumMessage : public cip::tibia::game::TPremiumMessage
{
    Q_OBJECT
public:
    TPremiumMessage(int id, QObject* parent);
};

/// The premium time line, feature 13 (CIP: game::TPremiumMessageRenewPremium,
/// message : "premium_time_expires(_plural)" with the days left).
class TPremiumMessageRenewPremium final : public cip::tibia::game::TPremiumMessageRenewPremium
{
    Q_OBJECT
public:
    explicit TPremiumMessageRenewPremium(QObject* parent);
    /// CIP .
    void setDaysLeft(int days);
    QString message() const override;

private:
    int m_daysLeft = 0;
};

/// PremiumPanel.qml (CIP: gamewindow::TPremiumPanelController, ctor
/// , item): the features the server says a free
/// account just missed (premium trigger message), highlighted until the
/// player looks at them. Shown at login when advertisePremium holds,
/// toggled by TOGGLE_PREMIUM_PANEL (74) and hidden while the tutorial mode is
/// on (CHANGE_TUTORIAL_MODE_ENABLED, 223).
///
/// The premium time is CIP's game::TPremiumTimeStorage (vtable),
/// fed from the player basic data (premium flag and expiration; CIP's
/// parsePlayerInfo publishes onPremiumExpiration on the notify bus).
class TPremiumPanelController final : public cip::tibia::gamewindow::TPremiumPanelController
{
    Q_OBJECT
public:
    TPremiumPanelController();
    ~TPremiumPanelController() override;

    void setPanelItem(QQuickItem* item);

public Q_SLOTS:
    /// CIP : the highlight timer ran out.
    void stopHighlight() override;
    /// CIP : the renew line's days left.
    void onPremiumTimeChanged() override;
    /// CIP .
    void onIsVisibleChanged() override;

public:
    /// CIP (session set): CIP's premium time storage already holds
    /// the login service's premium state here; the in-game premium packet
    /// arrives after the game start.
    void onSessionStarted(bool premium, qint64 premiumUntil);

    Q_INVOKABLE void maximizePanel() override;
    Q_INVOKABLE void minimizePanel() override;

private:
    /// CIP : the triggered features become the messages.
    void setTriggers(const std::vector<uint8_t>& features);
    /// CIP , the store's SetStoreButtonDeeplink (0xA8): a deep
    /// link to anything but the home page or the XP boost (detail 1)
    /// highlights the maximize button for 10 minutes.
    void onStoreDeeplinkChanged(int openAction, int detail);
    /// CIP .
    void setMaximized(bool maximized);
    /// CIP : highlights unless maximized or already highlighted
    /// for longer than `minimumMs`.
    void startHighlight(int minimumMs);
    void setHighlight(bool highlight);

    /// CIP TPremiumTimeStorage v0x60 .
    bool isPremium() const { return m_premium; }
    /// v0x68 : premium without an expiration.
    bool hasUnlimitedPremium() const { return m_premiumExpiration == 0 && m_premium; }
    /// v0x70 : premium, or an expiration is known.
    bool hasPremiumTime() const { return m_premiumExpiration != 0 || m_premium; }
    /// v0x78 : the state (v0x88, : 2 unlimited,
    /// else the premium flag) is 0.
    bool isFreeAccount() const { return !hasUnlimitedPremium() && !m_premium; }
    /// v0x80 : whole days from when the time was set to the
    /// expiration.
    int premiumDaysLeft() const;
    /// v0x90 / v0x98 .
    void setPremiumTime(bool premium, uint32_t expiration);
    void resetPremiumTime();

    /// CIP (inlined in and): a free
    /// account that never had premium, or premium ending in under 4 days.
    bool advertisePremium() const;
    /// CIP at a session start: collapses and shows the panel when
    /// advertisePremium; hidden without a game.
    void applySessionVisibility();
    /// CIP (TOGGLE_PREMIUM_PANEL): hides a shown panel, collapses
    /// and shows a hidden one; a free account that never had premium cannot
    /// toggle it.
    void togglePanel();
    /// CIP , action 223: the tutorial mode hides the panel; leaving
    /// it shows the panel again when advertisePremium.
    void onTutorialModeChanged(bool enabled);
    void showCollapsed();
    void setPanelVisible(bool visible);

    bool m_premium = false;
    uint32_t m_premiumExpiration = 0;
    uint32_t m_premiumTimeSetAt = 0;
    std::map<int, cip::tibia::game::TPremiumMessage*> m_messages;
    TPremiumMessageRenewPremium* m_renewPremium = nullptr;
    QPointer<QQuickItem> m_item;
    QTimer m_highlightTimer;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
