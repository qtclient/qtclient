#pragma once

#include <QJsonObject>
#include <QString>

namespace tibia {
namespace config {

/// What a per-character configuration storage gives the manager (CIP:
/// ICharacterConfigurationBase's name / toJson / fromJson). The storage's
/// QObject side is cip::tibia::config::ICharacterConfigurationBase, whose
/// configurationChangedFromUserInput marks the configuration dirty.
class ICharacterConfiguration
{
public:
    virtual ~ICharacterConfiguration() = default;
    /// The file's base name in the character's data directory.
    virtual QString configurationName() const = 0;
    virtual QJsonObject configurationToJson() const = 0;
    virtual void configurationFromJson(const QJsonObject& json) = 0;
    /// Host: qtclient's storages outlive a session (CIP builds them per
    /// session), so a new character starts them from their defaults.
    virtual void resetConfiguration() = 0;
};

}
}
