#pragma once

#include "client/gamecore.h"

#include <QColor>
#include <QImage>
#include <QObject>
#include <QRect>
#include <QString>

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace tibia::cyclopedia {

struct TMapCoordinate
{
    int x = 0;
    int y = 0;
    int z = 0;
};

/// The Cyclopedia map's static data: map.dat (upstream/proto/map.proto) areas,
/// NPCs and the satellite / minimap / subarea images (CIP:
/// TCyclopediaMapStaticDataStorage). Images decode on the thread pool.
class TCyclopediaMapStaticData final : public QObject
{
    Q_OBJECT
public:
    enum class EFileType { Subarea = 0, Satellite = 1, Minimap = 2 };

    struct TArea
    {
        int id = 0;
        QString name;
        bool isSubarea = false;
        int parentId = 0;
        std::vector<int> subareaIds;
        bool hasLabel = false;
        TMapCoordinate label;
        bool rejectDonations = false;
        /// CIP Area.alias (map.proto field 7); Team Finder help tooltip.
        QString alias;
    };

    struct TNpc
    {
        QString name;
        TMapCoordinate coordinate;
        int subareaId = 0;
    };

    struct TFile
    {
        EFileType type = EFileType::Subarea;
        TMapCoordinate topLeft;
        std::string file;
        int width = 0;
        int height = 0;
        int areaId = 0;
        double scale = 1.0;
        QRect fields() const { return QRect(topLeft.x, topLeft.y, width, height); }
    };

    static TCyclopediaMapStaticData& instance();

    const std::vector<TArea>& areas();
    const TArea* area(int id);
    const std::vector<TNpc>& npcs();
    const std::vector<TFile>& files();
    /// CIP 0x100a88ca0: the largest scale of `type`'s images not above
    /// `scale`, else the smallest.
    double levelOfDetail(EFileType type, double scale);
    /// files()[index]'s image, null while it decodes (imageLoaded follows).
    QImage image(size_t index);
    /// Same as image(), but decodes on the calling thread so hit-tests can use
    /// cold masks (subareaAt) without waiting for imageLoaded.
    QImage imageSync(size_t index);
    /// The subarea whose mask covers field (x, y, z); masks lie on layer 7,
    /// so a field above it is projected isometrically. 0 for none.
    int subareaAt(int x, int y, int z);
    /// The index of a subarea's mask in files(), or -1.
    int subareaFile(int subareaId);

Q_SIGNALS:
    void imageLoaded();

private:
    explicit TCyclopediaMapStaticData(QObject* parent = nullptr);
    void ensureLoaded();
    void evict();

    struct TCachedImage
    {
        QImage image;
        quint64 lastUse = 0;
    };

    bool m_loaded = false;
    std::string m_directory;
    std::vector<TArea> m_areas;
    std::unordered_map<int, size_t> m_areaIndex;
    std::vector<TNpc> m_npcs;
    std::vector<TFile> m_files;
    std::unordered_map<int, size_t> m_subareaFiles;
    std::map<EFileType, std::set<double>> m_scales;
    std::unordered_map<size_t, TCachedImage> m_images;
    std::set<size_t> m_pending;
    quint64 m_useCounter = 0;
};

/// The player's Cyclopedia map progress from the 0xDD packets (CIP:
/// TCyclopediaMapStorage, fed by handleCyclopediaMapMessage 0x100a516a0).
class TCyclopediaMapStorage final : public QObject
{
    Q_OBJECT
public:
    struct TMainArea
    {
        int status = 0;
        /// CIP 0x100f03940: the wire byte / 100; above 1 = not explorable.
        float progress = 0;
    };

    struct TViewpoint
    {
        TMapCoordinate coordinate;
        int state = 0;
    };

    /// CIP TPassageMapAreaData / CyclopediaMapDataPassage (0xDD subtype 6).
    struct TPassage
    {
        TMapCoordinate coordinate;
        std::vector<int> raceIds;
    };

    explicit TCyclopediaMapStorage(QObject* parent = nullptr);

    const TMainArea* mainArea(int areaId) const;
    /// CIP 0x100a77fd0 / 0x100a78020: the two subarea lists of DiscoveryData.
    bool isDiscovered(int subareaId) const { return m_discovered.contains(subareaId); }
    bool isDiscoverable(int subareaId) const { return m_discoverable.contains(subareaId); }
    /// CIP 0x100a77f70: the area's status is 3 (completed).
    bool isAreaCompleted(int areaId) const;
    /// CIP +0xc8: the subarea of the last active SetDiscoveryArea.
    int discoverySubarea() const { return m_discoverySubarea; }
    /// CIP SetDiscoveryArea poiTarget (session total; usually matches viewpoints().size()).
    int poiTarget() const { return m_poiTarget; }
    const std::vector<TViewpoint>& viewpoints() const { return m_viewpoints; }
    const std::vector<TPassage>& passages() const { return m_passages; }
    quint64 donationGoal() const { return m_donationGoal; }
    quint64 donated(int areaId) const;
    /// CIP 0x100a79370.
    bool isImprovedRespawnActive(int areaId) const;
    int currentArea() const { return m_currentArea; }
    int exploringSubarea() const { return m_exploringSubarea; }
    /// CIP 0x100a792f0 (+0x300): the selected area or subarea.
    void setSelectedId(int id);

    /// CIP 0x100a791d0 / 0x100a790b0: the caption colours.
    QColor subareaColor(int subareaId) const;
    QColor areaColor(int areaId) const;

Q_SIGNALS:
    void changed();

private:
    std::map<int, TMainArea> m_mainAreas;
    std::set<int> m_discovered;
    std::set<int> m_discoverable;
    int m_discoverySubarea = 0;
    int m_poiTarget = 0;
    std::vector<TViewpoint> m_viewpoints;
    std::vector<TPassage> m_passages;
    quint64 m_donationGoal = 0;
    std::map<int, std::pair<bool, quint64>> m_donations;
    int m_currentArea = 0;
    int m_exploringSubarea = 0;
    int m_selectedId = 0;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
