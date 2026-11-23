#pragma once

#include <cip_gen.h>

#include <QString>
#include <QStringList>

#include <cstdint>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>
#include <optional>

namespace tibia {
namespace spells {

/// One cast's requirements and costs (CIP keeps a block for the spell and,
/// for runes, a second one for using the rune).
struct TSpellCastParameters
{
    /// The protocol's spell group ids (1 attack, 2 healing, 3 support, ...).
    uint8_t groupPrimary = 0;
    uint8_t groupSecondary = 0;
    uint16_t minimumLevel = 0;
    bool onlyPromoted = false;
    bool aggressive = false;
    /// CIP vocation ids: 1 knight, 2 paladin, 3 sorcerer, 4 druid, 5 monk.
    std::set<uint8_t> vocations;
    int mana = 0;
    uint8_t soul = 0;
    /// Milliseconds.
    qint64 cooldownSelf = 0;
    qint64 cooldownPrimaryGroup = 0;
    qint64 cooldownSecondaryGroup = 0;
    /// CIP's damage type (0 physical, 1 fire, 2 earth, 3 energy, 4 ice, 5 holy,
    /// 6 death, 7 healing, 8 drowning, 9 life leech, 10 mana drain, 11 agony).
    std::optional<uint8_t> damageType;
    int mean = 0;
};

/// A rune's object and the cast parameters of using it.
struct TRuneParameters
{
    uint16_t objectId = 0;
    double useMagicLevel = 0;
    uint8_t amount = 0;
    TSpellCastParameters cast;
};

/// One spell or rune of CIP's spells.json (CIP: the record of the global
/// spell table).
struct TSpellInfo
{
    /// CIP's spell kinds: an instant spell, a rune a spell creates, a rune no
    /// spell creates.
    enum EType
    {
        Instant = 1,
        CreatableRune = 2,
        Rune = 3,
    };
    /// CIP's scalings of spells.json.
    enum EScaling
    {
        WeaponSkills = 0,
        MagicLevel = 1,
        DistanceSkill = 2,
        FistSkill = 3,
        Weapon = 4,
        ShieldingSkill = 5,
    };

    int id = 0;
    QString name;
    /// The words without parameters ("exura sio").
    QString words;
    QString parameterHints;
    int iconIndex = 0;
    EType type = Instant;
    /// Unlocked by reaching the level instead of being taught (player guide).
    bool autoUnlock = false;
    bool crossHairTarget = false;
    bool premium = false;
    /// Wave/beam spells that can cast towards mouse/target (`aimattarget` in spells.json).
    bool aimAtTarget = false;
    QString description;
    /// CIP ESpellSource: 1 level up, 2 wheel of destiny, 3 threefold path.
    int source = 0;
    std::set<int> scalings;
    /// The spell's own cast parameters.
    TSpellCastParameters cast;
    /// Runes: the rune object and using it.
    std::optional<TRuneParameters> rune;

    // The fields below repeat the record for code that reads spells.json's
    // values directly (cooldowns in seconds, the rune's cooldowns for runes).
    /// The protocol's spell group ids (1 attack, 2 healing, 3 support, ...).
    int groupPrimary = 0;
    int groupSecondary = 0;
    /// Seconds.
    int cooldownSelf = 0;
    int cooldownPrimaryGroup = 0;
    int cooldownSecondaryGroup = 0;
    int minimumLevel = 0;
    int mana = 0;
    int soul = 0;
    bool aggressive = false;
    bool isRune = false;
    bool isRuneCreatable = false;
    QStringList vocations;
    /// Runes: the rune item, 0 for instant spells.
    int runeObjectId = 0;
    int runeAmount = 0;
    int minimumMagicLevel = 0;
    int mean = 0;
    QString damagetype;
    QStringList scaling;
    /// Rune-use vocations when different from cast vocations.
    QStringList runeVocations;
};

/// Resolve CIP ``:/spells/<file>`` or the extract/disk copy under data/assets.
QString spellsAssetPath(const QString& fileName);

/// CIP's spell table (CIP: spells::TSpellInfoStorage over the global table
/// read from :/spells/spells.json; qtclient reads `make extract`'s copy of
/// that resource) and the server's modifications of it.
class TSpellInfoStorage final : public cip::tibia::spells::TSpellInfoStorage
{
    Q_OBJECT
public:
    /// Reads the spell table (spells.json) from the data directories.
    TSpellInfoStorage();
    /// The game client's table while a client exists (setInstance), else a
    /// table of its own (tests, tools).
    static TSpellInfoStorage* instance();
    /// Installs the game client's table; null goes back to the fallback.
    static void setInstance(TSpellInfoStorage* storage);

    /// The table's record of a spell, without the server's modifications;
    /// null for none.
    const TSpellInfo* spell(int spellId) const;
    /// The number of spells in the table.
    int spellCount() const;
    /// A spell with the server's modifications applied; `spellId` must be in
    /// the table (CIP aborts otherwise, qtclient answers an empty record).
    TSpellInfo spellInfoAt(int spellId) const;
    /// A spell with the server's modifications applied, none for an unknown id.
    std::optional<TSpellInfo> spellInfo(int spellId) const;
    /// The formula a player types: the words, then the trimmed parameter hints.
    QString spellFormula(int spellId) const;
    /// The parameters of casting a spell: a rune's use for runes.
    std::optional<TSpellCastParameters> castParameters(int spellId) const;
    /// The rune spell of a rune object with the server's modifications applied.
    std::optional<TSpellInfo> runeSpellInfo(int objectId) const;
    bool isCrossHairTarget(int spellId) const;
    bool isAimAtTarget(int spellId) const;
    /// Applies the server's modifications of the spell's mana and cooldowns.
    void applySpellModifications(TSpellInfo& spell) const;
    /// A chat text read as a spell: the spell (null for none) and its parameters.
    struct TSpellText
    {
        const TSpellInfo* spell = nullptr;
        QString parameters;
    };
    /// The longest spell formula the trimmed text starts with (case-insensitive).
    /// Text after the formula is the parameters; a spell without parameter
    /// hints only matches when nothing follows its formula.
    TSpellText parseSpellText(QString text) const;
    /// The spell a chat text casts, null for none.
    const TSpellInfo* spellOfText(const QString& text) const;
    /// The id of the spell a chat text casts, 0 for none.
    int spellIdOfText(const QString& text) const;
    /// The rune spell of a rune item, without modifications.
    const TSpellInfo* runeSpellOfObject(int objectId) const;
    const std::map<int, TSpellInfo>& spells() const { return m_spells; }

    /// One server change of a spell's data: the modification type and its value.
    struct TSpellModification
    {
        uint32_t type = 0;
        uint32_t value = 0;
    };
    /// Replaces every spell modification with the server's list.
    void setSpellModifications(std::unordered_map<uint16_t, std::vector<TSpellModification>> modifications);
    void clearSpellModifications();
    const std::vector<TSpellModification>* spellModifications(uint16_t spellId) const;

    /// The server's unlocked spells (UnlockedSpells): replaced, cleared, queried.
    void setUnlockedSpells(const std::vector<uint16_t>& spellIds);
    void clearUnlockedSpells();
    bool isSpellUnlocked(uint16_t spellId) const;

    /// image://spell-icons-32x32/<icon>.
    static QString iconUrl(const TSpellInfo& spell);

private:
    std::map<int, TSpellInfo> m_spells;
    std::unordered_map<uint16_t, std::vector<TSpellModification>> m_modifications;
    std::vector<uint16_t> m_unlockedSpells;
    /// Rune object -> spell.
    std::map<int, int> m_byRune;
};

}
}
