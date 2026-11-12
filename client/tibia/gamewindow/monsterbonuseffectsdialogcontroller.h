#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <QString>

#include <optional>
#include <vector>

namespace tibia {
namespace cyclopedia {
class TBestiaryStorage;
}
}

namespace tibia {
namespace gamewindow {

class TMessageDialogController;

/// The Cyclopedia's charm page, MonsterBonusEffectsDialog.qml (CIP:
/// TMonsterBonusEffectsDialogController, metacall).
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
    /// CIP .
    void restoreContext(const Context& context);
    /// CIP : shows the charm's tab and selects it once the data arrives.
    void selectCharm(int charmId);

    /// CIP : the index of the selected charm in the list, 0 if absent.
    int selectedBonusEffectId() const override;
    void setSelectedBonusEffectId(int v) override { setSelectedBonusEffectFromIndex(v); }
    void setTemporarySelectedMonsterRaceID(int v) override;
    void setUnassignedMonstersNameFilter(const QString& v) override;

    void showDialogTab() override;
    void hideDialogTab() override;
    /// CIP /, answered by .
    Q_INVOKABLE void unlockBonusEffect(int BonusEffectID) override;
    Q_INVOKABLE void clearBonusEffect(int BonusEffectID) override;
    Q_INVOKABLE void assignBonusEffectToMonster(int BonusEffectID, int monsterRaceID) override;
    Q_INVOKABLE QString findMonsterNameByRaceID(int RaceID) override;
    /// CIP .
    Q_INVOKABLE void onStoreButtonClicked() override;
    /// CIP .
    Q_INVOKABLE void onResetAllCharmsClicked() override;
    /// CIP .
    Q_INVOKABLE void onActiveTabChanged(int activeTabId) override;
    Q_INVOKABLE bool isMonsterOnlyAssignableForMinorCharms(int RaceID) override;
    Q_INVOKABLE bool isMajorCharmTabActive() override { return m_activeTab == 0; }
    Q_INVOKABLE bool isMinorCharmTabActive() override { return m_activeTab == 1; }
    /// CIP .
    Q_INVOKABLE void setSelectedBonusEffectFromIndex(int BonusEffectIndex) override;

private:
    using Args = std::vector<std::any>;

    /// CIP : the charm list, the races and the pending selection.
    void refresh();
    /// CIP .
    void rebuildBonusEffects();
    /// CIP .
    void rebuildMonsterInfos();
    void updatePlayerData();
    /// CIP .
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
}
