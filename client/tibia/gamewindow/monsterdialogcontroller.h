#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>
#include <client/staticdata.h>

#include <QList>
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

/// One bestiary race (CIP: TQmlMonsterRace, ctor , metacall
/// ): a race summary tile, or the single monster page when its
/// details arrived.
class TQmlMonsterRace final : public cip::tibia::gamewindow::TQmlMonsterRace
{
    Q_OBJECT
public:
    TQmlMonsterRace(int raceId, int progress, double animusMasteryBonus, int animusMasteryPoints,
                    const BestiaryMonsterData* details, const cyclopedia::TBestiaryStorage& storage,
                    QObject* parent = nullptr);

    /// CIP (+ the controller's onSelectedCharmChanged).
    void setSelectedCharmID(int v) override;
    /// CIP .
    void setIndexOfSelectedCharmInCombobox(int v) override;
    /// CIP : also rebuilds the combo box .
    void setSelectedCharmSlot(int v) override;

    /// CIP : whether the gold pays the selected charm's removal.
    void updateCanAfford();

private:
    void rebuildCombobox();
    void updateUnassignCost();

    const cyclopedia::TBestiaryStorage& m_storage;
    int m_progress = 0;
};

/// The Cyclopedia's bestiary page, MonstersDialog.qml (CIP:
/// TMonsterDialogController, metacall , vtable).
class TMonsterDialogController final : public cip::tibia::gamewindow::TMonsterDialogController
{
    Q_OBJECT
public:
    /// The page left for another tab (CIP).
    struct Context
    {
        DialogState state = MonsterClassification;
        QString classification;
        QString search;
        int raceId = 0;
        bool operator==(const Context&) const = default;
    };

    TMonsterDialogController(cyclopedia::TBestiaryStorage& storage, QObject* parent = nullptr);
    ~TMonsterDialogController() override;

    Context captureContext() const;
    void restoreContext(const Context& context) { m_pendingContext = context; }
    /// The race the next showDialogTab opens (CIP).
    void setNextRace(int raceId);

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

private:
    using Args = std::vector<std::any>;

    void hookEvents();
    void setDialogState(DialogState state);
    /// CIP /.
    void onClassificationSummaryChanged();
    void onMonsterRacesChanged();
    void onMonsterRaceDetailsChanged();
    void rebuildSingleMonster();
    void onConfirmDialogAccepted();
    TQmlMonsterRace* singleRace() const;

    cyclopedia::TBestiaryStorage& m_storage;
    std::vector<client::gamecore::Hook> m_hooks;
    TMessageDialogController* m_confirmDialog;
    bool m_shown = false;

    std::vector<CyclopediaBestiaryRace> m_classifications;
    std::vector<BestiaryOverviewMonsters> m_races;
    std::optional<BestiaryMonsterData> m_details;
    int m_animusMasteryPoints = 0;
    QString m_selectedClassification;
    QString m_search;
    int m_selectedRace = 0;
    std::optional<Context> m_pendingContext;

    /// The confirmation's action (CIP: the dialog's +0x58: 0 assign, 1 remove).
    int m_confirmAction = 0;
    int m_confirmCharm = 0;
};

}
}
