#pragma once

#include "config/icharacterconfiguration.h"

#include <cip_gen.h>

#include <cstdint>
#include <map>

namespace tibia {
namespace config {

/// The character's action bar scroll positions, actionbars.json: the first
/// visible button of each action bar.
class TActionBarOptions final : public cip::tibia::config::TActionBarOptions, public ICharacterConfiguration
{
    Q_OBJECT
public:
    explicit TActionBarOptions(QObject* parent = nullptr);

    QString configurationName() const override;
    /// {"firstVisibleButtons": {"<bar>": index}}.
    QJsonObject configurationToJson() const override;
    /// Replaces every entry when firstVisibleButtons is an object; keeps them otherwise.
    void configurationFromJson(const QJsonObject& json) override;
    void resetConfiguration() override { m_firstVisibleButtons.clear(); }

    /// 0 for a bar without an entry.
    int firstVisibleButton(uint8_t actionBarId) const;
    /// Announces configurationChangedFromUserInput when the index changes.
    void setFirstVisibleButton(uint8_t actionBarId, int index);

private:
    std::map<uint8_t, int> m_firstVisibleButtons;
};

}
}
