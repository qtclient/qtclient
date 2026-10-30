#include "things.h"

#include "loader.h"
#include "spritesheet.h"

#include "appearances/appearanceinstance.h"
#include "sprites/sprite.h"

#include <tibia/world/game.h>
#include <client/gameconfig.h>

#include <framework/core/logger.h>
#include <framework/core/resourcemanager.h>
#include <framework/stdext/string.h>

#include <appearances.pb.h>
#include <nlohmann/json.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStringList>

#include <algorithm>
#include <cstring>
#include <limits>

namespace {

const RaceType kEmptyRace{};

std::string trimAssetIdentifier(std::string text)
{
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' '))
        text.pop_back();
    return text;
}

// CipSoft keeps assets.json.sha256 at the package root (sibling of Contents/),
// not under Resources/assets/. Local extract / publish may also stamp it next
// to catalog-content.json.
std::string loadAssetIdentifier(const std::string& directory)
{
    auto tryVirtual = [](const std::string& path) -> std::string {
        try {
            return trimAssetIdentifier(tibia::files::data().readFileContents(
                tibia::files::data().resolvePath(path)));
        } catch (const std::exception&) {
            return {};
        }
    };

    if (auto id = tryVirtual(directory + "assets.json.sha256"); !id.empty())
        return id;
    if (auto id = tryVirtual(tibia::files::data().guessFilePath(directory + "assets", "json.sha256"));
        !id.empty())
        return id;

    QStringList roots;
    const QString appDir = QCoreApplication::applicationDirPath();
#if defined(Q_OS_MACOS)
    roots << QDir(appDir + QStringLiteral("/../..")).canonicalPath();
#endif
    roots << QDir(appDir).canonicalPath();
    roots << QDir(appDir + QStringLiteral("/..")).canonicalPath();
    for (const QString& root : roots) {
        if (root.isEmpty())
            continue;
        QFile file(QDir(root).filePath(QStringLiteral("assets.json.sha256")));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const auto id = trimAssetIdentifier(QString::fromUtf8(file.readAll()).toStdString());
        if (!id.empty())
            return id;
    }
    return {};
}

struct WireField
{
    uint32_t id = 0;
    bool blob = false;
    uint64_t varint = 0;
    const uint8_t* data = nullptr;
    size_t size = 0;
};

bool readVarint(const uint8_t*& cursor, const uint8_t* end, uint64_t& value)
{
    value = 0;
    int shift = 0;
    while (cursor < end && shift <= 63) {
        const uint8_t byte = *cursor++;
        value |= static_cast<uint64_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0)
            return true;
        shift += 7;
    }
    return false;
}

std::vector<WireField> readMessage(const uint8_t* cursor, const uint8_t* end)
{
    std::vector<WireField> fields;
    while (cursor < end) {
        uint64_t tag = 0;
        if (!readVarint(cursor, end, tag))
            break;
        WireField field;
        field.id = static_cast<uint32_t>(tag >> 3);
        const uint32_t wire = static_cast<uint32_t>(tag & 7);
        if (wire == 0) {
            if (!readVarint(cursor, end, field.varint))
                break;
        } else if (wire == 2) {
            uint64_t length = 0;
            if (!readVarint(cursor, end, length) || cursor + length > end)
                break;
            field.blob = true;
            field.data = cursor;
            field.size = static_cast<size_t>(length);
            cursor += length;
        } else if (wire == 5) {
            if (cursor + 4 > end)
                break;
            cursor += 4;
            continue;
        } else if (wire == 1) {
            if (cursor + 8 > end)
                break;
            cursor += 8;
            continue;
        } else {
            break;
        }
        fields.push_back(field);
    }
    return fields;
}

const WireField* findField(const std::vector<WireField>& fields, const uint32_t id)
{
    for (const auto& field : fields)
        if (field.id == id)
            return &field;
    return nullptr;
}

uint64_t fieldVarint(const std::vector<WireField>& fields, const uint32_t id)
{
    const WireField* field = findField(fields, id);
    return field && !field->blob ? field->varint : 0;
}

bool hasField(const std::vector<WireField>& fields, const uint32_t id)
{
    return findField(fields, id) != nullptr;
}

std::string fieldString(const std::vector<WireField>& fields, const uint32_t id)
{
    const WireField* field = findField(fields, id);
    if (!field || !field->blob || !field->data)
        return {};
    return { reinterpret_cast<const char*>(field->data), field->size };
}

