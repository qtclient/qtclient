#pragma once

#include "gamewindow/lenshelpcontroller.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <QPointer>
#include <QQuickItem>

#include <set>

class QQmlEngine;

namespace tibia {
namespace qmlcomponents {

class TLenshelpQuickItem;

/// CIP qmlcomponents::TNotMouseMoveEventFilter: while lenshelp is on, any key
/// press, close, mouse button or wheel event (anything but moving the mouse)
/// is announced; mouse buttons and the wheel go no further.
class TNotMouseMoveEventFilter final : public cip::tibia::qmlcomponents::TNotMouseMoveEventFilter
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TNotMouseMoveEventFilter::TNotMouseMoveEventFilter;

    bool eventFilter(QObject* watched, QEvent* event) override;
};

/// CIP qmlcomponents::TLenshelpManager: lenshelp mode. Enabling it turns the
/// lenshelp cursor on, makes the popup and shows every Lenshelp QML item; the
/// first input that is not a mouse move ends it. Hovering a Lenshelp item (or a
/// LENSHELP_SHOW for a world object) shows its text in the popup.
class TLenshelpManager final : public cip::tibia::qmlcomponents::TLenshelpManager
{
    Q_OBJECT
public:
    TLenshelpManager(QQmlEngine* engine, QQuickItem* parentItem, QObject* parent = nullptr);
    ~TLenshelpManager() override;

    static TLenshelpManager* instance();
    static bool isEnabled();
    /// Lenshelp items exist (and are enabled and visible) only in lenshelp mode.
    static void registerItem(TLenshelpQuickItem* item);
    static void unregisterItem(TLenshelpQuickItem* item);

    void enableLenshelp() override;
    void disableLenshelp() override;
    /// LENSHELP_ACTIVATE, LENSHELP_SHOW (the world object's lenshelp id) and LENSHELP_HIDE.
    bool handleGameAction(const input::TGameAction& action);
    /// A Lenshelp item's text at `sceneRect`.
    void show(const QString& caption, const QString& content, const QRectF& sceneRect, const QQuickItem* item);
    /// The mouse left `item`: its text goes.
    void hide(const QQuickItem* item);

    gamewindow::TLenshelpController& controller() { return m_controller; }

private:
    static void setEnabledState(bool enabled);
    void showLenshelp(const input::TGameAction& action);

    TNotMouseMoveEventFilter* m_filter = nullptr;
    gamewindow::TLenshelpController m_controller;
    const QQuickItem* m_shownFor = nullptr;

    static TLenshelpManager* s_instance;
    static bool s_enabled;
    static std::set<TLenshelpQuickItem*>& items();
};

}
}
