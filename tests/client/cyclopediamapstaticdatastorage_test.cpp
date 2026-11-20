#include "cyclopedia/cyclopediamapstaticdatastorage.h"

#include <map.pb.h>


#include <gtest/gtest.h>

#include <map>
#include <vector>
#include <stdexcept>

using tibia::cyclopedia::TCyclopediaMapStaticDataStorage;
using tibia::worldmap::TWorldMapCoordinate;
using EMapFileType = TCyclopediaMapStaticDataStorage::EMapFileType;
using tibia::worldmap::TWorldMapAreaElement;
using tibia::worldmap::TWorldMapCuboid;
using tibia::worldmap::TWorldMapExtent;

namespace {

void setCoordinate(tibia::protobuf::shared::Coordinate* coordinate, const int x, const int y, const int z)
{
    coordinate->set_x(static_cast<uint32_t>(x));
    coordinate->set_y(static_cast<uint32_t>(y));
    coordinate->set_z(static_cast<uint32_t>(z));
}

tibia::protobuf::map::Area* addArea(tibia::protobuf::map::Map& map, const int id, const char* name,
                                    const tibia::protobuf::map::AREA_TYPE type)
{
    auto* area = map.add_areas();
    area->set_area_id(static_cast<uint32_t>(id));
    area->set_name(name);
    area->set_area_type(type);
    return area;
}

void addFile(tibia::protobuf::map::Map& map, const tibia::protobuf::map::MAP_FILE_TYPE type, const char* name, const int x,
             const int y, const int width, const int height, const double scale, const int areaId = 0)
{
    auto* file = map.add_resource_files();
    file->set_file_type(type);
    file->set_file_name(name);
    setCoordinate(file->mutable_top_left_coordinate(), x, y, 7);
    file->set_fields_width(static_cast<uint32_t>(width));
    file->set_fields_height(static_cast<uint32_t>(height));
    file->set_scale_factor(scale);
    if (areaId)
        file->set_area_id(static_cast<uint32_t>(areaId));
}

QByteArray serialized(const tibia::protobuf::map::Map& map)
{
    const std::string bytes = map.SerializeAsString();
    return QByteArray(bytes.data(), static_cast<qsizetype>(bytes.size()));
}

/// Two areas: 1 without a label (two subareas with masks), 2 with a label.
tibia::protobuf::map::Map sampleMap()
{
    tibia::protobuf::map::Map map;
    setCoordinate(map.mutable_top_left_tile_coordinate(), 31000, 31000, 0);
    setCoordinate(map.mutable_bottom_right_tile_coordinate(), 34000, 33000, 15);
    auto* first = addArea(map, 1, "Thais", tibia::protobuf::map::AREA_TYPE_AREA);
    first->add_subarea_ids(10);
    first->add_subarea_ids(11);
    first->set_reject_donations(true);
    auto* second = addArea(map, 2, "Venore", tibia::protobuf::map::AREA_TYPE_AREA);
    second->add_subarea_ids(12);
    setCoordinate(second->mutable_label_coordinate(), 32900, 32100, 7);
    auto* sub = addArea(map, 10, "Thais Sewers", tibia::protobuf::map::AREA_TYPE_SUBAREA);
    sub->set_alias("Rats");
    addArea(map, 11, "Thais Graveyard", tibia::protobuf::map::AREA_TYPE_SUBAREA);
    addArea(map, 12, "Venore Swamp", tibia::protobuf::map::AREA_TYPE_SUBAREA);
    auto* npc = map.add_npcs();
    npc->set_name("Frodo");
    setCoordinate(npc->mutable_tile_coordinate(), 32300, 32200, 7);
    npc->set_subarea_id(10);
    addFile(map, tibia::protobuf::map::MAP_FILE_TYPE_SUBAREA, "sub-10.bmp", 32000, 32000, 10, 10, 1.0, 10);
    addFile(map, tibia::protobuf::map::MAP_FILE_TYPE_SUBAREA, "sub-11.bmp", 32020, 32030, 20, 10, 1.0, 11);
    addFile(map, tibia::protobuf::map::MAP_FILE_TYPE_SATELLITE, "sat-a.bmp", 32000, 32000, 256, 256, 0.5);
    addFile(map, tibia::protobuf::map::MAP_FILE_TYPE_SATELLITE, "sat-b.bmp", 32256, 32000, 256, 256, 0.125);
    addFile(map, tibia::protobuf::map::MAP_FILE_TYPE_MINIMAP, "mini-a.bmp", 32000, 32000, 256, 256, 1.0);
    return map;
}

}

