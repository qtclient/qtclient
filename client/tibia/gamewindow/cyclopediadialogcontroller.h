#pragma once

#include "client/gamecore.h"
#include "gamewindow/bosstiarydialogcontroller.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/housesinfodialogcontroller.h"
#include "gamewindow/iteminfodialogcontroller.h"
#include "gamewindow/monsterbonuseffectsdialogcontroller.h"
#include "gamewindow/monsterdialogcontroller.h"

#include <cip_gen.h>

#include "worldmap/coordinates.h"

#include <QTimer>

#include <deque>
#include <variant>
#include <vector>

namespace tibia {
namespace cyclopedia {
class TBestiaryStorage;
}
}

namespace tibia {
namespace mods {
class InstanceBackend;
}
}

namespace tibia {
namespace gamewindow {

class TCharacterInfoDialogController;
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

    void setAmbientLightStorage(worldmap::TAmbientLightStorage* ambientLight);
    void setInstanceStorage(mods::InstanceBackend* instance);
    void setControlScheme(int controlScheme);

private:
    /// A tab left for another (CIP: the backlog entries, each tab's context).
    struct Context
    {
        DialogTab tab = NoTabInitialState;
        int characterInfoDataType = 0;
        TItemInfoDialogController::Context itemInfo;
        TMonsterDialogController::Context monsters;
        TMonsterBonusEffectsDialogController::Context bonusEffects;
        TBosstiaryDialogController::Context bosstiary;
        THousesInfoDialogController::Context houses;
        bool operator==(const Context&) const = default;
    };

    void hookActions();
    void switchTab(DialogTab tab);
    cip::tibia::gamewindow::ICyclopediaDialogTab* page(DialogTab tab) const;
    Context captureContext() const;
    /// Remembers the shown page's context for requestBack.
    void pushBacklog();
    /// Forgets every step back.
    void clearBacklog();

    static constexpr size_t MaxBacklog = 100;

    cyclopedia::TBestiaryStorage* m_bestiary;
    TItemInfoDialogController* m_itemInfo;
    TCharacterInfoDialogController* m_characterInfo;
    TMonsterDialogController* m_monsters;
    TMonsterBonusEffectsDialogController* m_bonusEffects;
    cyclopedia::TBosstiaryStorage* m_bosstiaryStorage;
    TBosstiaryDialogController* m_bosstiary;
    TBossSlotsDialogController* m_bossSlots;
    THousesInfoDialogController* m_houses;
    TCyclopediaMapDialogController* m_map;
    TMagicalArchiveDialogController* m_magicalArchive;
    std::deque<Context> m_backlog;
    std::vector<client::gamecore::Hook> m_hooks;
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
    bool highlighted() const override { return m_highlightTimer.isActive(); }
    void onCyclopediaButtonClicked() override;
    void onHighlightCyclopediaButtonEnds() override;

    /// A new game session: no highlight.
    void connectToGameSession();
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

    void startHighlight();
    void stopHighlight();

    QTimer m_highlightTimer;
    THighlightReason m_highlightReason;
    std::vector<client::gamecore::Hook> m_hooks;
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
    /// Clears the highlight and listens to the new session's highlight requests.
    void connectToGameSession();
    void onHighlightRequested(uint16_t BossRaceID);

private:
    uint16_t m_highlightedBossRaceId = 0;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
