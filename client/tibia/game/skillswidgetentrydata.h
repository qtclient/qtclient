#pragma once

#include <cip_gen.h>

namespace tibia {
namespace game {

/// One row of the skills widget and the status bar (CIP: TSkillsWidgetEntryData,
/// ctor ; one setter per field, each emitting changed).
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

/// CIP's skill types : 1 experience, 2 level, 3 experience
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

class TSkillsWidgetEntryData final : public cip::tibia::game::TSkillsWidgetEntryData
{
    Q_OBJECT
public:
    /// An experience row with nothing else set (shown, no bar, no separator).
    explicit TSkillsWidgetEntryData(QObject* parent = nullptr);
    /// The row of `Type`: its skill name as caption, a progress bar when the
    /// type has one, and `Separator` below it; shown, nothing else set.
    TSkillsWidgetEntryData(SkillType Type, ESeparatorType Separator, QObject* parent = nullptr);

    SkillType type() const { return m_type; }

    void assign(const SkillsWidgetEntry& entry);

    // The QML properties, one plain read each.
    QString caption() const override { return m_caption; }
    QString valueString() const override { return m_valueString; }
    EValueColorType valueStringColor() const override { return m_valueStringColor; }
    QString tooltip() const override { return m_tooltip; }
    QColor progressBarColor() const override { return m_progressBarColor; }
    double progressBarValue() const override { return m_progressBarValue; }
    ESeparatorType separatorType() const override { return m_separatorType; }
    bool changeHighlight() const override { return m_changeHighlight; }
    bool showButton() const override { return m_showButton; }
    QString iconSource() const override { return m_iconSource; }
    QString iconTooltip() const override { return m_iconTooltip; }
    QString additionalValueString() const override { return m_additionalValueString; }
    bool entryVisible() const override { return m_entryVisible; }
    /// Shows or hides the row; announces changed every time.
    void setEntryVisible(bool EntryVisible);

private:
    SkillType m_type = SkillType::Experience;
    SkillsWidgetEntry m_entry;
};


/// The skill's name; experience carries its short form as a length variant.
QString skillName(SkillType type);
/// CIP (mask 0x19fc14): level, magic level, the fighting
/// skills, fishing, stamina and offline training have a progress bar the
/// skills widget can hide.
bool skillHasProgressBar(SkillType type);
/// The type's name in skillsWidgetOptions.invisibleProgressBars ("experience"
/// for the level's bar); empty without a bar.
QString progressBarKey(SkillType type);
/// The skills widget's entry of `type` (CIP:). The bar shows
/// when the type has one; the widget hides it per its options.
SkillsWidgetEntry buildSkillsWidgetEntry(SkillType type);
/// "image://skills-icons/<n>" (CIP:).
QString skillIconSource(SkillType type);

/// The local player's entry for `type` (CIP: XP , skills).
/// CIP: a skill's entry; `precisePercent` shows the progress left with two
/// decimals, otherwise rounded ("almost there" above 99%).
SkillsWidgetEntry buildSkillEntry(SkillType type, bool precisePercent = true);

/// Experience needed for `level` (CIP:).
qint64 experienceForLevel(int level);

}
}
