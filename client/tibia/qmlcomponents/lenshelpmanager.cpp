#include "lenshelpmanager.h"

#include "gamewindow/mousecursorcontroller.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/lenshelpquickitem.h"

#include <QCoreApplication>
#include <QEvent>

#include <map>
#include <utility>

namespace tibia::qmlcomponents {

namespace {

using A = input::EGameAction;

/// The lenshelp texts of the world objects, by their lenshelp id.
const std::map<int, std::pair<const char*, const char*>>& worldObjectTexts()
{
    static const std::map<int, std::pair<const char*, const char*>> texts = {
        { 1000, { "lenshelp_creatures_title", "lenshelp_creatures_text" } },
        { 1001, { "lenshelp_player_title", "lenshelp_player_text" } },
        { 1002, { "lenshelp_container_title", "lenshelp_container_text" } },
        { 1100, { "lenshelp_ladder_title", "lenshelp_ladder_text" } },
        { 1101, { "lenshelp_sewergrate_title", "lenshelp_sewergrate_text" } },
        { 1102, { "lenshelp_dungeonfloor_title", "lenshelp_dungeonfloor_text" } },
        { 1103, { "lenshelp_lever_title", "lenshelp_lever_text" } },
        { 1104, { "lenshelp_door_title", "lenshelp_door_text" } },
        { 1105, { "lenshelp_nameddoor_title", "lenshelp_nameddoor_text" } },
        { 1106, { "lenshelp_stairs_title", "lenshelp_stairs_text" } },
        { 1107, { "lenshelp_mailbox_title", "lenshelp_mailbox_text" } },
        { 1108, { "lenshelp_depotbox_title", "lenshelp_depotbox_text" } },
        { 1109, { "lenshelp_dustbin_title", "lenshelp_dustbin_text" } },
        { 1110, { "lenshelp_stonepile_title", "lenshelp_stonepile_text" } },
        { 1111, { "lenshelp_sign_title", "lenshelp_sign_text" } },
        { 1112, { "lenshelp_book_title", "lenshelp_book_text" } },
    };
    return texts;
}

}

bool TNotMouseMoveEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::Wheel:
    case QEvent::MouseTrackingChange:
        Q_EMIT notMouseMoveEventHappend();
        return true;
    case QEvent::KeyPress:
    case QEvent::Close:
        Q_EMIT notMouseMoveEventHappend();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

TLenshelpManager* TLenshelpManager::s_instance = nullptr;
bool TLenshelpManager::s_enabled = false;

std::set<TLenshelpQuickItem*>& TLenshelpManager::items()
{
    static std::set<TLenshelpQuickItem*> registered;
    return registered;
}

TLenshelpManager::TLenshelpManager(QQmlEngine* engine, QQuickItem* parentItem, QObject* parent)
    : cip::tibia::qmlcomponents::TLenshelpManager(parent)
    , m_filter(new TNotMouseMoveEventFilter(this))
    , m_controller(engine, parentItem)
{
    s_instance = this;
    for (const A type : { A::LENSHELP_ACTIVATE, A::LENSHELP_SHOW, A::LENSHELP_HIDE })
        m_gameActions.on(type, [this](const input::IGameAction& action) { return handleGameAction(action); });
}

TLenshelpManager::~TLenshelpManager()
{
    s_instance = nullptr;
}

TLenshelpManager* TLenshelpManager::instance()
{
    return s_instance;
}

bool TLenshelpManager::isEnabled()
{
    return s_enabled;
}

void TLenshelpManager::registerItem(TLenshelpQuickItem* item)
{
    items().insert(item);
}

void TLenshelpManager::unregisterItem(TLenshelpQuickItem* item)
{
    items().erase(item);
}

void TLenshelpManager::setEnabledState(const bool enabled)
{
    for (TLenshelpQuickItem* item : items()) {
        item->setEnabled(enabled);
        item->setVisible(enabled);
    }
    s_enabled = enabled;
}

void TLenshelpManager::enableLenshelp()
{
    if (s_enabled)
        return;
    if (gamewindow::TMouseCursorController* cursor = gamewindow::TMouseCursorController::instance())
        cursor->setLenshelp(true);
    m_controller.createPopup();
    setEnabledState(true);
    qApp->installEventFilter(m_filter);
    connect(m_filter, &cip::tibia::qmlcomponents::TNotMouseMoveEventFilter::notMouseMoveEventHappend, this,
            &TLenshelpManager::disableLenshelp, Qt::UniqueConnection);
}

void TLenshelpManager::disableLenshelp()
{
    if (!s_enabled)
        return;
    if (gamewindow::TMouseCursorController* cursor = gamewindow::TMouseCursorController::instance())
        cursor->setLenshelp(false);
    m_controller.hide();
    m_controller.destroyPopup();
    setEnabledState(false);
    qApp->removeEventFilter(m_filter);
    disconnect(m_filter, &cip::tibia::qmlcomponents::TNotMouseMoveEventFilter::notMouseMoveEventHappend, this,
               &TLenshelpManager::disableLenshelp);
}

bool TLenshelpManager::handleGameAction(const input::IGameAction& action)
{
    switch (action.gameActionType()) {
    case A::LENSHELP_HIDE:
        m_controller.hide();
        return true;
    case A::LENSHELP_ACTIVATE:
        enableLenshelp();
        return true;
    case A::LENSHELP_SHOW:
        if (const auto* show = input::gameActionAs<input::TGameActionShowLenshelp>(&action))
            showLenshelp(*show);
        return true;
    default:
        return false;
    }
}

void TLenshelpManager::showLenshelp(const input::TGameActionShowLenshelp& action)
{
    const auto& texts = worldObjectTexts();
    const auto entry = texts.find(action.lensHelpId());
    if (entry == texts.end())
        return;
    const QString caption = qtTrId(entry->second.first);
    const QString content = qtTrId(entry->second.second);
    // The same text at the same place stays as it is.
    if (m_controller.caption() == caption && m_controller.content() == content && m_controller.triggerRect() == action.rect())
        return;
    m_controller.show(caption, content, action.rect());
}

void TLenshelpManager::show(const QString& caption, const QString& content, const QRectF& sceneRect, const QQuickItem* item)
{
    m_controller.show(caption, content, sceneRect);
    m_shownFor = item;
}

void TLenshelpManager::hide(const QQuickItem* item)
{
    if (m_shownFor != item)
        return;
    m_controller.hide();
}

}
