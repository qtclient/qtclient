#include <QString>
#include <QUtf8StringView>
#include <stdexcept>
#include "things.h"

#include "loader.h"

#include "cyclopedia/staticcontentinformation.h"

#include "appearances/appearanceinstance.h"
#include "appearances/appearanceinstancetracker.h"
#include "assets/assetasyncloader.h"
#include "sprites/appearancetypespritesheetmapping.h"
#include "sprites/spritesystem.h"

#include <tibia/game/gamesession.h>
#include <tibia/config/runtimegameconfig.h>

#include <QtGlobal>
#include <format>
#include "shared/tibiafilesystemhelper.h"

#include <appearances.pb.h>
#include <staticdata.pb.h>
#include "assets/contentcatalog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

Outfit outfitFrom(const tibia::protobuf::staticdata::Outfit& message)
{
    Outfit outfit;
    outfit.setId(static_cast<uint16_t>(message.look_type()));
    outfit.setAuxId(static_cast<uint16_t>(message.look_type_ex()));
    outfit.setHead(static_cast<uint8_t>(message.colors().head()));
    outfit.setBody(static_cast<uint8_t>(message.colors().body()));
    outfit.setLegs(static_cast<uint8_t>(message.colors().legs()));
    outfit.setFeet(static_cast<uint8_t>(message.colors().feet()));
    outfit.setAddons(static_cast<uint8_t>(message.addons() & 3));
    return outfit;
}

/// The protobuf difficulty / occurrence values map one to one; others are rejected.
uint8_t checkedEnum(const uint32_t value, const uint32_t count)
{
    if (value >= count)
        throw std::runtime_error("unhandled enum value");
    return static_cast<uint8_t>(value);
}

QJsonDocument catalogDocument(const std::string& directory)
{
    const auto path = tibia::files::data().resolvePath(tibia::files::data().guessFilePath(directory + "catalog-content", "json"));
    const auto bytes = tibia::files::data().readFileContents(path);
    return QJsonDocument::fromJson(QByteArray::fromStdString(bytes));
}

std::string catalogFile(const tibia::assets::TAssetCatalog& catalog, const tibia::assets::TAsset::EType type)
{
    return catalog.fileOf(type).toStdString();
}

std::string proficienciesFileOf(const std::string& directory)
{
    tibia::assets::TAssetCatalog catalog;
    catalog.load(catalogDocument(directory));
    return catalogFile(catalog, tibia::assets::TAsset::Proficiencies);
}

}

