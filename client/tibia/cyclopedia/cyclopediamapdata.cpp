#include "cyclopediamapdata.h"

#include "client/gameclient.h"
#include "client/gamecore.h"

#include <framework/core/resourcemanager.h>
#include <framework/graphics/image.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThreadPool>

#include <cstring>
#include <limits>

namespace tibia::cyclopedia {

namespace {

/// Decoded images kept before the least recently drawn are dropped (one
/// 1/16 satellite image is 512x512 RGBA).
constexpr qsizetype ImageCacheBytes = 256 * 1024 * 1024;

quint64 readVarint(const std::string_view data, size_t& at)
{
    quint64 value = 0;
    for (int shift = 0; at < data.size() && shift < 64; shift += 7) {
        const auto byte = static_cast<quint8>(data[at++]);
        value |= quint64(byte & 0x7f) << shift;
        if (!(byte & 0x80))
            break;
    }
    return value;
}

/// The fields of one protobuf message: varints, 64-bit values and
/// length-delimited bodies.
template <class F>
void forEachField(const std::string_view data, F&& visit)
{
    size_t at = 0;
    while (at < data.size()) {
        const quint64 key = readVarint(data, at);
        const int field = static_cast<int>(key >> 3);
        switch (key & 7) {
            case 0: visit(field, readVarint(data, at), std::string_view()); break;
            case 1: {
                if (at + 8 > data.size())
                    return;
                quint64 bits = 0;
                std::memcpy(&bits, data.data() + at, 8);
                at += 8;
                visit(field, bits, std::string_view());
                break;
            }
            case 2: {
                const auto length = static_cast<size_t>(readVarint(data, at));
                if (at + length > data.size())
                    return;
                visit(field, 0, data.substr(at, length));
                at += length;
                break;
            }
            case 5: at += 4; break;
            default: return;
        }
    }
}

TMapCoordinate readCoordinate(const std::string_view data)
{
    TMapCoordinate coordinate;
    forEachField(data, [&coordinate](const int field, const quint64 value, std::string_view) {
        if (field == 1)
            coordinate.x = static_cast<int>(value);
        else if (field == 2)
            coordinate.y = static_cast<int>(value);
        else if (field == 3)
            coordinate.z = static_cast<int>(value);
    });
    return coordinate;
}

QImage decodeImage(const std::string& data)
{
    const ImagePtr image = Image::fromCipBmpData(data);
    if (!image || image->getBpp() != 4)
        return {};
    const Size size = image->getSize();
    return QImage(image->getPixelData(), size.width(), size.height(), size.width() * 4, QImage::Format_RGBA8888).copy();
}

}

TCyclopediaMapStaticData& TCyclopediaMapStaticData::instance()
{
    static auto* data = new TCyclopediaMapStaticData;
    return *data;
}

TCyclopediaMapStaticData::TCyclopediaMapStaticData(QObject* parent)
    : QObject(parent)
{
}

void TCyclopediaMapStaticData::ensureLoaded()
{
    if (m_loaded)
        return;
    m_loaded = true;
    std::string mapData;
    try {
        m_directory = client::gamecore::assetsRoot(client::TGameClient::ClientVersion);
        const QJsonDocument catalog = QJsonDocument::fromJson(QByteArray::fromStdString(tibia::files::data().readFileContents(m_directory + "catalog-content.json")));
        QString file;
        for (const QJsonValue entry : catalog.array()) {
            if (entry.toObject().value(QStringLiteral("type")).toString() == QStringLiteral("map"))
                file = entry.toObject().value(QStringLiteral("file")).toString();
        }
        if (file.isEmpty())
            return;
        mapData = tibia::files::data().readFileContents(m_directory + file.toStdString());
    } catch (const std::exception& e) {
        qWarning("cyclopedia map: no map.dat: %s", e.what());
        return;
    }

    forEachField(mapData, [this](const int field, quint64, const std::string_view body) {
        if (field == 1) {
            TArea area;
            forEachField(body, [&area](const int areaField, const quint64 value, const std::string_view text) {
                switch (areaField) {
                    case 1: area.id = static_cast<int>(value); break;
                    case 2: area.name = QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); break;
                    case 3: area.isSubarea = value == 2; break;
                    case 4: area.subareaIds.push_back(static_cast<int>(value)); break;
                    case 5:
                        area.hasLabel = true;
                        area.label = readCoordinate(text);
                        break;
                    case 6: area.rejectDonations = value != 0; break;
                    case 7: {
                        const QString alias = QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
                        if (alias.isEmpty())
                            break;
                        if (area.alias.isEmpty())
                            area.alias = alias;
                        else
                            area.alias += QStringLiteral(" / ") + alias;
                        break;
                    }
                    default: break;
                }
            });
            m_areaIndex[area.id] = m_areas.size();
            m_areas.push_back(std::move(area));
        } else if (field == 2) {
            TNpc npc;
            forEachField(body, [&npc](const int npcField, const quint64 value, const std::string_view text) {
                if (npcField == 1)
                    npc.name = QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
                else if (npcField == 2)
                    npc.coordinate = readCoordinate(text);
                else if (npcField == 3)
                    npc.subareaId = static_cast<int>(value);
            });
            m_npcs.push_back(std::move(npc));
        } else if (field == 3) {
            TFile file;
            forEachField(body, [&file](const int fileField, const quint64 value, const std::string_view text) {
                switch (fileField) {
                    case 1: file.type = static_cast<EFileType>(value); break;
                    case 2: file.topLeft = readCoordinate(text); break;
                    case 3: file.file.assign(text); break;
                    case 4: file.width = static_cast<int>(value); break;
                    case 5: file.height = static_cast<int>(value); break;
                    case 6: file.areaId = static_cast<int>(value); break;
                    case 7: std::memcpy(&file.scale, &value, sizeof(double)); break;
                    default: break;
                }
            });
            if (file.type == EFileType::Subarea)
                m_subareaFiles[file.areaId] = m_files.size();
            else
                m_scales[file.type].insert(file.scale);
            m_files.push_back(std::move(file));
        }
    });