std::vector<WireField> subMessage(const std::vector<WireField>& fields, const uint32_t id)
{
    const WireField* field = findField(fields, id);
    if (!field || !field->blob)
        return {};
    return readMessage(field->data, field->data + field->size);
}

void addFlag(AppearanceRecord& record, const uint64_t flag)
{
    record.flags |= flag;
}

void applyFlags(AppearanceRecord& record, const tibia::protobuf::appearances::AppearanceFlags& flags)
{
    using namespace tibia::protobuf;

    if (flags.has_bank()) {
        record.groundSpeed = static_cast<int>(flags.bank().waypoints());
        addFlag(record, ThingFlagAttrGround);
    }
    if (flags.clip())
        addFlag(record, ThingFlagAttrGroundBorder);
    if (flags.bottom())
        addFlag(record, ThingFlagAttrOnBottom);
    if (flags.top())
        addFlag(record, ThingFlagAttrOnTop);
    if (flags.container())
        addFlag(record, ThingFlagAttrContainer);
    if (flags.cumulative())
        addFlag(record, ThingFlagAttrStackable);
    if (flags.multiuse())
        addFlag(record, ThingFlagAttrMultiUse);
    if (flags.forceuse())
        addFlag(record, ThingFlagAttrForceUse);
    if (flags.usable())
        addFlag(record, ThingFlagAttrUsable);
    if (flags.has_write()) {
        addFlag(record, ThingFlagAttrWritable);
        record.maxTextLength = static_cast<int>(flags.write().max_text_length());
    }
    if (flags.has_write_once()) {
        addFlag(record, ThingFlagAttrWritableOnce);
        record.maxTextLength = static_cast<int>(flags.write_once().max_text_length_once());
    }
    if (flags.liquidpool())
        addFlag(record, ThingFlagAttrSplash);
    if (flags.unpass())
        addFlag(record, ThingFlagAttrNotWalkable);
    if (flags.unmove())
        addFlag(record, ThingFlagAttrNotMoveable);
    if (flags.unsight())
        addFlag(record, ThingFlagAttrBlockProjectile);
    if (flags.avoid())
        addFlag(record, ThingFlagAttrNotPathable);
    if (flags.no_movement_animation())
        addFlag(record, ThingFlagAttrNoMovementAnimation);
    if (flags.take())
        addFlag(record, ThingFlagAttrPickupable);
    if (flags.liquidcontainer())
        addFlag(record, ThingFlagAttrFluidContainer);
    if (flags.hang())
        addFlag(record, ThingFlagAttrHangable);
    if (flags.has_hook()) {
        const auto direction = flags.hook().direction();
        if (direction == shared::HOOK_TYPE_SOUTH)
            addFlag(record, ThingFlagAttrHookSouth);
        else if (direction == shared::HOOK_TYPE_EAST)
            addFlag(record, ThingFlagAttrHookEast);
    }
    if (flags.has_light()) {
        addFlag(record, ThingFlagAttrLight);
        record.light = { static_cast<uint8_t>(std::min<uint32_t>(flags.light().brightness(), 255)),
                         static_cast<uint8_t>(std::min<uint32_t>(flags.light().color(), 255)) };
    }
    if (flags.rotate())
        addFlag(record, ThingFlagAttrRotateable);
    if (flags.dont_hide())
        addFlag(record, ThingFlagAttrDontHide);
    if (flags.translucent())
        addFlag(record, ThingFlagAttrTranslucent);
    if (flags.has_shift()) {
        record.displacement = Point(static_cast<int>(flags.shift().x()), static_cast<int>(flags.shift().y()));
        addFlag(record, ThingFlagAttrDisplacement);
    }
    if (flags.has_height()) {
        record.elevation = static_cast<int>(flags.height().elevation());
        addFlag(record, ThingFlagAttrElevation);
    }
    if (flags.lying_object())
        addFlag(record, ThingFlagAttrLyingCorpse);
    if (flags.animate_always())
        addFlag(record, ThingFlagAttrAnimateAlways);
    if (flags.has_automap()) {
        record.minimapColor = static_cast<int>(flags.automap().color());
        addFlag(record, ThingFlagAttrMinimapColor);
    }
    if (flags.has_lenshelp()) {
        record.lensHelp = static_cast<int>(flags.lenshelp().id());
        addFlag(record, ThingFlagAttrLensHelp);
    }
    if (flags.fullbank())
        addFlag(record, ThingFlagAttrFullGround);
    if (flags.ignore_look())
        addFlag(record, ThingFlagAttrLook);
    if (flags.has_clothes()) {
        record.clothSlot = static_cast<int>(flags.clothes().slot());
        addFlag(record, ThingFlagAttrCloth);
    }
    if (flags.has_market()) {
        record.market.category = static_cast<ITEM_CATEGORY>(flags.market().category());
        record.market.tradeAs = static_cast<uint16_t>(flags.market().trade_as_object_id());
        record.market.showAs = static_cast<uint16_t>(flags.market().show_as_object_id());
        record.market.name = record.name;
        record.market.requiredLevel = static_cast<uint16_t>(std::min<uint32_t>(flags.minimum_level(), 65535));
        addFlag(record, ThingFlagAttrMarket);
    }
    for (int i = 0; i < flags.restrict_to_vocation_size(); ++i) {
        const auto vocation = static_cast<int32_t>(flags.restrict_to_vocation(i));
        record.restrictVocation.push_back(static_cast<uint32_t>(vocation));
        if (vocation > 0 && vocation < 16)
            record.market.restrictVocation = static_cast<uint16_t>(record.market.restrictVocation | (1u << (vocation - 1)));
    }
    if (flags.has_minimum_level())
        record.minimumLevel = flags.minimum_level();
    if (flags.wrap())
        addFlag(record, ThingFlagAttrWrapable);
    if (flags.unwrap())
        addFlag(record, ThingFlagAttrUnwrapable);
    if (flags.topeffect())
        addFlag(record, ThingFlagAttrTopEffect);
    if (flags.has_default_action()) {
        record.defaultAction = static_cast<PLAYER_ACTION>(flags.default_action().action());
        addFlag(record, ThingFlagAttrDefaultAction);
    }
    for (const auto& sale : flags.npcsaledata()) {
        NPCData data;
        data.name = sale.name();
        data.location = sale.location();
        data.salePrice = sale.sale_price();
        data.buyPrice = sale.buy_price();
        data.currencyObjectTypeId = sale.currency_object_type_id();
        data.currencyQuestFlagDisplayName = sale.currency_quest_flag_display_name();
        record.npcSale.push_back(std::move(data));
    }
    if (!record.npcSale.empty())
        addFlag(record, ThingFlagAttrNPC);
    if (flags.corpse())
        addFlag(record, ThingFlagAttrCorpse);
    if (flags.player_corpse())
        addFlag(record, ThingFlagAttrPlayerCorpse);
    if (flags.has_cyclopediaitem()) {
        record.cyclopediaType = flags.cyclopediaitem().cyclopedia_type();
        addFlag(record, ThingFlagAttrCyclopediaItem);
    }
    if (flags.ammo())
        addFlag(record, ThingFlagAttrAmmo);
    if (flags.show_off_socket())
        addFlag(record, ThingFlagAttrPodium);
    if (flags.reportable())
        addFlag(record, ThingFlagAttrReportable);
    if (flags.has_upgradeclassification())
        record.upgradeClassification = static_cast<int>(flags.upgradeclassification().upgrade_classification());
    if (flags.wearout())
        addFlag(record, ThingFlagAttrWearOut);
    if (flags.clockexpire())
        addFlag(record, ThingFlagAttrClockExpire);
    if (flags.expire())
        addFlag(record, ThingFlagAttrExpire);
    if (flags.expirestop())
        addFlag(record, ThingFlagAttrExpireStop);
    if (flags.deco_item_kit())
        addFlag(record, ThingFlagAttrDecoKit);
    if (flags.has_proficiency() && g_game.getFeature(Otc::GameProficiency)) {
        record.proficiencyId = flags.proficiency().proficiency_id();
        addFlag(record, ThingFlagAttrProficiency);
    }
    if (flags.has_skillwheel_gem()) {
        record.gemQualityId = flags.skillwheel_gem().gem_quality_id();
        record.gemVocationId = flags.skillwheel_gem().vocation_id();
        addFlag(record, ThingFlagAttrSkillWheelGem);
    }
    if (flags.dual_wielding())
        addFlag(record, ThingFlagAttrDualWield);
    if (flags.has_imbueable()) {
        record.imbueSlots = flags.imbueable().slot_count();
        addFlag(record, ThingFlagAttrImbueable);
    }
    if (flags.has_weapon_type()) {
        const auto weapon = flags.weapon_type();
        record.appearanceWeaponType = static_cast<uint32_t>(weapon);
        if (weapon == shared::WEAPON_TYPE_SWORD)
            record.weaponType = ITEM_CATEGORY_SWORDS;
        else if (weapon == shared::WEAPON_TYPE_AXE)
            record.weaponType = ITEM_CATEGORY_AXES;
        else if (weapon == shared::WEAPON_TYPE_CLUB)
            record.weaponType = ITEM_CATEGORY_CLUBS;
        else if (weapon == shared::WEAPON_TYPE_FIST)
            record.weaponType = ITEM_CATEGORY_FIST_WEAPONS;
        else if (weapon == shared::WEAPON_TYPE_BOW || weapon == shared::WEAPON_TYPE_CROSSBOW || weapon == shared::WEAPON_TYPE_THROW)
            record.weaponType = ITEM_CATEGORY_DISTANCE_WEAPONS;
        else if (weapon == shared::WEAPON_TYPE_WAND_ROD)
            record.weaponType = ITEM_CATEGORY_WANDS_RODS;
    }
}

