#include "cyclopedia/cyclopediamapstaticdatastorage.h"

#include <map.pb.h>


#include <gtest/gtest.h>

#include <map>
#include <vector>
#include <stdexcept>

using tibia::cyclopedia::TCyclopediaMapStaticDataStorage;
using tibia::worldmap::TWorldMapCoordinate;
using EMapFileType = TCyclopediaMapStaticDataStorage::EMapFileType;

namespace {

std::map<QString, QImage>& testImages()
{
    static std::map<QString, QImage> images;
    return images;
}

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

QImage TCyclopediaMapStaticDataStorage::mapImage(const QString& fileName)
{
    const auto it = testImages().find(fileName);
    return it == testImages().end() ? QImage() : it->second;
}

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
    EXPECT_EQ(storage.npcs()[0].subareaId, 10);
    EXPECT_EQ(storage.npcs()[0].coordinate, TWorldMapCoordinate(32300, 32200, 7));
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

    ASSERT_EQ(storage.mapFiles(EMapFileType::Satellite).size(), 2u);
    const auto& tile = storage.mapFiles(EMapFileType::Satellite)[0];
    EXPECT_EQ(tile.fileName, QStringLiteral("sat-a.bmp"));
    EXPECT_EQ(tile.width, 128);
    EXPECT_EQ(tile.height, 128);
    EXPECT_EQ(storage.mapFiles(EMapFileType::Minimap).size(), 1u);
    EXPECT_EQ(storage.subArea(11)->fileName, QStringLiteral("sub-11.bmp"));
    EXPECT_EQ(storage.subArea(11)->extent.origin, TWorldMapCoordinate(32020, 32030, 7));
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
    testImages()[QStringLiteral("sub-10.bmp")] = sewers;
    QImage graveyard(20, 10, QImage::Format_ARGB32);
    graveyard.fill(qRgba(0, 0, 255, 255));
    testImages()[QStringLiteral("sub-11.bmp")] = graveyard;

    TCyclopediaMapStaticDataStorage storage;
    storage.initialize(serialized(sampleMap()));
    EXPECT_TRUE(storage.isCoordinateInSubArea(10, TWorldMapCoordinate(32003, 32004, 7)));
    EXPECT_FALSE(storage.isCoordinateInSubArea(10, TWorldMapCoordinate(32004, 32004, 7)));
    EXPECT_FALSE(storage.isCoordinateInSubArea(10, TWorldMapCoordinate(32003, 32004, 6)));
    EXPECT_EQ(storage.subAreaAt(TWorldMapCoordinate(32003, 32004, 7)), 10);
    EXPECT_EQ(storage.subAreaAt(TWorldMapCoordinate(32025, 32035, 7)), 11);
    EXPECT_EQ(storage.subAreaAt(TWorldMapCoordinate(31000, 31000, 7)), 0);
    testImages().clear();
}
