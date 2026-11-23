#pragma once

#include "client/gamecore.h"
#include "cyclopedia/bestiarystorage.h"
#include "gamewindow/cyclopediadialogcontext.h"

#include <cip_gen.h>
#include <client/staticdata.h>

#include <QList>
#include <QPointer>
#include <QString>

#include <memory>
#include <optional>
#include <vector>

namespace tibia {
namespace cyclopedia {
class TCreatureTrackerStorage;
class TMonsterRaceStorage;
}
namespace game {
class IGameSession;
}
namespace gamewindow {

class TMessageDialogController;

/// One bestiary race: a race summary tile, or the single monster page when
/// its details arrived. The properties are read from the race's data; the
/// details' parts unlock with the race's progress.
class TQmlMonsterRace final : public cip::tibia::gamewindow::TQmlMonsterRace
{
    Q_OBJECT
public:
    /// `charms` are the unlocked charms (none for a summary tile).
    TQmlMonsterRace(int raceId, int progress, double animusMasteryBonus, int animusMasteryPoints,
                    const BestiaryMonsterData* details, QList<cyclopedia::TCharm> charms, QObject* parent = nullptr);
    ~TQmlMonsterRace() override;

    QString name() const override;
    bool hasAppearanceInstance() const override;
    bool graphicEffectsSupported() const override;
    QString unlockState() const override;
    bool isUnknown() const override;
    bool hasDetails1() const override;
    bool isFullyUnlocked() const override;
    bool isAnimusMasteryUnlocked() const override;
    QString animusMasteryTooltip() const override;
    QString leaderKilledTooltip() const override;
    uint totalKills() const override;
    uint details1Kills() const override;
    uint details2Kills() const override;
    uint details3Kills() const override;
    bool isLeaderKilled() const override;
    int monsterDifficulty() const override;
    QString monsterDifficultyString() const override;
    int monsterRarity() const override;
    QString monsterRarityString() const override;
    bool isVeryRare() const override;
    bool isMeleeAttacker() const override;
    bool isRangedAttacker() const override;
    bool isSpellcaster() const override;
    bool doesNotAttack() const override;
    QString hitpointsString() const override;
    QString experienceString() const override;
    QString speedString() const override;
    QString armorString() const override;
    QString mitigationString() const override;
    QString bonusPointsOnUnlockString() const override;
    QString monsterLocations() const override;
    uint creatureOutfitId() const override;
    QColor creatureHeadColor() const override;
    QColor creatureTorsoColor() const override;
    QColor creatureLegsColor() const override;
    QColor creatureDetailColor() const override;
    bool creatureFirstAddOn() const override;
    bool creatureSecondAddOn() const override;
    QString unassignCostString() const override;

    void setSelectedCharmID(int v) override;
    void setIndexOfSelectedCharmInCombobox(int v) override;
    /// Also rebuilds the combo box.
    void setSelectedCharmSlot(int v) override;

    /// Whether the gold pays the selected charm's removal.
    void updateCanAfford();

private:
    void rebuildCombobox();
    /// The details' parts: difficulty and loot from the first stage, the
    /// stats from the second, sensitivities and locations from the third.
    const BestiaryMonsterData* difficultyData() const;
    const BestiaryMonsterData* statsData() const;
    const BestiaryMonsterData* sensitivityData() const;

    int m_progress = 0;
    double m_animusMasteryBonus = 0.0;
    int m_animusMasteryPoints = 0;
    std::optional<BestiaryMonsterData> m_details;
    QList<cyclopedia::TCharm> m_charms;
};

/// The Cyclopedia's bestiary page, MonstersDialog.qml (CIP:
/// TMonsterDialogController, metacall , vtable).
class TMonsterDialogController final : public cip::tibia::gamewindow::TMonsterDialogController
{
    Q_OBJECT
public:
    TMonsterDialogController(cyclopedia::TBestiaryStorage& storage, QObject* parent = nullptr);
    ~TMonsterDialogController() override;
    bool clientSettingSmoothFiltering() const override;