// The router slots live in the host file (sprite system and game actions).
void TCyclopediaMapStaticDataStorage::handleGameAction(tibia::input::IGameActionPtr) {}
void TCyclopediaMapStaticDataStorage::onGameClientMainWindowInitialized() {}

TEST(CyclopediaMapStaticDataStorage, MalformedDataThrows)
{
    TCyclopediaMapStaticDataStorage storage;
    EXPECT_THROW(storage.initialize(QByteArray("\xff\xff\xff", 3)), std::invalid_argument);
}

TEST(CyclopediaMapStaticDataStorage, AreasSubareasAndNpcs)
{
    TCyclopediaMapStaticDataStorage storage;
    storage.initialize(serialized(sampleMap()));

    EXPECT_EQ(storage.areaIds(), (std::set<quint16>{ 1, 2 }));
    EXPECT_TRUE(storage.hasArea(1));
    EXPECT_FALSE(storage.hasArea(10));
    EXPECT_TRUE(storage.hasSubArea(10));
    EXPECT_EQ(storage.areaName(1), QStringLiteral("Thais"));
    EXPECT_EQ(storage.subAreaName(10), QStringLiteral("Thais Sewers"));
    EXPECT_EQ(storage.subAreaAlias(10), QStringLiteral("Rats"));
    EXPECT_TRUE(storage.doesAreaRejectDonations(1));
    EXPECT_FALSE(storage.doesAreaRejectDonations(2));
    EXPECT_EQ(storage.subAreaIdsOfArea(1), (std::set<quint16>{ 10, 11 }));
    // The listing area becomes each subarea's area.
    EXPECT_EQ(storage.subArea(10)->areaId, 1);
    EXPECT_EQ(storage.subArea(12)->areaId, 2);

    ASSERT_EQ(storage.npcs().size(), 1u);
    EXPECT_EQ(storage.npcs()[0].name, QStringLiteral("Frodo"));
    EXPECT_EQ(storage.npcs()[0].subAreaId, 10);
    EXPECT_EQ(storage.npcs()[0].coordinate, TWorldMapCoordinate(32300, 32200, 7));
    // The NPC gets the area of its subarea.
    EXPECT_EQ(storage.npcs()[0].areaId, 1);
    EXPECT_EQ(storage.areaIdOfSubArea(10), 1);
    EXPECT_EQ(storage.areaIdOfSubArea(99), 0);
}

TEST(CyclopediaMapStaticDataStorage, AreaWithoutLabelGetsTheMiddleOfItsSubareas)
{
    TCyclopediaMapStaticDataStorage storage;
    storage.initialize(serialized(sampleMap()));

    // Union of (32000,32000)+10x10 and (32020,32030)+20x10: (32000,32000) .. (32040,32040).
    const auto* thais = storage.area(1);
    ASSERT_NE(thais, nullptr);
    EXPECT_EQ(thais->extent.origin, TWorldMapCoordinate(32000, 32000, 7));
    EXPECT_EQ(thais->extent.extent.width, 40);
    EXPECT_EQ(thais->extent.extent.height, 40);
    EXPECT_EQ(thais->labelCoordinate, TWorldMapCoordinate(32020, 32020, 7));

    // A labelled area keeps its label and gets no extent.
    const auto* venore = storage.area(2);
    ASSERT_NE(venore, nullptr);
    EXPECT_EQ(venore->labelCoordinate, TWorldMapCoordinate(32900, 32100, 7));
    EXPECT_EQ(venore->extent.extent.width, 0);
}

