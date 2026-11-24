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

#include <memory>

class QQmlEngine;
class QQuickItem;

namespace tibia::appearances {
class TAppearanceInstanceFactory;
}
namespace tibia::config {
class TClientOptions;
}
namespace tibia::game {
class IGameSession;
class TOutfitDialogStorage;
struct TConfigureCreaturePodiumData;
}

namespace tibia {
namespace gamewindow {

/// Boss / bestiary skins for ConfigureCreaturePodiumDialog.qml (CIP:
/// TBossSkinModel behind TConfigureCreaturePodiumDialogController::bossSkinsModel).
class TBossSkinModel final : public cip::TBossSkinModel
{
    Q_OBJECT
public:
    /// CIP's role ids (0x101..0x10b).
    enum ERole {
        OutfitIDRole = Qt::UserRole + 1,
        OutfitNameRole,
        OutfitTypeRole,
        FirstAddOnRole,
        SecondAddOnRole,
        HeadColorRole,
        TorsoColorRole,
        LegsColorRole,
        DetailColorRole,
        IsObjectOutfitRole,
        RaceRole,
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
/// TConfigureCreaturePodiumDialogController). Opens when the session's player
/// protocol handler asks for it, on the podium data in the session's outfit
/// dialog storage.
class TConfigureCreaturePodiumDialogController final
    : public TDialogController<cip::tibia::gamewindow::TConfigureCreaturePodiumDialogController>
{
    Q_OBJECT
public:
    using RaceRow = std::tuple<uint16_t, std::string, Outfit>;

    /// The dialog is made with `Engine` under `DialogParentItem`.
    TConfigureCreaturePodiumDialogController(std::shared_ptr<appearances::TAppearanceInstanceFactory> AppearanceInstanceFactory,
                                             config::TClientOptions* ClientOptions, QQmlEngine* Engine,
                                             QQuickItem* DialogParentItem, QObject* parent = nullptr);

    /// CIP: forgets the previous session's connections, keeps the session and
    /// its outfit dialog storage, and opens on the player protocol handler's
    /// showConfigureCreaturePodiumDialog.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// CIP: fills the skin list from the outfit dialog storage's podium data
    /// and shows the dialog.
    void open();

    /// The look of the chosen race's skin, 0 without one.
    int chosenOutfit() const override;
    QVariantMap initialDialogProperties() const override;

public Q_SLOTS:
    void onIndexChanged(int SelectedIndex, int ChosenRace) override;
    void onCancelClicked() override;
    void onOkClicked() override;
    void onControllerFinished() override;

private:
    /// The preview shows `outfit` (selectedBossOutfitChanged).
    void setPreset(const Outfit& outfit);

    std::shared_ptr<appearances::TAppearanceInstanceFactory> m_pAppearanceInstanceFactory;
    /// clientSettingSmoothFiltering reads its antialiasing mode.
    config::TClientOptions* m_pClientOptions;
    TBossSkinModel m_skins;
    TBossSkinSortProxyModel m_proxy;
    qmlcomponents::TOutfitPresetQmlType* m_preset = nullptr;
    Outfit m_selected;
    Position m_position;
    uint8_t m_stackpos = 0;
    std::vector<QMetaObject::Connection> m_sessionConnections;
    std::shared_ptr<game::IGameSession> m_pGameSession;
    std::shared_ptr<game::TOutfitDialogStorage> m_pOutfitDialogStorage;
    /// The podium data open() last read.
    std::shared_ptr<const game::TConfigureCreaturePodiumData> m_pData;
};

}
}