    for (const TArea& area : m_areas) {
        if (area.isSubarea)
            continue;
        for (const int subareaId : area.subareaIds) {
            if (const auto it = m_areaIndex.find(subareaId); it != m_areaIndex.end())
                m_areas[it->second].parentId = area.id;
        }
    }
}

const std::vector<TCyclopediaMapStaticData::TArea>& TCyclopediaMapStaticData::areas()
{
    ensureLoaded();
    return m_areas;
}

const TCyclopediaMapStaticData::TArea* TCyclopediaMapStaticData::area(const int id)
{
    ensureLoaded();
    const auto it = m_areaIndex.find(id);
    return it == m_areaIndex.end() ? nullptr : &m_areas[it->second];
}

const std::vector<TCyclopediaMapStaticData::TNpc>& TCyclopediaMapStaticData::npcs()
{
    ensureLoaded();
    return m_npcs;
}

const std::vector<TCyclopediaMapStaticData::TFile>& TCyclopediaMapStaticData::files()
{
    ensureLoaded();
    return m_files;
}

double TCyclopediaMapStaticData::levelOfDetail(const EFileType type, const double scale)
{
    ensureLoaded();
    const auto it = m_scales.find(type);
    if (it == m_scales.end() || it->second.empty())
        return 0;
    auto above = it->second.upper_bound(scale);
    if (above == it->second.begin())
        return *above;
    return *--above;
}

QImage TCyclopediaMapStaticData::image(const size_t index)
{
    ensureLoaded();
    if (index >= m_files.size())
        return {};
    if (const auto it = m_images.find(index); it != m_images.end()) {
        it->second.lastUse = ++m_useCounter;
        return it->second.image;
    }
    if (m_pending.contains(index))
        return {};
    std::string data;
    try {
        data = tibia::files::data().readFileContents(m_directory + m_files[index].file);
    } catch (const std::exception& e) {
        qWarning("cyclopedia map: %s: %s", m_files[index].file.c_str(), e.what());
        m_images[index] = {};
        return {};
    }
    m_pending.insert(index);
    QThreadPool::globalInstance()->start([this, index, data = std::move(data)] {
        QImage decoded = decodeImage(data);
        QMetaObject::invokeMethod(this, [this, index, decoded = std::move(decoded)] {
            m_pending.erase(index);
            m_images[index] = { decoded, ++m_useCounter };
            evict();
            Q_EMIT imageLoaded();
        }, Qt::QueuedConnection);
    });
    return {};
}

void TCyclopediaMapStaticData::evict()
{
    qsizetype bytes = 0;
    for (const auto& [index, cached] : m_images)
        bytes += cached.image.sizeInBytes();
    while (bytes > ImageCacheBytes) {
        auto oldest = m_images.end();
        for (auto it = m_images.begin(); it != m_images.end(); ++it) {
            if (m_files[it->first].type == EFileType::Subarea)
                continue;
            if (oldest == m_images.end() || it->second.lastUse < oldest->second.lastUse)
                oldest = it;
        }
        if (oldest == m_images.end())
            return;
        bytes -= oldest->second.image.sizeInBytes();
        m_images.erase(oldest);
    }
}

