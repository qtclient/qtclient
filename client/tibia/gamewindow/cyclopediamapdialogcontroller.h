#pragma once

#include "cyclopedia/cyclopediamapgameactionprovider.h"
#include "client/gamecore.h"
#include "gamewindow/cyclopediadialogcontext.h"
#include "gamewindow/gamewindowciptypes.h"
#include "input/gameactionmapper.h"

#include <cip_gen.h>
#include <client/position.h>

#include <QPointer>
#include <QTimer>

#include <map>
#include <memory>
#include <variant>
#include <vector>

namespace tibia {
namespace game {
class IGameSession;
}
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
    /// What the selection knows of a subarea (CIP: TSubAreaData).
    struct Values
    {
        quint16 id = 0;
        QString name;
        TCyclopediaMapAreaValues area;
        int viewpointsCount = 0;
        int viewpointsFound = 0;
        bool unlockable = false;
        bool unlocked = false;
        bool viewpointInfoDelayed = false;
        TCyclopediaCreatureOccurrencesData creatureOccurrences;
    };

    TCyclopediaMapSelectionInfoSubArea(const Values& values, QObject* parent);

    /// The subarea's data as last set.
    Values subAreaData() const { return m_data; }

    cip::tibia::qmlcomponents::enums::ECyclopediaMapSelectionType selectionType() const override
    {
        return cip::tibia::qmlcomponents::enums::ECyclopediaMapSelectionType::SelectionTypeSubArea;
    }
    /// Takes the subarea's data and its area's, then makes a new creature
    /// occurrences object (the old one is deleted later).
    void setSubAreaData(const Values& values);

private:
    Values m_data;
};

/// The selected passage (CIP: TCyclopediaMapSelectionInfoPassage): the creatures
/// of every subarea the passage leads to.
class TCyclopediaMapSelectionInfoPassage final : public cip::tibia::gamewindow::TCyclopediaMapSelectionInfoPassage
{
    Q_OBJECT
public:
    /// What the selection knows of a passage (CIP: TPassageData): its field and
    /// the subareas it leads to.
    struct Values
    {
        Position coordinate{ 0, 0, 0 };
        std::vector<quint16> subAreaIds;
        /// The creatures of each subarea the passage leads to.
        std::map<quint16, TCyclopediaCreatureOccurrencesData> creatureOccurrences;
    };

    TCyclopediaMapSelectionInfoPassage(const Values& values, QObject* parent);

    cip::tibia::qmlcomponents::enums::ECyclopediaMapSelectionType selectionType() const override
    {
        return cip::tibia::qmlcomponents::enums::ECyclopediaMapSelectionType::SelectionTypePassage;
    }
    /// Takes the passage's data and makes one creature occurrences object per
    /// subarea; the old ones are deleted later.
    void setPassageData(const Values& values);

private:
    Values m_data;
};

/// The Cyclopedia's Map tab (CIP: TCyclopediaMapDialogController): the selection
/// (an area, a subarea or a passage, kept in the character's map configuration),
/// its info panel, the hover tooltips and cursor, and the clicks on the map.
class TCyclopediaMapDialogController final : public cip::tibia::gamewindow::TCyclopediaMapDialogController
{
    Q_OBJECT
public:
    explicit TCyclopediaMapDialogController(QObject* parent = nullptr);
    ~TCyclopediaMapDialogController() override;

    /// CIP connector: the session's map storage and the character's map
    /// configuration; nothing is selected.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    void setAmbientLightStorage(worldmap::TAmbientLightStorage* ambientLight);
    /// mod: the instance whose subarea the tab selects when it opens with
    /// nothing selected.
    void setInstanceStorage(mods::InstanceBackend* instance);
    void setControlScheme(int controlScheme) { m_mapper.setControlScheme(controlScheme); }
    /// The session's map storage the Cyclopedia protocol handler fills.
    cyclopedia::TCyclopediaMapStorage* storage() const { return m_storage.get(); }
    cyclopedia::TCyclopediaMapCharacterConfigurationStorage* characterConfiguration() const { return m_configuration.get(); }
    /// Centers the map on a field; a NaN scale keeps the current one. Nothing
    /// without the map's QML item.
    void centerAtWorldMapCoordinate(const Position& position, float scale);
    /// The page left for another tab: the camera's top-left and zoom.
    TCyclopediaDialogContext captureContext() const;
    /// The camera back where it was (on the own character for a context taken
    /// without a camera); another tab's context throws.
    void restoreContext(const TCyclopediaDialogContext& CyclopediaContext);

    /// Restores the selection the character's configuration holds (also when
    /// the configuration is loaded).
    void showDialogTab() override;
    void hideDialogTab() override;