struct FrameGroupInfo
{
    int start = 0;
    int numPatternX = 1;
    int numPatternY = 1;
    int numPatternZ = 1;
    int layers = 1;
    int phases = 1;
};

void fillSprites(AppearanceRecord& record, std::unique_ptr<Animator>& moving, std::unique_ptr<Animator>& idle,
                 const tibia::protobuf::appearances::Appearance& appearance)
{
    record.animationPhases = 0;
    int totalSprites = 0;
    std::vector<FrameGroupInfo> groups;
    bool sizeSet = false;

    for (const auto& frameGroup : appearance.frame_group()) {
        const auto& spriteInfo = frameGroup.sprite_info();
        const int patternX = std::max(1, static_cast<int>(spriteInfo.pattern_width()));
        const int patternY = std::max(1, static_cast<int>(spriteInfo.pattern_height()));
        const int patternZ = std::max(1, static_cast<int>(spriteInfo.pattern_depth()));
        const int layers = std::max(1, static_cast<int>(spriteInfo.layers()));
        const int phases = std::max(1, spriteInfo.animation().sprite_phase_size());
        record.numPatternX = patternX;
        record.numPatternY = patternY;
        record.numPatternZ = patternZ;
        record.layers = layers;
        record.opaque = spriteInfo.is_opaque() ? 1 : 0;
        record.animationPhases += phases;

        if (groups.empty()) {
            for (const auto& box : spriteInfo.bounding_box_per_direction())
                record.boundingBoxes.emplace_back(static_cast<int>(box.x()), static_cast<int>(box.y()),
                                                   static_cast<int>(box.width()), static_cast<int>(box.height()));
        }
        if (!sizeSet && spriteInfo.sprite_id_size() > 0) {
            bool loading = false;
            if (const auto sheet = tibia::assets::sprites().sheetBySpriteId(static_cast<int>(spriteInfo.sprite_id(0)), loading, false)) {
                const Size pixels = sheet->getSpriteSize();
                const int spriteSize = std::max(1, static_cast<int>(g_gameConfig.getSpriteSize()));
                record.size = Size(std::max(1, pixels.width() / spriteSize), std::max(1, pixels.height() / spriteSize));
                record.realSize = std::min(255, std::max(pixels.width(), pixels.height()));
                sizeSet = true;
            }
        }
        if (phases > 1) {
            auto animator = std::make_unique<Animator>();
            animator->unserializeAppearance(spriteInfo.animation());
            const auto group = frameGroup.fixed_frame_group();
            if (group == tibia::protobuf::appearances::FIXED_FRAME_GROUP_OUTFIT_MOVING)
                moving = std::move(animator);
            else
                idle = std::move(animator);
        }

        const int count = layers * patternX * patternY * patternZ * phases;
        if (totalSprites + count > 4096) {
            g_logger.error("appearance {} has more than 4096 sprites", record.id);
            break;
        }
        const int available = spriteInfo.sprite_id_size();
        const int start = totalSprites;
        record.sprites.resize(static_cast<size_t>(totalSprites + count));
        for (int sprite = 0; sprite < count; ++sprite) {
            record.sprites[static_cast<size_t>(totalSprites + sprite)] = sprite < available ? spriteInfo.sprite_id(sprite) : 0;
        }
        groups.push_back({ start, patternX, patternY, patternZ, layers, phases });
        totalSprites += count;
    }

    if (groups.size() < 2)
        return;

    int targetX = 1, targetY = 1, targetZ = 1, targetLayers = 1;
    for (const auto& group : groups) {
        targetX = std::max(targetX, group.numPatternX);
        targetY = std::max(targetY, group.numPatternY);
        targetZ = std::max(targetZ, group.numPatternZ);
        targetLayers = std::max(targetLayers, group.layers);
    }
    bool mismatch = false;
    for (const auto& group : groups) {
        if (group.numPatternX != targetX || group.numPatternY != targetY || group.numPatternZ != targetZ || group.layers != targetLayers)
            mismatch = true;
    }
    if (!mismatch)
        return;

    const auto old = std::move(record.sprites);
    record.sprites.clear();
    record.sprites.reserve(static_cast<size_t>(record.animationPhases * targetX * targetY * targetZ * targetLayers));
    for (const auto& group : groups) {
        for (int phase = 0; phase < group.phases; ++phase) {
            for (int z = 0; z < targetZ; ++z) {
                for (int y = 0; y < targetY; ++y) {
                    for (int x = 0; x < targetX; ++x) {
                        for (int layer = 0; layer < targetLayers; ++layer) {
                            const int srcX = std::min(x, group.numPatternX - 1);
                            const int srcY = std::min(y, group.numPatternY - 1);
                            const int srcZ = std::min(z, group.numPatternZ - 1);
                            const int srcLayer = std::min(layer, group.layers - 1);
                            const int src = group.start + (((phase * group.numPatternZ + srcZ) * group.numPatternY + srcY) * group.numPatternX + srcX) * group.layers + srcLayer;
                            record.sprites.push_back(src >= 0 && static_cast<size_t>(src) < old.size() ? old[static_cast<size_t>(src)] : 0);
                        }
                    }
                }
            }
        }
    }
    record.numPatternX = targetX;
    record.numPatternY = targetY;
    record.numPatternZ = targetZ;
    record.layers = targetLayers;
}

