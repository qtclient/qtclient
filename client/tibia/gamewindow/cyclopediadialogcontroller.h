#pragma once

#include "client/gamecore.h"
#include "gamewindow/bosstiarydialogcontroller.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/housesinfodialogcontroller.h"
#include "gamewindow/iteminfodialogcontroller.h"
#include "gamewindow/monsterbonuseffectsdialogcontroller.h"
#include "gamewindow/monsterdialogcontroller.h"

#include <cip_gen.h>

#include <QTimer>

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

    /// CIP / /.
    QString bankGoldBalanceString() const override;
    QString inventoryGoldBalanceString() const override;
    qulonglong totalGoldBalance() const override;
    QString totalGoldBalanceString() const override;
    /// CIP: the charm storage's +0x28 / +0x2c / +0x30 / +0x34.
    QString availableCharmPointsString() const override;
    QString unlockedCharmPointsString() const override;
    QString availableMinorCharmEchoesString() const override;
    QString unlockedMinorCharmEchoesString() const override;
    bool backPossible() const override { return !m_backlog.empty(); }

    /// CIP .
    void showDialog(const DialogTab& Tab) override;
    /// CIP .
    void captureNavigationBacklogContextIfNeedAndShowDialog(const DialogTab& NewTab) override;
    /// CIP .
    void closeDialog() override;
    /// CIP .
    void requestClose() override;
    /// CIP .
    void requestTabSwitch(const DialogTab& NewTab) override;
    /// CIP .
    void requestOpenManageLootContainerDialog() override;
    /// CIP .
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
    /// CIP .
    void switchTab(DialogTab tab);
    cip::tibia::gamewindow::ICyclopediaDialogTab* page(DialogTab tab) const;
    Context captureContext() const;
    void pushBacklog();
    /// CIP .
    void clearBacklog();
    void hideCurrentPage();

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
    std::vector<Context> m_backlog;
    std::vector<client::gamecore::Hook> m_hooks;
};

/// The button bar's Cyclopedia button (CIP: TCyclopediaButtonController,
/// ctor , onCyclopediaButtonClicked). A new bestiary
/// detail (0xD9, CIP) highlights it for ten minutes; a click
/// opens that race's page.
class TCyclopediaButtonController final : public cip::tibia::gamewindow::TCyclopediaButtonController
{
    Q_OBJECT
public:
    explicit TCyclopediaButtonController(QObject* parent = nullptr);

    /// CIP : the highlight timer runs.
    bool highlighted() const override { return m_highlightTimer.isActive(); }
    void onCyclopediaButtonClicked() override;
    /// CIP .
    void onHighlightCyclopediaButtonEnds() override;

private:
    void stopHighlight();

    QTimer m_highlightTimer;
    /// CIP +0x38: the page the next click opens (0 = none).
    int m_newDetailsRaceId = 0;
    std::vector<client::gamecore::Hook> m_hooks;
};

/// The button bar's Bosstiary button (CIP: TBosstiaryButtonController,
/// onOpenBosstiaryButtonClicked).
class TBosstiaryButtonController final : public cip::tibia::gamewindow::TBosstiaryButtonController
{
    Q_OBJECT
public:
    using cip::tibia::gamewindow::TBosstiaryButtonController::TBosstiaryButtonController;

    void onOpenBosstiaryButtonClicked() override;
};

}
}
