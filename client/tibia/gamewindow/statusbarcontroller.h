#pragma once

#include "client/gamecore.h"
#include "game/playerstatestracker.h"
#include "game/skillswidgetentrydata.h"
#include "game/statusbarstorage.h"

#include <cip_gen.h>

#include <QPointer>

#include <QList>

namespace tibia {
namespace gamewindow {

/// StatusBar's controller (CIP: TStatusBarController): the player's hit
/// points, mana and magic shield, the XP bar and the skills the character
/// chose, the special-condition icons and a Monk's harmony and serenity. The
/// bar sits at the storage's position. Not yet: proficiency progress.
class TStatusBarController final : public cip::tibia::gamewindow::TStatusBarController
{
    Q_OBJECT
public:
    explicit TStatusBarController(QObject* parent = nullptr);

    /// The character's storage; null outside the world.
    void setStorage(game::TStatusbarStorage* storage);

    DialogAnchor dialogAnchor() const override { return m_dialogAnchor; }
    DialogStyle dialogStyle() const override { return m_dialogStyle; }

    int maxHitpoints() const override;
    int currentHitpoints() const override;
    float hitpointsPercent() const override;
    int maxMana() const override;
    int currentMana() const override;
    float manaPercent() const override;
    int manaShield() const override;
    int maxManaShield() const override;
    float manaShieldPercent() const override;
    bool showCombopointsAndSerene() const override;
    int comboPoints() const override;
    bool isSerene() const override;
    bool showManaShield() const override { return m_showManaShield; }
    QAbstractListModel* playerStates() const override;
    /// The XP bar's entry while the storage shows it.
    QObject* xpSkill() const override;
    QList<QObject*> skillList() const override { return m_skillList; }
    /// The last skill alone on its row; only with an odd number of skills.
    QObject* remainingSkill() const override { return m_oddSkillCount ? m_remainingSkill : nullptr; }
    bool showPlayerStatesInBar() const override;

    /// CIP : the other positions and styles, the shown skills and
    /// options 0x71 statusBarEnabled / 0x72 statusPanelEnabled.
    /// The other positions and styles, the shown skills and options 0x71
    /// statusBarEnabled / 0x72 statusPanelEnabled.
    Q_INVOKABLE void requestContextMenu() override;
    Q_INVOKABLE void onClickedOpenStoreXPBoost() override;

private:
    /// On entering the world.
    void applyStorage();
    /// The storage changed.
    void onStorageChanged();
    /// New entries for the shown skills; an odd count leaves the last one alone.
    void rebuildSkillLists();
    /// The shown entries' values.
    void refreshSkills();
    void updateFromPlayer();
    void setAnchorAndStyle(int anchor, int style, bool forceAnchorSignal);
    void setDialogAnchor(DialogAnchor anchor);
    void setDialogStyle(DialogStyle style);
    /// The storage's position as an anchor: anything past Right is Invalid.
    static DialogAnchor anchorFromPosition(int position);
    /// The storage's style: anything but Compact, Large and Parallel is Default.
    static DialogStyle styleFromStorage(int style);
    /// Whether the storage shows the entry at `index` (Storage::Entry order).
    bool isSkillShown(int index) const;
    void setSkillShown(int index, bool shown);

    QPointer<game::TStatusbarStorage> m_storage;
    game::TPlayerStatesTracker m_states;
    /// The XP bar's entry lives as long as the controller; the skills' are
    /// made anew on each rebuild.
    game::TSkillsWidgetEntryData* m_xpEntry = nullptr;
    /// The skill type of each m_skillList entry, then of m_remainingSkill.
    QList<game::SkillType> m_skillTypes;
    game::SkillType m_remainingSkillType = game::SkillType::Experience;
    bool m_oddSkillCount = false;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