Outfit outfitFrom(const std::vector<WireField>& fields)
{
    Outfit outfit;
    const auto colors = subMessage(fields, 2);
    const auto lookItem = static_cast<uint16_t>(fieldVarint(fields, 4));
    if (lookItem != 0) {
        outfit.setAuxId(lookItem);
    } else {
        outfit.setId(static_cast<uint16_t>(fieldVarint(fields, 1)));
        outfit.setAddons(static_cast<uint8_t>(fieldVarint(fields, 3)));
        if (!colors.empty()) {
            outfit.setHead(static_cast<uint8_t>(fieldVarint(colors, 1)));
            outfit.setBody(static_cast<uint8_t>(fieldVarint(colors, 2)));
            outfit.setLegs(static_cast<uint8_t>(fieldVarint(colors, 3)));
            outfit.setFeet(static_cast<uint8_t>(fieldVarint(colors, 4)));
        }
    }
    return outfit;
}

RaceType creatureFrom(const WireField& field, const bool boss, const bool official)
{
    const auto fields = readMessage(field.data, field.data + field.size);
    RaceType race;
    race.raceId = static_cast<uint32_t>(fieldVarint(fields, 1));
    race.name = fieldString(fields, 2);
    race.outfit = outfitFrom(subMessage(fields, 3));
    race.boss = boss;
    if (official) {
        race.hasCategory = hasField(fields, 6);
        race.category = static_cast<uint32_t>(fieldVarint(fields, 6));
    } else {
        race.hasCategory = hasField(fields, 4);
        race.category = static_cast<uint32_t>(fieldVarint(fields, 4));
    }
    return race;
}

