#pragma once

#include "game/tgamenotify.h"

#include <QObject>
#include <QString>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace tibia {
namespace appearances {
class TAppearancesManager;
}
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
    int missileId = 0;
};

struct TProficiencyLevel
{
    std::vector<TProficiencyPerk> perks;
};

struct TProficiencyInfo
{
    int proficiencyId = 0;
    int version = 0;
    QString name;
    std::vector<TProficiencyLevel> levels;
};

/// Parses the proficiencies asset (a JSON array). Throws std::invalid_argument
/// on malformed JSON.
std::vector<TProficiencyInfo> parseProficiencies(const QByteArray& contents);

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
    /// The storage keeps the game client's appearances manager.
    explicit TProficiencyStorage(std::shared_ptr<appearances::TAppearancesManager> appearancesManager);
    ~TProficiencyStorage() override;
    /// The game client's new session (or none) starts without the weapons'
    /// records, the wielded weapon's progress, the unspent perk, the list
    /// request and the orbs; the perk tables stay.
    void clear();

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

    /// The equipped weapon's proficiency XP (the notification's, when it named the wielded weapon).
    uint32_t currentProficiencyXP() const { return m_currentProficiencyXP; }
    /// The server reports an unspent perk for the wielded weapon.
    bool hasUnspentPerk() const { return m_unspentPerk; }
    void setUnspentPerk(bool unspent);
    /// Replaces a weapon's record.
    void setProficiencyData(uint16_t typeId, TProficiencyData data);
    /// The server's latest reshape offers.
    void setReshapeOffers(TReshapeOffers offers);
    /// A weapon's XP from a notification; `isCurrentWeapon` also updates the
    /// wielded weapon's progress.
    void setProficiencyExperience(uint32_t xp, uint16_t typeId, bool isCurrentWeapon);
    /// Lunar ascension orbs, the reshape resource.
    uint32_t lunarAscensionOrbs() const { return m_lunarAscensionOrbs; }
    void setLunarAscensionOrbs(uint32_t orbs);

    /// CIP : the last reshape offers, null when none arrived.
    const TReshapeOffers* reshapeOffers() const { return m_reshapeOffers ? &*m_reshapeOffers : nullptr; }
    /// Keeps the parsed catalogue by proficiency id; the first entry of an id wins.
    void setTables(const std::vector<TProficiencyInfo>& proficiencies) const;

Q_SIGNALS:
    void proficiencyDataChanged(uint typeId);
    void proficiencyXPChanged();
    /// CIP moc currentProficiencyProgressChanged — XP / level progress for the open weapon.
    void currentProficiencyProgressChanged();
    /// CIP moc resourceBalanceChanged — forge dust / lunar orbs used by reshape UI.
    void resourceBalanceChanged();
    /// CIP moc shapedPerkReshapeOffersChanged.
    void shapedPerkReshapeOffersChanged();
    /// CIP moc unspentPerkChanged — a level's perk pick arrived or cleared.
    void unspentPerkChanged();

private:
    int reachedLevel(uint16_t typeId, int count) const;

    std::shared_ptr<appearances::TAppearancesManager> m_appearancesManager;
    std::unordered_map<uint16_t, TProficiencyData> m_data;
    /// The Proficiencies asset (the asset handler sets it).
    mutable std::map<uint32_t, TProficiencyInfo> m_tables;
    bool m_listRequested = false;
    uint32_t m_currentProficiencyXP = 0;
    bool m_unspentPerk = false;
    uint32_t m_lunarAscensionOrbs = 0;
    std::optional<TReshapeOffers> m_reshapeOffers;
};

}
}
