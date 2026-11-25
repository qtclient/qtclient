// Host API v9: the UI table (abi/ui_api.h) over the game window's CipSoft sidebars
// (TSidebarManager, TSidebarWidgetsManager, TSidebarWidget, TContainerStorageController),
// the minimap panel (TMinimapController), Cyclopedia map renderer items
// (TMinimapRendererQuickItem + TWorldMapCamera), the store image base and sprite pixels.
// Sidebar widgets get host ids (stable while the widget lives).
#include "mods/abi_internal.h"

#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"
#include "gamewindow/containerstoragecontroller.h"
#include "gamewindow/minimapcontroller.h"
#include "gamewindow/sidebarmanager.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "gamewindow/sidebarwidgetsmanager.h"
#include "gamewindow/storedialogcontroller.h"
#include "minimap/minimapmanager.h"
#include "minimap/minimapvisiblearea.h"
#include "qmlcomponents/minimapquickitem.h"
#include "qmlcomponents/minimaprendererquickitem.h"
#include "sprites/spritesession.h"
#include "store/dynamicallyloadedimage.h"
#include "store/storestorage.h"

#include <QDebug>
#include <QHash>
#include <QImage>
#include <QMatrix4x4>
#include <QPointer>
#include <QQuickItem>
#include <QRect>
#include <QString>
#include <QUrl>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>

