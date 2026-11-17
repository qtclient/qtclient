#include "appearances/appearanceinstance.h"
#include "singleobjectrenderer.h"

#include <tibia/config/runtimegameconfig.h>
#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>
#include "clockservice.h"
#include "appearances/spriteinfo.h"
#include "renderer/spritetextureprovider.h"

#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGTransformNode>

#include <algorithm>
#include <tibia/game/thingsession_forward.h>

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
/// liquid and hook (appearances::objectPatternsFor).
void TSingleObjectAppearanceInstanceRenderer::changed()
{
    m_type = nullptr;
    m_patternX = m_patternY = m_patternZ = 0;
    if (m_typeid > 0 && tibia::game::isValidDatId(static_cast<uint16_t>(m_typeid), ThingCategoryItem)) {
        m_type = tibia::game::thingType(static_cast<uint16_t>(m_typeid), ThingCategoryItem);
        if (m_type && m_type->isNull())
            m_type = nullptr;
    }
    if (m_type) {
        const bool liquid = m_type->isFluidContainer() || m_type->isSplash();
        int x = 0;
        int y = 0;
        int z = 0;
        tibia::appearances::objectPatternsFor(m_type, liquid ? static_cast<int>(m_liquidType) : m_cumulativeCount, x, y, z);
        m_patternX = x;
        m_patternY = y;
        m_patternZ = z;
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
    if (const tibia::animation::TAnimationInfo* info = m_type->idleAnimation()) {
        if (!m_animator || m_animatorType != m_type) {
            m_animator.emplace(*info, 0);
            m_animatorType = m_type;
        }
        m_animator->update(static_cast<qint64>(tibia::shared::TClockService::frameMillis()));
        return m_animator->phase();
    }
    const int phases = m_type->getAnimationPhases();
    if (phases <= 1)
        return 0;
    const int ticks = tibia::config::itemTicksPerFrame();
    return static_cast<int>((tibia::shared::TClockService::frameMillis() % (static_cast<long long>(ticks) * phases)) / ticks);
}

void TSingleObjectAppearanceInstanceRenderer::updatePolish()
{
    m_sprite = {};
    m_target = {};
    QRect bounds;
    if (m_type && m_type->getAnimationPhases() > 0) {
        if (const auto sprite = m_type->spriteIdAt(0, m_patternX, m_patternY, m_patternZ, m_phase))
            m_sprite.info = std::make_shared<appearances::TSpritesheetAppearanceInstanceSpriteInfo>(*sprite);
    }
    if (m_sprite.info) {
        const QSize size(m_type->getSize().width() * FieldSize, m_type->getSize().height() * FieldSize);
        // The sprite ends at the bottom-right of the object's field.
        const QPoint spriteAt(FieldSize - size.width(), FieldSize - size.height());
        const auto& boxes = m_type->getBoundingBoxes();
        if (m_patternX < static_cast<int>(boxes.size())) {
            const QRect& box = boxes[m_patternX];
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
    if (!m_sprite.info || m_target.isEmpty()) {
        delete oldNode;
        return nullptr;
    }
    const renderer::TSpriteTexture sprite = renderer::TSpriteTextureProviderManager::sharedProvider(window()).texture(m_sprite);
    QSGTexture* texture = sprite.texture;
    if (!texture) {
        // CipSoft pending: keep the prior tree while the sheet
        // decompresses (same class as TAppearanceInstanceRenderer char-select).
        QMetaObject::invokeMethod(this, &QQuickItem::update, Qt::QueuedConnection);
        if (oldNode)
            return oldNode;
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
    // The texture holds the part `sprite.rect` of the sprite drawn at m_target.
    const QSize spriteSize = m_sprite.info->spriteSize();
    const qreal sx = spriteSize.width() > 0 ? m_target.width() / spriteSize.width() : 1.0;
    const qreal sy = spriteSize.height() > 0 ? m_target.height() / spriteSize.height() : 1.0;
    node->setRect(QRectF(m_target.left() + sprite.rect.left() * sx, m_target.top() + sprite.rect.top() * sy,
                         sprite.rect.width() * sx, sprite.rect.height() * sy));
    return node;
}

}