    /// The panel states are the character's cyclopedia map configuration
    /// (shown without a game).
    bool showMapMarkerPanel() const override;
    void setShowMapMarkerPanel(bool v) override;
    bool showNavigationPanel() const override;
    void setShowNavigationPanel(bool v) override;
    bool showAreaAndSubAreaInfoPanel() const override;
    void setShowAreaAndSubAreaInfoPanel(bool v) override;
    /// Bank and carried gold.
    qlonglong totalGoldBalance() const override;
    int currentPlayerArea() const override;
    /// Empty without a current area.
    QString currentPlayerAreaName() const override;
    int currentExploringSubAreaID() const override;
    /// The subarea under the player.
    int currentPlayerSubAreaID() const override;
    int explorationRewardAmountGoStrength() const override;

    /// Highlights the selection on the map.
    void onSelectedItemChanged() override;
    void setMinimapRendererQuickItem(QObject* pItem) override;
    /// Asks to reset the viewpoints of the subarea last offered for a reset.
    void requestResetViewpoints() override;
    /// The tooltip of the topmost marker under the point (shown after a delay),
    /// and the cursor timer.
    void hoverAtPixelCoordinate(int x, int y) override;
    /// Rule context 9: the selection and field actions under the point; the
    /// context menu offers only the marker actions.
    void onClickAtPixelCoordinate(int x, int y, int TriggeringMouseButton, Qt::KeyboardModifiers Modifiers) override;
    void centerToPlayer() override;
    /// Opens the bestiary on the race.
    void onCreatureClicked(int RaceIDRaw) override;
    /// Asks to confirm a viewpoint reset, unless the subarea is unlocked.
    void resetSubAreaViewPoints(int SubAreaID) override;
    /// Asks to confirm a donation.
    void donateMoneyForArea(int AreaID, int Value) override;
    void onMouseExited() override;

public Q_SLOTS:
    /// The router's game actions: a map selection item selects its subarea or
    /// passage (stored in the configuration) or opens the house in the houses tab.
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    struct TNoSelection
    {
    };
    struct TPassageSelection
    {
        TCyclopediaMapSelectionInfoPassage::Values data;
    };
    struct TAreaSelection
    {
        TCyclopediaMapAreaValues data;
    };
    struct TSubAreaSelection
    {
        TCyclopediaMapSelectionInfoSubArea::Values data;
    };
    /// CIP's selection: none, a passage, an area or a subarea.
    using TSelection = std::variant<TNoSelection, TPassageSelection, TAreaSelection, TSubAreaSelection>;

    /// A click on an area or subarea caption: selects it and stores it.
    void onAreaClicked(int id, bool isArea);
    /// Sends the confirmed viewpoint reset or donation.
    void onConfirmDialogAccepted();
    void onTooltipTimerTimeout();
    /// The cursor shape for the left-button action under the mouse.
    void onMouseCursorTimerTimeout();
    void updateTimeDisplay();
    /// Viewpoints, areas or donations changed: the selection's info is rebuilt.
    void refreshSelection();

    void selectArea(quint16 areaId);
    void selectSubArea(quint16 subAreaId);
    void selectPassage(const Position& coordinate);
    /// The info object of the selection (reused when its type stays).
    cip::tibia::gamewindow::ICyclopediaMapSelectionInfo* buildSelectionInfo();
    void updateAreaData(TCyclopediaMapAreaValues& area) const;
    void updateSubAreaData(TCyclopediaMapSelectionInfoSubArea::Values& subArea) const;
    void updatePassageData(TCyclopediaMapSelectionInfoPassage::Values& passage) const;
    /// The subarea's creatures by occurrence; a race the player does not know
    /// from the bestiary is listed unnamed.
    TCyclopediaCreatureOccurrencesData creatureOccurrences(quint16 subAreaId) const;
    /// Every donating area, most gold first.
    QString donationsTooltip() const;
    /// The selection and field actions under a pixel of the map.
    input::TGameActionList gameActionsAt(int x, int y) const;
    /// The floor the map's walk targets lie on: the player's while both the
    /// player and the map are on or above the ground, else the map's.
    int displayedFloor() const;

    std::shared_ptr<cyclopedia::TCyclopediaMapCharacterConfigurationStorage> m_configuration;
    std::shared_ptr<cyclopedia::TCyclopediaMapStorage> m_storage;
    std::vector<QMetaObject::Connection> m_sessionConnections;
    TMessageDialogController* m_confirmDialog;
    cyclopedia::TCyclopediaMapGameActionProvider m_provider;
    QPointer<qmlcomponents::TMinimapRendererQuickItem> m_renderer;
    /// What the confirmation is for: 0 a viewpoint reset, 1 a donation.
    int m_confirmType = 0;
    quint16 m_resetSubAreaId = 0;
    quint16 m_donationAreaId = 0;
    qint64 m_donationAmount = 0;
    TSelection m_selection;
    QTimer m_tooltipTimer;
    QTimer m_mouseCursorTimer;
    QPointer<worldmap::TAmbientLightStorage> m_ambientLight;
    QPointer<mods::InstanceBackend> m_instance;
    input::TGameActionMapper m_mapper;
};

}
}