namespace tibia::mods {
namespace {

using gamewindow::TSidebarWidget;

const HostObjects* host()
{
    return hostObjects();
}

// --- sidebar events ---

struct TSidebarWatch
{
    Subscribers<std::function<void(int, uint32_t, uint32_t)>> listeners;
    std::vector<QMetaObject::Connection> connections;
};

TSidebarWatch& sidebarWatch()
{
    static TSidebarWatch w;
    return w;
}

void sidebarEvent(const int event, const uint32_t a = 0, const uint32_t b = 0)
{
    sidebarWatch().listeners.call(event, a, b);
}

// --- widget ids ---

struct TWidgetIds
{
    uint32_t next = 1;
    std::map<uint32_t, QPointer<TSidebarWidget>> byId;
    QHash<const TSidebarWidget*, uint32_t> ids;
};

TWidgetIds& widgetIds()
{
    static TWidgetIds w;
    return w;
}

uint32_t idOf(TSidebarWidget* widget)
{
    if (!widget)
        return 0;
    TWidgetIds& w = widgetIds();
    if (const auto it = w.ids.constFind(widget); it != w.ids.constEnd())
        return it.value();
    const uint32_t id = w.next++;
    w.byId[id] = widget;
    w.ids.insert(widget, id);
    QObject::connect(widget, &QObject::destroyed, [id, widget] {
        widgetIds().byId.erase(id);
        widgetIds().ids.remove(widget);
    });
    QObject::connect(widget, &TSidebarWidget::widgetWantsToMoveToSidebar, widget,
                     [id](const int target, TSidebarWidget*) {
                         sidebarEvent(QTCLIENT_SIDEBARS_WIDGET_WANTS_TO_MOVE, id, static_cast<uint32_t>(target));
                     });
    return id;
}

TSidebarWidget* widgetOf(const uint32_t id)
{
    const auto& byId = widgetIds().byId;
    const auto it = byId.find(id);
    return it == byId.end() ? nullptr : it->second.data();
}

/// The TSidebarWidget of a widget controller (the widget is the controller's child).
TSidebarWidget* widgetOfController(QObject* controller)
{
    return controller ? controller->findChild<TSidebarWidget*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
}

gamewindow::TSidebarManager* sidebars()
{
    return host() ? host()->sidebarManager : nullptr;
}
gamewindow::TSidebarWidgetsManager* widgetsManager()
{
    return host() ? host()->sidebarWidgets : nullptr;
}
gamewindow::TSidebarController* sidebarOf(const uint32_t id)
{
    gamewindow::TSidebarManager* manager = sidebars();
    return manager && manager->hasSidebar(id) ? manager->sidebar(id) : nullptr;
}
gamewindow::TSidebarManager::Side sideOf(const int side)
{
    return side == QTCLIENT_SIDEBAR_SIDE_LEFT ? gamewindow::TSidebarManager::Left : gamewindow::TSidebarManager::Right;
}

int uiSidebarCount(int side)
{
    gamewindow::TSidebarManager* manager = sidebars();
    return manager ? manager->sidebarCount(sideOf(side)) : 0;
}
void uiSetSidebarCount(int side, int count)
{
    if (gamewindow::TSidebarManager* manager = sidebars())
        manager->setSidebarCount(sideOf(side), count);
}
size_t uiSidebarIds(uint32_t* out, size_t cap)
{
    gamewindow::TSidebarManager* manager = sidebars();
    if (!manager)
        return 0;
    const std::vector<uint> ids = manager->sidebarIds();
    for (size_t i = 0; out && i < ids.size() && i < cap; ++i)
        out[i] = ids[i];
    return ids.size();
}
int uiHasSidebar(uint32_t id)
{
    gamewindow::TSidebarManager* manager = sidebars();
    return manager && manager->hasSidebar(id) ? 1 : 0;
}
QtClientQQuickItemPtr uiSidebarItem(uint32_t id)
{
    gamewindow::TSidebarController* sb = sidebarOf(id);
    return sb ? sb->item() : nullptr;
}
QtClientQQuickItemPtr uiSidebarUpperPane(uint32_t id)
{
    gamewindow::TSidebarController* sb = sidebarOf(id);
    return sb ? sb->upperPane() : nullptr;
}
QtClientQQuickItemPtr uiSidebarLowerPane(uint32_t id)
{
    gamewindow::TSidebarController* sb = sidebarOf(id);
    return sb ? sb->lowerPane() : nullptr;
}
int uiSidebarRemainingHeight(uint32_t id)
{
    gamewindow::TSidebarController* sb = sidebarOf(id);
    return sb ? sb->remainingHeight() : 0;
}
int uiSidebarReserveIdentifier(uint32_t id)
{
    gamewindow::TSidebarController* sb = sidebarOf(id);
    return sb ? static_cast<int>(sb->reserveIdentifier()) : 0;
}
int uiAddWidget(uint32_t widget_id)
{
    gamewindow::TSidebarManager* manager = sidebars();
    TSidebarWidget* widget = widgetOf(widget_id);
    if (!manager || !widget)
        return 0;
    manager->addWidget(widget);
    return widget->parentSidebarId() != TSidebarWidget::NoSidebar ? 1 : 0;
}
int uiAddWidgetToSidebar(uint32_t sidebar_id, uint32_t widget_id, int index)
{
    gamewindow::TSidebarManager* manager = sidebars();
    TSidebarWidget* widget = widgetOf(widget_id);
    if (!manager || !widget || !manager->hasSidebar(sidebar_id))
        return 0;
    manager->addWidget(sidebar_id, widget, static_cast<uint>(std::max(0, index)));
    return widget->parentSidebarId() != TSidebarWidget::NoSidebar ? 1 : 0;
}
void uiRemoveWidget(uint32_t widget_id)
{
    gamewindow::TSidebarManager* manager = sidebars();
    if (TSidebarWidget* widget = widgetOf(widget_id); manager && widget)
        manager->removeWidget(widget);
}
int uiSubscribeSidebars(void (*fn)(void* user, int event, uint32_t a, uint32_t b), void* user)
{
    if (!fn)
        return 0;
    return sidebarWatch().listeners.add([fn, user](const int e, const uint32_t a, const uint32_t b) { fn(user, e, a, b); });
}
void uiUnsubscribeSidebars(int id)
{
    sidebarWatch().listeners.remove(id);
}

// --- widgets ---

uint32_t uiWidgetFromObject(QtClientQObjectPtr object)
{
    auto* o = static_cast<QObject*>(object);
    if (!o)
        return 0;
    if (auto* widget = qobject_cast<TSidebarWidget*>(o))
        return idOf(widget);
    if (TSidebarWidget* widget = widgetOfController(o))
        return idOf(widget);
    // An item or a drag source naming its widget controller.
    if (auto* controller = o->property("widgetController").value<QObject*>())
        return idOf(widgetOfController(controller));
    return 0;
}
QtClientQQuickItemPtr uiWidgetItem(uint32_t id)
{
    TSidebarWidget* widget = widgetOf(id);
    return widget ? widget->quickItem() : nullptr;
}
QtClientQObjectPtr uiWidgetController(uint32_t id)
{
    TSidebarWidget* widget = widgetOf(id);
    return widget ? static_cast<QObject*>(widget->controller()) : nullptr;
}
uint32_t uiWidgetParentSidebar(uint32_t id)
{
    TSidebarWidget* widget = widgetOf(id);
    return widget ? widget->parentSidebarId() : static_cast<uint32_t>(QTCLIENT_SIDEBAR_NONE);
}
void uiWidgetResetParentSidebar(uint32_t id)
{
    if (TSidebarWidget* widget = widgetOf(id))
        widget->resetParentSidebarId();
}
void uiWidgetMaximize(uint32_t id)
{
    if (TSidebarWidget* widget = widgetOf(id))
        widget->maximize();
}
void uiWidgetShow(uint32_t id)
{
    if (TSidebarWidget* widget = widgetOf(id))
        widget->show();
}
void uiWidgetHide(uint32_t id)
{
    if (TSidebarWidget* widget = widgetOf(id))
        widget->hide();
}
void uiWidgetRequestMove(uint32_t id, uint32_t sidebar_id)
{
    if (TSidebarWidget* widget = widgetOf(id))
        widget->requestMoveToSidebar(static_cast<int>(sidebar_id));
}

// --- widget kinds (pack kinds included) ---

struct TPackWidget;
std::map<int, std::unique_ptr<TPackWidget>>& packWidgets();

int uiWidgetKindIsOpen(int kind)
{
    gamewindow::TSidebarWidgetsManager* manager = widgetsManager();
    return manager && manager->isWidgetOpen(kind) ? 1 : 0;
}
void uiOpenWidgetKind(int kind)
{
    if (gamewindow::TSidebarWidgetsManager* manager = widgetsManager())
        manager->openWidget(kind);
}
uint32_t uiWidgetOfKind(int kind)
{
    gamewindow::TSidebarWidgetsManager* manager = widgetsManager();
    if (!manager || !manager->isWidgetOpen(kind))
        return 0;
    return uiWidgetFromObject(manager->widgetQuickItem(kind));
}

struct TPackWidget
{
    QtClientSidebarWidgetDescV1 desc{};
    QByteArray name;
    QString qmlUrl;
    QPointer<QObject> controller;
    bool open = false;
};

std::map<int, std::unique_ptr<TPackWidget>>& packWidgets()
{
    static std::map<int, std::unique_ptr<TPackWidget>> widgets;
    return widgets;
}

int uiWidgetKindName(int kind, char* buf, size_t buf_len)
{
    if (const auto it = packWidgets().find(kind); it != packWidgets().end())
        return writeSizedOut(it->second->name, buf, buf_len);
    const QString name = gamewindow::TSidebarWidgetsManager::widgetTypeName(kind);
    return name.isEmpty() ? -1 : writeSizedOut(name, buf, buf_len);
}
int uiWidgetKindOfName(const char* name_utf8)
{
    if (!name_utf8)
        return -1;
    for (const auto& [kind, w] : packWidgets()) {
        if (w->name == name_utf8)
            return kind;
    }
    return gamewindow::TSidebarWidgetsManager::widgetTypeOf(QString::fromUtf8(name_utf8));
}
size_t uiContainerWidgets(QtClientContainerWidget* out, size_t cap)
{
    gamewindow::TContainerStorageController* containers = host() ? host()->containers : nullptr;
    if (!containers)
        return 0;
    const auto widgets = containers->widgets();
    for (size_t i = 0; out && i < widgets.size() && i < cap; ++i)
        out[i] = QtClientContainerWidget{ static_cast<uint8_t>(widgets[i].first), idOf(widgets[i].second) };
    return widgets.size();
}

/// A pack widget: CipSoft's TSidebarWidgetControllerBase behaviour with the pack's QML.
class TPackSidebarWidgetController final
    : public gamewindow::TSidebarWidgetController<cip::tibia::gamewindow::TSidebarWidgetControllerBase>
{
public:
    using TSidebarWidgetController::TSidebarWidgetController;
};

TPackWidget* packWidgetOf(const uint32_t widget_id)
{
    TSidebarWidget* widget = widgetOf(widget_id);
    for (const auto& [kind, w] : packWidgets()) {
        if (w->controller && widgetOfController(w->controller) == widget)
            return w.get();
    }
    return nullptr;
}

bool createItem(TPackWidget& w)
{
    TSidebarWidget* widget = widgetOfController(w.controller);
    if (!widget)
        return false;
    widget->createItemFromUrl(QUrl(w.qmlUrl));
    return widget->quickItem() != nullptr;
}

uint32_t uiCreateSidebarWidget(const QtClientSidebarWidgetDescV1* desc)
{
    const HostObjects* h = host();
    if (!desc || !h || !h->sidebarWidgets || !h->engine || !h->gameWindowItem || !desc->qml_url
        || desc->kind < QTCLIENT_SIDEBAR_WIDGET_EXTENSION_BASE || packWidgets().contains(desc->kind))
        return 0;
    auto w = std::make_unique<TPackWidget>();
    std::memcpy(&w->desc, desc, std::min(desc->struct_size ? desc->struct_size : sizeof(w->desc), sizeof(w->desc)));
    w->name = QByteArray(desc->name_utf8 ? desc->name_utf8 : "");
    w->qmlUrl = QString::fromUtf8(desc->qml_url);
    w->desc.name_utf8 = nullptr;
    w->desc.qml_url = nullptr;
    auto* controller = new TPackSidebarWidgetController(h->gameWindowItem, h->engine, h->lifetimeParent);
    w->controller = controller;
    if (!createItem(*w)) {
        qCritical() << "Component creation error for" << w->qmlUrl;
        delete controller;
        return 0;
    }
    TPackWidget* raw = w.get();
    QObject::connect(controller, &cip::tibia::gamewindow::TSidebarWidgetControllerBase::widgetClosed, controller, [raw] {
        if (raw->desc.close_requested)
            raw->desc.close_requested(raw->desc.user);
    });
    gamewindow::TSidebarWidgetsManager::WidgetHooks hooks;
    hooks.open = [raw]() -> TSidebarWidget* {
        if (!raw->controller)
            return nullptr;
        if (raw->desc.open && !raw->desc.open(raw->desc.user))
            return nullptr;
        raw->open = true;
        return widgetOfController(raw->controller);
    };
    hooks.close = [raw] {
        raw->open = false;
        if (raw->desc.closed)
            raw->desc.closed(raw->desc.user);
    };
    hooks.current = [raw]() -> TSidebarWidget* { return raw->open ? widgetOfController(raw->controller) : nullptr; };
    hooks.discard = [raw] {
        raw->open = false;
        if (raw->desc.discarded)
            raw->desc.discarded(raw->desc.user);
    };
    h->sidebarWidgets->registerWidget(desc->kind, std::move(hooks));
    const uint32_t id = idOf(widgetOfController(controller));
    packWidgets()[desc->kind] = std::move(w);
    return id;
}

int uiRecreateSidebarWidgetItem(uint32_t widget_id)
{
    TPackWidget* w = packWidgetOf(widget_id);
    if (!w)
        return 0;
    TSidebarWidget* widget = widgetOf(widget_id);
    return widget && (widget->quickItem() || createItem(*w)) ? 1 : 0;
}

void uiSidebarWidgetKindClosed(int kind)
{
    if (gamewindow::TSidebarWidgetsManager* manager = widgetsManager())
        manager->onWidgetClosed(kind);
}

/// Hooks of a kind that has no widget any more (the manager has no unregister).
gamewindow::TSidebarWidgetsManager::WidgetHooks emptyHooks()
{
    return { [] { return static_cast<TSidebarWidget*>(nullptr); }, [] {}, [] { return static_cast<TSidebarWidget*>(nullptr); },
             [] {} };
}

void uiDestroySidebarWidget(uint32_t widget_id)
{
    TPackWidget* w = packWidgetOf(widget_id);
    if (!w)
        return;
    const int kind = w->desc.kind;
    if (gamewindow::TSidebarWidgetsManager* manager = widgetsManager()) {
        if (manager->isWidgetOpen(kind))
            manager->onWidgetClosed(kind, false);
        manager->registerWidget(kind, emptyHooks());
    }
    if (gamewindow::TSidebarManager* manager = sidebars()) {
        if (TSidebarWidget* widget = widgetOf(widget_id))
            manager->removeWidget(widget);
    }
    delete w->controller.data();
    packWidgets().erase(kind);
}

// --- minimap panel ---

gamewindow::TMinimapController* minimap()
{
    return host() ? host()->minimapController : nullptr;
}

QtClientQObjectPtr uiMinimapControllerObject()
{
    return static_cast<QObject*>(minimap());
}
QtClientQQuickItemPtr uiMinimapItem()
{
    gamewindow::TMinimapController* m = minimap();
    return m ? static_cast<QQuickItem*>(m->minimapItem()) : nullptr;
}
void uiSetMinimapPanelItem(QtClientQQuickItemPtr item)
{
    if (gamewindow::TMinimapController* m = minimap())
        m->setPanelItem(static_cast<QQuickItem*>(item));
}
int uiMinimapView(QtClientMinimapView* out)
{
    gamewindow::TMinimapController* m = minimap();
    if (!m || !out)
        return 0;
    const minimap::TMinimapVisibleArea& area = m->visibleArea();
    *out = QtClientMinimapView{};
    out->x = area.topLeft().x;
    out->y = area.topLeft().y;
    out->z = area.z();
    out->width = area.size().width();
    out->height = area.size().height();
    out->scale = area.scale();
    std::memcpy(out->world_to_item, area.matrix().constData(), sizeof out->world_to_item);
    return 1;
}

struct TMinimapWatch
{
    Subscribers<std::function<void()>> listeners;
    QPointer<gamewindow::TMinimapController> bound;
    QMetaObject::Connection connection;
};

TMinimapWatch& minimapWatch()
{
    static TMinimapWatch w;
    return w;
}

void bindMinimap()
{
    TMinimapWatch& w = minimapWatch();
    gamewindow::TMinimapController* m = minimap();
    if (w.bound == m)
        return;
    QObject::disconnect(w.connection);
    w.bound = m;
    if (m)
        w.connection = QObject::connect(m, &cip::tibia::gamewindow::TMinimapController::currentLayerChanged, m,
                                        [] { minimapWatch().listeners.call(); });
}

int uiSubscribeMinimapChanged(void (*fn)(void* user), void* user)
{
    if (!fn)
        return 0;
    bindMinimap();
    return minimapWatch().listeners.add([fn, user] { fn(user); });
}
void uiUnsubscribeMinimapChanged(int id)
{
    minimapWatch().listeners.remove(id);
}
int uiMinimapDir(char* buf, size_t buf_len)
{
    return writeSizedOut(minimap::TMinimapManager::minimapDir(), buf, buf_len);
}

// --- Cyclopedia map renderer ---

qmlcomponents::TMinimapRendererQuickItem* rendererOf(QtClientQQuickItemPtr item)
{
    return qobject_cast<qmlcomponents::TMinimapRendererQuickItem*>(static_cast<QObject*>(item));
}
qmlcomponents::TWorldMapCamera* cameraOf(QtClientQQuickItemPtr item)
{
    qmlcomponents::TMinimapRendererQuickItem* renderer = rendererOf(item);
    return renderer ? renderer->worldCamera() : nullptr;
}

int uiRendererSetTileType(QtClientQQuickItemPtr item, int tile_type)
{
    qmlcomponents::TMinimapRendererQuickItem* renderer = rendererOf(item);
    if (!renderer || tile_type < QTCLIENT_MAP_TILE_SATELLITE || tile_type > QTCLIENT_MAP_TILE_LOCAL_MINIMAP)
        return 0;
    renderer->setTileType(static_cast<cip::tibia::qmlcomponents::TMinimapRendererQuickItem::EMinimapTileType>(tile_type));
    return 1;
}
int uiRendererSetHighlightedSubareas(QtClientQQuickItemPtr item, const int* ids, size_t count)
{
    qmlcomponents::TMinimapRendererQuickItem* renderer = rendererOf(item);
    if (!renderer)
        return 0;
    std::set<int> set;
    for (size_t i = 0; ids && i < count; ++i)
        set.insert(ids[i]);
    renderer->setHighlightedSubareas(set);
    return 1;
}
int uiRendererSetExtraZoneMasks(QtClientQQuickItemPtr item, const QtClientZoneMask* masks, size_t count)
{
    qmlcomponents::TMinimapRendererQuickItem* renderer = rendererOf(item);
    if (!renderer)
        return 0;
    std::vector<qmlcomponents::TMinimapRendererQuickItem::TExtraZoneMask> list;
    for (size_t i = 0; masks && i < count; ++i) {
        const QtClientZoneMask& m = masks[i];
        list.push_back({ m.id, QString::fromUtf8(m.file_utf8 ? m.file_utf8 : ""), QRect(m.x, m.y, m.width, m.height), m.z });
    }
    renderer->setExtraZoneMasks(std::move(list));
    return 1;
}
int uiRendererLayer(QtClientQQuickItemPtr item)
{
    qmlcomponents::TWorldMapCamera* camera = cameraOf(item);
    return camera ? camera->layer() : 0;
}
int uiRendererChangeLayer(QtClientQQuickItemPtr item, int layer)
{
    qmlcomponents::TWorldMapCamera* camera = cameraOf(item);
    if (!camera)
        return 0;
    camera->changeLayerWithIsometricProjection(layer);
    return 1;
}
int uiRendererSetReferencePoint(QtClientQQuickItemPtr item, int x, int y)
{
    qmlcomponents::TWorldMapCamera* camera = cameraOf(item);
    if (!camera)
        return 0;
    camera->setReferencePoint(QPoint(x, y));
    return 1;
}
int uiRendererSetScaleFactor(QtClientQQuickItemPtr item, float scale)
{
    qmlcomponents::TWorldMapCamera* camera = cameraOf(item);
    if (!camera)
        return 0;
    camera->setScaleFactor(scale);
    return 1;
}
int uiRendererCenterDelayed(QtClientQQuickItemPtr item, int subfield_x, int subfield_y, int layer)
{
    qmlcomponents::TWorldMapCamera* camera = cameraOf(item);
    if (!camera)
        return 0;
    camera->centerDelayed(subfield_x, subfield_y, layer);
    return 1;
}
int uiRendererPixelOf(QtClientQQuickItemPtr item, int x, int y, int layer, double* out_x, double* out_y)
{
    qmlcomponents::TWorldMapCamera* camera = cameraOf(item);
    if (!camera)
        return 0;
    const QPointF p = camera->pixelOf(x, y, layer);
    if (out_x)
        *out_x = p.x();
    if (out_y)
        *out_y = p.y();
    return 1;
}
int uiRendererFieldAt(QtClientQQuickItemPtr item, double px, double py, int* out_x, int* out_y)
{
    qmlcomponents::TWorldMapCamera* camera = cameraOf(item);
    if (!camera)
        return 0;
    const QPoint f = camera->fieldAt(QPointF(px, py));
    if (out_x)
        *out_x = f.x();
    if (out_y)
        *out_y = f.y();
    return 1;
}

/// A renderer subscription: its connections live on a child of the renderer.
std::map<int, QPointer<QObject>>& rendererSubscriptions()
{
    static std::map<int, QPointer<QObject>> subscriptions;
    return subscriptions;
}

int uiRendererSubscribe(QtClientQQuickItemPtr item, void (*fn)(void* user, int event, int a, int b), void* user)
{
    qmlcomponents::TMinimapRendererQuickItem* renderer = rendererOf(item);
    if (!renderer || !fn)
        return 0;
    static int nextId = 1;
    const int id = nextId++;
    auto* context = new QObject(renderer);
    QObject::connect(renderer->worldCamera(), &cip::tibia::qmlcomponents::TWorldMapCameraQmlType::cameraViewportChanged, context,
                     [fn, user] { fn(user, QTCLIENT_MAP_RENDERER_VIEWPORT_CHANGED, 0, 0); });
    QObject::connect(renderer, &qmlcomponents::TMinimapRendererQuickItem::areaClicked, context,
                     [fn, user](const int area, const bool isArea) {
                         fn(user, QTCLIENT_MAP_RENDERER_AREA_CLICKED, area, isArea ? 1 : 0);
                     });
    rendererSubscriptions()[id] = context;
    return id;
}

void uiRendererUnsubscribe(int id)
{
    auto& subscriptions = rendererSubscriptions();
    if (const auto it = subscriptions.find(id); it != subscriptions.end()) {
        delete it->second.data();
        subscriptions.erase(it);
    }
}

// --- store images ---

store::TStoreProtocolMessageHandler* storeHandler()
{
    const game::TGameSessionBase* session = game::currentGameSession();
    return session ? session->storeProtocolMessageHandler().get() : nullptr;
}

int uiStoreServerImageBase(char* buf, size_t buf_len)
{
    const store::TStoreProtocolMessageHandler* handler = storeHandler();
    return writeSizedOut(handler ? handler->imageBaseUrl().toString() : QString(), buf, buf_len);
}

struct TStoreWatch
{
    Subscribers<std::function<void(const QByteArray&)>> listeners;
    int sessionListener = 0;
    QMetaObject::Connection connection;
    QString imageBase;
};

TStoreWatch& storeWatch()
{
    static TStoreWatch w;
    return w;
}

void bindStoreHandler()
{
    TStoreWatch& w = storeWatch();
    QObject::disconnect(w.connection);
    store::TStoreProtocolMessageHandler* handler = storeHandler();
    if (handler)
        w.connection = QObject::connect(handler, &cip::tibia::store::TStoreProtocolMessageHandler::storeImageBaseUrlChanged,
                                        handler, [](const QUrl& url) { storeWatch().listeners.call(url.toString().toUtf8()); });
    w.listeners.call((handler ? handler->imageBaseUrl().toString() : QString()).toUtf8());
}

int uiSubscribeStoreServerImageBase(void (*fn)(void* user, const char* url_utf8), void* user)
{
    if (!fn)
        return 0;
    TStoreWatch& w = storeWatch();
    const int id = w.listeners.add([fn, user](const QByteArray& url) { fn(user, url.constData()); });
    if (w.sessionListener == 0) {
        w.sessionListener = game::subscribeCurrentGameSessionChanged([] { bindStoreHandler(); });
        QObject::disconnect(w.connection);
        if (store::TStoreProtocolMessageHandler* handler = storeHandler())
            w.connection = QObject::connect(handler, &cip::tibia::store::TStoreProtocolMessageHandler::storeImageBaseUrlChanged,
                                            handler, [](const QUrl& url) { storeWatch().listeners.call(url.toString().toUtf8()); });
    }
    return id;
}

void uiUnsubscribeStoreServerImageBase(int id)
{
    storeWatch().listeners.remove(id);
}

void applyStoreImageBase()
{
    const HostObjects* h = host();
    const QString& base = storeWatch().imageBase;
    if (!h || !h->storeDialog || base.isEmpty())
        return;
    if (auto* images = qobject_cast<store::TDynamicallyLoadedImageManager*>(
            h->storeDialog->property("dynamicallyLoadedImageManager").value<QObject*>()))
        images->setBaseUrl(QUrl(base));
}

void uiSetStoreImageBase(const char* url_utf8)
{
    storeWatch().imageBase = QString::fromUtf8(url_utf8 ? url_utf8 : "");
    applyStoreImageBase();
}

// --- sprites ---

std::mutex& spriteMutex()
{
    static std::mutex m;
    return m;
}

std::unordered_map<const uint8_t*, QImage>& spriteImages()
{
    static std::unordered_map<const uint8_t*, QImage> images;
    return images;
}

int uiSpriteImage(uint32_t sprite_id, int* loading, QtClientImageView* out)
{
    if (loading)
        *loading = 0;
    if (!out)
        return 0;
    bool stillLoading = false;
    QImage image = sprites::spriteImageDecoded(sprite_id, stillLoading);
    if (loading)
        *loading = stillLoading ? 1 : 0;
    if (image.isNull())
        return 0;
    if (image.format() != QImage::Format_RGBA8888)
        image = image.convertToFormat(QImage::Format_RGBA8888);
    image.detach();
    out->pixels = image.bits();
    out->width = image.width();
    out->height = image.height();
    out->bytes_per_line = static_cast<int>(image.bytesPerLine());
    out->format = QTCLIENT_IMAGE_FORMAT_RGBA8888;
    std::lock_guard lock(spriteMutex());
    spriteImages().emplace(out->pixels, std::move(image));
    return 1;
}

void uiSpriteImageRelease(QtClientImageView* image)
{
    if (!image || !image->pixels)
        return;
    std::lock_guard lock(spriteMutex());
    spriteImages().erase(image->pixels);
    image->pixels = nullptr;
}

const QtClientUiApiV1 g_uiApi = {
    .struct_size = sizeof(QtClientUiApiV1),
    .sidebar_count = &uiSidebarCount,
    .set_sidebar_count = &uiSetSidebarCount,
    .sidebar_ids = &uiSidebarIds,
    .has_sidebar = &uiHasSidebar,
    .sidebar_item = &uiSidebarItem,
    .sidebar_upper_pane = &uiSidebarUpperPane,
    .sidebar_lower_pane = &uiSidebarLowerPane,
    .sidebar_remaining_height = &uiSidebarRemainingHeight,
    .sidebar_reserve_identifier = &uiSidebarReserveIdentifier,
    .add_widget = &uiAddWidget,
    .add_widget_to_sidebar = &uiAddWidgetToSidebar,
    .remove_widget = &uiRemoveWidget,
    .subscribe_sidebars = &uiSubscribeSidebars,
    .unsubscribe_sidebars = &uiUnsubscribeSidebars,
    .widget_from_object = &uiWidgetFromObject,
    .widget_item = &uiWidgetItem,
    .widget_controller = &uiWidgetController,
    .widget_parent_sidebar = &uiWidgetParentSidebar,
    .widget_reset_parent_sidebar = &uiWidgetResetParentSidebar,
    .widget_maximize = &uiWidgetMaximize,
    .widget_show = &uiWidgetShow,
    .widget_hide = &uiWidgetHide,
    .widget_request_move_to_sidebar = &uiWidgetRequestMove,
    .widget_kind_is_open = &uiWidgetKindIsOpen,
    .open_widget_kind = &uiOpenWidgetKind,
    .widget_of_kind = &uiWidgetOfKind,
    .widget_kind_name = &uiWidgetKindName,
    .widget_kind_of_name = &uiWidgetKindOfName,
    .container_widgets = &uiContainerWidgets,
    .create_sidebar_widget = &uiCreateSidebarWidget,
    .recreate_sidebar_widget_item = &uiRecreateSidebarWidgetItem,
    .sidebar_widget_kind_closed = &uiSidebarWidgetKindClosed,
    .destroy_sidebar_widget = &uiDestroySidebarWidget,
    .minimap_controller_object = &uiMinimapControllerObject,
    .minimap_item = &uiMinimapItem,
    .set_minimap_panel_item = &uiSetMinimapPanelItem,
    .minimap_view = &uiMinimapView,
    .subscribe_minimap_changed = &uiSubscribeMinimapChanged,
    .unsubscribe_minimap_changed = &uiUnsubscribeMinimapChanged,
    .minimap_dir = &uiMinimapDir,
    .map_renderer_set_tile_type = &uiRendererSetTileType,
    .map_renderer_set_highlighted_subareas = &uiRendererSetHighlightedSubareas,
    .map_renderer_set_extra_zone_masks = &uiRendererSetExtraZoneMasks,
    .map_renderer_layer = &uiRendererLayer,
    .map_renderer_change_layer = &uiRendererChangeLayer,
    .map_renderer_set_reference_point = &uiRendererSetReferencePoint,
    .map_renderer_set_scale_factor = &uiRendererSetScaleFactor,
    .map_renderer_center_delayed = &uiRendererCenterDelayed,
    .map_renderer_pixel_of = &uiRendererPixelOf,
    .map_renderer_field_at = &uiRendererFieldAt,
    .map_renderer_subscribe = &uiRendererSubscribe,
    .map_renderer_unsubscribe = &uiRendererUnsubscribe,
    .store_server_image_base = &uiStoreServerImageBase,
    .subscribe_store_server_image_base = &uiSubscribeStoreServerImageBase,
    .unsubscribe_store_server_image_base = &uiUnsubscribeStoreServerImageBase,
    .set_store_image_base = &uiSetStoreImageBase,
    .sprite_image = &uiSpriteImage,
    .sprite_image_release = &uiSpriteImageRelease,
};

/// The game window's sidebar signals (re-bound for each game window).
void bindSidebars()
{
    TSidebarWatch& w = sidebarWatch();
    for (const QMetaObject::Connection& c : w.connections)
        QObject::disconnect(c);
    w.connections.clear();
    const HostObjects* h = host();
    if (!h)
        return;
    if (gamewindow::TSidebarManager* manager = h->sidebarManager) {
        w.connections.push_back(QObject::connect(manager, &gamewindow::TSidebarManager::numberOfSidebarsChanged, manager,
                                                 [] { sidebarEvent(QTCLIENT_SIDEBARS_COUNT_CHANGED); }));
        w.connections.push_back(QObject::connect(manager, &cip::tibia::gamewindow::TSidebarManager::widgetConfigurationChanged,
                                                 manager, [] { sidebarEvent(QTCLIENT_SIDEBARS_CONFIGURATION_CHANGED); }));
    }
    if (gamewindow::TSidebarWidgetsManager* widgets = h->sidebarWidgets)
        w.connections.push_back(QObject::connect(widgets, &gamewindow::TSidebarWidgetsManager::widgetHidden, widgets,
                                                 [](const int kind) {
                                                     sidebarEvent(QTCLIENT_SIDEBARS_WIDGET_HIDDEN, static_cast<uint32_t>(kind));
                                                 }));
    if (gamewindow::TContainerStorageController* containers = h->containers)
        w.connections.push_back(QObject::connect(containers, &gamewindow::TContainerStorageController::containerWidgetsChanged,
                                                 containers, [] { sidebarEvent(QTCLIENT_SIDEBARS_CONTAINER_WIDGETS_CHANGED); }));
}

} // namespace

void bindUiWatchersToHost()
{
    bindSidebars();
    bindMinimap();
    applyStoreImageBase();
    bindGameHostWatcher();
    attachMenuExtensions();
    bindChatWatchers();
    bindPackHostToHost();
}

void releaseUiOnHostTeardown()
{
    // The pack widgets' controllers are children of the game window's lifetime parent; the
    // packs must have destroyed them (destroy_sidebar_widget). Forget the rest.
    packWidgets().clear();
}

const QtClientUiApiV1* uiApiV1()
{
    return &g_uiApi;
}

} // namespace tibia::mods
