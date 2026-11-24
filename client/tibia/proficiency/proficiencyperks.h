#pragma once

#include "proficiency/proficiencystorage.h"

#include <cip_gen.h>

#include <QString>
#include <QUrl>

namespace tibia {
namespace proficiency {

/// CIP (GetWeaponProficiencyPerkType + the *_perk_name ids).
QString perkName(const TProficiencyPerk& perk);
/// CIP (tibia::proficiency::TPerkDescriptionVisitor).
QString perkDescription(const TProficiencyPerk& perk);
/// CIP (tibia::client::ProficiencyPerkImageStringVisitor).
QUrl perkImageSource(const TProficiencyPerk& perk);
/// CIP / : spell augments show the augment's small perk icon.
bool perkHasAugmentEffect(const TProficiencyPerk& perk);
QUrl perkAugmentEffectImageSource(const TProficiencyPerk& perk);
/// CIP 's spell name and augment type name of a spell augment.
QString perkAugmentSpellName(const TProficiencyPerk& perk);
QString perkAugmentTypeName(const TProficiencyPerk& perk);
/// The signed value text of a spell augment.
QString perkAugmentValue(const TProficiencyPerk& perk);

/// How the combat stats and the perk lists show a spell augment: the spell's name,
/// the augment type's name, the value text and whether the spell is a rune spell.
struct TAugmentTexts
{
    QString spellName;
    QString typeName;
    QString value;
    bool runeSpell = false;
};
TAugmentTexts augmentTexts(const TProficiencyPerk& perk);

/// One perk of a level column (CIP: TQMLProficiencyPerk, vtable , 0x50 bytes).
/// No Q_OBJECT: a QList<QObject*> delegate model only maps the most-derived
/// metaobject's own properties to required delegate properties.
class TQMLProficiencyPerk final : public cip::tibia::gamewindow::TQMLProficiencyPerk
{
public:
    TQMLProficiencyPerk(int index, const TProficiencyPerk& perk, bool picked, bool shaped, uint rank, QObject* parent);

    int perkIndex() const override { return m_perkIndex; }
    bool isPicked() const override { return m_isPicked; }
    QString perkDescription() const override;
    QString perkName() const override;
    QUrl imageSource() const override;
    bool hasAugmentEffect() const override;
    QUrl augmentEffectImageSource() const override;
    /// The shaped rank, 0 when the perk is not shaped.
    uint perkRank() const override;

private:
    TProficiencyPerk m_perk;
};

/// One level column of the dialog (CIP: TQMLProficiencyLevelInformation,
/// ctor).
class TQMLProficiencyLevelInformation final : public cip::tibia::gamewindow::TQMLProficiencyLevelInformation
{
public:
    /// `localPick` is this level's pending choice, `serverPicked` whether the
    /// server has one; `tableLevels` = min(levels, 9).
    TQMLProficiencyLevelInformation(int levelIndex, int tableLevels, uint16_t typeId, const TProficiencyLevel& level,
                                    const TPerkPick* localPick, const std::vector<TShapedSlot>& shaped, bool serverPicked,
                                    QObject* parent);

    int levelIndex() const override { return m_levelIndex; }
    QList<QObject*> perks() const override { return m_perks; }
    bool isPerkSelected() const override { return m_isPerkSelected; }
};

}
}
