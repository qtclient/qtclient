#pragma once

#include "client/gamecore.h"
#include "game/playerstatestracker.h"
#include "game/skillswidgetentrydata.h"
#include "game/statusbarstorage.h"

#include <cip_gen.h>

#include <QPointer>

#include <array>

namespace tibia::gamewindow {

/// StatusBar's controller (CIP: TStatusBarController; reads 0x100c0da10..
/// 0x100c0ec50): the player's hit points, mana and magic shield, the XP bar
/// and the skills the character chose, the special-condition icons and a
/// Monk's harmony and serenity. The bar sits at the storage's position.
/// Context menu (0x100c11390) is ported. The store XP boost button is the one
/// in CIP StatusBar.qml. Not yet: proficiency progress and the magic level
/// boost lines.
class TStatusBarController final : public cip::tibia::gamewindow::TStatusBarController
{
    Q_OBJECT
public:
    explicit TStatusBarController(QObject* parent = nullptr);

    /// The character's storage; null outside the world.
    void setStorage(game::TStatusbarStorage* storage);

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
    bool showPlayerStatesInBar() const override;

    /// CIP 0x100c11390: the other positions and styles, the shown skills and
    /// options 0x71 statusBarEnabled / 0x72 statusPanelEnabled.
    Q_INVOKABLE void requestContextMenu() override;
    Q_INVOKABLE void onClickedOpenStoreXPBoost() override;

private:
    /// CIP 0x100c0d530: on entering the world.
    void applyStorage();
    /// CIP 0x100c0d800: the storage changed.
    void onStorageChanged();
    /// CIP 0x100c0f810: which entries show, and where.
    void rebuildSkillLists();
    /// CIP 0x100c0f010.
    void refreshSkills();
    /// CIP 0x100c0d460.
    void updateFromPlayer();
    void setAnchorAndStyle(int anchor, int style, bool forceAnchorSignal);

    QPointer<game::TStatusbarStorage> m_storage;
    game::TPlayerStatesTracker m_states;
    std::array<game::TSkillsWidgetEntryData*, game::TStatusbarStorage::EntryCount> m_entries{};
    std::vector<client::gamecore::Hook> m_hooks;
};

}
