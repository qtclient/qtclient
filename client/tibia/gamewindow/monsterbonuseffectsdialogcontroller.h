#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <QString>

#include <optional>
#include <vector>

namespace tibia::cyclopedia {
class TBestiaryStorage;
}

namespace tibia::gamewindow {

class TMessageDialogController;

/// The Cyclopedia's charm page, MonsterBonusEffectsDialog.qml (CIP:
/// TMonsterBonusEffectsDialogController, metacall 0x10082ea70).
class TMonsterBonusEffectsDialogController final : public cip::tibia::gamewindow::TMonsterBonusEffectsDialogController
{
    Q_OBJECT
public:
    /// The page left for another tab (CIP context type 2: the charm, the tab).
    struct Context
    {
        int charmId = 0;
        int activeTab = 0;
        bool operator==(const Context&) const = default;
    };

    TMonsterBonusEffectsDialogController(cyclopedia::TBestiaryStorage& storage, QObject* parent = nullptr);
    ~TMonsterBonusEffectsDialogController() override;

    Context captureContext() const;
    /// CIP 0x100cffc30.
    void restoreContext(const Context& context);
    /// CIP 0x100cffa70: shows the charm's tab and selects it once the data arrives.
    void selectCharm(int charmId);

    /// CIP 0x100d01850: the index of the selected charm in the list, 0 if absent.
    int selectedBonusEffectId() const override;
    void setSelectedBonusEffectId(int v) override { setSelectedBonusEffectFromIndex(v); }
    void setTemporarySelectedMonsterRaceID(int v) override;
    void setUnassignedMonstersNameFilter(const QString& v) override;

    void showDialogTab() override;
    void hideDialogTab() override;
    /// CIP 0x100cffce0 / 0x100d00290 / 0x100d006e0, answered by 0x100cfde50.
    Q_INVOKABLE void unlockBonusEffect(int BonusEffectID) override;
    Q_INVOKABLE void clearBonusEffect(int BonusEffectID) override;
    Q_INVOKABLE void assignBonusEffectToMonster(int BonusEffectID, int monsterRaceID) override;
    Q_INVOKABLE QString findMonsterNameByRaceID(int RaceID) override;
    /// CIP 0x100d00ae0.
    Q_INVOKABLE void onStoreButtonClicked() override;
    /// CIP 0x100d00e50.
    Q_INVOKABLE void onResetAllCharmsClicked() override;
    /// CIP 0x100cffae0.
    Q_INVOKABLE void onActiveTabChanged(int activeTabId) override;
    Q_INVOKABLE bool isMonsterOnlyAssignableForMinorCharms(int RaceID) override;
    Q_INVOKABLE bool isMajorCharmTabActive() override { return m_activeTab == 0; }
    Q_INVOKABLE bool isMinorCharmTabActive() override { return m_activeTab == 1; }
    /// CIP 0x100d018e0.
    Q_INVOKABLE void setSelectedBonusEffectFromIndex(int BonusEffectIndex) override;

private:
    using Args = std::vector<std::any>;

    /// CIP 0x100cff040: the charm list, the races and the pending selection.
    void refresh();
    /// CIP 0x100cfe130.
    void rebuildBonusEffects();
    /// CIP 0x100d01bd0.
    void rebuildMonsterInfos();
    void updatePlayerData();
    /// CIP 0x100d01300.
    void updateStoreTooltip();
    void onConfirmDialogAccepted();

    cyclopedia::TBestiaryStorage& m_storage;
    std::vector<client::gamecore::Hook> m_hooks;
    TMessageDialogController* m_confirmDialog;
    bool m_shown = false;
    /// The charm id behind selectedBonusEffectId (CIP +0xb2).
    int m_selectedCharm = -1;
    /// CIP +0xe4 / +0xe5.
    std::optional<int> m_pendingCharm;

    /// The confirmation's charm action (Crystal: 0 unlock, 1 assign, 2 remove, 3 reset all).
    int m_confirmAction = 0;
    int m_confirmCharm = 0;
    int m_confirmRace = 0;
};

}
