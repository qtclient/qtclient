#include "assets/asset.h"

namespace tibia::assets {

TAsset::TAsset() = default;

TAsset::TAsset(const EType type, const QString& file)
    : m_type(type)
    , m_file(file)
    , m_valid(true)
{
}

TAsset::TAsset(const QJsonObject& json)
{
    m_valid = readJson(json);
}

bool TAsset::readJson(const QJsonObject& json)
{
    if (!json.contains(QLatin1String("file")) || !json.contains(QLatin1String("type")))
        return false;
    m_valid = true;
    m_file = json.value(QLatin1String("file")).toString();
    m_type = typeFromString(json.value(QLatin1String("type")).toString());
    return m_type != Unknown && m_valid;
}

TAsset::EType TAsset::typeFromString(const QString& type)
{
    if (type == QLatin1String("sprite"))
        return Sprite;
    if (type == QLatin1String("appearances"))
        return Appearances;
    if (type == QLatin1String("staticdata"))
        return StaticData;
    if (type == QLatin1String("staticmapdata"))
        return StaticMapData;
    if (type == QLatin1String("proficiencies"))
        return Proficiencies;
    if (type == QLatin1String("map"))
        return Map;
    if (type == QLatin1String("mapviewerdata"))
        return MapViewerData;
    if (type == QLatin1String("sound"))
        return Sound;
    if (type == QLatin1String("sounds"))
        return Sounds;
    if (type == QLatin1String("fullmap"))
        return FullMap;
    return Unknown;
}

}
