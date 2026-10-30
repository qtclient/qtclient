#include "thingtype.h"

#include "spritesheet.h"

#include <client/gameconfig.h>

#include <algorithm>
#include <numeric>

namespace {

ThingFlagAttr thingAttrToFlag(const ThingAttr attr)
{
    switch (attr) {
        case ThingAttrDisplacement: return ThingFlagAttrDisplacement;
        case ThingAttrLight: return ThingFlagAttrLight;
        case ThingAttrElevation: return ThingFlagAttrElevation;
        case ThingAttrFloorChange: return ThingFlagAttrFloorChange;
        case ThingAttrGround: return ThingFlagAttrGround;
        case ThingAttrWritable: return ThingFlagAttrWritable;
        case ThingAttrWritableOnce: return ThingFlagAttrWritableOnce;
        case ThingAttrMinimapColor: return ThingFlagAttrMinimapColor;
        case ThingAttrCloth: return ThingFlagAttrCloth;
        case ThingAttrLensHelp: return ThingFlagAttrLensHelp;
        case ThingAttrDefaultAction: return ThingFlagAttrDefaultAction;
        case ThingAttrUsable: return ThingFlagAttrUsable;
        case ThingAttrGroundBorder: return ThingFlagAttrGroundBorder;
        case ThingAttrOnBottom: return ThingFlagAttrOnBottom;
        case ThingAttrOnTop: return ThingFlagAttrOnTop;
        case ThingAttrContainer: return ThingFlagAttrContainer;
        case ThingAttrStackable: return ThingFlagAttrStackable;
        case ThingAttrForceUse: return ThingFlagAttrForceUse;
        case ThingAttrMultiUse: return ThingFlagAttrMultiUse;
        case ThingAttrChargeable: return ThingFlagAttrChargeable;
        case ThingAttrFluidContainer: return ThingFlagAttrFluidContainer;
        case ThingAttrSplash: return ThingFlagAttrSplash;
        case ThingAttrNotWalkable: return ThingFlagAttrNotWalkable;
        case ThingAttrNotMoveable: return ThingFlagAttrNotMoveable;
        case ThingAttrBlockProjectile: return ThingFlagAttrBlockProjectile;
        case ThingAttrNotPathable: return ThingFlagAttrNotPathable;
        case ThingAttrPickupable: return ThingFlagAttrPickupable;
        case ThingAttrHangable: return ThingFlagAttrHangable;
        case ThingAttrHookSouth: return ThingFlagAttrHookSouth;
        case ThingAttrHookEast: return ThingFlagAttrHookEast;
        case ThingAttrRotateable: return ThingFlagAttrRotateable;
        case ThingAttrDontHide: return ThingFlagAttrDontHide;
        case ThingAttrTranslucent: return ThingFlagAttrTranslucent;
        case ThingAttrLyingCorpse: return ThingFlagAttrLyingCorpse;
        case ThingAttrAnimateAlways: return ThingFlagAttrAnimateAlways;
        case ThingAttrFullGround: return ThingFlagAttrFullGround;
        case ThingAttrLook: return ThingFlagAttrLook;
        case ThingAttrWrapable: return ThingFlagAttrWrapable;
        case ThingAttrUnwrapable: return ThingFlagAttrUnwrapable;
        case ThingAttrWearOut: return ThingFlagAttrWearOut;
        case ThingAttrClockExpire: return ThingFlagAttrClockExpire;
        case ThingAttrExpire: return ThingFlagAttrExpire;
        case ThingAttrExpireStop: return ThingFlagAttrExpireStop;
        case ThingAttrPodium: return ThingFlagAttrPodium;
        case ThingAttrTopEffect: return ThingFlagAttrTopEffect;
        case ThingAttrMarket: return ThingFlagAttrMarket;
        case ThingAttrDecoKit: return ThingFlagAttrDecoKit;
        default: break;
    }
    return ThingFlagAttrNone;
}

}

void ThingType::adopt(AppearanceRecord record)
{
    m_record = std::move(record);
    m_castsByFlags.store(-1, std::memory_order_relaxed);
}

void ThingType::setAnimators(std::unique_ptr<Animator> moving, std::unique_ptr<Animator> idle)
{
    m_moving = std::move(moving);
    m_idle = std::move(idle);
}

