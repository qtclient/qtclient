#pragma once

#include "proficiency/proficiencystorage.h"

#include <cip_gen.h>

#include <QString>
#include <QUrl>

namespace tibia::proficiency {

/// CIP FUN_100ee6830 (GetWeaponProficiencyPerkType + the *_perk_name ids).
QString perkName(const TProficiencyPerk& perk);
/// CIP FUN_100ee67e0 (tibia::proficiency::TPerkDescriptionVisitor).
QString perkDescription(const TProficiencyPerk& perk);
/// CIP FUN_1009271b0 (tibia::client::ProficiencyPerkImageStringVisitor).
QUrl perkImageSource(const TProficiencyPerk& perk);
/// CIP FUN_100e29140 / FUN_100e29150: spell augments show the augment's small perk icon.
bool perkHasAugmentEffect(const TProficiencyPerk& perk);
QUrl perkAugmentEffectImageSource(const TProficiencyPerk& perk);
/// CIP FUN_100ee6b70's spell name and augment type name of a spell augment.
QString perkAugmentSpellName(const TProficiencyPerk& perk);
QString perkAugmentTypeName(const TProficiencyPerk& perk);

/// One perk of a level column (CIP: TQMLProficiencyPerk, vtable 0x10263a0d8, 0x50 bytes).
/// No Q_OBJECT: a QList<QObject*> delegate model only maps the most-derived
/// metaobject's own properties to required delegate properties.
class TQMLProficiencyPerk final : public cip::tibia::gamewindow::TQMLProficiencyPerk
{
public:
    TQMLProficiencyPerk(int index, const TProficiencyPerk& perk, bool picked, bool shaped, uint rank, QObject* parent);
};

/// One level column of the dialog (CIP: TQMLProficiencyLevelInformation,
/// ctor 0x100e29230).
class TQMLProficiencyLevelInformation final : public cip::tibia::gamewindow::TQMLProficiencyLevelInformation
{
public:
    /// `localPick` is this level's pending choice, `serverPicked` whether the
    /// server has one; `tableLevels` = min(levels, 9).
    TQMLProficiencyLevelInformation(int levelIndex, int tableLevels, uint16_t typeId, const TProficiencyLevel& level,
                                    const TPerkPick* localPick, const std::vector<TShapedSlot>& shaped, bool serverPicked,
                                    QObject* parent);
};

}