TEST(CyclopediaMapStaticDataStorage, FilesAreRequestedAsMapImages)
{
    TCyclopediaMapStaticDataStorage storage;
    std::vector<std::pair<tibia::assets::TAsset, bool>> requests;
    QObject::connect(&storage, &TCyclopediaMapStaticDataStorage::newAssetDataToLoad,
                     [&requests](const tibia::assets::TAsset& asset, const bool prioritized) { requests.emplace_back(asset, prioritized); });
    storage.initialize(serialized(sampleMap()));

    ASSERT_EQ(requests.size(), 5u);
    std::map<QString, bool> reported;
    for (const auto& [asset, prioritized] : requests) {
        EXPECT_EQ(asset.type(), tibia::assets::TAsset::MapImage);
        EXPECT_FALSE(prioritized);
        reported[asset.file()] = asset.isLoadErrorReported();
    }
    // Missing subarea masks are reported, missing satellite/minimap tiles are not.
    EXPECT_TRUE(reported[QStringLiteral("sub-10.bmp")]);
    EXPECT_FALSE(reported[QStringLiteral("sat-a.bmp")]);
    EXPECT_FALSE(reported[QStringLiteral("mini-a.bmp")]);

    const auto satelliteFiles = storage.mapFiles(EMapFileType::Satellite);
    ASSERT_EQ(satelliteFiles.size(), 2u);
    const auto& tile = satelliteFiles[0];
    EXPECT_EQ(tile.fileName, QStringLiteral("sat-a.bmp"));
    EXPECT_EQ(tile.width, 128);
    EXPECT_EQ(tile.height, 128);
    EXPECT_EQ(storage.mapFiles(EMapFileType::Minimap).size(), 1u);
    EXPECT_EQ(storage.subArea(11)->fileName, QStringLiteral("sub-11.bmp"));
    EXPECT_EQ(storage.subArea(11)->extent.origin, TWorldMapCoordinate(32020, 32030, 7));
    EXPECT_EQ(storage.subArea(11)->fieldsWidth, 20);
    EXPECT_EQ(storage.subArea(11)->fieldsHeight, 10);
    // A subarea without a file keeps -1.
    EXPECT_EQ(storage.subArea(12)->fieldsWidth, -1);
}

TEST(CyclopediaMapStaticDataStorage, LevelOfDetail)
{
    TCyclopediaMapStaticDataStorage storage;
    EXPECT_FLOAT_EQ(storage.levelOfDetail(0.3f, EMapFileType::Satellite), 1.0f);
    storage.initialize(serialized(sampleMap()));
    EXPECT_FLOAT_EQ(storage.levelOfDetail(0.3f, EMapFileType::Satellite), 0.125f);
    EXPECT_FLOAT_EQ(storage.levelOfDetail(0.5f, EMapFileType::Satellite), 0.5f);
    EXPECT_FLOAT_EQ(storage.levelOfDetail(4.0f, EMapFileType::Satellite), 0.5f);
    // Below every scale: the smallest.
    EXPECT_FLOAT_EQ(storage.levelOfDetail(0.01f, EMapFileType::Satellite), 0.125f);
}

TEST(CyclopediaMapStaticDataStorage, SubareaMasks)
{
    QImage sewers(10, 10, QImage::Format_ARGB32);
    sewers.fill(Qt::transparent);
    sewers.setPixel(3, 4, qRgba(255, 0, 0, 255));
    QImage graveyard(20, 10, QImage::Format_ARGB32);
    graveyard.fill(qRgba(0, 0, 255, 255));

    TCyclopediaMapStaticDataStorage storage;
    storage.initialize(serialized(sampleMap()));
    // Without its resolved file a subarea holds no field.
    EXPECT_FALSE(storage.isCoordinateInSubArea(10, TWorldMapCoordinate(32003, 32004, 7)));
    EXPECT_EQ(storage.subAreaAt(TWorldMapCoordinate(32003, 32004, 7)), 0);

    storage.setSubAreaMaskImage(QStringLiteral("sub-10.bmp"), sewers);
    storage.setSubAreaMaskImage(QStringLiteral("sub-11.bmp"), graveyard);
    // A file no subarea uses changes nothing.
    storage.setSubAreaMaskImage(QStringLiteral("sat-a.bmp"), graveyard);
    EXPECT_TRUE(storage.isCoordinateInSubArea(10, TWorldMapCoordinate(32003, 32004, 7)));
    EXPECT_FALSE(storage.isCoordinateInSubArea(10, TWorldMapCoordinate(32004, 32004, 7)));
    EXPECT_FALSE(storage.isCoordinateInSubArea(10, TWorldMapCoordinate(32003, 32004, 6)));
    EXPECT_EQ(storage.subAreaAt(TWorldMapCoordinate(32003, 32004, 7)), 10);
    EXPECT_EQ(storage.subAreaAt(TWorldMapCoordinate(32025, 32035, 7)), 11);
    EXPECT_EQ(storage.subAreaAt(TWorldMapCoordinate(31000, 31000, 7)), 0);
}

namespace {

template <typename T>
std::vector<T> dataOf(const std::vector<TWorldMapAreaElement>& elements)
{
    std::vector<T> found;
    for (const TWorldMapAreaElement& element : elements) {
        if (const auto* data = std::get_if<T>(&element.data))
            found.push_back(*data);
    }
    return found;
}

}

