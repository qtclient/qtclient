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

namespace tibia::cyclopedia {
class TBestiaryStorage;
}

namespace tibia::mods {
class InstanceBackend;
}

namespace tibia::gamewindow {

class TCharacterInfoDialogController;
class TCyclopediaMapDialogController;
class TMagicalArchiveDialogController;

/// CyclopediaDialog.qml (CIP: TCyclopediaDialogController, showDialog
/// 0x100cf06e0, handleGameAction 0x100cf1260). The tab bar, the gold and
/// charm balances, and the back navigation between tabs. The character page
/// is ported; the other tabs are their CIP base classes until they are.
class TCyclopediaDialogController final : public TDialogController<cip::tibia::gamewindow::TCyclopediaDialogController>
{
    Q_OBJECT
public:
    explicit TCyclopediaDialogController(QObject* parent = nullptr);
    ~TCyclopediaDialogController() override;

    /// CIP 0x100cf1e90 / 0x100cf1f70 / 0x100cf2100 / 0x100cf2050.
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

    /// CIP 0x100cf06e0.
    void showDialog(const DialogTab& Tab) override;
    /// CIP 0x100cf0bc0.
    void captureNavigationBacklogContextIfNeedAndShowDialog(const DialogTab& NewTab) override;
    /// CIP 0x100cf10a0.
    void closeDialog() override;
    /// CIP 0x100cef9d0.
    void requestClose() override;
    /// CIP 0x100cf18c0.
    void requestTabSwitch(const DialogTab& NewTab) override;
    /// CIP 0x100cf1a50.
    void requestOpenManageQuickLootDialog() override;
    /// CIP 0x100cf1b40.
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
    /// CIP 0x100cf0980.
    void switchTab(DialogTab tab);
    cip::tibia::gamewindow::ICyclopediaDialogTab* page(DialogTab tab) const;
    Context captureContext() const;
    void pushBacklog();
    /// CIP 0x100cf1720.
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
/// ctor 0x100cf39f0, onCyclopediaButtonClicked 0x100cf3c90). A new bestiary
/// detail (0xD9, CIP 0x100cf40d0) highlights it for ten minutes; a click
/// opens that race's page.
class TCyclopediaButtonController final : public cip::tibia::gamewindow::TCyclopediaButtonController
{
    Q_OBJECT
public:
    explicit TCyclopediaButtonController(QObject* parent = nullptr);

    /// CIP 0x100cf3c80: the highlight timer runs.
    bool highlighted() const override { return m_highlightTimer.isActive(); }
    void onCyclopediaButtonClicked() override;
    /// CIP 0x100cf3bc0.
    void onHighlightCyclopediaButtonEnds() override;

private:
    void stopHighlight();

    QTimer m_highlightTimer;
    /// CIP +0x38: the page the next click opens (0 = none).
    int m_newDetailsRaceId = 0;
    std::vector<client::gamecore::Hook> m_hooks;
};

/// The button bar's Bosstiary button (CIP: TBosstiaryButtonController,
/// onOpenBosstiaryButtonClicked 0x100cf4a70).
class TBosstiaryButtonController final : public cip::tibia::gamewindow::TBosstiaryButtonController
{
    Q_OBJECT
public:
    using cip::tibia::gamewindow::TBosstiaryButtonController::TBosstiaryButtonController;

    void onOpenBosstiaryButtonClicked() override;
};

}
