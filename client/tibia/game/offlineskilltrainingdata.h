#pragma once

#include "game/skillswidgetentrydata.h"

#include <cip_gen.h>

namespace tibia {
namespace game {

/// One MultiOfflineTrainingDialog.qml skill row (CIP: TOfflineSkillTrainingData).
/// Extends the skills-widget entry with `trainingSkill` for the Train button.
class TOfflineSkillTrainingData final : public cip::TOfflineSkillTrainingData
{
    Q_OBJECT
public:
    using TrainingSkill = cip::tibia::qmlcomponents::enums::EOfflineTrainingSkillType;

    explicit TOfflineSkillTrainingData(TrainingSkill skill, QObject* parent = nullptr);

    void assign(const SkillsWidgetEntry& entry);
    void setTrainingSkill(TrainingSkill skill);

    QString caption() const override;
    QString valueString() const override;
    EValueColorType valueStringColor() const override;
    QString tooltip() const override;
    bool progressBarVisible() const override;
    QColor progressBarColor() const override;
    double progressBarValue() const override;
    ESeparatorType separatorType() const override;
    bool changeHighlight() const override;
    bool showButton() const override;
    QString iconSource() const override;
    QString iconTooltip() const override;
    QString additionalValueString() const override;
    bool entryVisible() const override;
    TrainingSkill trainingSkill() const override;

private:
    SkillsWidgetEntry m_entry;
};

/// Maps CIP offline-training enum → skills-widget SkillType (Sword..Fist + Magic).
SkillType skillTypeForOfflineTraining(TOfflineSkillTrainingData::TrainingSkill skill);

/// Builds the dialog row for `skill` (live player values via buildSkillsWidgetEntry).
SkillsWidgetEntry buildOfflineTrainingEntry(TOfflineSkillTrainingData::TrainingSkill skill);

}
}
