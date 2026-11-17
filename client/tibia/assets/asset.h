#pragma once

#include <QJsonObject>
#include <QString>

namespace tibia {
namespace assets {

/// One entry of the asset catalog (catalog-content.json): what the file holds
/// and its name (CIP: TAsset).
class TAsset
{
public:
    enum EType {
        Unknown = 0,
        Sprite = 1,
        Appearances = 2,
        StaticData = 5,
        StaticMapData = 6,
        Proficiencies = 7,
        Map = 8,
        MapViewerData = 10,
        Sounds = 11,
        Sound = 12,
        FullMap = 13,
    };

    TAsset();
    TAsset(EType type, const QString& file);
    /// From a catalog entry; valid with a "file" and a known "type".
    explicit TAsset(const QJsonObject& json);
    virtual ~TAsset() = default;

    EType type() const { return m_type; }
    const QString& file() const { return m_file; }
    bool isValid() const { return m_valid; }
    /// A failed load of this asset is reported (assetLoadError); on by default.
    bool isLoadErrorReported() const { return m_loadErrorReported; }
    void setLoadErrorReported(bool reported) { m_loadErrorReported = reported; }

    static EType typeFromString(const QString& type);

private:
    bool readJson(const QJsonObject& json);

    EType m_type = Unknown;
    QString m_file;
    bool m_valid = false;
    bool m_loadErrorReported = true;
};

}
}
