#pragma once

#include "config/hotkeyoptions.h"

#include <cip_gen.h>

#include <QPointer>

#include <array>
#include <vector>

class QQmlEngine;
class QQuickItem;

namespace tibia::gamewindow {

class TActionBarManager;
class TActionButtonController;

using EActionBarPosition = cip::tibia::qmlcomponents::enums::EActionBarPosition;

/// One action bar, ActionBar.qml (CIP: TActionBarController, its 50 buttons
/// made by the ctor; mappings applied by FUN_100c80fc0, cooldowns by
/// FUN_100c80300). The first visible button is the current preset's
/// firstVisibleButtons entry (read by FUN_100c80df0, written by the property
/// setter FUN_100c80ec0 while the group allows it).
class TActionBarController final : public cip::tibia::gamewindow::TActionBarController
{
    Q_OBJECT
public:
    /// CIP's bars have 50 buttons.
    static constexpr int ButtonCount = 50;

    enum EAssignKind
    {
        ASSIGN_SPELL,
        ASSIGN_OBJECT,
        ASSIGN_TEXT,
        ASSIGN_PASSIVE_ABILITY,
    };

    TActionBarController(TActionBarManager* manager, int actionBarId, EActionBarPosition position, QObject* buttonsDisplayOptions,
                         QQuickItem* gameWindowItem, QQuickItem* mapWindowPane, QObject* parent = nullptr);

    TActionBarManager* manager() const { return m_manager; }
    /// 1-based, as CIP's button ids.
    TActionButtonController* button(int buttonId) const;
    const std::vector<TActionButtonController*>& buttons() const { return m_buttons; }

    /// ActionBar.qml in `container` (an actionBarContainer GridLayout).
    void createItem(QQmlEngine* engine, QQuickItem* container);
    void destroyItem();
    QQuickItem* item() const { return m_item; }

    /// CIP FUN_100c80fc0: the current hotkey set's mappings.
    void applyMappings(bool chatOn);
    void refreshObjects();
    /// Returns whether a cooldown runs.
    bool refreshCooldowns(qint64 now);
    void setLocked(bool locked);

    /// The setting of a button or of one of its multi-action slots.
    config::TActionSetting settingAt(int buttonId, int multiActionIndex) const;
    /// Writes a setting to the current hotkey set (CIP: the storage's
    /// setMapping, then onHotkeyOptionsChanged).
    void assign(int buttonId, int multiActionIndex, const config::TActionSetting& setting);
    void openAssignDialog(EAssignKind kind, int buttonId, int multiActionIndex);
    void editHotkey(int buttonId);
    /// CIP insertSpell (FUN_100c83620): puts the spell on the first empty
    /// button scrolled into view and starts its insert animation. Returns
    /// whether a button took it.
    bool insertSpell(int spellId, bool newlyUnlocked);
    /// CIP FUN_100c81f90.
    void setFirstVisibleButtonIdCanBeSaved(bool canBeSaved) { m_firstVisibleCanBeSaved = canBeSaved; }

    /// CIP FUN_100c80ec0.
    void setFirstVisibleButtonId(int v) override;

public Q_SLOTS:
    /// CIP FUN_100c833d0 / 0x100c9bcc0: an object dragged onto a button is
    /// assigned with its default use (FUN_1009d71a0, Alt held passed on).
    void onDropped(uint ButtonId, int MultiActionIndex) override;
    /// CIP FUN_100c83500.
    void onSwapActions(uint SourceActionBarId, uint SourceButtonId, int SourceMultiActionIndex, uint DestinationActionBarId,
                       uint DestinationButtonId, int DestinationMultiActionIndex) override;
    /// CIP FUN_100c830d0.
    void onSpellIconDropped(uint ButtonId, int SpellEnumId, int MultiActionIndex) override;
    void onSpellIconDropped(uint ButtonId, int SpellEnumId) override;
    void closeOpenMultiActionButtons() override;
    /// CIP FUN_100c837d0.
    void setNumVisibleSlots(int Amount) override;

private:
    TActionBarManager* m_manager;
    std::vector<TActionButtonController*> m_buttons;
    QPointer<QQuickItem> m_item;
    int m_numVisibleSlots = 0;
    bool m_firstVisibleCanBeSaved = true;
};

/// The bars at one side of the map, HorizontalActionBarContainer.qml or
/// VerticalActionBarContainer.qml (CIP: TActionBarGroup). Which bars show and
/// the lock come from the options actionBarsShow*, actionBarShow*N and
/// actionBar*Locked (CIP 0x100c856c0; lock button 0x100c86940).
class TActionBarGroup final : public cip::tibia::gamewindow::TActionBarGroup
{
    Q_OBJECT
public:
    TActionBarGroup(TActionBarManager* manager, EActionBarPosition position, QQuickItem* container, QQmlEngine* engine,
                    QQuickItem* gameWindowItem, QQuickItem* mapWindowPane, QObject* buttonsDisplayOptions, QObject* parent = nullptr);
    ~TActionBarGroup() override;

    EActionBarPosition position() const { return m_position; }
    const std::vector<TActionBarController*>& bars() const { return m_bars; }
    /// The ids of this side's bars (CIP FUN_100c84f80 / 85080 / 85180).
    static std::array<int, 3> barIds(EActionBarPosition position);
    /// Shows the bars the options ask for.
    void applyOptions();

public Q_SLOTS:
    void onActionBarsOptionsChanged() override;
    /// Toggles actionBar*Locked.
    void onLockButtonClicked() override;
    void setFirstVisibleButtonIdCanBeSaved(bool CanBeSaved) override;

private:
    EActionBarPosition m_position;
    QPointer<QQuickItem> m_container;
    QPointer<QQuickItem> m_barContainer;
    QQmlEngine* m_engine;
    std::vector<TActionBarController*> m_bars;
};

}