nlohmann::json catalogOf(const std::string& directory)
{
    const auto path = tibia::files::data().resolvePath(tibia::files::data().guessFilePath(directory + "catalog-content", "json"));
    return nlohmann::json::parse(tibia::files::data().readFileContents(path));
}

std::string catalogFile(const nlohmann::json& catalog, const std::string& type)
{
    for (const auto& entry : catalog) {
        if (entry.value("type", std::string{}) == type)
            return entry.value("file", std::string{});
    }
    return {};
}

}

ThingTypeManager g_things;

void ThingTypeManager::init()
{
    if (!m_nullThingType)
        m_nullThingType = std::make_shared<ThingType>();
    for (auto& things : m_thingTypes)
        things.assign(1, m_nullThingType);
}

void ThingTypeManager::terminate()
{
    for (auto& things : m_thingTypes)
        things.clear();
    m_nullThingType.reset();
    m_monsterRaces.clear();
    m_cyclopediaHouses.clear();
    m_cyclopediaAchievements.clear();
    m_cyclopediaQuests.clear();
    m_proficiencyThingsCache.clear();
    m_proficienciesFile.clear();
    m_effectVariants.clear();
    m_datLoaded = false;
    tibia::assets::sprites().clear();
}

bool ThingTypeManager::loadAppearances(const std::string& directory)
{
    try {
        if (!m_nullThingType)
            init();
        // CIP appearances::TAppearancesManager owns the catalog load path.
        auto& appearancesManager = tibia::appearances::TAppearancesManager::instance();
        (void)appearancesManager.tracker();
        (void)appearancesManager.storages();
        (void)appearancesManager.genericStorage();
        m_assetIdentifier = loadAssetIdentifier(directory);
        if (m_assetIdentifier.empty()) {
            // Expected for bare --data extracts until make extract stamps the file.
            m_assetIdentifier = "appearancesHash";
            g_logger.debug("assets.json.sha256 missing; using fallback appearancesHash");
        }

        tibia::assets::sprites().clear();
        const auto catalog = catalogOf(directory);
        std::string appearancesFile;
        int spritesCount = 0;
        for (const auto& entry : catalog) {
            const auto type = entry.value("type", std::string{});
            if (type == "appearances") {
                appearancesFile = entry.value("file", std::string{});
            } else if (type == "sprite") {
                int lastSpriteId = entry.value("lastspriteid", 0);
                const std::string file = entry.value("file", std::string{});
                // CIP sprites::TAssetSpritesheetIdentifier / TSpritesheetsMetadataManager.
                const tibia::sprites::TAssetSpritesheetIdentifier sheetId(file);
                auto sheet = std::make_shared<SpriteSheet>(entry.value("firstspriteid", 0), lastSpriteId,
                                                           static_cast<SpriteLayout>(entry.value("spritetype", 0)),
                                                           sheetId.file());
                const int maxSpriteId = sheet->firstId + sheet->getSpritesPerSheet() - 1;
                if (lastSpriteId > maxSpriteId) {
                    lastSpriteId = maxSpriteId;
                    sheet->lastId = maxSpriteId;
                }
                tibia::assets::sprites().addSheet(sheet);
                spritesCount = std::max(spritesCount, lastSpriteId);
            }
        }
        tibia::assets::sprites().setSpritesCount(spritesCount + 1);
        tibia::assets::sprites().setPath(directory);
        (void)tibia::sprites::TSpritesheetsMetadataManager::instance().spritesCount();
        if (appearancesFile.empty())
            throw stdext::exception("catalog has no appearances file");

        const auto bytes = tibia::files::data().readFileContents(tibia::files::data().resolvePath(directory + appearancesFile));
        tibia::protobuf::appearances::Appearances appearances;
        if (!appearances.ParseFromString(bytes))
            throw stdext::exception("Couldn't parse appearances lib.");

        const auto& special = appearances.special_meaning_appearance_ids();
        m_supplyStashId = special.supply_stash_id();
        m_standardRewardChestId = special.standard_reward_chest_id();
        m_stampedLetterId = special.stamped_letter_id();

        for (int category = ThingCategoryItem; category < ThingLastCategory; ++category) {
            const google::protobuf::RepeatedPtrField<tibia::protobuf::appearances::Appearance>* bank = nullptr;
            switch (category) {
                case ThingCategoryItem: bank = &appearances.object(); break;
                case ThingCategoryCreature: bank = &appearances.outfit(); break;
                case ThingCategoryEffect: bank = &appearances.effect(); break;
                case ThingCategoryMissile: bank = &appearances.missile(); break;
                default: return false;
            }
            uint32_t lastId = 0;
            for (const auto& appearance : *bank)
                lastId = std::max(lastId, appearance.id());
            if (lastId > std::numeric_limits<uint16_t>::max())
                lastId = std::numeric_limits<uint16_t>::max();
            auto& things = m_thingTypes[category];
            things.assign(lastId + 1, m_nullThingType);
            for (const auto& appearance : *bank) {
                if (appearance.id() == 0 || appearance.id() > lastId)
                    continue;
                AppearanceRecord record;
                record.null = false;
                record.id = static_cast<uint16_t>(appearance.id());
                record.category = static_cast<ThingCategory>(category);
                record.name = appearance.name();
                record.description = appearance.description();
                if (!record.description.empty() && !stdext::is_valid_utf8(record.description))
                    record.description = stdext::latin1_to_utf8(record.description);
                if (appearance.has_flags())
                    applyFlags(record, appearance.flags());
                std::unique_ptr<Animator> moving;
                std::unique_ptr<Animator> idle;
                fillSprites(record, moving, idle, appearance);
                auto type = std::make_shared<ThingType>();
                type->adopt(std::move(record));
                type->setAnimators(std::move(moving), std::move(idle));
                if (category == ThingCategoryItem) {
                    if (const auto it = m_thingShapes.find(type->getId()); it != m_thingShapes.end())
                        type->setThingShape(it->second);
                }
                // CIP sprites::TAppearanceTypeSpritesheetMapping — appearance id → first sheet sprite.
                const auto& spriteIds = type->getSprites();
                if (!spriteIds.empty()) {
                    (void)tibia::sprites::TAppearanceTypeSpritesheetMapping(type->getId(), spriteIds.front());
                }
                things[type->getId()] = std::move(type);
            }
        }
        m_datLoaded = true;
        m_proficiencyThingsCacheDirty = true;
        return true;
    } catch (const std::exception& e) {
        g_logger.error("Failed to load '{}' (Appearances): {}", directory, e.what());
        return false;
    }
}

