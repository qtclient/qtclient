#pragma once

#include "input/gameactiondispatch.h"
#include "client/gamecore.h"
#include "input/gameactionmapper.h"

#include <cip_gen.h>
#include <client/position.h>

#include <QPointer>
#include <QTimer>

#include <optional>
#include <utility>
#include <vector>

namespace tibia {
namespace cyclopedia {
class TCyclopediaMapStorage;
class TCyclopediaMapCharacterConfigurationStorage;
}
}

namespace tibia {
namespace mods {
class InstanceBackend;
}
}

namespace tibia {
namespace qmlcomponents {
class TMinimapRendererQuickItem;
}
}

namespace tibia {
namespace worldmap {
class TAmbientLightStorage;
}
}

namespace tibia {
namespace gamewindow {

class TMessageDialogController;

/// What the map knows of an area (CIP cyclopedia::TAreaData, the fields the
/// selection shows).
struct TCyclopediaMapAreaValues
{
    quint16 id = 0;
    QString name;
    float percentExplored = 0;
    quint64 donated = 0;
    QString donationsTooltip;
    quint64 donationGoal = 0;
    bool rejectDonations = false;
};

/// The selected area: its data, and the texts and ratios the info panel shows.
class TCyclopediaMapSelectionInfoArea final : public cip::tibia::gamewindow::TCyclopediaMapSelectionInfoArea
{
    Q_OBJECT
public:
    TCyclopediaMapSelectionInfoArea(const TCyclopediaMapAreaValues& values, QObject* parent);

    const TCyclopediaMapAreaValues& areaData() const { return m_data; }
    void setAreaData(const TCyclopediaMapAreaValues& values);

    cip::tibia::qmlcomponents::enums::ECyclopediaMapSelectionType selectionType() const override
    {
        return cip::tibia::qmlcomponents::enums::ECyclopediaMapSelectionType::SelectionTypeArea;
    }
    QString areaName() const override { return m_data.name; }
    int areaID() const override { return m_data.id; }
    double percentExplored() const override { return m_data.percentExplored; }
    /// The gold donated, English digit grouping.
    QString areaDonationString() const override;
    /// The gold the improved respawn needs, English digit grouping.
    QString areaDonationThreshold() const override;
    QString areaDonationsTooltip() const override { return m_data.donationsTooltip; }
    /// Donated over the goal, at most 1; 0 without a goal.
    double percentUnlockedImprovedRespawn() const override;
    bool rejectDonations() const override { return m_data.rejectDonations; }

private:
    TCyclopediaMapAreaValues m_data;
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
/// static_metacall): the selection, its info panel, the hover
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
    /// CIP : a NaN scale keeps the current one.
    void centerAt(const Position& position, float scale);

    void showDialogTab() override;
    void hideDialogTab() override;
    /// The panel states are the character's cyclopedia map configuration.
    void setShowNavigationPanel(bool v) override;
    void setShowAreaAndSubAreaInfoPanel(bool v) override;
    cyclopedia::TCyclopediaMapCharacterConfigurationStorage* characterConfiguration() const { return m_configuration; }
    /// A loaded configuration sets the panels.
    void applyCharacterConfiguration();
    /// CIP .
    void onSelectedItemChanged() override;
    /// CIP .
    void setMinimapRendererQuickItem(QObject* pItem) override;
    /// CIP .
    void hoverAtPixelCoordinate(int x, int y) override;
    /// CIP / (rule context 9).
    void onClickAtPixelCoordinate(int x, int y, int TriggeringMouseButton, Qt::KeyboardModifiers Modifiers) override;
    /// CIP .
    void centerToPlayer() override;
    /// CIP : asks to confirm, then sends it.
    void donateMoneyForArea(int AreaID, int Value) override;
    /// CIP.
    void requestResetViewpoints() override;
    void resetSubAreaViewPoints(int SubAreaID) override;
    /// CIP → OPEN_CYCLOPEDIA monsters + race.
    void onCreatureClicked(int RaceIDRaw) override;
    /// CIP .
    void onMouseExited() override;

private:
    /// CIP confirm for reset (dialog result type 0 at +0x58).
    void confirmResetViewpoints(int subAreaId);
    /// CIP: sends the confirmed viewpoint reset or donation.
    void onConfirmDialogAccepted();
    /// CIP (TAreaData: area or subarea).
    void onAreaClicked(int id, bool isArea);
    /// CIP.
    void select(int id, bool isArea);
    /// CIP : the info object of the selection.
    void rebuildSelection();
    TCyclopediaMapAreaValues areaValues(int areaId) const;
    /// CIP : every donating area, most gold first.
    QString donationsTooltip() const;
    void updateTimeDisplay();
    void updatePlayerArea();
    void setTooltip(const QString& tooltip);
    void updateMouseCursor(int x, int y);

    cyclopedia::TCyclopediaMapCharacterConfigurationStorage* m_configuration;
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

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