    /// The page left for another tab: the view, classification, search and race.
    TCyclopediaDialogContext captureContext() const;
    /// Shows the context's view again, asking the server for it; another
    /// tab's context throws.
    void restoreContext(const TCyclopediaDialogContext& CyclopediaContext);
    /// CIP connector: the session's monster race storage (the classifications
    /// and races) and creature tracker storage (whether the shown race is
    /// tracked and whether more can be).
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    bool isMonsterTracked() const override;
    bool canTrackMoreMonsters() const override;

    /// CIP.
    void showDialogTab() override;
    void hideDialogTab() override;
    /// CIP .
    Q_INVOKABLE void requestClassificationSelection(const QString& ClassificationID) override;
    /// CIP .
    Q_INVOKABLE void requestMonsterSearch(const QString& SearchString) override;
    /// CIP.
    Q_INVOKABLE void requestClassificationPageSwitch(int NewPage) override;
    Q_INVOKABLE void requestMonsterPageSwitch(int NewPage) override;
    /// CIP .
    Q_INVOKABLE void onMonsterRaceClicked(uint MonsterRaceID) override;
    /// CIP.
    Q_INVOKABLE void requestSwitchBackToClassificationSummary() override;
    Q_INVOKABLE void requestSwitchBackToRaceSummary() override;
    /// CIP .
    Q_INVOKABLE void requestSwitchToItemInfo(int AppearanceTypeID) override;
    /// Right-click loot tile in MonstersDialog.qml.
    void requestContextMenuForItem(int AppearanceTypeID) override;
    /// CIP .
    Q_INVOKABLE void requestToggleBestiaryTracking() override;
    /// CIP .
    Q_INVOKABLE void viewSelectedCharm(int charmID) override;
    /// CIP, answered by .
    Q_INVOKABLE void removeSelectedCharm() override;
    Q_INVOKABLE void assignSelectedCharm(int SelectedCharmID) override;
    /// Toggles the bestiary tracker widget.
    Q_INVOKABLE void requestOpenBestiaryTrackerWidget() override;

Q_SIGNALS:
    /// A loot item's object type to show on the items page.
    void itemInfoDialogRequested(uint16_t AppearanceTypeID);

private:
    using Args = std::vector<std::any>;

    void hookEvents();
    void setDialogState(DialogState state);
    /// The monster race storage's classification summary / races changed.
    void onMonsterClassificationSummaryChanged();
    void onMonsterRacesChanged(const QString& MonsterClassificationID);
    void onMonsterRaceDetailsChanged();
    void rebuildSingleMonster();
    void onConfirmDialogAccepted();
    /// The gold may have changed: whether the selected charm's removal is
    /// affordable.
    void onPlayerDataChanged();
    /// Whether the gold pays the shown monster's selected charm's removal.
    void onSelectedCharmChanged();
    TQmlMonsterRace* singleRace() const;

    cyclopedia::TBestiaryStorage& m_storage;
    std::shared_ptr<cyclopedia::TMonsterRaceStorage> m_monsterRaceStorage;
    std::shared_ptr<cyclopedia::TCreatureTrackerStorage> m_pCreatureTrackerStorage;
    std::vector<QMetaObject::Connection> m_sessionConnections;
    std::vector<client::gamecore::Hook> m_hooks;
    TMessageDialogController* m_confirmDialog;
    bool m_shown = false;
    /// Set by the destructor: the page refreshes run but build no QML objects.
    bool m_tearingDown = false;

    std::optional<BestiaryMonsterData> m_details;
    QString m_selectedClassification;
    QString m_search;
    int m_selectedRace = 0;

    /// The confirmation's action (CIP: the dialog's +0x58: 0 assign, 1 remove).
    int m_confirmAction = 0;
    int m_confirmCharm = 0;
};

}
}
