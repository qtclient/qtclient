#include "appearances/appearanceinstance.h"
#include "singleobjectrenderer.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>
#include "clockservice.h"
#include "appearances/spriteinfo.h"
#include "renderer/spritetextureprovider.h"

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
    : TAppearanceInstanceRendererBaseOfSingleObject(parent)
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
    requestStorageEnabledUpdate();
    changed();
}

void TSingleObjectAppearanceInstanceRenderer::setScaleFactor(const float v)
{
    if (v == m_scaleFactor)
        return;
    cip::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer::setScaleFactor(v);
    requestDrawCommandsUpdate();
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
    TAppearanceInstanceRendererBaseOfSingleObject::componentComplete();
    changed();
}

void TSingleObjectAppearanceInstanceRenderer::itemChange(const ItemChange change, const ItemChangeData& value)
{
    cip::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer::itemChange(change, value);
    if (change == ItemSceneChange)
        watchFrames();
}

/// The shown type, looked up by id each time: the appearances behind it can go
/// away (session end, shutdown) while the item still polishes.
TAppearanceType* TSingleObjectAppearanceInstanceRenderer::type() const
{
    if (m_typeid <= 0 || !tibia::appearances::TAppearancesManager::instance().isValidDatId(static_cast<uint16_t>(m_typeid), ThingCategoryItem))
        return nullptr;
    TAppearanceType* const found = tibia::appearances::TAppearanceInstanceFactory::installed()->appearanceType(ThingCategoryItem, static_cast<uint16_t>(m_typeid));
    return found && !found->isNull() ? found : nullptr;
}

/// The type and its patterns, as the engine picks them for an item of that count,
/// liquid and hook (appearances::objectPatternsFor).
void TSingleObjectAppearanceInstanceRenderer::changed()
{
    TAppearanceType* const appearanceType = type();
    m_patternX = m_patternY = m_patternZ = 0;
    if (appearanceType) {
        const bool liquid = appearanceType->isFluidContainer() || appearanceType->isSplash();
        int x = 0;
        int y = 0;
        int z = 0;
        tibia::appearances::objectPatternsFor(appearanceType, liquid ? static_cast<int>(m_liquidType) : m_cumulativeCount, x, y, z);
        m_patternX = x;
        m_patternY = y;
        m_patternZ = z;
        if (appearanceType->isHangable()) {
            const int patterns = appearanceType->getNumPatternX();
            if (m_hookDirection == Info::EHookDirection::SOUTH)
                m_patternX = patterns >= 2 ? 1 : 0;
            else if (m_hookDirection == Info::EHookDirection::EAST)
                m_patternX = patterns >= 3 ? 2 : 0;
        }
    }
    m_phase = currentPhase();
    watchFrames();
    requestDrawCommandsUpdate();
}

/// Animated objects re-lay out when their phase moves on.
void TSingleObjectAppearanceInstanceRenderer::watchFrames()
{
    TAppearanceType* const appearanceType = type();
    disconnect(m_frameConnection);
    if (!window() || !m_animated || !appearanceType || appearanceType->getAnimationPhases() <= 1)
        return;
    m_frameConnection = connect(window(), &QQuickWindow::afterAnimating, this, [this] {
        if (currentPhase() != m_phase) {
            m_phase = currentPhase();
            requestDrawCommandsUpdate();
        }
    });
}

int TSingleObjectAppearanceInstanceRenderer::currentPhase() const
{
    TAppearanceType* const appearanceType = type();
    if (!m_animated || !appearanceType)
        return 0;
    if (const tibia::animation::TAnimationInfo* info = appearanceType->idleAnimation()) {
        if (!m_animator || m_animatorType != appearanceType) {
            m_animator.emplace(*info, 0);
            m_animatorType = appearanceType;
        }
        m_animator->update(static_cast<qint64>(tibia::shared::TClockService::frameMillis()));
        return m_animator->phase();
    }
    // Without animation info the object does not animate.
    return 0;
}

void TSingleObjectAppearanceInstanceRenderer::updateDrawCommands()
{
    TAppearanceType* const appearanceType = type();
    m_sprite = {};
    m_target = {};
    QRect bounds;
    if (appearanceType && appearanceType->getAnimationPhases() > 0) {
        if (const auto sprite = appearanceType->spriteIdAt(0, m_patternX, m_patternY, m_patternZ, m_phase))
            m_sprite.identifier = std::make_shared<appearances::TSpritesheetAppearanceInstanceSpriteInfo>(*sprite);
    }
    if (m_sprite.identifier) {
        const QSize size(appearanceType->getSize().width() * FieldSize, appearanceType->getSize().height() * FieldSize);
        // The sprite ends at the bottom-right of the object's field.
        const QPoint spriteAt(FieldSize - size.width(), FieldSize - size.height());
        const auto& boxes = appearanceType->getBoundingBoxes();
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
    if (!m_sprite.identifier || m_target.isEmpty()) {
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
    // The texture holds the part `sprite.rect()` of the sprite drawn at m_target.
    const QSize spriteSize = spriteSizeOf(m_sprite);
    const qreal sx = spriteSize.width() > 0 ? m_target.width() / spriteSize.width() : 1.0;
    const qreal sy = spriteSize.height() > 0 ? m_target.height() / spriteSize.height() : 1.0;
    node->setRect(QRectF(m_target.left() + sprite.rect().left() * sx, m_target.top() + sprite.rect().top() * sy,
                         sprite.rect().width() * sx, sprite.rect().height() * sy));
    return node;
}

}
