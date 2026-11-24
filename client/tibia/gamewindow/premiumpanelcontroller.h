#pragma once

#include "input/gameactiondispatch.h"
#include "client/gamecore.h"
#include "game/premiummessagerenewpremium.h"
#include "gamewindow/sidebarpanelcontrollerbase.h"

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

#include <map>
#include <vector>

class QQuickItem;

namespace tibia {
namespace store {
struct TStoreOpenParameters;
}
namespace game {
class TPremiumTimeStorage;
}
namespace gamewindow {

/// PremiumPanel.qml (CIP: gamewindow::TPremiumPanelController, ctor
/// , item): the features the server says a free
/// account just missed (premium trigger message), highlighted until the
/// player looks at them. Shown at login when advertisePremium holds,
/// toggled by TOGGLE_PREMIUM_PANEL (74) and hidden while the tutorial mode is
/// on (CHANGE_TUTORIAL_MODE_ENABLED, 223).
///
/// The premium time comes from the account's game::TPremiumTimeStorage.
class TPremiumPanelController final : public TSidebarPanelController<cip::tibia::gamewindow::TPremiumPanelController>
{
    Q_OBJECT
public:
    TPremiumPanelController();
    ~TPremiumPanelController() override;

    /// CIP v14: the base's, then the controller follows the item's
    /// visibility; a new item starts hidden.
    void setPanelItem(QQuickItem* PanelItem) override;
    /// CIP v15: stops following the item, then the base's.
    void releasePanelItem() override;

public Q_SLOTS:
    /// CIP : the highlight timer ran out.
    void stopHighlight() override;
    /// CIP : the renew line's days left.
    void onPremiumTimeChanged() override;
    /// CIP .
    void onIsVisibleChanged() override;

public:
    /// The account's premium time: its changes update the renew line (applied
    /// at once).
    void setPremiumTimeStorage(game::TPremiumTimeStorage* PremiumTimeStorage);
    /// The premium time storage already holds the login service's premium
    /// state here; the in-game premium packet arrives after the game start.
    void onSessionStarted();

    /// The store button deep link: a link to anything but the home page or
    /// the XP boost (detail 1) highlights the maximize button for 10 minutes.
    void onStoreDeeplinkChanged(const store::TStoreOpenParameters& StoreDeeplink);

    Q_INVOKABLE void maximizePanel() override;
    Q_INVOKABLE void minimizePanel() override;

public:
    /// CIP: one message per premium feature 0..0x23 but 0xd (the renew line,
    /// which the panel makes itself); the character selection's messages have
    /// large icons, the panel's small ones.
    static std::map<int, cip::tibia::game::TPremiumMessage*> createPremiumMessages(bool largeIcon, QObject* parent);

private:
    /// Premium, unlimited premium or a known premium time.
    bool canTogglePanel() const;
    /// CIP : the triggered features become the messages.
    void onNewPremiumFeatureList(const std::vector<uint8_t>& features);
    /// CIP .
    void setMaximized(bool maximized);
    /// CIP : highlights unless maximized or already highlighted
    /// for longer than `minimumMs`.
    void startHighlight(int minimumMs);
    void setHighlight(bool highlight);

    /// The storage's state (all false / 0 without a storage).
    bool isPremium() const;
    bool hasUnlimitedPremium() const;
    bool hasPremiumTime() const;
    bool isFreeAccount() const;
    int premiumDaysLeft() const;

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

    QPointer<game::TPremiumTimeStorage> m_premiumTimeStorage;
    std::map<int, cip::tibia::game::TPremiumMessage*> m_messages;
    game::TPremiumMessageRenewPremium* m_renewPremium = nullptr;
    QTimer m_highlightTimer;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
