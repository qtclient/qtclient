#pragma once

#include "config/icharacterconfiguration.h"
#include "skillwheel/skillwheeldata.h"

#include <cip_gen.h>

#include <QJsonObject>
#include <QString>

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace tibia {
namespace skillwheel {

/// One row of the fragment workshop's mod list: a supreme (0) or basic (1) mod, whether a
/// socketed gem of the current wheel has it, how many gems of the inventory have it and its grade.
struct TModListEntry
{
    bool basic = false;
    bool active = false;
    int mod = 0;
    int gemCount = 0;
    int grade = 0;

    bool operator==(const TModListEntry&) const = default;
};

/// The Wheel of Destiny data of the shown character: the presets (the character configuration
/// "wheelOfDestiny"), the edited current wheel, the server's wheel with its points, the gem
/// inventory, the mod grades, the unrevealed gems and the fragments.
class TSkillWheelStorage final : public cip::tibia::skillwheel::TSkillWheelStorage, public config::ICharacterConfiguration
{
    Q_OBJECT
public:
    static inline const QString DefaultPresetName = QStringLiteral("Default-Preset");

    explicit TSkillWheelStorage(QObject* parent = nullptr);
    ~TSkillWheelStorage() override;

    QString configurationName() const override;
    QJsonObject configurationToJson() const override;
    void configurationFromJson(const QJsonObject& json) override;
    void resetConfiguration() override;

    int unspentSkillPoints() const { return m_unspentSkillPoints; }
    void setUnspentSkillPoints(int points);

    /// The presets by name; a change emits configurationChangedFromUserInput.
    void setPresets(const std::map<QString, TWheel>& presets);
    std::map<QString, TWheel> presets() const { return m_presets; }
    void addOrReplacePreset(const QString& name, const TWheel& wheel);
    std::optional<TWheel> preset(const QString& name) const;
    void removePreset(const QString& name);
    bool hasPreset(const QString& name) const;
    /// False when there is no preset `oldName`.
    bool renamePreset(const QString& oldName, const QString& newName);

    void setCurrentSkillWheel(const TWheel& wheel);
    void setCurrentSkillWheel(const std::optional<TWheel>& wheel);
    void resetCurrentSkillWheel();
    std::optional<TWheel> currentSkillWheel() const { return m_currentSkillWheel; }
    /// The current wheel differs from the server's.
    bool isCurrentSkillWheelChanged() const;

    /// 0 view only, 1 increase and decrease (temple), 2 increase only.
    int changeState() const { return m_changeState; }
    QString playerName() const { return m_playerName; }
    bool isOwnCharacter() const { return m_ownCharacter; }
    int regularPoints() const { return m_regularPoints; }
    int extraPoints() const { return m_extraPoints; }
    /// The consumed promotion scroll objects and their points.
    std::vector<std::pair<int, int>> promotionScrolls() const { return m_promotionScrolls; }
    bool hasShrinesOfInsightPoints() const { return m_shrinesOfInsightPoints; }
    std::optional<TWheel> serverSkillWheel() const { return m_serverSkillWheel; }
    int huntingTaskPoints() const { return m_huntingTaskPoints; }
    void setServerData(int changeState, const QString& playerName, bool ownCharacter, int regularPoints, int extraPoints,
                       const std::vector<std::pair<int, int>>& promotionScrolls, bool shrinesOfInsightPoints, int huntingTaskPoints,
                       const TWheel& wheel);
    /// The server data of an empty wheel of the own character with `vocation` (no points, view only).
    void setPreview(const QString& playerName, int vocation);
    void clearServerData();

    const std::vector<TGem>& gemInventory() const { return m_gemInventory; }
    /// A gem of the current wheel whose id comes back with other mods leaves its vessel.
    void setGemInventory(std::vector<TGem> gems);
    std::optional<TGem> gem(int id) const;
    /// The inventory gem with the id and mods of `gem`, else the first with its mods.
    std::optional<TGem> findGem(const TGem& gem) const;
    /// How many inventory gems have the basic (or supreme) mod; the mod of a socketed gem is active.
    int basicModGemCount(int mod) const;
    int supremeModGemCount(int mod) const;
    bool isBasicModActive(int mod) const;
    bool isSupremeModActive(int mod) const;

    void setModGrades(TGrades basicGrades, TGrades supremeGrades);
    const TGrades& basicGrades() const { return m_basicGrades; }
    const TGrades& supremeGrades() const { return m_supremeGrades; }

    void setUnrevealedGems(int quality, int count);
    int unrevealedGems(int quality) const;

    const std::vector<TModListEntry>& modList() const { return m_modList; }
    void setModList(const std::vector<TModListEntry>& modList);
    /// The mod list from the supreme mods of the current wheel's vocation and the basic mods.
    void updateModList();

    int lesserFragments() const { return m_lesserFragments; }
    int greaterFragments() const { return m_greaterFragments; }
    void setLesserFragments(int fragments);
    void setGreaterFragments(int fragments);

    /// The gem atelier's reshape: the gem and the mod (0 first basic, 1 second basic, 2 supreme) a
    /// reshape attempt was sent for (-1 once the reshape is chosen), the gem's mods before the attempt
    /// and the mods the server rolled for it.
    int reshapeModIdx() const { return m_reshapeModIdx; }
    void setReshapeModIdx(int modIdx) { m_reshapeModIdx = modIdx; }
    int reshapeGemId() const { return m_reshapeGemId; }
    void setReshapeGemId(int gemId) { m_reshapeGemId = gemId; }
    /// The mods of the inventory gem with the reshape gem id become the mods before the reshape
    /// (unchanged when there is no such gem).
    void rememberGemModsBeforeReshape();
    TGemMods gemModsBeforeReshape() const { return m_gemModsBeforeReshape; }
    TGemMods temporaryReshapedGem() const { return m_temporaryReshapedGem; }
    /// Emits temporaryReshapedGemChanged.
    void setTemporaryReshapedGem(const TGemMods& mods);

    /// There is a server wheel.
    bool hasData() const { return m_serverSkillWheel.has_value(); }
    /// The vocation of the server wheel (1 knight .. 5 monk).
    int vocation() const;
    int vocationIndex() const { return transformCreatureToSkillWheelVocation(vocation()); }
    /// The current wheel, else an empty wheel of the vocation.
    TWheel currentWheel() const;

    int modGradesPoints() const;
    /// Regular, extra and mod grade points.
    int totalPoints() const;
    int spentPoints() const;
    int freePoints() const { return totalPoints() - spentPoints(); }
    /// Bank balance and carried gold.
    uint64_t gold() const;

private:
    int m_unspentSkillPoints = 0;
    std::map<int, int> m_unrevealedGems;
    std::map<QString, TWheel> m_presets;
    std::optional<TWheel> m_currentSkillWheel;
    int m_changeState = 0;
    QString m_playerName;
    bool m_ownCharacter = false;
    int m_regularPoints = 0;
    int m_extraPoints = 0;
    std::vector<std::pair<int, int>> m_promotionScrolls;
    bool m_shrinesOfInsightPoints = false;
    int m_huntingTaskPoints = 0;
    std::optional<TWheel> m_serverSkillWheel;
    std::vector<TGem> m_gemInventory;
    TGrades m_basicGrades;
    TGrades m_supremeGrades;
    std::vector<TModListEntry> m_modList;
    int m_lesserFragments = 0;
    int m_greaterFragments = 0;
    TGemMods m_temporaryReshapedGem;
    TGemMods m_gemModsBeforeReshape;
    int m_reshapeModIdx = 0;
    int m_reshapeGemId = 0;
};

}
}
