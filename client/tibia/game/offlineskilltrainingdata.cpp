#include "offlineskilltrainingdata.h"

namespace tibia::game {

namespace {

using TrainingSkill = TOfflineSkillTrainingData::TrainingSkill;

}

TOfflineSkillTrainingData::TOfflineSkillTrainingData(const TrainingSkill skill, QObject* parent)
    : cip::TOfflineSkillTrainingData(parent)
{
    m_entryVisible = true;
    setTrainingSkill(skill);
}

void TOfflineSkillTrainingData::assign(const SkillsWidgetEntry& entry)
{
    if (entry == m_entry)
        return;
    m_entry = entry;
    m_caption = entry.caption;
    m_valueString = entry.valueString;
    m_valueStringColor = entry.valueStringColor;
    m_tooltip = entry.tooltip;
    m_progressBarVisible = entry.progressBarVisible;
    m_progressBarColor = entry.progressBarColor;
    m_progressBarValue = entry.progressBarValue;
    m_separatorType = entry.separatorType;
    m_changeHighlight = entry.changeHighlight;
    m_showButton = entry.showButton;
    m_iconSource = entry.iconSource;
    m_iconTooltip = entry.iconTooltip;
    m_additionalValueString = entry.additionalValueString;
    m_entryVisible = entry.entryVisible;
    Q_EMIT changed();
}

void TOfflineSkillTrainingData::setTrainingSkill(const TrainingSkill skill)
{
    if (m_trainingSkill == skill)
        return;
    m_trainingSkill = skill;
}

QString TOfflineSkillTrainingData::caption() const { return m_caption; }
QString TOfflineSkillTrainingData::valueString() const { return m_valueString; }
TOfflineSkillTrainingData::EValueColorType TOfflineSkillTrainingData::valueStringColor() const { return m_valueStringColor; }
QString TOfflineSkillTrainingData::tooltip() const { return m_tooltip; }
bool TOfflineSkillTrainingData::progressBarVisible() const { return m_progressBarVisible; }
QColor TOfflineSkillTrainingData::progressBarColor() const { return m_progressBarColor; }
double TOfflineSkillTrainingData::progressBarValue() const { return m_progressBarValue; }
TOfflineSkillTrainingData::ESeparatorType TOfflineSkillTrainingData::separatorType() const { return m_separatorType; }
bool TOfflineSkillTrainingData::changeHighlight() const { return m_changeHighlight; }
bool TOfflineSkillTrainingData::showButton() const { return m_showButton; }
QString TOfflineSkillTrainingData::iconSource() const { return m_iconSource; }
QString TOfflineSkillTrainingData::iconTooltip() const { return m_iconTooltip; }
QString TOfflineSkillTrainingData::additionalValueString() const { return m_additionalValueString; }
bool TOfflineSkillTrainingData::entryVisible() const { return m_entryVisible; }
TOfflineSkillTrainingData::TrainingSkill TOfflineSkillTrainingData::trainingSkill() const { return m_trainingSkill; }

SkillType skillTypeForOfflineTraining(const TrainingSkill skill)
{
    switch (skill) {
        case TrainingSkill::OfflineTrainingSkillTypeSword: return SkillType::Sword;
        case TrainingSkill::OfflineTrainingSkillTypeAxe: return SkillType::Axe;
        case TrainingSkill::OfflineTrainingSkillTypeClub: return SkillType::Club;
        case TrainingSkill::OfflineTrainingSkillTypeDistance: return SkillType::Distance;
        case TrainingSkill::OfflineTrainingSkillTypeMagic: return SkillType::MagicLevel;
        case TrainingSkill::OfflineTrainingSkillTypeFist: return SkillType::Fist;
    }
    return SkillType::Sword;
}

SkillsWidgetEntry buildOfflineTrainingEntry(const TrainingSkill skill)
{
    return buildSkillsWidgetEntry(skillTypeForOfflineTraining(skill));
}

}
