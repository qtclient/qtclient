#include "singleobjectrenderer.h"

#include <client/animator.h>
#include <client/gameconfig.h>
#include <client/item.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>
#include <framework/core/clock.h>

#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGTransformNode>

#include <algorithm>

namespace tibia::qmlcomponents {

namespace {

using Info = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;
constexpr int FieldSize = 32;

}

TSingleObjectAppearanceInstanceRenderer::TSingleObjectAppearanceInstanceRenderer(QQuickItem* parent)
    : cip::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer(parent)
{
    m_scaleFactor = 1.f;
    setFlag(ItemHasContents);
}

TSingleObjectAppearanceInstanceRenderer::~TSingleObjectAppearanceInstanceRenderer() = default;

void TSingleObjectAppearanceInstanceRenderer::setTypeid(const int v)
{
    if (v == m_typeid)
        return;
    cip::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer::setTypeid(v);
    changed();
}

void TSingleObjectAppearanceInstanceRenderer::setCumulativeCount(const int v)
{
    if (v == m_cumulativeCount)
        return;
    cip::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer::setCumulativeCount(v);
    changed();
}

void TSingleObjectAppearanceInstanceRenderer::setLiquidType(const Info::ELiquidType v)
{
    if (v == m_liquidType)
        return;
    cip::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer::setLiquidType(v);
    changed();
}

void TSingleObjectAppearanceInstanceRenderer::setHookDirection(const Info::EHookDirection v)
{
    if (v == m_hookDirection)
        return;
    cip::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer::setHookDirection(v);
    changed();
}

void TSingleObjectAppearanceInstanceRenderer::setAnimated(const bool v)
{
    if (v == m_animated)
        return;
    cip::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer::setAnimated(v);
    changed();
}

void TSingleObjectAppearanceInstanceRenderer::setScaleFactor(const float v)
{
    if (v == m_scaleFactor)
        return;
    cip::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer::setScaleFactor(v);
    polish();
}

void TSingleObjectAppearanceInstanceRenderer::setSmoothTextureFiltering(const bool v)
{
    if (v == m_smoothTextureFiltering)
        return;
    cip::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer::setSmoothTextureFiltering(v);
    update();
}

void TSingleObjectAppearanceInstanceRenderer::componentComplete()
{
    cip::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer::componentComplete();
    changed();
}

void TSingleObjectAppearanceInstanceRenderer::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    cip::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        polish();
}

void TSingleObjectAppearanceInstanceRenderer::itemChange(const ItemChange change, const ItemChangeData& value)
{
    cip::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer::itemChange(change, value);
    if (change == ItemSceneChange)
        watchFrames();
}

/// The type and its patterns, as the engine picks them for an item of that count,
/// liquid and hook (Item::updatePatterns).
void TSingleObjectAppearanceInstanceRenderer::changed()
{
    m_type = nullptr;
    m_patternX = m_patternY = m_patternZ = 0;
    if (m_typeid > 0 && g_things.isValidDatId(static_cast<uint16_t>(m_typeid), ThingCategoryItem)) {
        m_type = g_things.getThingType(static_cast<uint16_t>(m_typeid), ThingCategoryItem).get();
        if (m_type && m_type->isNull())
            m_type = nullptr;
    }
    if (m_type) {
        const ItemPtr item = Item::create(m_typeid);
        const bool liquid = m_type->isFluidContainer() || m_type->isSplash();
        item->setCountOrSubType(liquid ? static_cast<int>(m_liquidType) : m_cumulativeCount);
        m_patternX = item->getPatternX();
        m_patternY = item->getPatternY();
        m_patternZ = item->getPatternZ();
        if (m_type->isHangable()) {
            const int patterns = m_type->getNumPatternX();
            if (m_hookDirection == Info::EHookDirection::SOUTH)
                m_patternX = patterns >= 2 ? 1 : 0;
            else if (m_hookDirection == Info::EHookDirection::EAST)
                m_patternX = patterns >= 3 ? 2 : 0;
        }
    }
    m_phase = currentPhase();
    watchFrames();
    polish();
}