bool ThingTypeManager::loadStaticData(const std::string& directory)
{
    try {
        const auto catalog = catalogOf(directory);
        const auto fileName = catalogFile(catalog, "staticdata");
        if (fileName.empty())
            throw stdext::exception("catalog has no staticdata file");
        const auto bytes = tibia::files::data().readFileContents(tibia::files::data().resolvePath(directory + fileName));
        const auto top = readMessage(reinterpret_cast<const uint8_t*>(bytes.data()),
                                      reinterpret_cast<const uint8_t*>(bytes.data()) + bytes.size());
        const bool official = hasField(top, 6);
        m_monsterRaces.clear();
        const auto takeCreatures = [&](const uint32_t field, const bool boss) {
            for (const auto& entry : top) {
                if (entry.id == field && entry.blob)
                    m_monsterRaces.push_back(creatureFrom(entry, boss, official));
            }
        };
        takeCreatures(1, false);
        takeCreatures(official ? 5 : 4, true);

        m_cyclopediaHouses.clear();
        for (const auto& entry : top) {
            if (entry.id != (official ? 4u : 3u) || !entry.blob)
                continue;
            const auto fields = readMessage(entry.data, entry.data + entry.size);
            const auto position = subMessage(fields, 6);
            m_cyclopediaHouses.push_back({ static_cast<uint32_t>(fieldVarint(fields, 1)), fieldString(fields, 2), fieldString(fields, 3),
                                           static_cast<uint32_t>(fieldVarint(fields, 4)), static_cast<uint32_t>(fieldVarint(fields, 5)),
                                           Position(static_cast<uint16_t>(fieldVarint(position, 1)), static_cast<uint16_t>(fieldVarint(position, 2)),
                                                    static_cast<uint8_t>(fieldVarint(position, 3))),
                                           static_cast<uint32_t>(fieldVarint(fields, 7)), fieldVarint(fields, 8) != 0, fieldString(fields, 9),
                                           fieldVarint(fields, 10) != 0 });
        }

        m_cyclopediaAchievements.clear();
        for (const auto& entry : top) {
            if (entry.id != (official ? 3u : 2u) || !entry.blob)
                continue;
            const auto fields = readMessage(entry.data, entry.data + entry.size);
            m_cyclopediaAchievements.push_back({ static_cast<uint16_t>(fieldVarint(fields, 1)), fieldString(fields, 2), fieldString(fields, 3),
                                                 static_cast<uint8_t>(fieldVarint(fields, 4)) });
        }

        // Official: 6=quests; OTC remap: 5=quests (CIP Team Finder activity list).
        m_cyclopediaQuests.clear();
        for (const auto& entry : top) {
            if (entry.id != (official ? 6u : 5u) || !entry.blob)
                continue;
            const auto fields = readMessage(entry.data, entry.data + entry.size);
            const auto id = static_cast<uint16_t>(fieldVarint(fields, 1));
            const auto name = fieldString(fields, 2);
            if (id && !name.empty())
                m_cyclopediaQuests.push_back({ id, name });
        }
        return true;
    } catch (const std::exception& e) {
        g_logger.error("Failed to load '{}' (StaticData): {}", directory, e.what());
        return false;
    }
}

