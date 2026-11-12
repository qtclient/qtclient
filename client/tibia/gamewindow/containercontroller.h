#pragma once

#include "container/containerciptypes.h"
#include "gamewindow/appearanceinstancelistmodel.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "input/gameaction.h"
#include "input/gameactionmapper.h"

#include <cip_gen.h>

#include <client/declarations.h>

#include <QPointer>
#include <QTimer>

namespace tibia {
namespace config {
class TSidebarOptions;
}
}

namespace tibia {
namespace gamewindow {

/// One open container's sidebar widget, container.qml (CIP:
/// TContainerController, vtable). View-filter / manual-sort /
/// "more pages" stay hidden until qtclient protocol carries those flags; depot
/// search visibility comes from the open-container search-icon byte.
class TContainerController final : public TSidebarWidgetController<cip::tibia::gamewindow::TContainerController>
{
    Q_OBJECT
public:
    TContainerController(int containerId, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

    int containerId() const { return m_containerId; }
    void setControlScheme(int controlScheme) { m_mapper.setControlScheme(controlScheme); }
    void setLootScheme(int lootScheme) { m_mapper.setLootScheme(lootScheme); }
    /// CIP TOGGLE_MANUAL_SORT_MODE: orange caption + slot drop markers.
    void setManualSortMode(bool enabled)
    {
        if (m_isInManualSortMode == enabled)
            return;
        m_isInManualSortMode = enabled;
        Q_EMIT containerPropertyChanged();
    }
    /// CIP : restores the widget's size from the character's
    /// containersOptions and follows them.
    void setSidebarOptions(config::TSidebarOptions* options);
    /// CIP : the container's state from TContainerStorage; the scroll
    /// view resets when it shows another container (replace-open).
    void update(bool resetScrollView);

    /// CIP moc property getters (live members filled by update / setIcon).
    QVariant appearanceTypeListModel() const override;
    int indexOfFirstVisibleObject() const override;
    QList<int> tutorialMarkerIDs() const override;
    QString caption() const override;
    uint numberOfPages() const override;
    uint currentPage() const override;
    uint numberOfContainerSlots() const override;
    uint numberOfFilledContainerSlots() const override;
    bool levelUpButtonVisible() const override;
    bool searchButtonVisible() const override;
    bool configButtonVisible() const override;
    bool isUpdatable() const override;
    bool hasPossiblyMorePages() const override;
    bool hasViewFilterOptions() const override;
    bool isInManualSortMode() const override;
    uint objectID() const override;
    uint objectCount() const override;
    cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::ELiquidType liquidType() const override;
    cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::EHookDirection hookDirection() const override;
    uint decoItemObjectID() const override;

    QQuickItem* gameWindowQuickItem() const override;

public Q_SLOTS:
    /// CIP .
    void onRequestClose() override;
    void onCloseClicked() override;
    /// CIP .
    void onLevelUpClicked() override;
    /// CIP .
    void saveWidgetOptions() override;
    /// CIP : a closed container forgets its size.
    void removeThisContainersOptions() override;
    /// CIP config menu — no view-filter / sort options in qtclient protocol.
    void onShowConfigurationMenu() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void setCursorToResizeVertical(bool Enabled) override;
    void startContentSizeChangedTimer();

Q_SIGNALS:
    /// CIP sidebar chrome (TSidebarWidgetControllerBase) — qtclient NOTIFY deepen.
    void contentSizeChanged();
    void positionInSidebarChanged();
    void sidebarIdChanged();
    void widgetClosed();
    /// CIP sidebar chrome (drag/move) — qtclient redeclare; forwarded from chrome slots.
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, tibia::gamewindow::TSidebarWidget* widget);
    /// CIP moc signal (shared_ptr arg skipped from cip_gen); qtclient name parity.
    void publishGameAction();
    /// CIP moc signal (shared_ptr arg skipped); qtclient name parity — live split via SHOW_SPLIT_STACK_DIALOG.
    void showSplitStackDialog();

private Q_SLOTS:
    void onSlotClicked(int slotId, int mouseButton, int keyboardModifiers);
    void onSlotEntered(int slotId);
    void onSlotExited(int slotId);
    void onSlotStartDrag(int slotId, QObject* source);
    void onSlotDragDropped(int slotId);
    void onSlotTargetSelected(int slotId);
    /// CIP.
    void onPreviousPageClicked();
    void onNextPageClicked();
    /// CIP .
    void onSearchButtonClicked();
    /// CIP .
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    const container::TStoredContainer* stored() const;
    /// CIP's container game action provider: the slot's object's actions.
    input::TGameActionList slotActions(int slotId) const;
    Position slotPosition(int pageSlot) const;
    void updateMouseCursorForCurrentSlot();
    void readExpireOptions();
    template <class T>
    void setProperty(T& member, const T& value);
    void setIcon(const container::TInventorySlot& slot);

    int m_containerId;
    TAppearanceInstanceListModel m_model;
    input::TGameActionMapper m_mapper;
    QPointer<config::TSidebarOptions> m_options;
    QTimer m_propertyTimer;
    QTimer m_clockTimer;
    int m_hoveredSlot = -1;
};

}
}