TEST(CyclopediaMapStaticDataStorage, MarkersAreFoundByFieldAndFloor)
{
    TCyclopediaMapStaticDataStorage storage;
    storage.initialize(serialized(sampleMap()));

    // The NPC's piece is its field; boxes meet edges included.
    const TWorldMapCuboid nearNpc(TWorldMapCoordinate(32290, 32190, 7), TWorldMapExtent(10, 10, 1));
    const auto npcs = dataOf<tibia::worldmap::TNPCMarkerWorldMapAreaData>(storage.markersIn(nearNpc));
    ASSERT_EQ(npcs.size(), 1u);
    EXPECT_EQ(npcs[0].name, QStringLiteral("Frodo"));
    EXPECT_EQ(npcs[0].areaId, 1);
    EXPECT_EQ(npcs[0].subAreaId, 10);
    // Another floor or another place: nothing.
    EXPECT_TRUE(storage.markersIn(TWorldMapCuboid(TWorldMapCoordinate(32290, 32190, 6), TWorldMapExtent(10, 10, 1))).empty());
    EXPECT_TRUE(storage.markersIn(TWorldMapCuboid(TWorldMapCoordinate(32302, 32202, 7), TWorldMapExtent(5, 5, 1))).empty());
}

TEST(CyclopediaMapStaticDataStorage, AreaAndSubareaPieces)
{
    TCyclopediaMapStaticDataStorage storage;
    storage.initialize(serialized(sampleMap()));

    // Thais's box and label, plus the sewers' label, are area pieces at (32005, 32005).
    const TWorldMapCuboid thais(TWorldMapCoordinate(32005, 32005, 7), TWorldMapExtent(1, 1, 1));
    const auto areas = dataOf<tibia::worldmap::TAreaWorldMapAreaData>(storage.areasIn(thais));
    ASSERT_EQ(areas.size(), 1u);
    EXPECT_EQ(areas[0].id, 1);
    EXPECT_EQ(areas[0].labelCoordinate, TWorldMapCoordinate(32020, 32020, 7));
    EXPECT_EQ(areas[0].color, QColor(Qt::white));
    const auto subAreaLabels = dataOf<tibia::worldmap::TSubAreaWorldMapAreaData>(storage.areasIn(thais));
    ASSERT_EQ(subAreaLabels.size(), 1u);
    // The sewers have no label: the middle of their box.
    EXPECT_EQ(subAreaLabels[0].id, 10);
    EXPECT_EQ(subAreaLabels[0].labelCoordinate, TWorldMapCoordinate(32005, 32005, 7));
    EXPECT_EQ(storage.subArea(10)->labelCoordinate, TWorldMapCoordinate(32005, 32005, 7));

    const auto subAreas = dataOf<tibia::worldmap::TSubAreaWorldMapAreaData>(storage.subAreasIn(thais));
    ASSERT_EQ(subAreas.size(), 1u);
    EXPECT_EQ(subAreas[0].fileName, QStringLiteral("sub-10.bmp"));
    EXPECT_EQ(subAreas[0].fieldsWidth, 10);
    EXPECT_EQ(subAreas[0].extent.origin, TWorldMapCoordinate(32000, 32000, 7));
}

TEST(CyclopediaMapStaticDataStorage, MapTilePieces)
{
    TCyclopediaMapStaticDataStorage storage;
    storage.initialize(serialized(sampleMap()));

    const TWorldMapCuboid field(TWorldMapCoordinate(32100, 32100, 7), TWorldMapExtent(1, 1, 1));
    const auto satellite = dataOf<tibia::worldmap::TMapTileMapAreaData>(storage.mapTilesIn(EMapFileType::Satellite, field, 1));
    ASSERT_EQ(satellite.size(), 1u);
    EXPECT_EQ(satellite[0].fileName, QStringLiteral("sat-a.bmp"));
    // map.dat's minimap files are the Minimap kind.
    const auto minimap = dataOf<tibia::worldmap::TMapTileMapAreaData>(storage.mapTilesIn(EMapFileType::Minimap, field, 1));
    ASSERT_EQ(minimap.size(), 1u);
    EXPECT_EQ(minimap[0].fileName, QStringLiteral("mini-a.bmp"));

    // The minimap blocks: 256-field squares from the map's corner cut to 256, floors 0 to 15.
    const auto blocks = storage.mapTilesIn(EMapFileType::MinimapTiles, field, 1);
    ASSERT_EQ(blocks.size(), 1u);
    EXPECT_EQ(blocks[0].area.origin, TWorldMapCoordinate(32000, 32000, 7));
    const auto* block = std::get_if<tibia::worldmap::TMinimapTileMapAreaData>(&blocks[0].data);
    ASSERT_NE(block, nullptr);
    EXPECT_FLOAT_EQ(block->scale, 0.03125f);
    EXPECT_EQ(block->width, 256);
    EXPECT_EQ(block->origin, TWorldMapCoordinate(32000, 32000, 7));
    // Every floor of the block's column.
    const TWorldMapCuboid column(TWorldMapCoordinate(32100, 32100, 0), TWorldMapExtent(1, 1, 1));
    EXPECT_EQ(storage.mapTilesIn(EMapFileType::MinimapTiles, column, 16).size(), 16u);
    EXPECT_FLOAT_EQ(storage.levelOfDetail(1.0f, EMapFileType::MinimapTiles), 0.03125f);
    // Map corner 31000 cuts to 30976.
    const TWorldMapCuboid corner(TWorldMapCoordinate(30976, 30976, 0), TWorldMapExtent(0, 0, 1));
    const auto cornerBlocks = storage.mapTilesIn(EMapFileType::MinimapTiles, corner, 1);
    ASSERT_EQ(cornerBlocks.size(), 1u);
    EXPECT_EQ(cornerBlocks[0].area.origin, TWorldMapCoordinate(30976, 30976, 0));
}

