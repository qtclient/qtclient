#pragma once

#include <memory>

#include "input/gameactiondispatch.h"
#include "client/gamecore.h"
#include "gamewindow/bosstiarydialogcontroller.h"
#include "gamewindow/characterinfodialogcontroller.h"
#include "gamewindow/cyclopediadialogcontext.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/housesinfodialogcontroller.h"
#include "gamewindow/iteminfodialogcontroller.h"
#include "gamewindow/monsterbonuseffectsdialogcontroller.h"
#include "gamewindow/monsterdialogcontroller.h"

#include <cip_gen.h>

#include "worldmap/coordinates.h"

#include "highlighttimer.h"

#include <QTimer>

#include <deque>
#include <variant>
#include <vector>

namespace tibia::game {
class IGameSession;
}

namespace tibia {
namespace cyclopedia {
class TBestiaryStorage;
class TMonsterRaceStorage;
}
}

namespace tibia {
namespace mods {
class InstanceBackend;
}
namespace game {
class TPremiumTimeStorage;
}
}

namespace tibia {
namespace gamewindow {

class TCyclopediaMapDialogController;
class TMagicalArchiveDialogController;

/// CyclopediaDialog.qml (CIP: TCyclopediaDialogController, showDialog
/// , handleGameAction). The tab bar, the gold and
/// charm balances, and the back navigation between tabs. The character page
/// is ported; the other tabs are their CIP base classes until they are.
class TCyclopediaDialogController final : public TDialogController<cip::tibia::gamewindow::TCyclopediaDialogController>
{
    Q_OBJECT
public:
    explicit TCyclopediaDialogController(QObject* parent = nullptr);
    ~TCyclopediaDialogController() override;

    TCyclopediaMapDialogController* mapDialog() const { return m_map; }

    /// The local player's gold; "unknown" (charms: "0") outside the game.
    QString bankGoldBalanceString() const override;
    QString inventoryGoldBalanceString() const override;
    qulonglong totalGoldBalance() const override;
    QString totalGoldBalanceString() const override;
    /// The charm points and minor charm echoes, available and unlocked.
    QString availableCharmPointsString() const override;
    QString unlockedCharmPointsString() const override;
    QString availableMinorCharmEchoesString() const override;
    QString unlockedMinorCharmEchoesString() const override;
    bool backPossible() const override { return !m_backlog.empty(); }

    /// Reopens the dialog on `Tab` and asks the server for the charm, bank
    /// and inventory balances.
    void showDialog(const DialogTab& Tab) override;
    /// showDialog, remembering the shown page for requestBack first.
    void captureNavigationBacklogContextIfNeedAndShowDialog(const DialogTab& NewTab) override;
    void closeDialog() override;
    /// The close button: forgets the backlog and closes.
    void requestClose() override;
    void requestTabSwitch(const DialogTab& NewTab) override;
    void requestOpenManageLootContainerDialog() override;
    /// Returns to the last remembered page and its context.
    void requestBack() override;

    /// CIP connector: no backlog and the dialog closed; the pages connect to
    /// the session, whose charm balances and death close come to the dialog.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// The monster page's loot item: the items page on its object type.
    void onSwitchToItemInfoDialogRequested(uint16_t AppearanceTypeID);

    void setAmbientLightStorage(worldmap::TAmbientLightStorage* ambientLight);
    /// The session's premium state for the item, charm and character pages.
    void setPremiumTimeStorage(game::TPremiumTimeStorage* storage);
    void setInstanceStorage(mods::InstanceBackend* instance);
    void setControlScheme(int controlScheme);

private:
    void hookActions();
    void switchTab(DialogTab tab);
    cip::tibia::gamewindow::ICyclopediaDialogTab* page(DialogTab tab) const;
    /// The context of `tab`'s page, which must have one.
    TCyclopediaDialogContext captureContext(DialogTab tab) const;
    /// The tab a context belongs to.
    static DialogTab tabOf(const TCyclopediaDialogContext& context);
    /// Hands `context` back to its tab's page.
    void restoreContext(const TCyclopediaDialogContext& context);
    /// Remembers a page's context for requestBack, unless it is the last one.
    void pushBacklog(const TCyclopediaDialogContext& context);
    /// Forgets every step back.
    void clearBacklog();

    static constexpr size_t MaxBacklog = 100;

    cyclopedia::TBestiaryStorage* m_bestiary;
    TItemInfoDialogController* m_itemInfo;
    TCharacterInfoDialogController* m_characterInfo;
    TMonsterDialogController* m_monsters;
    TMonsterBonusEffectsDialogController* m_bonusEffects;
    TBosstiaryDialogController* m_bosstiary;
    TBossSlotsDialogController* m_bossSlots;
    THousesInfoDialogController* m_houses;
    TCyclopediaMapDialogController* m_map;
    TMagicalArchiveDialogController* m_magicalArchive;
    std::deque<TCyclopediaDialogContext> m_backlog;
    std::shared_ptr<cyclopedia::TMonsterRaceStorage> m_monsterRaceStorage;
    std::vector<QMetaObject::Connection> m_sessionConnections;
    /// Hidden while a spell is assigned to an action button.
    bool m_hiddenForTargetSelection = false;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

/// The button bar's Cyclopedia button (CIP: TCyclopediaButtonController).
/// New bestiary details, a raid that started and an imminent raid highlight
/// it for ten minutes; a click opens the race's page or the map at the raid,
/// else the item page.
class TCyclopediaButtonController final : public cip::tibia::gamewindow::TCyclopediaButtonController
{
    Q_OBJECT
public:
    explicit TCyclopediaButtonController(QObject* parent = nullptr);

    /// While the highlight timer runs.
    bool highlighted() const override { return m_highlightTimer.isHighlighted(); }
    void onCyclopediaButtonClicked() override;
    void onHighlightCyclopediaButtonEnds() override;

    /// CIP connector: no highlight; the session's new bestiary details and
    /// map raids highlight the button.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    void onMonsterCyclopediaNewDetailsMessage(uint16_t NewDetailsMonsterRaceID);
    void onCyclopediaMapActiveRaidAdded(const worldmap::TWorldMapCoordinate& WorldMapCoordinate);
    void onCyclopediaMapImminentRaidsChanged(const worldmap::TWorldMapCoordinate& WorldMapCoordinate, float MapScaleFactor);

private:
    struct TImminentRaid
    {
        worldmap::TWorldMapCoordinate coordinate;
        float scale = 0;
    };
    /// What the next click opens: the item page, a race, a started raid, an
    /// imminent raid.
    using THighlightReason = std::variant<std::monostate, uint16_t, worldmap::TWorldMapCoordinate, TImminentRaid>;

    shared::THighlightTimer m_highlightTimer;
    THighlightReason m_highlightReason;
    std::vector<QMetaObject::Connection> m_sessionConnections;
};

/// The button bar's Bosstiary button (CIP: TBosstiaryButtonController). It is
/// highlighted while the server's last bosstiary highlight names a boss; a
/// click opens that boss's bosstiary entry.
class TBosstiaryButtonController final : public cip::tibia::gamewindow::TBosstiaryButtonController
{
    Q_OBJECT
public:
    explicit TBosstiaryButtonController(QObject* parent = nullptr);

    bool highlighted() const override { return m_highlightedBossRaceId != 0; }

    void onOpenBosstiaryButtonClicked() override;
    /// CIP connector: clears the highlight and listens to the session's
    /// highlight requests.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    void onHighlightRequested(uint16_t BossRaceID);

private:
    uint16_t m_highlightedBossRaceId = 0;
    QMetaObject::Connection m_highlightConnection;
};

}
}
