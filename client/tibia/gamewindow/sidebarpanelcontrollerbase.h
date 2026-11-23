#pragma once

#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <QPointer>
#include <QQuickItem>
#include <QVariant>

namespace tibia {
namespace gamewindow {

/// What CIP's TSidebarPanelControllerBase adds to every sidebar panel
/// controller (minimap, inventory and status, hitpoints and mana, control
/// buttons, premium): the panel's QML item, which it owns. `Base` is the cip
/// class of the panel controller (deriving from the cip
/// TSidebarPanelControllerBase, which declares the slots and signals).
template <class Base>
class TSidebarPanelController : public Base
{
public:
    explicit TSidebarPanelController(QObject* parent = nullptr)
        : Base(parent)
    {}
    /// CIP: the panel item goes with the controller.
    ~TSidebarPanelController() override { releasePanelItem(); }

    /// v12 / v13 (CIP).
    virtual void hidePanel()
    {
        if (m_panelItem)
            m_panelItem->setVisible(false);
    }
    virtual void showPanel()
    {
        if (m_panelItem)
            m_panelItem->setVisible(true);
    }

    /// v14 (CIP): the item knows this as its panelController; its
    /// positionInSidebarChanged is the controller's, and its destruction
    /// releases it.
    virtual void setPanelItem(QQuickItem* PanelItem)
    {
        m_panelItem = PanelItem;
        PanelItem->setProperty("panelController", QVariant::fromValue<QObject*>(this));
        m_panelItemConnections.add(QObject::connect(PanelItem, SIGNAL(positionInSidebarChanged()), this, SIGNAL(positionInSidebarChanged())));
        m_panelItemConnections.add(
            QObject::connect(PanelItem, &QObject::destroyed, this, [this] { this->onPanelQuickItemDestroyed(); }));
    }

    /// v15 (CIP): the item leaves its sidebar, forgets the controller and is
    /// deleted.
    virtual void releasePanelItem()
    {
        if (!m_panelItem)
            return;
        m_panelItemConnections.disconnectAll();
        m_panelItem->setParentItem(nullptr);
        m_panelItem->setProperty("panelController", QVariant::fromValue<QObject*>(nullptr));
        delete m_panelItem.data();
        m_panelItem = nullptr;
    }

    /// v16 (CIP): the item goes and the panel asks to be closed.
    void onSidebarDestroyed() override
    {
        releasePanelItem();
        Q_EMIT this->requestClose();
    }
    /// v17 (CIP). Host: the item is already gone (the pointer is null), so
    /// nothing is left to release.
    void onPanelQuickItemDestroyed() override { releasePanelItem(); }

    QQuickItem* panelItem() const { return m_panelItem; }
    /// CIP: the item's "identifier" (the panel's place in the saved order),
    /// -1 without an item.
    int panelIdentifier() const { return m_panelItem ? m_panelItem->property("identifier").toInt() : -1; }

private:
    shared::TQtConnectionManager m_panelItemConnections;
    QPointer<QQuickItem> m_panelItem;
};

}
}