TEST(CyclopediaMapStaticDataStorage, HouseMarkersPerFloorOfTheHouse)
{
    TCyclopediaMapStaticDataStorage storage;
    storage.initialize(serialized(sampleMap()));
    EXPECT_TRUE(storage.houseMarkersIn(TWorldMapCuboid(TWorldMapCoordinate(32000, 32000, 7), TWorldMapExtent(500, 500, 1))).empty());

    // House 5 spans floors 5 to 7; its entrance is at (32100, 32100, 7).
    storage.addHouseMarkers(5, 1, TWorldMapCoordinate(32100, 32100, 7),
                            TWorldMapCuboid(TWorldMapCoordinate(32095, 32095, 5), TWorldMapExtent(10, 10, 3)));
    // Floor 7: the entrance itself.
    const auto ground = storage.houseMarkersIn(TWorldMapCuboid(TWorldMapCoordinate(32100, 32100, 7), TWorldMapExtent(0, 0, 1)));
    ASSERT_EQ(ground.size(), 1u);
    EXPECT_EQ(ground[0].area.origin, TWorldMapCoordinate(32100, 32100, 7));
    const auto houses = dataOf<tibia::worldmap::THouseMarkerWorldMapAreaData>(ground);
    ASSERT_EQ(houses.size(), 1u);
    EXPECT_EQ(houses[0].houseId, 5u);
    EXPECT_EQ(houses[0].listing, 1);
    // Every floor up: the entrance one field further down and right.
    const auto floor6 = storage.houseMarkersIn(TWorldMapCuboid(TWorldMapCoordinate(32101, 32101, 6), TWorldMapExtent(0, 0, 1)));
    ASSERT_EQ(floor6.size(), 1u);
    EXPECT_EQ(floor6[0].area.origin, TWorldMapCoordinate(32101, 32101, 6));
    const auto floor5 = storage.houseMarkersIn(TWorldMapCuboid(TWorldMapCoordinate(32102, 32102, 5), TWorldMapExtent(0, 0, 1)));
    ASSERT_EQ(floor5.size(), 1u);
    // No piece outside the house's floors.
    EXPECT_TRUE(storage.houseMarkersIn(TWorldMapCuboid(TWorldMapCoordinate(32000, 32000, 4), TWorldMapExtent(500, 500, 1))).empty());
    EXPECT_TRUE(storage.houseMarkersIn(TWorldMapCuboid(TWorldMapCoordinate(32000, 32000, 8), TWorldMapExtent(500, 500, 1))).empty());
    // markersIn lists them after the NPCs and flags.
    EXPECT_EQ(dataOf<tibia::worldmap::THouseMarkerWorldMapAreaData>(
                  storage.markersIn(TWorldMapCuboid(TWorldMapCoordinate(32100, 32100, 7), TWorldMapExtent(0, 0, 1))))
                  .size(),
              1u);

    // A house without an area in the static map data has no pieces.
    storage.addHouseMarkers(6, 0, TWorldMapCoordinate(32200, 32200, 7), TWorldMapCuboid());
    EXPECT_TRUE(storage.houseMarkersIn(TWorldMapCuboid(TWorldMapCoordinate(32200, 32200, 7), TWorldMapExtent(0, 0, 1))).empty());
}