namespace tibia::appearances {

TAppearancesManager& TAppearancesManager::instance()
{
    static TAppearancesManager manager;
    return manager;
}

TAppearanceInstanceTracker& TAppearancesManager::tracker()
{
    return *sharedTracker();
}

const std::shared_ptr<TAppearanceInstanceTracker>& TAppearancesManager::sharedTracker()
{
    if (!m_spritesheetMapping)
        m_spritesheetMapping = std::make_shared<sprites::TAppearanceTypeSpritesheetMapping>();
    if (!m_tracker)
        m_tracker = std::make_shared<TAppearanceInstanceTracker>(m_spritesheetMapping);
    return m_tracker;
}

TAppearanceInstanceGenericStoragesManager& TAppearancesManager::storages()
{
    if (!m_storages)
        m_storages = std::make_unique<TAppearanceInstanceGenericStoragesManager>();
    return *m_storages;
}

TAppearanceInstanceGenericStorage& TAppearancesManager::genericStorage()
{
    if (!m_genericStorage)
        m_genericStorage = std::make_unique<TAppearanceInstanceGenericStorage>();
    return *m_genericStorage;
}


void TAppearancesManager::init()
{
    m_appearanceTypes.clear();
}

void TAppearancesManager::terminate()
{
    m_appearanceTypes.clear();
    m_monsterRaces.clear();
    m_marketObjectTypes.clear();
    m_skillWheelGemObjectTypes.clear();
    m_proficienciesFile.clear();
    m_effectVariants.clear();
    m_datLoaded = false;
    tibia::sprites::TSpriteSystem::instance().clear();
}

void TAppearancesManager::initialize(const std::string& bytes)
{
    tibia::protobuf::appearances::Appearances appearances;
    if (!appearances.ParseFromString(bytes))
        throw std::logic_error("TAppearancesManager::initialize malformed protobuf data");

    const auto& special = appearances.special_meaning_appearance_ids();
    setSpecialMeaningAppearanceIds({ static_cast<uint16_t>(special.gold_coin_id()),
                                     static_cast<uint16_t>(special.platinum_coin_id()),
                                     static_cast<uint16_t>(special.crystal_coin_id()),
                                     static_cast<uint16_t>(special.tibia_coin_id()),
                                     static_cast<uint16_t>(special.stamped_letter_id()),
                                     static_cast<uint16_t>(special.supply_stash_id()),
                                     static_cast<uint16_t>(special.standard_reward_chest_id()),
                                     static_cast<uint16_t>(special.blank_imbuement_scroll_id()) });

    // Each bank is converted into a map by id (a later duplicate id replaces the
    // earlier one) and added to the types of its category without replacing any.
    // The sprites' pixel sizes come from their spritesheets.
    const TSpriteSizeLookup spriteSize = [](const uint32_t spriteId) {
        return tibia::sprites::TSpriteSystem::instance().spriteSize(spriteId);
    };
    const auto convert = [this, &spriteSize](const EAppearanceCategory category,
                                             const google::protobuf::RepeatedPtrField<tibia::protobuf::appearances::Appearance>& bank) {
        std::map<uint16_t, TAppearanceType> types;
        for (const auto& appearance : bank) {
            TAppearanceType type = TAppearanceType::fromProtobuf(category, appearance, spriteSize);
            types.insert_or_assign(type.getId(), std::move(type));
        }
        auto& target = m_appearanceTypes[category];
        for (auto& [id, type] : types)
            target.try_emplace(id, std::move(type));
    };
    convert(EAppearanceCategory::Effect, appearances.effect());
    convert(EAppearanceCategory::Missile, appearances.missile());
    convert(EAppearanceCategory::Object, appearances.object());
    convert(EAppearanceCategory::Outfit, appearances.outfit());
    collectObjectLists();
    linkExpireCounterparts();
}

void TAppearancesManager::linkExpireCounterparts()
{
    auto& objects = m_appearanceTypes.at(EAppearanceCategory::Object);
    for (auto& [id, type] : objects) {
        if (!type.hasChangedToExpire())
            continue;
        TAppearanceType& former = objects.at(type.formerObjectId());
        const uint16_t formerId = former.getId();
        former.setExpireCounterpart(type.getId());
        type.setExpireCounterpart(formerId);
    }
}

void TAppearancesManager::insertAppearanceType(const EAppearanceCategory category, const uint16_t id, const TAppearanceType& type)
{
    m_appearanceTypes.at(category).try_emplace(id, type);
}

void TAppearancesManager::postprocessAppearances()
{
    // The creature placeholder: an outfit without id that plays effect 13 as one
    // synchronised endless loop, shifted half a field.
    const TAppearanceType* marker = findAppearanceType(ThingCategoryEffect, 13);
    if (!marker)
        throw std::logic_error(QStringLiteral("The TAppearanceType::ID %1 does not exist in %2!").arg(13).arg(QStringLiteral("EFFECTS")).toStdString());
    TAppearanceTypeSpriteInfo markerInfo = marker->frameGroup(EFixedFrameGroup::ObjectInitial);
    if (const auto* animation = markerInfo.animation()) {
        tibia::animation::TAnimationInfo looped = *animation;
        looped.synchronized = true;
        looped.loopType = tibia::animation::TAnimationInfo::ELoopType::Infinite;
        looped.finish();
        markerInfo.setAnimation(std::move(looped));
    }
    TAppearanceType placeholder;
    placeholder.setCategory(EAppearanceCategory::Outfit);
    placeholder.setId(0);
    placeholder.setDisplacement(8, 8);
    placeholder.addFrameGroup(EFixedFrameGroup::OutfitIdle, std::move(markerInfo));
    m_appearanceTypes[EAppearanceCategory::Outfit].insert_or_assign(placeholder.getId(), std::move(placeholder));

    // The three coins have a market category of their own.
    auto& objects = m_appearanceTypes[EAppearanceCategory::Object];
    const TSpecialMeaningAppearanceIds& special = specialMeaningAppearanceIds();
    for (const uint16_t coin : { special.goldCoin, special.platinumCoin, special.crystalCoin })
        objects[coin].setMarketCategory(30);

    // Every type's spritesheets, for the instance tracker to load.
    (void)sharedTracker();
    if (const auto& metadata = tibia::sprites::TSpriteSystem::instance().metadata()) {
        for (const auto& [category, types] : m_appearanceTypes) {
            for (const auto& [id, type] : types)
                m_spritesheetMapping->addAppearanceType(*metadata, type);
        }
    }

}

bool TAppearancesManager::loadAppearances(const std::string& directory)
{
    try {
        init();
        (void)storages();
        (void)genericStorage();
        m_assetIdentifier = loadAssetIdentifier(directory);
        if (m_assetIdentifier.empty()) {
            // Expected for bare --data extracts until make extract stamps the file.
            m_assetIdentifier = "appearancesHash";
            qDebug("%s", std::format("assets.json.sha256 missing; using fallback appearancesHash").c_str());
        }

        tibia::assets::TContentCatalog catalog;
        catalog.load(catalogDocument(directory));
        const std::string appearancesFile = catalogFile(catalog, tibia::assets::TAsset::Appearances);
        tibia::sprites::TSpriteSystem::instance().start(
            catalog, QString::fromStdString(tibia::files::data().getRealPath(directory)));
        if (appearancesFile.empty())
            throw std::runtime_error("catalog has no appearances file");

        const auto appearancesPath = tibia::files::data().resolvePath(directory + appearancesFile);
        initialize(tibia::files::data().readFileContents(appearancesPath));
        postprocessAppearances();
        if (auto* loader = tibia::sprites::TSpriteSystem::instance().assetLoader())
            QObject::connect(m_tracker.get(), &TAppearanceInstanceTracker::loadSpritesheetWithID, loader,
                             &tibia::assets::TAssetAsyncLoader::priorityLoadOfSpritesheet, Qt::UniqueConnection);
        m_datLoaded = true;
        return true;
    } catch (const std::exception& e) {
        qCritical("%s", std::format("Failed to load '{}' (Appearances): {}", directory, e.what()).c_str());
        return false;
    }
}

bool TAppearancesManager::loadStaticData(const std::string& directory)
{
    try {
        tibia::assets::TAssetCatalog catalog;
        catalog.load(catalogDocument(directory));
        const auto fileName = catalogFile(catalog, tibia::assets::TAsset::StaticData);
        if (fileName.empty())
            throw std::runtime_error("catalog has no staticdata file");
        const auto bytes = tibia::files::data().readFileContents(tibia::files::data().resolvePath(directory + fileName));
        setStaticInformation(parseStaticInformation(bytes));
        // The houses' map areas come with the static data (CIP: asset type 6).
        const auto mapFileName = catalogFile(catalog, tibia::assets::TAsset::StaticMapData);
        m_houseMapAreas.clear();
        if (!mapFileName.empty())
            m_houseMapAreas = parseStaticMapData(
                tibia::files::data().readFileContents(tibia::files::data().resolvePath(directory + mapFileName)));
        return true;
    } catch (const std::exception& e) {
        qCritical("%s", std::format("Failed to load '{}' (StaticData): {}", directory, e.what()).c_str());
        return false;
    }
}

TStaticInformation TAppearancesManager::parseStaticInformation(const std::string& bytes)
{
    tibia::protobuf::staticdata::StaticData message;
    if (!message.ParseFromString(bytes))
        throw std::invalid_argument("ParseStaticInformationFromProtobuf malformed protobuf data");
    TStaticInformation information;
    information.monsters.reserve(static_cast<size_t>(message.monster_size()));
    for (const auto& monster : message.monster()) {
        RaceType race;
        race.raceId = monster.race_id();
        race.name = monster.name();
        race.outfit = outfitFrom(monster.outfit());
        race.difficulty = checkedEnum(monster.difficulty(), 6);
        race.occurrence = checkedEnum(monster.occurrence(), 4);
        race.hasCategory = monster.has_flag6();
        race.category = monster.flag6() ? 1 : 0;
        information.monsters.push_back(std::move(race));
    }
    information.monsterClasses.reserve(static_cast<size_t>(message.monster_class_size()));
    for (const auto& monsterClass : message.monster_class())
        information.monsterClasses.push_back({ static_cast<uint8_t>(monsterClass.id()), monsterClass.name() });
    information.achievements.reserve(static_cast<size_t>(message.achievement_size()));
    for (const auto& achievement : message.achievement()) {
        information.achievements.push_back({ static_cast<uint16_t>(achievement.id()), achievement.name(),
                                             achievement.description(), static_cast<uint8_t>(achievement.grade()) });
    }
    information.houses.reserve(static_cast<size_t>(message.house_size()));
    for (const auto& house : message.house()) {
        information.houses.push_back({ house.house_id(), house.name(), house.description(), house.rent(),
                                       static_cast<uint8_t>(house.beds()),
                                       Position(static_cast<uint16_t>(house.position().x()), static_cast<uint16_t>(house.position().y()),
                                                static_cast<uint8_t>(house.position().z())),
                                       static_cast<uint16_t>(house.size()), house.guildhall(), house.city(), house.shop() });
    }
    information.bosses.reserve(static_cast<size_t>(message.boss_size()));
    for (const auto& boss : message.boss()) {
        RaceType race;
        race.raceId = boss.race_id();
        race.name = boss.name();
        race.outfit = outfitFrom(boss.outfit());
        race.boss = true;
        race.notInTeamFinder = boss.flag4();
        // Bosses take the difficulty after the bestiary's six.
        race.difficulty = 6;
        information.bosses.push_back(std::move(race));
    }
    for (const auto& quest : message.quest())
        information.quests.push_back({ static_cast<uint16_t>(quest.id()), quest.name() });
    return information;
}

std::vector<THouseMapArea> TAppearancesManager::parseStaticMapData(const std::string& bytes)
{
    tibia::protobuf::staticdata::StaticMapData message;
    if (!message.ParseFromString(bytes))
        throw std::invalid_argument("ParseStaticMapDataFromProtobuf malformed protobuf data");
    std::vector<THouseMapArea> areas;
    areas.reserve(static_cast<size_t>(message.house_map_area_size()));
    for (const auto& house : message.house_map_area()) {
        const auto& area = house.area();
        areas.push_back({ house.house_id(),
                          Position(static_cast<uint16_t>(area.top_left().x()), static_cast<uint16_t>(area.top_left().y()),
                                   static_cast<uint8_t>(area.top_left().z())),
                          area.extent().width(), area.extent().height(), area.extent().depth(), area.fields() });
    }
    return areas;
}

void TAppearancesManager::setStaticInformation(TStaticInformation information)
{
    cyclopedia::setStaticContentInformation(cyclopedia::staticContentInformation(information));
    m_monsterRaces = std::move(information.monsters);
    m_monsterRaces.insert(m_monsterRaces.end(), std::make_move_iterator(information.bosses.begin()),
                          std::make_move_iterator(information.bosses.end()));
    m_monsterClasses = std::move(information.monsterClasses);
    m_cyclopediaHouses = std::move(information.houses);
    m_cyclopediaAchievements = std::move(information.achievements);
    m_cyclopediaQuests = std::move(information.quests);
}

bool TAppearancesManager::resolveProficienciesFile(const std::string& directory)
{
    m_proficienciesFile.clear();
    try {
        const auto fileName = proficienciesFileOf(directory);
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

TAppearanceTypePtr TAppearancesManager::getThingType(const uint16_t id, const ThingCategory category)
{
    return TAppearanceInstanceFactory::appearanceType(category < ThingLastCategory ? category : ThingCategoryItem, id);
}

TAppearanceType* TAppearancesManager::getRawThingType(const uint16_t id, const ThingCategory category)
{
    return findType(category, id);
}

const RaceType& TAppearancesManager::getRaceData(const uint32_t raceId)
{
    if (raceId > UINT16_MAX)
        return kEmptyRace;
    if (const RaceType* race = cyclopedia::staticMonster(static_cast<uint16_t>(raceId)))
        return *race;
    if (const RaceType* race = cyclopedia::staticBoss(static_cast<uint16_t>(raceId)))
        return *race;
    return kEmptyRace;
}

RaceList TAppearancesManager::getRacesByName(const std::string& searchString)
{
    RaceList result;
    for (const auto& race : m_monsterRaces) {
        if (race.name.find(searchString) != std::string::npos)
            result.push_back(race);
    }
    return result;
}

TAppearanceType* TAppearancesManager::findType(const ThingCategory category, const uint16_t id) const
{
    if (category >= ThingLastCategory)
        return nullptr;
    const auto bank = m_appearanceTypes.find(static_cast<EAppearanceCategory>(category));
    if (bank == m_appearanceTypes.end())
        return nullptr;
    const auto it = bank->second.find(id);
    return it != bank->second.end() ? const_cast<TAppearanceType*>(&it->second) : nullptr;
}

const std::map<uint16_t, TAppearanceType>& TAppearancesManager::appearanceTypes(const EAppearanceCategory category) const
{
    static const std::map<uint16_t, TAppearanceType> none;
    const auto it = m_appearanceTypes.find(category);
    return it != m_appearanceTypes.end() ? it->second : none;
}

TAppearanceType* TAppearancesManager::findAppearanceType(const ThingCategory category, const uint16_t id) const
{
    if (category >= ThingLastCategory)
        throw std::out_of_range("map::at:  key not found");
    const auto& bank = m_appearanceTypes.at(static_cast<EAppearanceCategory>(category));
    const auto it = bank.find(id);
    return it != bank.end() ? const_cast<TAppearanceType*>(&it->second) : nullptr;
}

bool TAppearancesManager::hasAppearanceType(const ThingCategory category, const uint16_t id) const
{
    return findAppearanceType(category, id) != nullptr;
}

void TAppearancesManager::checkAppearanceType(const ThingCategory category, const uint16_t id) const
{
    if (findAppearanceType(category, id))
        return;
    static const char* const names[] = { "OBJECTS", "OUTFITS", "EFFECTS", "MISSILES" };
    const QString name = category < ThingLastCategory ? QString::fromLatin1(names[category]) : QString();
    throw std::logic_error(QStringLiteral("The TAppearanceType::ID %1 does not exist in %2!").arg(id).arg(name).toStdString());
}

void TAppearancesManager::forEachAppearanceType(const std::function<bool(const TAppearanceType&)>& visit) const
{
    for (const auto& [category, types] : m_appearanceTypes) {
        for (const auto& [id, type] : types) {
            if (!visit(type))
                return;
        }
    }
}

// Objects the market trades under their own id, and skill wheel gems.
void TAppearancesManager::collectObjectLists()
{
    m_marketObjectTypes.clear();
    m_skillWheelGemObjectTypes.clear();
    TSpecialAppearancesForProtocolParsing::IdSet cumulative, liquid, container, showOffSocket, upgradeable, wearOut,
        expire, expireStop, decoKit;
    for (auto& [objectId, object] : m_appearanceTypes[EAppearanceCategory::Object]) {
        TAppearanceType* type = &object;
        const auto id = static_cast<uint16_t>(type->getId());
        if (type->isMarketable())
            m_marketObjectTypes.push_back(type);
        if (type->isStackable())
            cumulative.insert(id);
        if (type->isFluidContainer() || type->isSplash())
            liquid.insert(id);
        if (type->isContainer())
            container.insert(id);
        if (type->isPodium())
            showOffSocket.insert(id);
        if (type->getClassification() > 0)
            upgradeable.insert(id);
        if (type->hasWearOut())
            wearOut.insert(id);
        if (type->hasExpire())
            expire.insert(id);
        if (type->hasExpireStop())
            expireStop.insert(id);
        if (type->isDecoKit())
            decoKit.insert(id);
        if (type->hasSkillWheelGem())
            m_skillWheelGemObjectTypes.push_back(type);
    }
    m_specialAppearances = std::make_shared<TSpecialAppearancesForProtocolParsing>(
        std::move(cumulative), std::move(liquid), std::move(container), std::move(showOffSocket),
        std::move(upgradeable), std::move(wearOut), std::move(expire), std::move(expireStop), std::move(decoKit));
    m_marketObjectTypes.shrink_to_fit();
    m_skillWheelGemObjectTypes.shrink_to_fit();
}

const TSpecialAppearancesForProtocolParsing& TAppearancesManager::specialAppearancesForProtocolParsing() const
{
    if (m_specialAppearances)
        return *m_specialAppearances;
    static const TSpecialAppearancesForProtocolParsing empty;
    return empty;
}

TAppearanceTypeList TAppearancesManager::getNpcSaleObjectTypes() const
{
    TAppearanceTypeList result;
    for (const auto& [id, type] : appearanceTypes(EAppearanceCategory::Object)) {
        if (!type.npcTradeData().empty())
            result.push_back(const_cast<TAppearanceType*>(&type));
    }
    return result;
}

TAppearanceTypeList TAppearancesManager::getCyclopediaObjectTypes() const
{
    TAppearanceTypeList result;
    for (const auto& [id, type] : appearanceTypes(EAppearanceCategory::Object)) {
        if (type.hasCyclopediaItem() && type.getCyclopediaType() == type.getId())
            result.push_back(const_cast<TAppearanceType*>(&type));
    }
    return result;
}

TAppearanceTypeList TAppearancesManager::getProficiencyObjectTypes() const
{
    TAppearanceTypeList result;
    for (const auto& [id, type] : appearanceTypes(EAppearanceCategory::Object)) {
        if (type.hasProficiency())
            result.push_back(const_cast<TAppearanceType*>(&type));
    }
    return result;
}

std::string TAppearancesManager::getCyclopediaItemName(const uint16_t id)
{
    const TAppearanceType* type = getRawThingType(id, ThingCategoryItem);
    if (!type)
        return {};
    if (const TAppearanceMarketProperties* market = type->marketProperties(); market && !market->name.isEmpty())
        return market->name.toStdString();
    return type->getName();
}

std::set<QString> TAppearancesManager::creatureProductBuyerNames() const
{
    static constexpr int CreatureProductsMarketCategory = 24;
    static std::set<QString> names;
    if (!names.empty())
        return names;
    std::unordered_map<QString, int> objectsBought;
    for (const TAppearanceType* type : getNpcSaleObjectTypes()) {
        if (!type->isMarketable() || type->marketCategory() != CreatureProductsMarketCategory)
            continue;
        for (const TNpcTradeData& npc : type->npcTradeData()) {
            if (npc.buyPrice != 0)
                ++objectsBought[npc.name];
        }
    }
    for (const auto& [name, count] : objectsBought) {
        if (count > 9)
            names.insert(name);
    }
    return names;
}

std::string TAppearancesManager::getProficienciesFile()
{
    if (!m_proficienciesFile.empty() && tibia::files::data().fileExists(m_proficienciesFile))
        return m_proficienciesFile;
    return {};
}

bool TAppearancesManager::setThingShape(const uint16_t itemId, const std::string& shape)
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
    return true;
}

ThingShape TAppearancesManager::thingShape(const uint16_t itemId) const
{
    const auto it = m_thingShapes.find(itemId);
    return it != m_thingShapes.end() ? it->second : ThingShape::Auto;
}

void TAppearancesManager::registerEffectVariant(const uint16_t virtualId, const uint16_t baseId, const std::string& shader, const bool underCreatures)
{
    if (virtualId == 0 || baseId == 0) {
        qWarning("%s", std::format("registerEffectVariant: invalid ids {} -> {}", virtualId, baseId).c_str());
        return;
    }
    m_effectVariants[virtualId] = EffectVariant{ baseId, shader, underCreatures };
}

}

namespace tibia::assets {

bool load(const std::string& directory)
{
    return tibia::appearances::TAppearancesManager::instance().loadAppearances(directory);
}

bool loadStaticData(const std::string& directory)
{
    return tibia::appearances::TAppearancesManager::instance().loadStaticData(directory);
}

bool resolveProficiencies(const std::string& directory)
{
    return tibia::appearances::TAppearancesManager::instance().resolveProficienciesFile(directory);
}

}
