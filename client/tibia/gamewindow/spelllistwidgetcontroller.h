#pragma once

#include "client/gamecore.h"
#include "gamewindow/magicalarchivedialogcontroller.h"
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

/// spelllistwidget.qml (CIP: TSpellListWidgetController). Reuses the Magical
/// Archive spell model + filter proxy. Archive rows refresh on
/// onUnlockedSpells / onSpellDataModification.
class TSpellListWidgetController final
    : public TSidebarWidgetController<cip::tibia::gamewindow::TSpellListWidgetController>
{
    Q_OBJECT
public:
    TSpellListWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);
    void reloadArchive();

    /// CIP moc property getters.
    QQuickItem* gameWindowQuickItem() const override;
    QVariant spellListModel() const override;
    bool hasFocus() const override;

public Q_SLOTS:
    void onShowContextMenu() override;
    void onShowMoreForSpell(int SpellID) override;
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
    /// CIP sidebar chrome (TSidebarWidgetControllerBase) — tip NOTIFY deepen.
    void contentSizeChanged();
    void positionInSidebarChanged();
    void sidebarIdChanged();
    void widgetClosed();
    /// CIP sidebar chrome (drag/move) — tip redeclare; forwarded from chrome slots.
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, tibia::gamewindow::TSidebarWidget* widget);
    /// CIP moc signal (shared_ptr arg skipped from cip_gen); tip name parity.
    void publishGameAction();
    /// CIP class NOTIFY — tip already emits.
    void focusChanged();
    void requestWidgetControlToTakeFocus();

private Q_SLOTS:
    /// CIP private slot — persist contentHeight / contentMaximized.
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    TMagicalArchiveSpellListModel* m_model;
    TSpellListDataModelSortFilterProxy* m_proxy;
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
    void close();
    TSidebarWidget* current() const;

private:
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<TSpellListWidgetController> m_controller;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