QImage TCyclopediaMapStaticData::imageSync(const size_t index)
{
    ensureLoaded();
    if (index >= m_files.size())
        return {};
    if (const auto it = m_images.find(index); it != m_images.end()) {
        it->second.lastUse = ++m_useCounter;
        return it->second.image;
    }
    // Hit-testing cannot wait for the async decode used by painting — otherwise the
    // first click on a cold mask returns 0 and the pick is lost.
    if (m_pending.contains(index))
        m_pending.erase(index);
    std::string data;
    try {
        data = tibia::files::data().readFileContents(m_directory + m_files[index].file);
    } catch (const std::exception& e) {
        qWarning("cyclopedia map: %s: %s", m_files[index].file.c_str(), e.what());
        m_images[index] = {};
        return {};
    }
    QImage decoded = decodeImage(data);
    m_images[index] = { decoded, ++m_useCounter };
    evict();
    return decoded;
}

int TCyclopediaMapStaticData::subareaAt(const int x, const int y, const int z)
{
    ensureLoaded();
    if (z > 7)
        return 0;
    const QPoint field(x + z - 7, y + z - 7);
    // Prefer the smallest covering mask so nested CIP subareas (Cyclopolis inside
    // Edron Surroundings, Upper Roshamuul inside Roshamuul) win over the parent.
    int bestId = 0;
    qint64 bestArea = std::numeric_limits<qint64>::max();
    for (const auto& [subareaId, index] : m_subareaFiles) {
        const TFile& file = m_files[index];
        if (!file.fields().contains(field))
            continue;
        const QImage mask = imageSync(index);
        if (mask.isNull())
            continue;
        const QPoint pixel = field - file.fields().topLeft();
        if (!mask.valid(pixel) || qAlpha(mask.pixel(pixel)) <= 0)
            continue;
        const qint64 area = qint64(file.width) * qint64(file.height);
        if (area < bestArea) {
            bestArea = area;
            bestId = subareaId;
        }
    }
    return bestId;
}

int TCyclopediaMapStaticData::subareaFile(const int subareaId)
{
    ensureLoaded();
    const auto it = m_subareaFiles.find(subareaId);
    return it == m_subareaFiles.end() ? -1 : static_cast<int>(it->second);
}

