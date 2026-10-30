#pragma once

#include "client/gamecore.h"
#include "skillwheel/skillwheeldata.h"

#include <QList>
#include <QObject>
#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

namespace tibia::skillwheel {

/// A wheel preset of the own character.
struct TPreset
{
    QString name;
    TWheel wheel;
};

/// The Wheel of Destiny data of the shown player (CIP: TSkillWheelStorage):
/// the server's wheel (onDestinyWheel), the edited current wheel, the revealed
/// gems and mod grades, the gem and fragment balances, and the own presets
/// (<app data>/characterdata/<player id>/skillWheel.json).
class TSkillWheelStorage final : public QObject
{
    Q_OBJECT
public:
    static inline const QString DefaultPresetName = QStringLiteral("Default-Preset");

    explicit TSkillWheelStorage(QObject* parent = nullptr);
    ~TSkillWheelStorage() override;

    bool hasData() const { return m_hasData; }
    uint32_t playerId() const { return m_playerId; }
    bool isLocalPlayer() const { return m_isLocal; }
    bool isPreview() const { return m_isPreview; }
    QString playerName() const { return m_playerName; }
    /// 0 view only, 1 increase and decrease (temple), 2 increase only.
    int changeState() const { return m_changeState; }
    /// EVocation: 1 knight .. 5 monk.
    int vocation() const { return m_vocation; }
    int vocationIndex() const { return m_vocation - 1; }
    int regularPoints() const { return m_points; }
    int extraPoints() const { return m_extraPoints; }
    int huntingTaskPoints() const { return m_huntingTaskPoints; }
    bool hasShrinesOfInsightPoints() const { return m_hasMonkQuest; }
    /// The consumed promotion scroll objects and their points.
    const std::vector<std::pair<int, int>>& promotionScrolls() const { return m_scrolls; }

    const TWheel& serverWheel() const { return m_serverWheel; }
    const TWheel& currentWheel() const { return m_currentWheel; }
    void setCurrentWheel(const TWheel& wheel);
    void useServerWheelAsCurrent() { setCurrentWheel(m_serverWheel); }

    const std::vector<TGem>& gems() const { return m_gems; }
    const TGem* gem(int id) const;
    const TGrades& basicGrades() const { return m_basicGrades; }
    const TGrades& supremeGrades() const { return m_supremeGrades; }

    int modGradesPoints() const;
    int totalPoints() const;
    int spentPoints() const;
    int freePoints() const { return totalPoints() - spentPoints(); }

    /// Unrevealed gems by quality and the lesser / greater fragments (resource balances 81..85).
    int unrevealedGems(int quality) const;
    int lesserFragments() const;
    int greaterFragments() const;
    uint64_t gold() const;

    /// CIP 0x100ddc1f0 (action 308, OK): an empty, view-only wheel of the own vocation.
    void setPreview(const QString& name, int vocation);
    void clear();

    const QList<TPreset>& presets() const { return m_presets; }
    QString currentPresetName() const { return m_currentPreset; }
    const TPreset* preset(const QString& name) const;
    bool canManagePresets() const;
    bool isPresetDirty() const;
    bool isWheelDirty() const { return !(m_currentWheel == m_serverWheel); }
    /// Activates a preset; returns false when the preset had to be adjusted
    /// (unknown gems, an invalid grid or more points than available).
    bool selectPreset(const QString& name);
    void saveCurrentPreset();
    bool addPreset(const QString& name, const TWheel& wheel);
    bool renameCurrentPreset(const QString& name);
    bool deleteCurrentPreset();

Q_SIGNALS:
    /// New server data (onDestinyWheel); `canView` false: the player has no wheel.
    void serverDataReceived(bool canView);
    void currentWheelChanged();
    void resourcesChanged();
    void presetsChanged();

private:
    void onDestinyWheel(const std::vector<std::any>& args);
    void loadPresets();
    void savePresets() const;
    QString presetsPath() const;
    TWheel sanitized(const TWheel& wheel, bool* adjusted) const;

    std::vector<client::gamecore::Hook> m_hooks;
    bool m_hasData = false;
    uint32_t m_playerId = 0;
    bool m_isLocal = false;
    bool m_isPreview = false;
    QString m_playerName;
    int m_changeState = 0;
    int m_vocation = 1;
    int m_points = 0;
    int m_extraPoints = 0;
    int m_huntingTaskPoints = 0;
    bool m_hasMonkQuest = false;
    std::vector<std::pair<int, int>> m_scrolls;
    TWheel m_serverWheel;
    TWheel m_currentWheel;
    std::vector<TGem> m_gems;
    TGrades m_basicGrades;
    TGrades m_supremeGrades;

    uint32_t m_presetsOwner = 0;
    QList<TPreset> m_presets;
    QString m_currentPreset;
};

}
