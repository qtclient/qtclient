#pragma once

#include "gamewindow/cyclopediadialogcontext.h"

#include <cip_gen.h>

#include <QPointer>
#include <QString>

#include <QList>
#include <QMetaObject>

#include <memory>
#include <optional>
#include <vector>

namespace tibia {
namespace cyclopedia {
struct TCharm;
class TMonsterBonusEffectStorage;
class TMonsterRaceStorage;
}
}

namespace tibia {
namespace game {
class IGameSession;
class TPremiumTimeStorage;
}
namespace gamewindow {

class TMessageDialogController;

/// The Cyclopedia's charm page, MonsterBonusEffectsDialog.qml (CIP:
/// TMonsterBonusEffectsDialogController, metacall).
class TMonsterBonusEffectsDialogController final : public cip::tibia::gamewindow::TMonsterBonusEffectsDialogController
{
    Q_OBJECT
public:
    explicit TMonsterBonusEffectsDialogController(QObject* parent = nullptr);
    ~TMonsterBonusEffectsDialogController() override;

    /// The session's charm and monster race storages the page reads.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    /// The session's premium state: its change changes the store button's tooltip.
    void setPremiumTimeStorage(game::TPremiumTimeStorage* storage);
    /// The page left for another tab: the charm and the monster picked for it.
    TCyclopediaDialogContext captureContext() const;
    /// Another tab's context throws.
    void restoreContext(const TCyclopediaDialogContext& CyclopediaContext);
    /// CIP : shows the charm's tab and selects it once the data arrives.
    void selectCharm(int charmId);

    /// CIP : the index of the selected charm in the list, 0 if absent.
    int selectedBonusEffectId() const override;
    void setSelectedBonusEffectId(int v) override { setSelectedBonusEffectFromIndex(v); }
    void setTemporarySelectedMonsterRaceID(int v) override;
    void setUnassignedMonstersNameFilter(const QString& v) override;
    /// Without a session: none.
    bool canAssignMoreBonusEffects() const override;
    /// The charm slots left to assign (0xFF: no limit).
    int remainingCharms() const override;
    /// The remaining charm slots and the premium hint; none with unlimited slots.
    QString storeButtonTooltip() const override;
    /// No tooltip with unlimited charm slots.
    bool showStoreButtonTooltip() const override;
    /// The storage's reset cost, "0" without a session.
    QString resetCostString() const override;
    bool notEnoughGoldReset() const override;
    /// Some charm is unlocked.
    bool enableResetButton() const override;

    void showDialogTab() override;
    /// Forgets the selected charm.
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
    bool isPremium() const;
    QPointer<game::TPremiumTimeStorage> m_premiumTimeStorage;

    /// The session's charms in id order, empty without a session.
    QList<cyclopedia::TCharm> charms() const;
    /// The session's charm of `charmId`, or none.
    std::optional<cyclopedia::TCharm> charm(int charmId) const;

    /// The charm list, the races and the pending selection.
    void onMonsterBonusEffectsChangedFromStorage();
    void onMonsterBonusEffectsChanged();
    void rebuildMonsterInfos();
    /// The player's gold: a change rebuilds the charm list (its gold flags).
    void onPlayerDataChanged();
    void onRemainingNumberOfAssignableBonusEffectsChanged();
    void onConfirmDialogAccepted();

    std::shared_ptr<cyclopedia::TMonsterBonusEffectStorage> m_bonusEffectStorage;
    std::shared_ptr<cyclopedia::TMonsterRaceStorage> m_monsterRaceStorage;
    std::vector<QMetaObject::Connection> m_sessionConnections;
    TMessageDialogController* m_confirmDialog;
    /// The player's gold the charm list was built with (CIP +0xd8).
    quint64 m_goldBalance = 0;
    /// The charm id behind selectedBonusEffectId (CIP +0xb2).
    int m_selectedCharm = -1;
    /// CIP +0xe4 / +0xe5.
    std::optional<int> m_pendingCharm;

    /// The confirmation's charm action (CIP: 0 unlock, 1 assign, 2 remove, 3 reset all).
    int m_confirmAction = 0;
    int m_confirmCharm = 0;
    int m_confirmRace = 0;
};

}
}