TCyclopediaMapStorage::TCyclopediaMapStorage(QObject* parent)
    : QObject(parent)
{
    using client::gamecore::argInt;
    m_hooks.emplace_back("g_game", "onCyclopediaMapDiscoveryData", [this](LuaObject*, const std::vector<std::any>& args) {
        const auto* mains = args.size() > 0 ? std::any_cast<std::vector<std::vector<int>>>(&args[0]) : nullptr;
        const auto* discovered = args.size() > 1 ? std::any_cast<std::vector<int>>(&args[1]) : nullptr;
        const auto* discoverable = args.size() > 2 ? std::any_cast<std::vector<int>>(&args[2]) : nullptr;
        m_mainAreas.clear();
        if (mains) {
            for (const std::vector<int>& main : *mains) {
                if (main.size() >= 3)
                    m_mainAreas[main[0]] = { main[1], static_cast<float>(main[2]) / 100.0f };
            }
        }
        m_discovered = discovered ? std::set<int>(discovered->begin(), discovered->end()) : std::set<int>();
        m_discoverable = discoverable ? std::set<int>(discoverable->begin(), discoverable->end()) : std::set<int>();
        Q_EMIT changed();
        return std::any{};
    });
    // CIP 0x100a774a0: an active area replaces the viewpoints (full session list + states).
    m_hooks.emplace_back("g_game", "onCyclopediaMapDiscoveryArea", [this](LuaObject*, const std::vector<std::any>& args) {
        if (!argInt(args, 1))
            return std::any{};
        m_discoverySubarea = static_cast<int>(argInt(args, 0));
        m_poiTarget = static_cast<int>(argInt(args, 2));
        m_viewpoints.clear();
        if (const auto* points = args.size() > 3 ? std::any_cast<std::vector<std::vector<int>>>(&args[3]) : nullptr) {
            for (const std::vector<int>& point : *points) {
                if (point.size() >= 4)
                    m_viewpoints.push_back({ { point[0], point[1], point[2] }, point[3] });
            }
        }
        if (m_poiTarget <= 0)
            m_poiTarget = static_cast<int>(m_viewpoints.size());
        Q_EMIT changed();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onCyclopediaMapPassages", [this](LuaObject*, const std::vector<std::any>& args) {
        m_passages.clear();
        if (const auto* rows = args.size() > 0 ? std::any_cast<std::vector<std::vector<int>>>(&args[0]) : nullptr) {
            for (const std::vector<int>& row : *rows) {
                if (row.size() < 4)
                    continue;
                TPassage passage;
                passage.coordinate = { row[0], row[1], row[2] };
                const int raceCount = row[3];
                for (int i = 0; i < raceCount && 4 + i < static_cast<int>(row.size()); ++i)
                    passage.raceIds.push_back(row[4 + i]);
                m_passages.push_back(std::move(passage));
            }
        }
        Q_EMIT changed();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onCyclopediaMapDonations", [this](LuaObject*, const std::vector<std::any>& args) {
        if (const auto* goal = args.size() > 0 ? std::any_cast<uint64_t>(&args[0]) : nullptr)
            m_donationGoal = *goal;
        m_donations.clear();
        if (const auto* areas = args.size() > 1 ? std::any_cast<std::vector<std::vector<double>>>(&args[1]) : nullptr) {
            for (const std::vector<double>& area : *areas) {
                if (area.size() >= 3)
                    m_donations[static_cast<int>(area[0])] = { area[1] != 0, static_cast<quint64>(area[2]) };
            }
        }
        Q_EMIT changed();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onCyclopediaMapCurrentArea", [this](LuaObject*, const std::vector<std::any>& args) {
        m_currentArea = static_cast<int>(argInt(args, 0));
        Q_EMIT changed();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onCyclopediaMapExploringArea", [this](LuaObject*, const std::vector<std::any>& args) {
        m_exploringSubarea = static_cast<int>(argInt(args, 0));
        Q_EMIT changed();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const std::vector<std::any>&) {
        m_mainAreas.clear();
        m_discovered.clear();
        m_discoverable.clear();
        m_discoverySubarea = 0;
        m_poiTarget = 0;
        m_viewpoints.clear();
        m_passages.clear();
        m_donationGoal = 0;
        m_donations.clear();
        m_currentArea = 0;
        m_exploringSubarea = 0;
        Q_EMIT changed();
        return std::any{};
    });
}

const TCyclopediaMapStorage::TMainArea* TCyclopediaMapStorage::mainArea(const int areaId) const
{
    const auto it = m_mainAreas.find(areaId);
    return it == m_mainAreas.end() ? nullptr : &it->second;
}

bool TCyclopediaMapStorage::isAreaCompleted(const int areaId) const
{
    const TMainArea* main = mainArea(areaId);
    return main && main->status == 3;
}

quint64 TCyclopediaMapStorage::donated(const int areaId) const
{
    const auto it = m_donations.find(areaId);
    return it == m_donations.end() ? 0 : it->second.second;
}

bool TCyclopediaMapStorage::isImprovedRespawnActive(const int areaId) const
{
    const auto it = m_donations.find(areaId);
    return it != m_donations.end() && it->second.first;
}

void TCyclopediaMapStorage::setSelectedId(const int id)
{
    if (m_selectedId == id)
        return;
    m_selectedId = id;
    Q_EMIT changed();
}

QColor TCyclopediaMapStorage::subareaColor(const int subareaId) const
{
    if (m_selectedId == subareaId)
        return QColor(QStringLiteral("#ffd015"));
    if (isDiscovered(subareaId))
        return QColor(QStringLiteral("#66ff5e"));
    if (m_discoverySubarea == subareaId)
        return QColor(QStringLiteral("#cc3391"));
    if (isDiscoverable(subareaId))
        return QColor(QStringLiteral("#ffffff"));
    return QColor(QStringLiteral("#8f8f8f"));
}

QColor TCyclopediaMapStorage::areaColor(const int areaId) const
{
    if (m_selectedId == areaId)
        return QColor(QStringLiteral("#ffd015"));
    const TMainArea* main = mainArea(areaId);
    if (!main)
        return QColor(QStringLiteral("#ffffff"));
    if (main->status == 3)
        return QColor(QStringLiteral("#66ff5e"));
    if (main->progress > 0)
        return QColor(main->progress > 1.0f ? QStringLiteral("#8f8f8f") : QStringLiteral("#cc3391"));
    return QColor(QStringLiteral("#ffffff"));
}

}
