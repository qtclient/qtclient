#pragma once

#include "appearances/objectappearanceinstancetypeid.h"
#include "config/hotkeyoptions.h"
#include "game/cooldownstorage.h"

#include <cip_gen.h>

#include <memory>

namespace tibia {
namespace game {
class IGameSession;
}
namespace gamewindow {

class TActionBarController;

/// One button of an action bar, or one slot of a multi-action button
/// (CIP: TActionButtonController; setters spell, 
/// text, object, passive ability; reset
/// ). Passive ability overlays use
/// TCooldownStorage::passiveCooldown (S 0x5E / onPassiveData).
class TActionButtonController final : public cip::tibia::gamewindow::TActionButtonController
{
    Q_OBJECT
public:
    /// `multiActionIndex` -1 for a bar button.
    TActionButtonController(TActionBarController* bar, int buttonId, int multiActionIndex, QObject* parent = nullptr);

    TActionBarController* bar() const { return m_bar; }
    bool isMultiActionChild() const { return m_multiActionIndex >= 0; }
    /// The slot's index in its button, -1 for a bar button.
    int multiActionIndex() const override { return m_multiActionIndex < 0 ? -1 : m_multiActionIndex; }
    /// Whether this button holds multi-action slots.
    bool isMultiAction() const { return !m_children.empty(); }
    /// A multi-action button's slot, or null.
    TActionButtonController* multiAction(int index) const
    {
        return index >= 0 && index < int(m_children.size()) ? m_children[size_t(index)] : nullptr;
    }

    /// Empties the button: no action, no cooldown, enabled, not newly
    /// unlocked; its slots are emptied too but kept.
    void reset();
    /// Drops the multi-action slots and closes the popup.
    void clearMultiActions();
    /// Makes this a multi-action button with `count` empty slots (no-op when
    /// it has slots already).
    void createMultiActions(int count);
    /// The action setters: each one leaves the button as it is when nothing
    /// changes, otherwise empties it, drops its slots and shows the action.
    void setSpell(int spellId, const QString& parameters, int castType);
    /// The spell's optional cast target type; emits changed.
    void setCastType(int castType);
    /// The GUI tutorial marker shown on the button (0: none).
    void setTutorialMarkerID(int tutorialMarkerID);
    void setText(const QString& text, bool sendAutomatically);
    /// The object instance (type, liquid or tier) and how it is used.
    void setObject(const appearances::TObjectAppearanceInstanceTypeID& objectInstance, config::EUseType useType, bool useEquipSmartMode);
    /// The amount shown on an object button; announces a change.
    void setObjectCount(uint objectCount);
    /// The server's name of the button's object (only while it holds an
    /// object); announces a change.
    void setObjectInfoName(const QString& objectInfoName);
    /// The object instance of the shown slot.
    const appearances::TObjectAppearanceInstanceTypeID& objectInstance() const { return shown()->m_objectInstance; }
    /// The shown slot's object as the tooltip names it: the server's name
    /// (with the tier when it has one), else a placeholder.
    QString objectTooltipName() const;
    void setPassiveAbility(int passiveAbility);
    /// The keys of TriggerActionButton_<bar>.<button>; the first one is shown
    /// on the button.
    void setHotkeys(const QList<QKeySequence>& hotkeys);
    // The action of the shown slot (a bar button's own action without slots).
    int spellId() const { return shown()->m_spellId; }
    /// The spell's parameters or the text, as entered.
    QString actionText() const { return shown()->m_type == SPELL ? shown()->m_spellParameters : shown()->m_text; }
    int castType() const { return shown()->m_castType; }
    bool sendAutomatically() const { return shown()->m_sendAutomatically; }
    config::EUseType useType() const { return shown()->m_useType; }
    bool useEquipSmartMode() const { return shown()->m_useEquipSmartMode; }
    int passiveAbility() const { return shown()->m_passiveAbility; }
    /// Takes the session whose cooldowns the button shows (its slots too).
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// Whether the button can be used now (the bar's enable conditions);
    /// announces a change.
    void setIsEnabled(bool isEnabled);
    /// Whether the equip button's object is worn; announces a change.
    void setObjectIsEquiped(bool objectIsEquiped);
    /// CIP 's per-button part with / :
    /// the remaining cooldown at `now`. Returns whether one runs.
    bool refreshCooldown(qint64 now);
    /// CIP / : the slot of a multi-action button
    /// that runs at `now` (the first usable slot without cooldown, else the one
    /// whose cooldown ends first by more than min(ping / 2, 400) ms, else the
    /// first unusable one), or null.
    TActionButtonController* currentMultiAction(qint64 now) const;
    bool isShownEmpty() const { return type() == EMPTY; }

