#pragma once

#include "client/gamecore.h"
#include "gamewindow/assignactiondialogs.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "gamewindow/uifocusmanager.h"

#include <cip_gen.h>

#include <QPointer>

#include <vector>

namespace tibia {
namespace config {
class TSidebarOptions;
}
}

namespace tibia {
namespace gamewindow {

/// spelllistwidget.qml (CIP: TSpellListWidgetController): the spell list
/// proxy, its filter boxes in the context menu and in the widget options.
class TSpellListWidgetController final
    : public TSidebarWidgetController<cip::tibia::gamewindow::TSpellListWidgetController>
{
    Q_OBJECT
public:
    TSpellListWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);

    /// CIP moc property getters.
    QQuickItem* gameWindowQuickItem() const override;
    QVariant spellListModel() const override;
    bool hasFocus() const override;

public Q_SLOTS:
    /// The filter boxes as checkable entries; "all vocations" and "all spell
    /// groups" tick every box of their block.
    void onShowContextMenu() override;
    /// Opens the spell in the Magical Archive.
    void onShowMoreForSpell(int SpellID) override;
    /// Stores the size and the filter boxes in the widget options.
    void saveWidgetOptions() override;
    void takeFocus() override;
    void releaseFocus() override;
    /// CIP sidebar chrome (depot/skills pattern).
    void onCloseClicked() override;
    void onRequestClose() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void onLevelUpClicked() override;
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
    /// CIP class NOTIFY — qtclient already emits.
    void focusChanged();
    void requestWidgetControlToTakeFocus();

private Q_SLOTS:
    /// Applies the widget options' size and filter boxes.
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    TSpellListDataModelSortFilterProxy* m_proxy;
    bool m_applyingOptions = false;
    TUiFocusManager m_focus;
    QPointer<config::TSidebarOptions> m_options;
};

class TSpellListWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TSpellListWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TSpellListWidgetManager() override;

    TSidebarWidget* open(config::TSidebarOptions* options);
    void close(bool saveWidgetOptions = true);
    TSidebarWidget* current() const;


Q_SIGNALS:
    /// The controller's publishGameAction, forwarded.
    void publishGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<TSpellListWidgetController> m_controller;
};

}
}