bool ThingTypeManager::resolveProficienciesFile(const std::string& directory)
{
    m_proficienciesFile.clear();
    if (!g_game.getFeature(Otc::GameProficiency))
        return false;
    try {
        const auto fileName = catalogFile(catalogOf(directory), "proficiencies");
        if (fileName.empty())
            return false;
        const auto path = directory + fileName;
        if (!tibia::files::data().fileExists(path))
            return false;
        m_proficienciesFile = path;
        return true;
    } catch (const std::exception&) {
        m_proficienciesFile.clear();
        return false;
    }
}

const ThingTypePtr& ThingTypeManager::getThingType(const uint16_t id, const ThingCategory category)
{
    static const ThingTypePtr kEmpty;
    if (category >= ThingLastCategory || !m_nullThingType || m_thingTypes[category].empty())
        return m_nullThingType ? m_nullThingType : kEmpty;
    if (id >= m_thingTypes[category].size()) {
        g_logger.error("Invalid thing type client id {} in category {}", id, static_cast<uint8_t>(category));
        return m_nullThingType;
    }
    return m_thingTypes[category][id];
}

ThingType* ThingTypeManager::getRawThingType(const uint16_t id, const ThingCategory category)
{
    if (category >= ThingLastCategory || !m_nullThingType || m_thingTypes[category].empty() || id >= m_thingTypes[category].size())
        return nullptr;
    return m_thingTypes[category][id].get();
}