bool ThingType::hasAttr(const ThingAttr attr) const
{
    return m_record.flags & thingAttrToFlag(attr);
}

int ThingType::getAnimationPhases() const
{
    return m_moving ? m_moving->getAnimationPhases() : m_record.animationPhases;
}

int ThingType::getIdleAnimationPhases() const
{
    if (m_idle)
        return m_idle->getAnimationPhases();
    if (m_moving)
        return std::max(0, m_record.animationPhases - m_moving->getAnimationPhases());
    return 0;
}

int ThingType::getMeanPrice() const
{
    static constexpr std::array<std::pair<uint32_t, uint32_t>, 3> forcedPrices = { {
        { 3043, 10000 },
        { 3031, 1 },
        { 3035, 100 },
    } };
    const uint32_t itemId = getId();
    const auto it = std::ranges::find_if(forcedPrices, [itemId](const auto& pair) { return pair.first == itemId; });
    if (it != forcedPrices.end())
        return static_cast<int>(it->second);
    if (m_record.npcSale.empty())
        return 0;
    const int total = std::accumulate(m_record.npcSale.begin(), m_record.npcSale.end(), 0,
                                       [](const int sum, const NPCData& npc) { return sum + static_cast<int>(npc.buyPrice); });
    return total / static_cast<int>(m_record.npcSale.size());
}

bool ThingType::isTall(const bool useRealSize) const
{
    return useRealSize ? getRealSize() > g_gameConfig.getSpriteSize() : getHeight() > 1;
}

bool ThingType::isLoading() const
{
    if (m_record.sprites.empty())
        return false;
    bool loading = false;
    for (const uint32_t id : m_record.sprites) {
        if (id == 0)
            continue;
        tibia::assets::sprites().sheetBySpriteId(static_cast<int>(id), loading, false);
        if (loading)
            return true;
    }
    return false;
}

uint32_t ThingType::getSpriteIndex(const int w, const int h, const int l, const int x, const int y, const int z, const int a) const
{
    const int phases = std::max(1, m_record.animationPhases);
    uint32_t index;
    if (w == -1 && h == -1) {
        index = static_cast<uint32_t>(((((a % phases) * m_record.numPatternZ + z) * m_record.numPatternY + y) * m_record.numPatternX + x) * m_record.layers + l);
    } else {
        index = static_cast<uint32_t>(((((((a % phases) * m_record.numPatternZ + z) * m_record.numPatternY + y) * m_record.numPatternX + x) * m_record.layers + l) * m_record.size.height() + h) * m_record.size.width() + w);
    }
    return index < m_record.sprites.size() ? index : 0;
}

int ThingType::getExactSize(const int layer, const int xPattern, const int yPattern, const int zPattern, const int animationPhase)
{
    (void)layer;
    (void)yPattern;
    (void)zPattern;
    (void)animationPhase;
    if (m_record.null)
        return 0;
    if (!m_record.boundingBoxes.empty()) {
        const int index = std::clamp(xPattern, 0, static_cast<int>(m_record.boundingBoxes.size()) - 1);
        const Rect& box = m_record.boundingBoxes[static_cast<size_t>(index)];
        return std::max(box.width(), box.height());
    }
    return m_record.realSize > 0 ? m_record.realSize : g_gameConfig.getSpriteSize();
}

bool ThingType::castsShadowByFlags() const
{
    if (isCreature())
        return true;
    if (!isItem() || isGround() || isGroundBorder() || isSplash() || isLyingCorpse() || isHangable())
        return false;
    if (isOnBottom() && hasElevation() && !isNotWalkable())
        return false;
    if (isNotWalkable() && isNotMoveable() && !blockProjectile() && !isOnBottom() && isSingleDimension() && !hasElevation())
        return false;
    return isNotWalkable() || blockProjectile() || !isSingleDimension() || hasElevation();
}

bool ThingType::castsShadow()
{
    const ThingShape shape = getThingShape();
    if (shape == ThingShape::Flat)
        return false;
    if (shape != ThingShape::Auto)
        return true;
    int8_t casts = m_castsByFlags.load(std::memory_order_relaxed);
    if (casts < 0) {
        casts = castsShadowByFlags() ? 1 : 0;
        m_castsByFlags.store(casts, std::memory_order_relaxed);
    }
    return casts > 0;
}
