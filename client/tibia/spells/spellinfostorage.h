#pragma once

#include <cip_gen.h>

#include <QString>
#include <QStringList>

#include <map>
#include <optional>

namespace tibia::spells {

/// One spell or rune of CIP's spells.json (CIP: the SpellInfo records
/// FUN_100fe2e70 reads).
struct TSpellInfo
{
    int id = 0;
    QString name;
    /// The words without parameters ("exura sio").
    QString words;
    QString parameterHints;
    int iconIndex = 0;
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
    bool premium = false;
    bool aggressive = false;
    bool crossHairTarget = false;
    bool isRune = false;
    bool isRuneCreatable = false;
    QStringList vocations;
    /// Runes: the rune item, 0 for instant spells.
    int runeObjectId = 0;
    int runeAmount = 0;
    int minimumMagicLevel = 0;
    int mean = 0;
    QString damagetype;
    QString description;
    /// CIP ESpellSource: 0 default, 1 wheel, 2 threefold path.
    int source = 0;
    QStringList scaling;
    /// Rune-use vocations when different from cast vocations.
    QStringList runeVocations;
};

/// CIP's spell table (CIP: spells::TSpellInfoStorage; the std::map<u16,
/// SpellInfo> singleton FUN_100fe6090 filled from :/spells/spells.json by
/// FUN_100fe2e70). The table is `make extract`'s copy of that resource.
class TSpellInfoStorage final : public cip::tibia::spells::TSpellInfoStorage
{
    Q_OBJECT
public:
    static TSpellInfoStorage* instance();

    /// CIP FUN_100fe61a0.
    const TSpellInfo* spell(int spellId) const;
    /// The spell a chat text casts: its words, optionally followed by
    /// parameters (CIP: the spell storage's vtable 0xa0, used by FUN_100e83f40).
    const TSpellInfo* spellOfText(const QString& text) const;
    /// The rune spell of a rune item (CIP FUN_100fe6570).
    const TSpellInfo* runeSpellOfObject(int objectId) const;
    const std::map<int, TSpellInfo>& spells() const { return m_spells; }

    /// image://spell-icons-32x32/<icon>.
    static QString iconUrl(const TSpellInfo& spell);

private:
    TSpellInfoStorage();

    std::map<int, TSpellInfo> m_spells;
    std::map<QString, int> m_byWords;
    std::map<int, int> m_byRune;
};

}