    /// CIP : "S.t" below 11 s, "S" below a minute, "M:SS" below
    /// 11 minutes, then "%1m", "%1h%2", "%1h".
    static QString cooldownText(qint64 remainingMs);
    /// The key as the button shows it: each modifier by its first letter
    /// (upper case), then the key, e.g. "CSF1" for Ctrl+Shift+F1.
    static QString shortHotkeyText(const QKeySequence& key);

    // A multi-action button shows its current slot (CIP: every getter walks
    // , e.g. type, object).
    EQmlActionButtonType type() const override { return shown()->m_type; }
    EQmlActionButtonProgressType progressType() const override { return shown()->m_progressType; }
    QString spellIconUrl() const override { return shown()->m_spellIconUrl; }
    /// The shown slot's spell parameters, HTML-escaped; empty unless it casts a spell.
    QString spellParameters() const override { return shown()->m_type == SPELL ? shown()->m_spellParameters.toHtmlEscaped() : QString(); }
    /// The shown slot's text, HTML-escaped; empty unless it says a text.
    QString text() const override { return shown()->m_type == TEXT ? shown()->m_text.toHtmlEscaped() : QString(); }
    uint objectID() const override { return shown()->m_objectID; }
    uint objectCount() const override { return shown()->m_objectCount; }
    uint objectUpgradeTier() const override { return shown()->m_objectUpgradeTier; }
    cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::ELiquidType liquidType() const override { return shown()->m_liquidType; }
    cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::EHookDirection hookDirection() const override
    {
        return shown()->m_hookDirection;
    }
    bool objectIsEquiped() const override { return shown()->m_objectIsEquiped; }
    bool isEquipeAction() const override { return shown()->m_isEquipeAction; }
    bool isEnabled() const override { return shown()->m_isEnabled; }
    QString passiveAbilityIconUrl() const override { return shown()->m_passiveAbilityIconUrl; }
    uint cooldownRemainingPercent() const override { return shown()->m_cooldownRemainingPercent; }
    QString cooldownText() const override { return shown()->m_cooldownText; }
    /// CIP .
    QString tooltip() const override;

public Q_SLOTS:
    /// CIP : publishes ACTION_BUTTON_TRIGGERED.
    void onTriggered() override;
    /// CIP .
    void contextMenuRequested() override;
    void onTargetSelected() override;
    void onDragChanged(bool Dragged) override;

Q_SIGNALS:
    /// Asks the bar for this button's context menu (-1: not a slot).
    void requestContextMenu(uint ButtonId, int MultiActionChild);

private:
    const TActionButtonController* shown() const { return m_shownSlot ? m_shownSlot : this; }
    /// Re-selects the shown slot; announces a change.
    void refreshShownSlot(qint64 now);
    /// CIP : the tooltip's table rows for this button or slot.
    QString tooltipRows() const;
    /// CIP .
    bool isUsable() const;
    /// CIP : the cooldown left at `now` in ms.
    qint64 remainingCooldown(qint64 now) const;
    game::TCooldown currentCooldown() const;

    TActionBarController* m_bar;
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::shared_ptr<game::TCooldownStorage> m_cooldownStorage;
    QList<QKeySequence> m_hotkeys;
    int m_spellId = 0;
    int m_passiveAbility = 0;
    bool m_sendAutomatically = false;
    int m_castType = 0;
    config::EUseType m_useType = config::EUseType::SelectUseTarget;
    bool m_useEquipSmartMode = false;
    /// The object of an object button; m_text holds its server name.
    appearances::TObjectAppearanceInstanceTypeID m_objectInstance;
    bool m_showsCooldown = false;
    std::vector<TActionButtonController*> m_children;
    TActionButtonController* m_shownSlot = nullptr;
};

}
}