const ThingTypeList& ThingTypeManager::getThingTypes(const ThingCategory category)
{
    if (category < ThingLastCategory)
        return m_thingTypes[category];
    throw stdext::exception("invalid thing type category");
}

const RaceType& ThingTypeManager::getRaceData(const uint32_t raceId)
{
    for (const auto& race : m_monsterRaces)
        if (race.raceId == raceId)
            return race;
    return kEmptyRace;
}

RaceList ThingTypeManager::getRacesByName(const std::string& searchString)
{
    RaceList result;
    for (const auto& race : m_monsterRaces) {
        if (race.name.find(searchString) != std::string::npos)
            result.push_back(race);
    }
    return result;
}

const CyclopediaQuestType* ThingTypeManager::getCyclopediaQuest(const uint16_t id) const
{
    for (const auto& quest : m_cyclopediaQuests) {
        if (quest.id == id)
            return &quest;
    }
    return nullptr;
}

void ThingTypeManager::buildProficiencyCache()
{
    m_proficiencyThingsCache.clear();
    if (ThingCategoryItem < ThingLastCategory) {
        for (const auto& type : m_thingTypes[ThingCategoryItem]) {
            if (type && type->getProficiencyId() > 0)
                m_proficiencyThingsCache.push_back(type);
        }
    }
    m_proficiencyThingsCacheDirty = false;
}

const ThingTypeList& ThingTypeManager::getProficiencyThings()
{
    if (m_proficiencyThingsCacheDirty)
        buildProficiencyCache();
    return m_proficiencyThingsCache;
}

std::string ThingTypeManager::getCyclopediaItemName(const uint16_t id)
{
    const auto& type = getThingType(id, ThingCategoryItem);
    if (!type || type->isNull())
        return {};
    if (!type->getMarketData().name.empty())
        return type->getMarketData().name;
    return type->getName();
}

std::string ThingTypeManager::getProficienciesFile()
{
    if (!g_game.getFeature(Otc::GameProficiency))
        return {};
    if (!m_proficienciesFile.empty() && tibia::files::data().fileExists(m_proficienciesFile))
        return m_proficienciesFile;
    return {};
}

bool ThingTypeManager::setThingShape(const uint16_t itemId, const std::string& shape)
{
    static constexpr std::pair<std::string_view, ThingShape> names[] = {
        { "auto", ThingShape::Auto }, { "flat", ThingShape::Flat }, { "wall", ThingShape::Wall },
        { "box", ThingShape::Box }, { "round", ThingShape::Round }, { "window", ThingShape::Window },
        { "ridge", ThingShape::Ridge },
    };
    const auto it = std::ranges::find(names, shape, &std::pair<std::string_view, ThingShape>::first);
    if (it == std::end(names))
        return false;
    m_thingShapes[itemId] = it->second;
    if (isValidDatId(itemId, ThingCategoryItem)) {
        if (const auto& type = m_thingTypes[ThingCategoryItem][itemId]; type && !type->isNull())
            type->setThingShape(it->second);
    }
    return true;
}

void ThingTypeManager::registerEffectVariant(const uint16_t virtualId, const uint16_t baseId, const std::string& shader, const bool underCreatures)
{
    if (virtualId == 0 || baseId == 0) {
        g_logger.warning("registerEffectVariant: invalid ids {} -> {}", virtualId, baseId);
        return;
    }
    m_effectVariants[virtualId] = EffectVariant{ baseId, shader, underCreatures };
}

namespace tibia::assets {

bool load(const std::string& directory)
{
    return g_things.loadAppearances(directory);
}

bool loadStaticData(const std::string& directory)
{
    return g_things.loadStaticData(directory);
}

bool resolveProficiencies(const std::string& directory)
{
    return g_things.resolveProficienciesFile(directory);
}

}
