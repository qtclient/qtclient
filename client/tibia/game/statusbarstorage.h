#pragma once

#include "config/characterconfigurationmanager.h"

#include <cip_gen.h>

#include <QJsonObject>

#include <array>

namespace tibia {
namespace game {

/// A character's status bar settings (CIP TStatusbarStorage), kept in
/// <app data>/characterdata/<character id>/statusBarData.json and written
/// when the session ends.
class TStatusbarStorage final : public cip::tibia::game::TStatusbarStorage, public config::ICharacterConfiguration
{
    Q_OBJECT
public:
    /// TStatusBarController::DialogAnchor; stored as "off", "top", ...
    enum Position { Off = 0, Top = 1, Bottom = 2, Left = 3, Right = 4 };
    /// TStatusBarController::DialogStyle; stored as "default", "compact", ...
    enum Style { Default = 0, Compact = 1, Large = 2, Parallel = 3 };
    /// The bar's entries in CIP's order ; Experience is the XP bar.
    enum Entry { Experience = 0, MagicLevel, AxeFighting, ClubFighting, DistanceFighting, FistFighting, Shielding, SwordFighting, Fishing };
    static constexpr int EntryCount = 9;

    explicit TStatusbarStorage(QObject* parent = nullptr);

    /// The configuration's file name in the character's data directory.
    QString configurationName() const override;
    /// position, style and the nine show* flags.
    QJsonObject toJson() const;
    /// Takes every key present with the right type (strings for position and
    /// style, booleans for the flags); an unknown name reads as the first
    /// value. Each value taken notifies configurationChangedFromUserInput.
    void fromJson(const QJsonObject& json);

    QJsonObject configurationToJson() const override { return toJson(); }
    void configurationFromJson(const QJsonObject& json) override { fromJson(json); }
    /// A new character starts from the defaults (CIP builds the storage per session).
    void resetConfiguration() override;

    int position() const { return m_position; }
    void setPosition(int position);
    int style() const { return m_style; }
    void setStyle(int style);
    bool shown(Entry entry) const { return m_shown[entry]; }
    void setShown(Entry entry, bool shown);

Q_SIGNALS:
    /// Any value changed, by loading or by the player.
    void changed();

private:
    void modified();

    int m_position = Off;
    int m_style = Default;
    std::array<bool, EntryCount> m_shown{ true };
};

}
}