/// Animated objects re-lay out when their phase moves on.
void TSingleObjectAppearanceInstanceRenderer::watchFrames()
{
    disconnect(m_frameConnection);
    if (!window() || !m_animated || !m_type || m_type->getAnimationPhases() <= 1)
        return;
    m_frameConnection = connect(window(), &QQuickWindow::afterAnimating, this, [this] {
        if (currentPhase() != m_phase) {
            m_phase = currentPhase();
            polish();
        }
    });
}

int TSingleObjectAppearanceInstanceRenderer::currentPhase() const
{
    if (!m_animated || !m_type)
        return 0;
    if (Animator* animator = m_type->getIdleAnimator())
        return animator->getPhase();
    const int phases = m_type->getAnimationPhases();
    if (phases <= 1)
        return 0;
    const int ticks = g_gameConfig.getItemTicksPerFrame();
    return static_cast<int>((g_clock.millis() % (static_cast<long long>(ticks) * phases)) / ticks);
}

void TSingleObjectAppearanceInstanceRenderer::updatePolish()
{
    m_sprite = {};
    m_target = {};
    QRect bounds;
    if (m_type && m_type->getAnimationPhases() > 0) {
        const auto& sprites = m_type->getSprites();
        const uint32_t index = m_type->getSpriteIndex(-1, -1, 0, m_patternX, m_patternY, m_patternZ, m_phase);
        if (index < sprites.size())
            m_sprite.spriteId = sprites[index];
    }
    if (m_sprite.spriteId) {
        const QSize size(m_type->getSize().width() * FieldSize, m_type->getSize().height() * FieldSize);
        // The sprite ends at the bottom-right of the object's field.
        const QPoint spriteAt(FieldSize - size.width(), FieldSize - size.height());
        const auto& boxes = m_type->getBoundingBoxes();
        if (m_patternX < static_cast<int>(boxes.size())) {
            const Rect& box = boxes[m_patternX];
            bounds = QRect(box.x(), box.y(), box.width(), box.height());
        } else {
            bounds = QRect(QPoint(), size);
        }
        bounds.translate(spriteAt);

        const double w = width();
        const double h = height();
        const bool field = size == QSize(FieldSize, FieldSize);
        if (field && bounds.width() <= w && bounds.height() <= h)
            bounds = QRect(0, 0, FieldSize, FieldSize);

        float scale = m_scaleFactor;
        if (w < scale * bounds.width() || h < scale * bounds.height()) {
            scale = std::min({ static_cast<float>(w) / bounds.width(), static_cast<float>(h) / bounds.height(), m_scaleFactor });
        }
        QPoint origin;
        const auto center = [&] {
            origin = QPoint(bounds.left() - (static_cast<int>(w / scale) - bounds.width()) / 2,
                            bounds.top() - (static_cast<int>(h / scale) - bounds.height()) / 2);
        };
        if (scale != m_scaleFactor)
            center();
        else if (!field && w >= bounds.width() && h >= bounds.height())
            center();
        if (scale > 0.f)
            m_target = QRectF(QPointF(spriteAt - origin) * scale, QSizeF(size) * scale);
    }
    if (bounds != m_completeBoundingRect) {
        m_completeBoundingRect = bounds;
        Q_EMIT completeBoundingRectChanged();
    }
    update();
}

QSGNode* TSingleObjectAppearanceInstanceRenderer::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    if (!m_sprite.spriteId || m_target.isEmpty()) {
        delete oldNode;
        return nullptr;
    }
    QSGTexture* texture = TSpriteTextureCache::shared(window()).texture(m_sprite);
    if (!texture) {
        // The sheet is still loading.
        QMetaObject::invokeMethod(this, &QQuickItem::update, Qt::QueuedConnection);
        delete oldNode;
        return nullptr;
    }
    auto* node = static_cast<QSGImageNode*>(oldNode);
    if (!node) {
        node = window()->createImageNode();
        node->setOwnsTexture(false);
    }
    node->setTexture(texture);
    node->setFiltering(m_smoothTextureFiltering ? QSGTexture::Linear : QSGTexture::Nearest);
    node->setSourceRect(QRectF(QPointF(), texture->textureSize()));
    node->setRect(m_target);
    return node;
}

}
