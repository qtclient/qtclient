#include "numericaleffectoverlay.h"

#include "effects/numericaleffectrenderprovider.h"
#include "renderer/mapscale.h"
#include "renderer/rendervolumeprovider.h"
#include "renderer/worldmaprenderprovider.h"

#include "worldmap/worldmapfield.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>

#include <cmath>
#include "worldmap/worldmapstorage.h"

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

void TNumericalEffectOverlayQuickItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    cip::tibia::qmlcomponents::TNumericalEffectOverlayQuickItem::geometryChange(newGeometry, oldGeometry);
}

void TNumericalEffectOverlayQuickItem::setSources(TWorldMapRenderProvider* provider,
                                                  std::shared_ptr<effects::TNumericalEffectRenderProvider> numericalEffects,
                                                  std::shared_ptr<renderer::TRenderVolumeProvider> renderVolume)
{
    disconnect(m_frameConnection);
    m_provider = provider;
    m_numericalEffectRenderProvider = std::move(numericalEffects);
    m_renderVolumeProvider = std::move(renderVolume);
    if (!provider || !m_numericalEffectRenderProvider || !m_renderVolumeProvider) {
        clear();
        return;
    }
    if (!m_component) {
        if (QQmlEngine* engine = qmlEngine(this)) {
            m_component = new QQmlComponent(engine, QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/NumericalEffectsTemplate.qml")),
                                            QQmlComponent::PreferSynchronous, this);
        }
    }
    m_frameConnection = connect(provider, &Provider::overlaysAdvanced, this, &TNumericalEffectOverlayQuickItem::update);
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
    // : set once the item exists, so onControllerChanged runs.
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
    if (!m_provider || !m_numericalEffectRenderProvider || !m_renderVolumeProvider)
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

    for (Field& field : m_fields)
        field.seen = false;

    // The fields with numbers, each in the order its numbers appeared.
    std::vector<std::pair<Position, std::vector<TNumericEffectsItemController::Item>>> groups;
    for (const effects::TNumericalEffectRenderGroup& group : m_numericalEffectRenderProvider->numericalEffectGroups(
             m_renderVolumeProvider->renderVolume(), m_renderVolumeProvider->visibleAreaFloor())) {
        const Position coordinate = group.coordinate.toPosition();
        const QRect field(m_provider->mapPixelOf(coordinate), QSize(Provider::FieldSize, Provider::FieldSize));
        if (!visible.intersects(field))
            continue;
        std::vector<TNumericEffectsItemController::Item> items;
        items.reserve(group.effects.size());
        for (const effects::TNumericalEffectRenderData& effect : group.effects)
            items.push_back({ effect.text, effect.color, effect.percent });
        groups.emplace_back(coordinate, std::move(items));
    }

    for (auto& [coordinate, items] : groups) {
        auto it = std::find_if(m_fields.begin(), m_fields.end(), [&coordinate](const Field& f) { return f.coordinate == coordinate; });
        if (it == m_fields.end()) {
            m_fields.push_back(create(coordinate));
            it = std::prev(m_fields.end());
        }
        it->seen = true;

        // : (16, 8) into the field, lifted by its elevation.
        int elevation = 0;
        if (tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(coordinate))
            elevation = tile->objectElevation();
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
