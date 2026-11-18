#pragma once

#include "gamewindow/dialogmanager.h"
#include "input/clickareaeventfilter.h"

#include <QCoreApplication>
#include <QQuickItem>

namespace tibia {
namespace gamewindow {

/// CIP gamewindow::TDialogControllerWithFocusHandlingBase: a dialog with text
/// input of its own. While shown it watches every click (TClickAreaEventFilter
/// on the application): one on its focus element (child "focusElement", else
/// the whole item) takes the keyboard focus, one elsewhere gives it back. The
/// focus counts only while it is allowed; taking or losing it tells the dialog
/// stack (the keyboard leaves or returns to the chat input when the dialog is
/// on top). `Base` derives from cip::...::TDialogControllerWithFocusHandlingBase.
template <class Base>
class TDialogControllerWithFocusHandlingBase : public TDialogController<Base>
{
public:
    explicit TDialogControllerWithFocusHandlingBase(const QString& qmlFile, QObject* parent = nullptr)
        : TDialogController<Base>(qmlFile, parent)
        , m_clickArea(new input::TClickAreaEventFilter(this))
    {
        QObject::connect(m_clickArea, &cip::tibia::input::TClickAreaEventFilter::clickedOutside, this,
                         [this] { TDialogControllerWithFocusHandlingBase::releaseFocus(); });
        QObject::connect(m_clickArea, &cip::tibia::input::TClickAreaEventFilter::clickedInside, this,
                         [this] { TDialogControllerWithFocusHandlingBase::takeFocus(); });
    }

    /// The focus counts while allowed and the dialog has its item.
    bool hasFocus() const override { return this->m_hasFocus && m_focusAllowed && this->dialog()->item(); }

    void takeFocus() override
    {
        if (this->m_hasFocus)
            return;
        this->m_hasFocus = true;
        announceFocus(m_focusAllowed);
        Q_EMIT this->focusChanged();
    }

    void releaseFocus() override
    {
        if (!this->m_hasFocus)
            return;
        this->m_hasFocus = false;
        announceFocus(false);
        Q_EMIT this->focusChanged();
    }

    /// v18 (CIP): the dialog blocks clicks itself.
    bool hasPersonalMouseShield() const override { return true; }

    /// v19 (CIP).
    void afterCreate() override
    {
        QQuickItem* item = this->dialog()->item();
        if (!item)
            return;
        auto* focusElement = item->findChild<QQuickItem*>(QStringLiteral("focusElement"));
        m_clickArea->setClickArea(focusElement ? focusElement : item);
    }

    /// v20 (CIP).
    void beforeDestroy() override
    {
        releaseFocus();
        m_clickArea->setClickArea(nullptr);
    }

    /// v22 (CIP).
    void beforeShow() override
    {
        qApp->installEventFilter(m_clickArea);
        takeFocus();
    }

    /// v23 (CIP).
    void beforeHide() override
    {
        qApp->removeEventFilter(m_clickArea);
        releaseFocus();
    }

protected:
    void setHasFocus(const bool hasFocus)
    {
        if (this->m_hasFocus == hasFocus)
            return;
        this->m_hasFocus = hasFocus;
        announceFocus(hasFocus && m_focusAllowed);
        Q_EMIT this->focusChanged();
    }

    bool isFocusAllowed() const { return m_focusAllowed; }

    /// The dialog's focus is lent elsewhere (CIP: the NPC dialog's trade pane).
    void setFocusAllowed(const bool allowed)
    {
        if (m_focusAllowed == allowed)
            return;
        m_focusAllowed = allowed;
        if (!allowed)
            announceFocus(false);
        else if (this->m_hasFocus)
            announceFocus(true);
        Q_EMIT this->focusChanged();
    }

private:
    /// The dialog stack moves the keyboard when this dialog is its top.
    void announceFocus(const bool taken)
    {
        if (TDialogStackController* stack = TDialogStackController::instance()) {
            if (taken)
                stack->dialogTookFocus(this->dialog());
            else
                stack->dialogReleasedFocus(this->dialog());
        }
    }

    input::TClickAreaEventFilter* m_clickArea;
    bool m_focusAllowed = true;
};

}
}
