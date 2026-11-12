#pragma once

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"
#include "qmlcomponents/outfittypes.h"

#include <cip_gen.h>

#include <client/outfit.h>
#include <client/position.h>

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace tibia {
namespace gamewindow {

/// Boss / bestiary skins for ConfigureCreaturePodiumDialog.qml (CIP:
/// TBossSkinModel behind TConfigureCreaturePodiumDialogController::bossSkinsModel).
class TBossSkinModel final : public cip::TBossSkinModel
{
    Q_OBJECT
public:
    enum ERole {
        OutfitIDRole = Qt::UserRole + 1,
        OutfitNameRole,
        FirstAddOnRole,
        SecondAddOnRole,
        OutfitTypeRole,
        OtherGenderOutfitIDRole,
        RaceRole,
        HeadColorRole,
        TorsoColorRole,
        LegsColorRole,
        DetailColorRole,
        IsObjectOutfitRole,
    };

    struct Entry
    {
        int raceId = 0;
        QString name;
        Outfit outfit;
    };

    using cip::TBossSkinModel::TBossSkinModel;

    void setEntries(QList<Entry> entries);
    const QList<Entry>& entries() const { return m_entries; }
    const Entry* entryAt(int row) const;

    int count() const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QList<Entry> m_entries;
};

/// Name filter for the podium skin grid (CIP: TBossSkinSortProxyModel).
class TBossSkinSortProxyModel final : public cip::TBossSkinSortProxyModel
{
    Q_OBJECT
public:
    explicit TBossSkinSortProxyModel(TBossSkinModel* source, QObject* parent = nullptr);

    QString outfitNameFilter() const override;
    void setOutfitNameFilter(const QString& v) override;
    void resetFilter() override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};

/// outfitdialog/ConfigureCreaturePodiumDialog.qml (CIP:
/// TConfigureCreaturePodiumDialogController). Opens on onConfigureCreaturePodium
/// (S 0xC2 / Crystal sendMonsterPodiumWindow; notify bus).
class TConfigureCreaturePodiumDialogController final
    : public TDialogController<cip::tibia::gamewindow::TConfigureCreaturePodiumDialogController>
{
    Q_OBJECT
public:
    using RaceRow = std::tuple<uint16_t, std::string, Outfit>;

    explicit TConfigureCreaturePodiumDialogController(QObject* parent = nullptr);

    /// Fills the skin list from the ConfigureCreaturePodium message and shows the dialog.
    void open(const Outfit& selected, bool bossPodium, const std::vector<RaceRow>& races, const Position& pos,
              uint16_t itemId, uint8_t stackpos, bool podiumVisible, bool monsterVisible, uint8_t direction);

public Q_SLOTS:
    void onIndexChanged(int SelectedIndex, int ChosenRace) override;
    void onCancelClicked() override;
    void onOkClicked() override;
    void onControllerFinished() override;

private:
    void applyEntry(const TBossSkinModel::Entry& entry);
    void clearSelection();

    TBossSkinModel m_skins;
    TBossSkinSortProxyModel m_proxy;
    qmlcomponents::TOutfitPresetQmlType* m_preset = nullptr;
    Position m_position;
    uint8_t m_stackpos = 0;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
