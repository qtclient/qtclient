#include "appearancetype.h"

#include <appearances.pb.h>

#include <QtGlobal>

#include <algorithm>
#include <array>
#include <numeric>
#include <stdexcept>

namespace tibia::appearances {

namespace {

constexpr std::array<QSize, 4> kSpriteLayoutSizes{ QSize(32, 32), QSize(32, 64), QSize(64, 32), QSize(64, 64) };

/// The market categories of objects that have no market data of their own.
constexpr int kCyclopediaOnlyMarketCategory = 28;

QString fromUtf8(const std::string& text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

uint8_t checkedVocation(const int value)
{
    if (value >= 0 && value < 6)
        return static_cast<uint8_t>(value);
    throw std::runtime_error("unhandled enum value");
}

int checkedMarketCategory(const int value)
{
    if (value >= 1 && value <= 27)
        return value;
    throw std::runtime_error("unhandled enum value");
}

uint32_t checkedWeaponType(const int value)
{
    if (value >= 0 && value <= 8)
        return static_cast<uint32_t>(value);
    throw std::runtime_error("unhandled enum value");
}

/// The lens help ids the client knows.
constexpr bool isKnownLensHelp(const uint32_t id) { return id - 1100u < 13u; }

/// The edge of a map field in pixels.
constexpr int kFieldSize = 32;

tibia::animation::TAnimationInfo animationFrom(const tibia::protobuf::appearances::SpriteAnimation& message)
{
    using L = tibia::animation::TAnimationInfo::ELoopType;
    tibia::animation::TAnimationInfo info;
    info.synchronized = message.synchronized();
    switch (static_cast<int>(message.loop_type())) {
        case -1: info.loopType = L::PingPong; break;
        case 1:
            info.loopType = L::Counted;
            info.loopCount = message.loop_count();
            break;
        default: info.loopType = L::Infinite; break;
    }
    for (const auto& phase : message.sprite_phase())
        info.phases.emplace_back(phase.duration_min(), phase.duration_max());
    info.finish();
    return info;
}

}

namespace {
TSpecialMeaningAppearanceIds gSpecialMeaningIds;
}

const TSpecialMeaningAppearanceIds& specialMeaningAppearanceIds()
{
    return gSpecialMeaningIds;
}

void setSpecialMeaningAppearanceIds(const TSpecialMeaningAppearanceIds& ids)
{
    gSpecialMeaningIds = ids;
}

EFixedFrameGroup fixedFrameGroupFromProtobuf(const uint32_t value)
{
    static constexpr std::array<EFixedFrameGroup, 3> groups{ EFixedFrameGroup::OutfitIdle, EFixedFrameGroup::OutfitMoving,
                                                             EFixedFrameGroup::ObjectInitial };
    return value < groups.size() ? groups[value] : EFixedFrameGroup::Invalid;
}

QSize spriteLayoutSize(const uint32_t layout)
{
    return layout < kSpriteLayoutSizes.size() ? kSpriteLayoutSizes[layout] : QSize(0, 0);
}

// TAppearanceTypeSpriteInfo

TAppearanceTypeSpriteInfo TAppearanceTypeSpriteInfo::fromProtobuf(const tibia::protobuf::appearances::SpriteInfo& message,
                                                                  const TSpriteSizeLookup& spriteSize)
{
    TAppearanceTypeSpriteInfo info;
    info.m_spriteIds.assign(message.sprite_id().begin(), message.sprite_id().end());
    if (!info.m_spriteIds.empty() && spriteSize)
        info.m_spriteSize = spriteSize(info.m_spriteIds.front());

    if (message.has_animation())
        info.m_animation = animationFrom(message.animation());

    // The box a direction without one of its own gets: the whole 32x32 sprite,
    // or the bottom-right square of the bounding square in the sprite.
    QPoint defaultOrigin(0, 0);
    QSize defaultSize = spriteLayoutSize(0);
    if (message.has_bounding_square()) {
        defaultSize = spriteLayoutSize(3);
        if (!info.m_spriteIds.empty())
            defaultSize = info.m_spriteSize;
        const int square = static_cast<int>(message.bounding_square());
        defaultOrigin = QPoint(defaultSize.width() - std::min(defaultSize.width(), square),
                               defaultSize.height() - std::min(defaultSize.height(), square));
    }
    for (const auto& box : message.bounding_box_per_direction()) {
        const int x = static_cast<int>(box.x());
        const int y = static_cast<int>(box.y());
        info.m_boundingBoxes.emplace_back(QPoint(x, y), QPoint(x + static_cast<int>(box.width()) - 1,
                                                               y + static_cast<int>(box.height()) - 1));
    }
    for (uint32_t direction = static_cast<uint32_t>(info.m_boundingBoxes.size()); direction < message.pattern_width(); ++direction)
        info.m_boundingBoxes.emplace_back(defaultOrigin, QPoint(defaultSize.width() - 1, defaultSize.height() - 1));

    info.m_patternWidth = message.pattern_width();
    info.m_patternHeight = message.pattern_height();
    info.m_patternDepth = message.pattern_depth();
    info.m_layers = message.layers();
    info.m_animated = message.has_animation();
    info.m_opaque = message.is_opaque();
    return info;
}

bool TAppearanceTypeSpriteInfo::isValid() const
{
    const uint32_t frame = m_patternHeight * m_patternWidth * m_patternDepth * m_layers;
    if (frame == 0)
        return false;
    return m_spriteIds.size() % frame == 0;
}

void TAppearanceTypeSpriteInfo::setAnimation(std::optional<tibia::animation::TAnimationInfo> animation)
{
    m_animation = std::move(animation);
}

int TAppearanceTypeSpriteInfo::phaseCount() const
{
    return m_animation && m_animation->phaseCount() > 0 ? static_cast<int>(m_animation->phaseCount()) : 1;
}

std::optional<uint32_t> TAppearanceTypeSpriteInfo::spriteId(const int layer, const int x, const int y, const int z,
                                                            const int phase) const
{
    const int width = static_cast<int>(std::max(1u, m_patternWidth));
    const int height = static_cast<int>(std::max(1u, m_patternHeight));
    const int depth = static_cast<int>(std::max(1u, m_patternDepth));
    const int layers = static_cast<int>(std::max(1u, m_layers));
    const int clampedX = std::min(x, width - 1);
    const int clampedY = std::min(y, height - 1);
    const int clampedZ = std::min(z, depth - 1);
    const int clampedPhase = std::min(phase, phaseCount() - 1);
    if (clampedX < 0 || clampedY < 0 || clampedZ < 0 || layer < 0 || layer >= layers || clampedPhase < 0)
        return std::nullopt;
    const size_t index = static_cast<size_t>(
        (((clampedPhase * depth + clampedZ) * height + clampedY) * width + clampedX) * layers + layer);
    if (index >= m_spriteIds.size())
        return std::nullopt;
    return m_spriteIds[index];
}

// TNpcTradeData

uint16_t TNpcTradeData::currencyObjectId() const
{
    if (currency)
        if (const auto* id = std::get_if<uint16_t>(&*currency))
            return *id;
    return 0;
}

QString TNpcTradeData::currencyQuestFlagName() const
{
    if (currency)
        if (const auto* name = std::get_if<QString>(&*currency))
            return *name;
    return {};
}

// TAppearanceType

TAppearanceType TAppearanceType::fromProtobuf(const EAppearanceCategory category,
                                              const tibia::protobuf::appearances::Appearance& appearance,
                                              const TSpriteSizeLookup& spriteSize)
{
    using tibia::protobuf::appearances::AppearanceFlags;

    TAppearanceType type;
    type.m_null = false;
    type.m_category = category;
    type.m_id = static_cast<uint16_t>(appearance.id());

    const AppearanceFlags& flags = appearance.flags();
    type.m_bank = flags.has_bank();
    type.m_waypoints = static_cast<uint16_t>(flags.bank().waypoints());
    type.m_clip = flags.clip();
    type.m_bottom = flags.bottom();
    type.m_top = flags.top();
    type.m_container = flags.container();
    type.m_cumulative = flags.cumulative();
    type.m_forceUse = flags.forceuse();
    type.m_multiUse = flags.multiuse();
    type.m_write = flags.has_write();
    type.m_writeOnce = flags.has_write_once();
    type.m_reportable = flags.reportable();
    type.m_wrap = flags.has_wrap();
    type.m_unwrap = flags.has_unwrap();
    type.m_topEffect = flags.topeffect();
    type.m_corpse = flags.corpse();
    type.m_playerCorpse = flags.player_corpse();
    type.m_liquidPool = flags.liquidpool();
    type.m_liquidContainer = flags.liquidcontainer();
    type.m_unpass = flags.unpass();
    type.m_unmove = flags.unmove();
    type.m_unsight = flags.unsight();
    type.m_avoid = flags.avoid();
    type.m_noMovementAnimation = flags.no_movement_animation();
    type.m_take = flags.take();
    type.m_hang = flags.hang();
    type.m_ammo = flags.ammo();
    type.m_dualWielding = flags.dual_wielding();
    type.m_rotate = flags.rotate();
    type.m_dontHide = flags.dont_hide();
    type.m_translucent = flags.translucent();
    type.m_elevation = static_cast<uint8_t>(flags.height().elevation());
    type.m_displacement = QPoint(static_cast<int>(flags.shift().x()), static_cast<int>(flags.shift().y()));
    type.m_lyingObject = flags.lying_object();
    type.m_fullBank = flags.fullbank();
    type.m_ignoreLook = flags.ignore_look();
    type.m_usable = flags.usable();
    type.m_showOffSocket = flags.show_off_socket();
    type.m_wearOut = flags.wearout();
    type.m_expire = flags.expire() || flags.clockexpire();
    type.m_expireStop = flags.expirestop();
    type.m_reverseAddonsEast = flags.has_reverse_addons_east() && flags.reverse_addons_east();
    type.m_reverseAddonsWest = flags.has_reverse_addons_west() && flags.reverse_addons_west();
    type.m_reverseAddonsNorth = flags.has_reverse_addons_north() && flags.reverse_addons_north();
    type.m_reverseAddonsSouth = flags.has_reverse_addons_south() && flags.reverse_addons_south();
    type.m_decoKit = flags.has_deco_item_kit();
    type.m_animateAlways = flags.animate_always();

    if (appearance.has_name())
        type.m_name = fromUtf8(appearance.name());
    if (appearance.has_description())
        type.m_description = fromUtf8(appearance.description());

    if (flags.has_automap())
        type.m_minimapColor = static_cast<int>(static_cast<uint8_t>(flags.automap().color()));
    if (flags.has_hook())
        type.m_hookDirection = static_cast<int>(flags.hook().direction());
    if (flags.has_clothes())
        type.m_clothSlot = static_cast<uint16_t>(flags.clothes().slot());
    if (flags.has_imbueable())
        type.m_imbueSlots = static_cast<uint8_t>(flags.imbueable().slot_count());
    if (flags.has_light()) {
        type.m_hasLight = true;
        type.m_light = Light(static_cast<uint8_t>(flags.light().brightness()), static_cast<uint8_t>(flags.light().color()));
    }
    if (flags.has_default_action())
        type.m_defaultAction = static_cast<int>(flags.default_action().action());
    if (flags.has_skillwheel_gem())
        type.m_skillWheelGem = std::make_pair(flags.skillwheel_gem().gem_quality_id(), flags.skillwheel_gem().vocation_id());
    if (flags.has_proficiency())
        type.m_proficiencyId = flags.proficiency().proficiency_id();

    // Every frame group; one with a sprite count that does not fit its patterns
    // is replaced by the empty default.
    bool opaque = true;
    for (const auto& group : appearance.frame_group()) {
        TAppearanceTypeSpriteInfo info = TAppearanceTypeSpriteInfo::fromProtobuf(group.sprite_info(), spriteSize);
        if (!info.isValid())
            info = TAppearanceTypeSpriteInfo();
        const EFixedFrameGroup fixed = fixedFrameGroupFromProtobuf(static_cast<uint32_t>(group.fixed_frame_group()));
        opaque = opaque && info.isOpaque();
        type.addFrameGroup(fixed, std::move(info));
    }
    type.m_opaque = opaque;

    if (flags.has_cyclopediaitem()) {
        type.m_cyclopediaType = static_cast<uint16_t>(flags.cyclopediaitem().cyclopedia_type());
        type.m_marketCategory = kCyclopediaOnlyMarketCategory;
    }
    if (flags.has_market() && flags.market().trade_as_object_id() == appearance.id()) {
        const int category = checkedMarketCategory(static_cast<int>(flags.market().category()));
        auto market = std::make_shared<TAppearanceMarketProperties>();
        market->name = appearance.has_name() ? fromUtf8(appearance.name()) : qtTrId("dummy_unknown");
        market->category = category;
        market->showAs = static_cast<uint16_t>(flags.market().show_as_object_id());
        type.m_market = std::move(market);
        type.m_marketCategory = category;
    }
    if (flags.has_minimum_level())
        type.m_minimumLevel = static_cast<uint16_t>(flags.minimum_level());
    for (const int vocation : flags.restrict_to_vocation())
        type.m_restrictVocations.push_back(checkedVocation(vocation));
    if (flags.has_weapon_type())
        type.m_weaponType = checkedWeaponType(static_cast<int>(flags.weapon_type()));

    // NPCs: the currency is kept when it is not gold.
    for (const auto& sale : flags.npcsaledata()) {
        TNpcTradeData data;
        if (sale.has_currency_object_type_id()) {
            if (sale.currency_object_type_id() != specialMeaningAppearanceIds().goldCoin)
                data.currency = static_cast<uint16_t>(sale.currency_object_type_id());
        } else if (sale.has_currency_quest_flag_display_name()) {
            data.currency = fromUtf8(sale.currency_quest_flag_display_name());
        }
        data.name = fromUtf8(sale.name());
        data.locations = QStringList{ fromUtf8(sale.location()) };
        data.salePrice = sale.sale_price();
        data.buyPrice = sale.buy_price();
        type.addNpcTradeData(std::move(data));
    }

    if (flags.has_lenshelp() && isKnownLensHelp(flags.lenshelp().id()))
        type.m_lensHelp = static_cast<int>(flags.lenshelp().id());
    if (flags.has_changedtoexpire())
        type.m_formerObjectId = static_cast<uint16_t>(flags.changedtoexpire().former_object_typeid());
    if (flags.has_upgradeclassification())
        type.m_upgradeClassification = static_cast<uint8_t>(flags.upgradeclassification().upgrade_classification());

    type.updateDerivedSize();
    return type;
}

const std::vector<TNpcTradeData>& TAppearanceType::npcTradeData() const
{
    static const std::vector<TNpcTradeData> none;
    return m_npcTradeData ? *m_npcTradeData : none;
}

void TAppearanceType::addNpcTradeData(TNpcTradeData data)
{
    if (!m_npcTradeData)
        m_npcTradeData = std::make_shared<std::vector<TNpcTradeData>>();
    // One entry per NPC and currency: the places are merged, the NPC buys for
    // the most and sells for the least (0 is "does not sell").
    for (TNpcTradeData& existing : *m_npcTradeData) {
        if (existing.name != data.name || existing.currency != data.currency)
            continue;
        for (QString& location : data.locations) {
            if (!existing.locations.contains(location))
                existing.locations.push_back(std::move(location));
        }
        std::sort(existing.locations.begin(), existing.locations.end());
        existing.buyPrice = std::max(existing.buyPrice, data.buyPrice);
        if (existing.salePrice == 0)
            existing.salePrice = data.salePrice;
        else if (data.salePrice != 0)
            existing.salePrice = std::min(existing.salePrice, data.salePrice);
        return;
    }
    m_npcTradeData->push_back(std::move(data));
}

int TAppearanceType::getMeanPrice() const
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
    const auto& npcs = npcTradeData();
    if (npcs.empty())
        return 0;
    const int total = std::accumulate(npcs.begin(), npcs.end(), 0,
                                      [](const int sum, const TNpcTradeData& npc) { return sum + static_cast<int>(npc.buyPrice); });
    return total / static_cast<int>(npcs.size());
}

uint16_t TAppearanceType::restrictVocationMask() const
{
    uint16_t mask = 0;
    for (const uint32_t vocation : m_restrictVocations) {
        if (vocation > 0 && vocation < 16)
            mask = static_cast<uint16_t>(mask | (1u << (vocation - 1)));
    }
    return mask;
}

uint32_t TAppearanceType::getWeaponType() const
{
    namespace shared = tibia::protobuf::shared;
    switch (m_weaponType) {
        case shared::WEAPON_TYPE_SWORD: return ITEM_CATEGORY_SWORDS;
        case shared::WEAPON_TYPE_AXE: return ITEM_CATEGORY_AXES;
        case shared::WEAPON_TYPE_CLUB: return ITEM_CATEGORY_CLUBS;
        case shared::WEAPON_TYPE_FIST: return ITEM_CATEGORY_FIST_WEAPONS;
        case shared::WEAPON_TYPE_BOW:
        case shared::WEAPON_TYPE_CROSSBOW:
        case shared::WEAPON_TYPE_THROW: return ITEM_CATEGORY_DISTANCE_WEAPONS;
        case shared::WEAPON_TYPE_WAND_ROD: return ITEM_CATEGORY_WANDS_RODS;
        default: return 0;
    }
}

const TAppearanceTypeSpriteInfo& TAppearanceType::frameGroup(const EFixedFrameGroup group) const
{
    const auto it = m_frameGroups.find(group);
    if (it == m_frameGroups.end())
        throw std::logic_error("invalid frame group requested");
    return it->second;
}

void TAppearanceType::addFrameGroup(const EFixedFrameGroup group, TAppearanceTypeSpriteInfo info)
{
    m_frameGroups[group] = std::move(info);
    updateDerivedSize();
}

int TAppearanceType::getNumPatternX() const
{
    int value = 1;
    for (const auto& [group, info] : m_frameGroups)
        value = std::max(value, static_cast<int>(info.patternWidth()));
    return value;
}

int TAppearanceType::getNumPatternY() const
{
    int value = 1;
    for (const auto& [group, info] : m_frameGroups)
        value = std::max(value, static_cast<int>(info.patternHeight()));
    return value;
}

int TAppearanceType::getNumPatternZ() const
{
    int value = 1;
    for (const auto& [group, info] : m_frameGroups)
        value = std::max(value, static_cast<int>(info.patternDepth()));
    return value;
}

int TAppearanceType::getLayers() const
{
    int value = 1;
    for (const auto& [group, info] : m_frameGroups)
        value = std::max(value, static_cast<int>(info.layers()));
    return value;
}

int TAppearanceType::getTotalPhases() const
{
    int phases = 0;
    for (const auto& [group, info] : m_frameGroups)
        phases += info.phaseCount();
    return phases;
}

namespace {

/// The phases of an animation with more than one phase, else 0.
int animatedPhases(const tibia::animation::TAnimationInfo* info)
{
    return info && info->phaseCount() > 1 ? static_cast<int>(info->phaseCount()) : 0;
}

}

int TAppearanceType::getAnimationPhases() const
{
    if (const int moving = animatedPhases(movingAnimation()))
        return moving;
    return getTotalPhases();
}

int TAppearanceType::getIdleAnimationPhases() const
{
    if (const int idle = animatedPhases(idleAnimation()))
        return idle;
    if (const int moving = animatedPhases(movingAnimation()))
        return std::max(0, getTotalPhases() - moving);
    return 0;
}

const tibia::animation::TAnimationInfo* TAppearanceType::idleAnimation() const
{
    for (const auto& [group, info] : m_frameGroups) {
        if (group == EFixedFrameGroup::OutfitMoving)
            continue;
        if (const auto* animation = info.animation())
            return animation;
    }
    return nullptr;
}

const tibia::animation::TAnimationInfo* TAppearanceType::movingAnimation() const
{
    const auto it = m_frameGroups.find(EFixedFrameGroup::OutfitMoving);
    return it != m_frameGroups.end() ? it->second.animation() : nullptr;
}

std::optional<uint32_t> TAppearanceType::spriteIdAt(const int layer, const int x, const int y, const int z,
                                                    const int phase) const
{
    const int total = getTotalPhases();
    if (total <= 0)
        return std::nullopt;
    int remaining = phase % total;
    for (const auto& [group, info] : m_frameGroups) {
        const int phases = info.phaseCount();
        if (remaining < phases)
            return info.spriteId(layer, x, y, z, remaining);
        remaining -= phases;
    }
    return std::nullopt;
}

const std::vector<QRect>& TAppearanceType::getBoundingBoxes() const
{
    static const std::vector<QRect> none;
    return m_frameGroups.empty() ? none : m_frameGroups.begin()->second.boundingBoxes();
}

uint32_t TAppearanceType::firstSpriteId() const
{
    for (const auto& [group, info] : m_frameGroups) {
        if (!info.spriteIds().empty())
            return info.spriteIds().front();
    }
    return 0;
}

void TAppearanceType::updateDerivedSize()
{
    m_size = QSize(1, 1);
    for (const auto& [group, info] : m_frameGroups) {
        const QSize pixels = info.spriteSize();
        if (pixels.isEmpty())
            continue;
        m_size = QSize(std::max(1, pixels.width() / kFieldSize), std::max(1, pixels.height() / kFieldSize));
        return;
    }
}

}
