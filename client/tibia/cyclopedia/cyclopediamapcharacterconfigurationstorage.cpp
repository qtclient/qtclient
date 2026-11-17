#include "cyclopediamapcharacterconfigurationstorage.h"

#include <QJsonArray>
#include <QJsonValue>

namespace tibia::cyclopedia {

namespace {

const QString SelectedMarkersKey = QStringLiteral("selectedMarkers");
const QString SurfaceViewKey = QStringLiteral("surfaceView");
const QString ShowNPCMarkerKey = QStringLiteral("showNPCMarker");
const QString ShowPassageMarkerKey = QStringLiteral("showPassageMarker");
const QString ShowHouseMarkerKey = QStringLiteral("showHouseMarker");
const QString ShowNavigationKey = QStringLiteral("showNavigation");
const QString ShowDisplayKey = QStringLiteral("showDisplay");
const QString ShowAreaAndSubAreaInfoKey = QStringLiteral("showAreaAndSubAreaInfo");
const QString ShadingValueKey = QStringLiteral("shadingValue");
const QString AreaIdKey = QStringLiteral("areaID");
const QString PassageKey = QStringLiteral("passage");
const QString XKey = QStringLiteral("x");
const QString YKey = QStringLiteral("y");
const QString ZKey = QStringLiteral("z");

/// The value of `key` when it is there with `type`.
std::optional<QJsonValue> typedValue(const QJsonObject& json, const QString& key, const QJsonValue::Type type)
{
    if (!json.contains(key))
        return std::nullopt;
    const QJsonValue value = json.value(key);
    if (value.type() != type)
        return std::nullopt;
    return value;
}

/// An integer coordinate of the passage; 0 when it is missing or no integer.
int passageCoordinate(const QJsonObject& passage, const QString& key)
{
    const std::optional<QJsonValue> value = typedValue(passage, key, QJsonValue::Double);
    if (!value)
        return 0;
    const double number = value->toDouble();
    return number == static_cast<double>(static_cast<int>(number)) ? static_cast<int>(number) : 0;
}

}

TCyclopediaMapCharacterConfigurationStorage::TCyclopediaMapCharacterConfigurationStorage(QObject* parent)
    : cip::tibia::cyclopedia::TCyclopediaMapCharacterConfigurationStorage(parent)
{
}

QString TCyclopediaMapCharacterConfigurationStorage::configurationName() const { return QStringLiteral("cyclopediaMapConfiguration"); }

QJsonObject TCyclopediaMapCharacterConfigurationStorage::toJson() const
{
    QJsonObject json;
    QJsonArray markers;
    for (const int marker : m_selectedMarkers)
        markers.append(marker);
    json.insert(SelectedMarkersKey, markers);
    json.insert(SurfaceViewKey, m_surfaceView);
    json.insert(ShowNPCMarkerKey, m_showNPCMarker);
    json.insert(ShowPassageMarkerKey, m_showPassageMarker);
    json.insert(ShowHouseMarkerKey, m_showHouseMarker);
    json.insert(ShowNavigationKey, m_showNavigation);
    json.insert(ShowDisplayKey, m_showDisplay);
    json.insert(ShowAreaAndSubAreaInfoKey, m_showAreaAndSubAreaInfo);
    json.insert(ShadingValueKey, static_cast<double>(m_shadingValue));
    if (const auto* areaId = std::get_if<uint16_t>(&m_location)) {
        json.insert(AreaIdKey, static_cast<int>(*areaId));
    } else {
        const TPassage& passage = std::get<TPassage>(m_location);
        QJsonObject position;
        position.insert(XKey, passage.x);
        position.insert(YKey, passage.y);
        position.insert(ZKey, passage.z);
        json.insert(PassageKey, position);
    }
    return json;
}

void TCyclopediaMapCharacterConfigurationStorage::fromJson(const QJsonObject& json)
{
    resetToDefaults();
    if (json.contains(SelectedMarkersKey) && json.value(SelectedMarkersKey).isArray()) {
        for (const QJsonValue marker : json.value(SelectedMarkersKey).toArray())
            if (marker.type() == QJsonValue::Double)
                m_selectedMarkers.insert(marker.toInt());
    }
    const auto readBool = [&](const QString& key, bool& member) {
        if (const std::optional<QJsonValue> value = typedValue(json, key, QJsonValue::Bool)) {
            member = value->toBool();
            Q_EMIT configurationChangedFromUserInput();
        }
    };
    readBool(SurfaceViewKey, m_surfaceView);
    readBool(ShowNPCMarkerKey, m_showNPCMarker);
    readBool(ShowPassageMarkerKey, m_showPassageMarker);
    readBool(ShowHouseMarkerKey, m_showHouseMarker);
    readBool(ShowNavigationKey, m_showNavigation);
    readBool(ShowDisplayKey, m_showDisplay);
    readBool(ShowAreaAndSubAreaInfoKey, m_showAreaAndSubAreaInfo);
    if (const std::optional<QJsonValue> value = typedValue(json, ShadingValueKey, QJsonValue::Double)) {
        m_shadingValue = static_cast<float>(value->toDouble());
        Q_EMIT configurationChangedFromUserInput();
    }
    if (const std::optional<QJsonValue> value = typedValue(json, AreaIdKey, QJsonValue::Double)) {
        m_location = static_cast<uint16_t>(value->toInt());
        Q_EMIT configurationChangedFromUserInput();
    }
    if (const std::optional<QJsonValue> value = typedValue(json, PassageKey, QJsonValue::Object)) {
        const QJsonObject position = value->toObject();
        const TPassage passage{ passageCoordinate(position, XKey), passageCoordinate(position, YKey), passageCoordinate(position, ZKey) };
        if (passage.x != 0 || passage.y != 0 || passage.z != 0) {
            m_location = passage;
            Q_EMIT configurationChangedFromUserInput();
        }
    }
}

void TCyclopediaMapCharacterConfigurationStorage::resetToDefaults()
{
    m_selectedMarkers.clear();
    m_surfaceView = true;
    m_showNPCMarker = false;
    m_showPassageMarker = false;
    m_showHouseMarker = false;
    m_showNavigation = true;
    m_showAreaAndSubAreaInfo = true;
    m_showDisplay = true;
    m_shadingValue = 0.0F;
    m_location = uint16_t{ 0 };
}

template<typename T>
void TCyclopediaMapCharacterConfigurationStorage::set(T& member, const T& value)
{
    if (member == value)
        return;
    member = value;
    Q_EMIT configurationChangedFromUserInput();
}

void TCyclopediaMapCharacterConfigurationStorage::setSelectedMarkers(const std::unordered_set<int>& markers) { set(m_selectedMarkers, markers); }
void TCyclopediaMapCharacterConfigurationStorage::setSurfaceView(const bool value) { set(m_surfaceView, value); }
void TCyclopediaMapCharacterConfigurationStorage::setShowNPCMarker(const bool value) { set(m_showNPCMarker, value); }
void TCyclopediaMapCharacterConfigurationStorage::setShowPassageMarker(const bool value) { set(m_showPassageMarker, value); }
void TCyclopediaMapCharacterConfigurationStorage::setShowHouseMarker(const bool value) { set(m_showHouseMarker, value); }
void TCyclopediaMapCharacterConfigurationStorage::setShowNavigation(const bool value) { set(m_showNavigation, value); }
void TCyclopediaMapCharacterConfigurationStorage::setShowAreaAndSubAreaInfo(const bool value) { set(m_showAreaAndSubAreaInfo, value); }
void TCyclopediaMapCharacterConfigurationStorage::setShowDisplay(const bool value) { set(m_showDisplay, value); }
void TCyclopediaMapCharacterConfigurationStorage::setShadingValue(const float value) { set(m_shadingValue, value); }
void TCyclopediaMapCharacterConfigurationStorage::setLocation(const TLocation& location) { set(m_location, location); }

}
