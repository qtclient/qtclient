#pragma once

#include "client/gamecore.h"

#include <QObject>
#include <QString>

#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace tibia {
namespace proficiency {

/// One perk of the proficiencies JSON (CIP: the std::variant of
/// tibia::common::proficiency::T*ProficiencyPerk; `type` is the variant
/// index, which is the JSON "Type").
struct TProficiencyPerk
{
    int type = -1;
    double value = 0.0;
    int skillId = 0;
    /// DamageType (type 4) or ElementId (9, 13, 31, 32).
    int element = 0;
    int augmentType = 0;
    int spellId = 0;
    int bestiaryId = 0;
    QString bestiaryName;
    int range = 0;
    double multiplier = 0.0;
    double probability = 0.0;
};

struct TProficiencyLevel
{
    std::vector<TProficiencyPerk> perks;
};

struct TProficiencyInfo
{
    QString name;
    std::vector<TProficiencyLevel> levels;
};

/// A level's perk choice (CIP: 3 bytes {level, perk, picked}, indexed by level).
struct TPerkPick
{
    uint8_t level = 0;
    uint8_t perk = 0;
    bool picked = false;

    bool operator==(const TPerkPick&) const = default;
};

/// A shaped perk slot (CIP: 64-byte entries; the server sends the modifier
/// enum and the refine level).
struct TShapedSlot
{
    uint8_t level = 0;
    uint8_t perk = 0;
    uint16_t perkType = 0;
    uint8_t rank = 0;

    bool operator==(const TShapedSlot&) const = default;
};

/// The server's reshape offers for one shaped slot (CIP /
/// : the slot and one shaped entry per offer).
struct TReshapeOffers
{
    uint16_t typeId = 0;
    uint8_t level = 0;
    uint8_t perk = 0;
    std::vector<TShapedSlot> offers;
};

/// What the server knows of one weapon type (CIP: TProficiencyStorage's
/// per-type record; compares two of them).
struct TProficiencyData
{
    uint32_t xp = 0;
    std::vector<TPerkPick> picks;
    std::vector<TShapedSlot> shaped;

    bool operator==(const TProficiencyData&) const = default;
    bool hasPick(int level) const;
    /// CIP : picks `perk` on `level`, growing the list.
    void pick(int level, int perk);
    const TShapedSlot* shapedSlot(int level, int perk) const;
};

/// CIP's tibia::proficiency::TProficiencyStorage: the weapons' proficiency XP
/// and perks from the server (GameServerWeaponProficiencyInfo /
/// ...Experience) and the perk tables of the proficiencies JSON.
class TProficiencyStorage final : public QObject
{
    Q_OBJECT
public:
    explicit TProficiencyStorage(QObject* parent = nullptr);
    ~TProficiencyStorage() override;
    static TProficiencyStorage* instance();

    /// CIP : the perk table of the type's proficiency, or null.
    const TProficiencyInfo* info(uint16_t typeId) const;
    /// CIP.
    const TProficiencyData* data(uint16_t typeId) const;
    /// CIP .
    uint32_t xp(uint16_t typeId) const;
    /// CIP : the table's levels plus mastery, at most 9 (9 without a table).
    int levelCount(uint16_t typeId) const;
    /// CIP : the reached level, 0..levelCount.
    int level(uint16_t typeId) const;
    /// CIP : the reached level capped at the table's levels (8 without one).
    int decorationLevel(uint16_t typeId) const;
    /// CIP : the mastery level is reached.
    bool hasMastery(uint16_t typeId) const;
    /// CIP 's tables: the XP of threshold `index` (INT_MAX past 9).
    static uint32_t threshold(int index, uint16_t typeId, bool mastery = false);
    /// CIP : the XP a level needs, `tableLevels` = min(levels, 9).
    static uint32_t levelXP(int level, int tableLevels, uint16_t typeId);

    /// CIP +0x90 / : the full list was asked for this session.
    bool listRequested() const { return m_listRequested; }
    void setListRequested(bool requested) { m_listRequested = requested; }

    /// CIP : the last reshape offers, null when none arrived.
    const TReshapeOffers* reshapeOffers() const { return m_reshapeOffers ? &*m_reshapeOffers : nullptr; }

Q_SIGNALS:
    void proficiencyDataChanged(uint typeId);
    void proficiencyXPChanged();
    /// CIP 's notification (qtclient alias of shapedPerkReshapeOffersChanged).
    void reshapeOffersChanged();
    /// CIP moc currentProficiencyProgressChanged — XP / level progress for the open weapon.
    void currentProficiencyProgressChanged();
    /// CIP moc resourceBalanceChanged — forge dust / lunar orbs used by reshape UI.
    void resourceBalanceChanged();
    /// CIP moc shapedPerkReshapeOffersChanged.
    void shapedPerkReshapeOffersChanged();
    /// CIP moc unspentPerkChanged — a level's perk pick arrived or cleared.
    void unspentPerkChanged();

private:
    void loadTables() const;
    int reachedLevel(uint16_t typeId, int count) const;

    std::unordered_map<uint16_t, TProficiencyData> m_data;
    mutable std::map<uint32_t, TProficiencyInfo> m_tables;
    mutable bool m_tablesLoaded = false;
    bool m_listRequested = false;
    std::optional<TReshapeOffers> m_reshapeOffers;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
