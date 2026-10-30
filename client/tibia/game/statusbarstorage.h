#pragma once

#include <cip_gen.h>

#include <QJsonObject>

#include <array>

namespace tibia::game {

/// A character's status bar settings (CIP: TStatusbarStorage, ctor
/// 0x100b4b230, read 0x100b4b700, write 0x100b4b410), kept in
/// <app data>/characterdata/<character id>/statusBarData.json and written
/// when the session ends.
class TStatusbarStorage final : public cip::tibia::game::TStatusbarStorage
{
    Q_OBJECT
public:
    /// TStatusBarController::DialogAnchor; stored as "off", "top", ...
    enum Position { Off = 0, Top = 1, Bottom = 2, Left = 3, Right = 4 };
    /// TStatusBarController::DialogStyle; stored as "default", "compact", ...
    enum Style { Default = 0, Compact = 1, Large = 2, Parallel = 3 };
    /// The bar's entries in CIP's order (FUN_100c10920); Experience is the XP bar.
    enum Entry { Experience = 0, MagicLevel, AxeFighting, ClubFighting, DistanceFighting, FistFighting, Shielding, SwordFighting, Fishing };
    static constexpr int EntryCount = 9;

    explicit TStatusbarStorage(QObject* parent = nullptr);

    void loadForCharacter(quint32 characterId);
    void save();

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
    QString filePath() const;
    void modified();

    quint32 m_characterId = 0;
    QJsonObject m_root;
    int m_position = Off;
    int m_style = Default;
    std::array<bool, EntryCount> m_shown{ true };
    bool m_dirty = false;
};

}
