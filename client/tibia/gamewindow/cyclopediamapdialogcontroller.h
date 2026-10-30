#pragma once

#include "client/gamecore.h"
#include "input/gameactionmapper.h"

#include <cip_gen.h>
#include <client/position.h>

#include <QPointer>
#include <QTimer>

#include <optional>
#include <utility>
#include <vector>

namespace tibia::cyclopedia {
class TCyclopediaMapStorage;
}

namespace tibia::mods {
class InstanceBackend;
}

namespace tibia::qmlcomponents {
class TMinimapRendererQuickItem;
}

namespace tibia::worldmap {
class TAmbientLightStorage;
}

namespace tibia::gamewindow {

class TMessageDialogController;

/// The area values both selection kinds show (CIP 0x100d57aa0).
struct TCyclopediaMapAreaValues
{
    QString name;
    int id = 0;
    double percentExplored = 0;
    QString donation;
    QString threshold;
    QString donationsTooltip;
    double percentUnlockedImprovedRespawn = 0;
    bool rejectDonations = false;
};

/// The selected area (CIP: TCyclopediaMapSelectionInfoArea).
class TCyclopediaMapSelectionInfoArea final : public cip::tibia::gamewindow::TCyclopediaMapSelectionInfoArea
{
    Q_OBJECT
public:
    TCyclopediaMapSelectionInfoArea(const TCyclopediaMapAreaValues& values, QObject* parent);
};

/// The selected subarea and its area (CIP: TCyclopediaMapSelectionInfoSubArea).
class TCyclopediaMapSelectionInfoSubArea final : public cip::tibia::gamewindow::TCyclopediaMapSelectionInfoSubArea
{
    Q_OBJECT
public:
    struct Values
    {
        QString name;
        int id = 0;
        int viewpointsCount = 0;
        int viewpointsFound = 0;
        bool unlockable = false;
        bool unlocked = false;
        bool viewpointInfoDelayed = false;
    };

    TCyclopediaMapSelectionInfoSubArea(const TCyclopediaMapAreaValues& area, const Values& values, QObject* parent);
};

/// The Cyclopedia's Map tab (CIP: TCyclopediaMapDialogController,
/// static_metacall 0x100817d10): the selection, its info panel, the hover
/// tooltips and the clicks on MinimapRenderer.
class TCyclopediaMapDialogController final : public cip::tibia::gamewindow::TCyclopediaMapDialogController
{
    Q_OBJECT
public:
    explicit TCyclopediaMapDialogController(QObject* parent = nullptr);
    ~TCyclopediaMapDialogController() override;

    void setAmbientLightStorage(worldmap::TAmbientLightStorage* ambientLight);
    /// mod: the instance whose subarea the tab selects when it opens with
    /// nothing selected.
    void setInstanceStorage(mods::InstanceBackend* instance);
    void setControlScheme(int controlScheme) { m_mapper.setControlScheme(controlScheme); }
    /// CIP 0x100d51b70: a NaN scale keeps the current one.
    void centerAt(const Position& position, float scale);

    void showDialogTab() override;
    void hideDialogTab() override;
    /// CIP 0x100d4fcd0.
    void onSelectedItemChanged() override;
    /// CIP 0x100d4fab0.
    void setMinimapRendererQuickItem(QObject* pItem) override;
    /// CIP 0x100d511d0.
    void hoverAtPixelCoordinate(int x, int y) override;
    /// CIP 0x100d51600 / 0x100d516d0 (rule context 9).
    void onClickAtPixelCoordinate(int x, int y, int TriggeringMouseButton, Qt::KeyboardModifiers Modifiers) override;
    /// CIP 0x100d4fee0.
    void centerToPlayer() override;
    /// CIP 0x100d52080: asks to confirm, then 0x100d4d2b0 sends it.
    void donateMoneyForArea(int AreaID, int Value) override;
    /// CIP 0x100d50040 / 0x100d51db0.
    void requestResetViewpoints() override;
    void resetSubAreaViewPoints(int SubAreaID) override;
    /// CIP 0x100d51c80 → OPEN_CYCLOPEDIA monsters + race.
    void onCreatureClicked(int RaceIDRaw) override;
    /// CIP 0x100d531d0.
    void onMouseExited() override;

private:
    /// CIP confirm for reset (dialog result type 0 at +0x58).
    void confirmResetViewpoints(int subAreaId);
    /// CIP 0x100d4fda0 (TAreaData: area or subarea).
    void onAreaClicked(int id, bool isArea);
    /// CIP 0x100d52f90 / 0x100d52be0.
    void select(int id, bool isArea);
    /// CIP 0x100d57aa0: the info object of the selection.
    void rebuildSelection();
    TCyclopediaMapAreaValues areaValues(int areaId) const;
    /// CIP 0x100a79490: every donating area, most gold first.
    QString donationsTooltip() const;
    void updateTimeDisplay();
    void updatePlayerArea();
    void setTooltip(const QString& tooltip);

    cyclopedia::TCyclopediaMapStorage* m_storage;
    TMessageDialogController* m_confirmDialog;
    int m_donationAreaId = 0;
    int m_donationAmount = 0;
    int m_resetSubAreaId = 0;
    /// A center requested before the map's QML exists.
    std::optional<std::pair<Position, float>> m_pendingCenter;
    QPointer<qmlcomponents::TMinimapRendererQuickItem> m_renderer;
    QPointer<worldmap::TAmbientLightStorage> m_ambientLight;
    QPointer<mods::InstanceBackend> m_instance;
    input::TGameActionMapper m_mapper;
    int m_selectedId = 0;
    bool m_selectedIsArea = false;
    QTimer m_tooltipTimer;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
