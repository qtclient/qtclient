#pragma once

#include "client/gamecore.h"
#include "game/playerstatestracker.h"
#include "game/skillswidgetentrydata.h"
#include "game/statusbarstorage.h"

#include <cip_gen.h>

#include "qtconnectionmanager.h"

#include <QPointer>

#include <QList>

#include <memory>

namespace tibia {
namespace config {
class TClientOptions;
}
namespace appearances {
class TAppearanceType;
}
namespace container {
class TInventoryContainer;
}
namespace game {
class IGameSession;
}
namespace proficiency {
class TProficiencyStorage;
}
namespace gamewindow {

/// StatusBar's controller (CIP: TStatusBarController): the player's hit
/// points, mana and magic shield, the XP bar and the skills the character
/// chose, the special-condition icons and a Monk's harmony and serenity. The
/// bar sits at the storage's position, and the wielded weapon's proficiency
/// progress.
class TStatusBarController final : public cip::tibia::gamewindow::TStatusBarController
{
    Q_OBJECT
public:
    /// CIP: made with the client options (the game window controller's);
    /// its player states follow the special-condition options.
    explicit TStatusBarController(config::TClientOptions* ClientOptions = nullptr, QObject* parent = nullptr);

    /// CIP: follows the session's player data, its character's status bar
    /// storage, the proficiency storage and the inventory; null leaves the world.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

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
    /// The wielded weapon has a proficiency.
    bool showProficiencyProgress() const override;
    /// The server reports an unspent perk.
    bool isProficiencyHighlighted() const override;
    /// The share of the current level's XP span the weapon has made; 1 at the last level.
    double proficiencyBarFillProgress() const override;
    /// The wielded weapon's proficiency XP.
    int currentProficiencyProgress() const override;
    /// The XP of the next level, never past the weapon's last level.
    int proficiencyProgressNextLevel() const override;

    /// CIP : the other positions and styles, the shown skills and
    /// options 0x71 statusBarEnabled / 0x72 statusPanelEnabled.
    /// The other positions and styles, the shown skills and options 0x71
    /// statusBarEnabled / 0x72 statusPanelEnabled.
    Q_INVOKABLE void requestContextMenu() override;
    Q_INVOKABLE void onClickedOpenStoreXPBoost() override;
    /// Opens the weapon proficiency dialog on the wielded weapon (on none
    /// with an empty hand).
    Q_INVOKABLE void onClickedOpenProficiencies() override;

private:
    /// The hit points, mana, harmony, skills and the mana shield's visibility.
    void onPlayerDataChanged();
    /// The storage changed or the character's configuration was loaded: a
    /// position outside Top..Right becomes Top while statusBarEnabled is on.
    void onStatusbarOptionsChanged();
    /// The storage's position and style as they are (no session: no anchor).
    void applyStatusbarOptions();
    /// Drops a set anchor to Invalid first so the bar is laid out anew.
    void resetAndSetDialogAnchor(int anchor);
    /// New entries for the shown skills; an odd count leaves the last one alone.
    void rebuildSkillLists();
    /// The shown entries' values.
    void refreshSkills();
    /// The experience entry: the experience, the progress to the next level
    /// and a tooltip with the experience left; with an XP rate in the
    /// session's XP analyser it also names the rate per hour and the time to
    /// the next level.
    game::SkillsWidgetEntry experienceEntry() const;
    void setDialogAnchor(DialogAnchor anchor);
    void setDialogStyle(DialogStyle style);
    /// The storage's position as an anchor: anything past Right is Invalid.
    static DialogAnchor anchorFromPosition(int position);
    /// The storage's style: anything but Compact, Large and Parallel is Default.
    static DialogStyle styleFromStorage(int style);
    /// Whether the storage shows the entry at `index` (Storage::Entry order).
    bool isSkillShown(int index) const;
    void setSkillShown(int index, bool shown);
    /// The type of the object in the weapon hand; null with an empty hand.
    const appearances::TAppearanceType* equippedWeapon() const;

    config::TClientOptions* m_pClientOptions = nullptr;
    std::shared_ptr<game::IGameSession> m_pGameSession;
    shared::TQtConnectionManager m_gameSessionConnections;
    QPointer<game::TStatusbarStorage> m_storage;
    /// The session's proficiency storage and inventory, while in the world.
    QPointer<proficiency::TProficiencyStorage> m_proficiencyStorage;
    QPointer<container::TInventoryContainer> m_inventory;
    game::TPlayerStatesTracker m_states;
    /// The XP bar's entry lives as long as the controller; the skills' are
    /// made anew on each rebuild.
    game::TSkillsWidgetEntryData* m_xpEntry = nullptr;
    /// The skill type of each m_skillList entry, then of m_remainingSkill.
    QList<game::SkillType> m_skillTypes;
    game::SkillType m_remainingSkillType = game::SkillType::Experience;
    bool m_oddSkillCount = false;
};

}
}
