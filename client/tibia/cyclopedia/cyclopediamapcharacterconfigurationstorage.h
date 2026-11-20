#pragma once

#include "config/icharacterconfiguration.h"

#include <cip_gen.h>

#include <QJsonObject>
#include <QString>

#include <cstdint>
#include <optional>
#include <unordered_set>
#include <variant>

namespace tibia {
namespace cyclopedia {

/// The Cyclopedia map settings of a character (the character configuration
/// "cyclopediaMapConfiguration"): the selected marker types, the surface view, which markers and
/// panels are shown, the shading and the location the map shows (an area or a passage).
class TCyclopediaMapCharacterConfigurationStorage final : public cip::tibia::cyclopedia::TCyclopediaMapCharacterConfigurationStorage,
                                                          public config::ICharacterConfiguration
{
    Q_OBJECT
public:
    struct TPassage
    {
        int x = 0;
        int y = 0;
        int z = 0;

        bool operator==(const TPassage&) const = default;
    };
    /// The area id, or a passage's position.
    using TLocation = std::variant<uint16_t, TPassage>;

    explicit TCyclopediaMapCharacterConfigurationStorage(QObject* parent = nullptr);

    QString configurationName() const override;
    QJsonObject configurationToJson() const override;
    /// Only the keys present with the right type change their setting; a passage of 0, 0, 0 is
    /// ignored.
    void configurationFromJson(const QJsonObject& json) override;
    void resetConfiguration() override;

    const std::unordered_set<int>& selectedMarkers() const { return m_selectedMarkers; }
    bool surfaceView() const { return m_surfaceView; }
    bool showNPCMarker() const { return m_showNPCMarker; }
    bool showPassageMarker() const { return m_showPassageMarker; }
    bool showHouseMarker() const { return m_showHouseMarker; }
    bool showNavigation() const { return m_showNavigation; }
    bool showAreaAndSubAreaInfo() const { return m_showAreaAndSubAreaInfo; }
    bool showDisplay() const { return m_showDisplay; }
    float shadingValue() const { return m_shadingValue; }
    const TLocation& location() const { return m_location; }

    /// The setters store a user's change (configurationChangedFromUserInput).
    void setSelectedMarkers(const std::unordered_set<int>& markers);
    /// Selects a marker type; announces the change even when it was selected.
    void addSelectedMarker(int marker);
    /// Deselects a marker type; announces only a removal.
    void removeSelectedMarker(int marker);
    /// These four and the shading announce every call, changed or not.
    void setSurfaceView(bool value);
    void setShowNPCMarker(bool value);
    void setShowPassageMarker(bool value);
    void setShowHouseMarker(bool value);
    void setShowNavigation(bool value);
    void setShowAreaAndSubAreaInfo(bool value);
    void setShowDisplay(bool value);
    void setShadingValue(float value);
    void setLocation(const TLocation& location);

private:
    /// Host: the storage outlives a game session (CIP makes one per session), so a load starts
    /// from the session defaults.
    void resetToDefaults();
    template<typename T>
    void set(T& member, const T& value);

    std::unordered_set<int> m_selectedMarkers;
    bool m_surfaceView = true;
    bool m_showNPCMarker = false;
    bool m_showPassageMarker = false;
    bool m_showHouseMarker = false;
    bool m_showNavigation = true;
    bool m_showAreaAndSubAreaInfo = true;
    bool m_showDisplay = true;
    float m_shadingValue = 0.0F;
    TLocation m_location = uint16_t{ 0 };
};

}
}
