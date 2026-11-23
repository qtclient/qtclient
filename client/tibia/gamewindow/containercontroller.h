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

#include <functional>
#include <memory>

namespace tibia {
namespace config {
class TSidebarOptions;
}
}

namespace tibia {
namespace gamewindow {

class TGenericConfirmWithDontShowAgainDialogController;

/// One open container's sidebar widget, container.qml (CIP:
/// TContainerController). The configuration menu offers the server's view
/// filter categories, sorting, manual sort mode and moving the content to the
/// managed containers.
class TContainerController final : public TSidebarWidgetController<cip::tibia::gamewindow::TContainerController>
{
    Q_OBJECT
public:
    /// `messages` is the session's container protocol handler the widget's
    /// requests go to.
    TContainerController(int containerId, container::TContainerProtocolMessageHandler* messages, QQuickItem* gameWindowItem,
                         QQmlEngine* engine, QObject* parent = nullptr);
    ~TContainerController() override;
    /// CIP: the frame update: the counting-down slot texts.
    void updateWithTimestamp(qint64 TimestampMilliseconds);

    int containerId() const { return m_containerId; }
    void setControlScheme(int controlScheme) { m_mapper.setControlScheme(controlScheme); }
    void setLootScheme(int lootScheme) { m_mapper.setLootScheme(lootScheme); }
    /// CIP : restores the widget's size from the character's
    /// containersOptions and follows them.
    void setSidebarOptions(config::TSidebarOptions* options);
    /// CIP : the container's state from TContainerStorage; the scroll
    /// view resets when it shows another container (replace-open).
    void update(bool resetScrollView);
    /// The live filter and manual sort properties.
    void refreshViewOptions();

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
    /// The filter, sort and managed-container entries the container supports.
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
    /// Manual sort mode is on and this container can be sorted.
    bool manualSortModeActive() const;
    /// Asks the server to sort the content (confirms a recursive sort first).
    void sortContainer(int sortType);
    /// Asks the server to move the content to the managed containers
    /// (confirms a recursive move first).
    void moveContentToManagedContainers();
    void showConfirmDialog(const char* caption, const char* content, std::function<void(bool showAgain)> onYes);
    template <class T>
    void setProperty(T& member, const T& value);
    void setIcon(const container::TInventorySlot& slot);

    int m_containerId;
    QPointer<container::TContainerProtocolMessageHandler> m_messages;
    TAppearanceInstanceListModel m_model;
    input::TGameActionMapper m_mapper;
    QPointer<config::TSidebarOptions> m_options;
    QTimer m_propertyTimer;
    int m_hoveredSlot = -1;
    std::unique_ptr<TGenericConfirmWithDontShowAgainDialogController> m_confirmDialog;
};

}
}
