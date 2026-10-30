#include "numericaleffectoverlay.h"

#include "chat/textstorage.h"
#include "renderer/mapscale.h"
#include "renderer/worldmaprenderprovider.h"
#include "worldmap/numericaleffectstorage.h"

#include <tibia/world/map.h>
#include <tibia/world/tile.h>

#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>

#include <cmath>

namespace tibia::qmlcomponents {

namespace {

using Provider = TWorldMapRenderProvider;

}

void TNumericEffectsItemController::setItems(std::vector<Item> items)
{
    m_items = std::move(items);
    m_itemCount = static_cast<int>(m_items.size());
    Q_EMIT itemsChanged();
}

QString TNumericEffectsItemController::getTextForIndex(const int index)
{
    return index >= 0 && index < m_itemCount ? m_items[index].text : QString();
}

QColor TNumericEffectsItemController::getColorForIndex(const int index)
{
    return index >= 0 && index < m_itemCount ? m_items[index].color : QColor();
}

double TNumericEffectsItemController::getEffectPercentForIndex(const int index)
{
    return index >= 0 && index < m_itemCount ? m_items[index].percent : 0.0;
}

TNumericalEffectOverlayQuickItem::TNumericalEffectOverlayQuickItem(QQuickItem* parent)
    : cip::tibia::qmlcomponents::TNumericalEffectOverlayQuickItem(parent)
{}

TNumericalEffectOverlayQuickItem::~TNumericalEffectOverlayQuickItem()
{
    clear();
}

void TNumericalEffectOverlayQuickItem::setSources(TWorldMapRenderProvider* provider, const worldmap::TNumericalEffectStorage* effects)
{
    disconnect(m_frameConnection);
    m_provider = provider;
    m_effects = effects;
    if (!provider || !effects) {
        clear();
        return;
    }
    if (!m_component) {
        if (QQmlEngine* engine = qmlEngine(this)) {
            m_component = new QQmlComponent(engine, QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/NumericalEffectsTemplate.qml")),
                                            QQmlComponent::PreferSynchronous, this);
            if (m_component->isError())
                qWarning().noquote() << "NumericalEffectsTemplate:" << m_component->errorString();
        }
    }
    m_frameConnection = connect(provider, &Provider::overlaysAdvanced, this, &TNumericalEffectOverlayQuickItem::update);
}

bool TNumericalEffectOverlayQuickItem::isShown(const Position& coordinate) const
{
    // FUN_1010607f0: numbers below the top floor show unless a floor between hides them.
    const int top = m_provider->firstFloor();
    if (coordinate.z == m_provider->camera().z || coordinate.z <= top)
        return true;
    Position above = coordinate;
    while (above.z > top) {
        if (!above.coveredUp())
            return true;
        if (const TilePtr& tile = g_map.getTile(above); tile && tile->limitsFloorsView())
            return false;
    }
    return true;
}

TNumericalEffectOverlayQuickItem::Field TNumericalEffectOverlayQuickItem::create(const Position& coordinate)
{
    Field field;
    field.coordinate = coordinate;
    field.controller = new TNumericEffectsItemController(this);
    if (!m_component || !m_component->isReady())
        return field;
    QObject* object = m_component->beginCreate(qmlContext(this));
    field.item = qobject_cast<QQuickItem*>(object);
    if (!field.item) {
        delete object;
        return field;
    }
    m_component->completeCreate();
    // FUN_1010d0930: set once the item exists, so onControllerChanged runs.
    field.item->setProperty("controller", QVariant::fromValue<QObject*>(field.controller));
    field.item->setProperty("outerParent", QVariant::fromValue<QObject*>(this));
    field.item->setParentItem(this);
    return field;
}

void TNumericalEffectOverlayQuickItem::remove(Field& field)
{
    if (field.item)
        field.item->deleteLater();
    if (field.controller)
        field.controller->deleteLater();
}

void TNumericalEffectOverlayQuickItem::clear()
{
    for (Field& field : m_fields)
        remove(field);
    m_fields.clear();
}

void TNumericalEffectOverlayQuickItem::update()
{
    if (!m_provider || !m_effects)
        return;
    const Provider::Frame& frame = m_provider->frame();
    if (!frame.valid) {
        clear();
        return;
    }

    const int MapWidth = m_provider->frame().visibleWidth * Provider::FieldSize;
    const int MapHeight = m_provider->frame().visibleHeight * Provider::FieldSize;
    const MapCoverScale cover = MapCoverScale::placed(width(), height(), MapWidth, MapHeight, frame.viewScale);
    const QRect visible(frame.camera, QSize(MapWidth, MapHeight));
    const qint64 now = chat::TTextStorage::now();

    for (Field& field : m_fields)
        field.seen = false;

    // Effects grouped by field, each group in the order its numbers appeared.
    std::vector<std::pair<Position, std::vector<TNumericEffectsItemController::Item>>> groups;
    for (const effects::TNumericalEffect& effect : m_effects->effects()) {
        auto group = std::find_if(groups.begin(), groups.end(), [&effect](const auto& g) { return g.first == effect.coordinate; });
        if (group == groups.end()) {
            if (!isShown(effect.coordinate))
                continue;
            const QRect field(m_provider->mapPixelOf(effect.coordinate), QSize(Provider::FieldSize, Provider::FieldSize));
            if (!visible.intersects(field))
                continue;
            groups.emplace_back(effect.coordinate, std::vector<TNumericEffectsItemController::Item>());
            group = std::prev(groups.end());
        }
        group->second.push_back({ QString::number(static_cast<int>(effect.value)), effect.color, effect.progress(now) });
    }

    for (auto& [coordinate, items] : groups) {
        auto it = std::find_if(m_fields.begin(), m_fields.end(), [&coordinate](const Field& f) { return f.coordinate == coordinate; });
        if (it == m_fields.end()) {
            m_fields.push_back(create(coordinate));
            it = std::prev(m_fields.end());
        }
        it->seen = true;

        // FUN_101062940: (16, 8) into the field, lifted by its elevation.
        int elevation = 0;
        if (const TilePtr& tile = g_map.getTile(coordinate))
            elevation = tile->getDrawElevation();
        const QPoint camera = frame.camera;
        const QPoint pixel = m_provider->mapPixelOf(coordinate) + QPoint(16 - elevation, 8 - elevation) - camera;
        const QPointF position = cover.toPaneRounded(pixel.x(), pixel.y());
        if (it->item && it->item->position() != position)
            it->item->setPosition(position);
        it->controller->setItems(std::move(items));
    }

    for (auto it = m_fields.begin(); it != m_fields.end();) {
        if (it->seen) {
            ++it;
            continue;
        }
        remove(*it);
        it = m_fields.erase(it);
    }
}

}
