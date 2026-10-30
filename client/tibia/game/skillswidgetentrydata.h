#pragma once

#include <cip_gen.h>

namespace tibia::game {

/// One row of the skills widget and the status bar (CIP: TSkillsWidgetEntryData,
/// ctor 0x100b4a7f0; one setter per field, each emitting changed()).
struct SkillsWidgetEntry
{
    using ValueColor = cip::tibia::game::TSkillsWidgetEntryData::EValueColorType;
    using Separator = cip::tibia::game::TSkillsWidgetEntryData::ESeparatorType;

    QString caption;
    QString valueString;
    ValueColor valueStringColor = ValueColor::DEFAULT;
    QString tooltip;
    bool progressBarVisible = false;
    QColor progressBarColor;
    double progressBarValue = 0;
    Separator separatorType = Separator::NONE;
    bool changeHighlight = false;
    bool showButton = false;
    QString iconSource;
    QString iconTooltip;
    QString additionalValueString;
    bool entryVisible = true;

    bool operator==(const SkillsWidgetEntry&) const = default;
};

class TSkillsWidgetEntryData final : public cip::tibia::game::TSkillsWidgetEntryData
{
    Q_OBJECT
public:
    explicit TSkillsWidgetEntryData(QObject* parent = nullptr);

    void assign(const SkillsWidgetEntry& entry);

private:
    SkillsWidgetEntry m_entry;
};

/// CIP's skill types (FUN_100b4a2c0): 1 experience, 2 level, 3 experience
/// gain, 4 magic level, 6 hit points, 7 mana, 8 speed, 9 capacity, 10
/// shielding, 11 distance, 12 club, 13 sword, 14 axe, 15 fist, 16 fishing,
/// 17 food, 18 soul points, 19 stamina, 20 offline training.
enum class SkillType {
    Experience = 1,
    Level = 2,
    ExperienceGain = 3,
    MagicLevel = 4,
    HitPoints = 6,
    Mana = 7,
    Speed = 8,
    Capacity = 9,
    Shielding = 10,
    Distance = 11,
    Club = 12,
    Sword = 13,
    Axe = 14,
    Fist = 15,
    Fishing = 16,
    Food = 17,
    SoulPoints = 18,
    Stamina = 19,
    OfflineTraining = 20,
};

/// The skill's name; experience carries its short form as a length variant.
QString skillName(SkillType type);
/// CIP FUN_100b4a620 (mask 0x19fc14): level, magic level, the fighting
/// skills, fishing, stamina and offline training have a progress bar the
/// skills widget can hide.
bool skillHasProgressBar(SkillType type);
/// The type's name in skillsWidgetOptions.invisibleProgressBars ("experience"
/// for the level's bar); empty without a bar.
QString progressBarKey(SkillType type);
/// The skills widget's entry of `type` (CIP: FUN_1010201b0). The bar shows
/// when the type has one; the widget hides it per its options.
SkillsWidgetEntry buildSkillsWidgetEntry(SkillType type);
/// "image://skills-icons/<n>" (CIP: FUN_10101bdd0).
QString skillIconSource(SkillType type);

/// The local player's entry for `type` (CIP: XP FUN_10101a5e0, skills FUN_10101b250).
SkillsWidgetEntry buildSkillEntry(SkillType type);

/// Experience needed for `level` (CIP: FUN_1010193a0).
qint64 experienceForLevel(int level);

}
