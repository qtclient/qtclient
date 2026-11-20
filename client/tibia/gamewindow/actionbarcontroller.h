#pragma once

#include "config/hotkeyoptions.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <QPointer>

#include <array>
#include <map>
#include <memory>
#include <vector>

class QQmlEngine;
class QQuickItem;

namespace tibia {
namespace gamewindow {

class TActionBarManager;
class TActionButtonController;
class TAssignObjectActionDialogController;
class TAssignPassiveAbilityActionDialogController;
class TAssignSpellActionDialogController;
class TAssignTextActionDialogController;
class TRecordKeyboardShortcutDialogController;

using EActionBarPosition = cip::tibia::qmlcomponents::enums::EActionBarPosition;

/// One action bar, ActionBar.qml (CIP: TActionBarController, its 50 buttons
/// made by the ctor; mappings applied by , cooldowns by
/// ). The first visible button is the current preset's
/// firstVisibleButtons entry (read by , written by the property
/// setter while the group allows it).
class TActionBarController final : public cip::tibia::gamewindow::TActionBarController
{
    Q_OBJECT
public:
    /// CIP's bars have 50 buttons.
    static constexpr int ButtonCount = 50;

    TActionBarController(TActionBarManager* manager, int actionBarId, EActionBarPosition position, QObject* buttonsDisplayOptions,
                         QQuickItem* gameWindowItem, QQuickItem* mapWindowPane, QObject* parent = nullptr);
    ~TActionBarController() override;

    TActionBarManager* manager() const { return m_manager; }
    /// 1-based, as CIP's button ids.
    TActionButtonController* button(int buttonId) const;
    const std::vector<TActionButtonController*>& buttons() const { return m_buttons; }

    /// ActionBar.qml in `container` (an actionBarContainer GridLayout).
    void createItem(QQmlEngine* engine, QQuickItem* container);
    void destroyItem();
    QQuickItem* item() const { return m_item; }

    /// The button with `buttonId`, or its multi-action slot when
    /// `multiActionChild` >= 0; null when there is none.
    TActionButtonController* buttonController(int buttonId, int multiActionChild) const;
    void refreshObjects();
    /// Returns whether a cooldown runs.
    bool refreshCooldowns(qint64 now);
    void setLocked(bool locked);

    /// The setting of a button or of one of its multi-action slots.
    config::TActionSetting settingAt(int buttonId, int multiActionIndex) const;
    /// Writes a setting to the current hotkey set (CIP: the storage's
    /// setMapping, then onHotkeyOptionsChanged).
    void assign(int buttonId, int multiActionIndex, const config::TActionSetting& setting);
    /// The spell dialog for a button, opened with `spellId` and no parameters.
    void openSpellDialog(int buttonId, int multiActionIndex, int spellId);
    /// CIP insertSpell : puts the spell on the first empty
    /// button scrolled into view and starts its insert animation. Returns
    /// whether a button took it.
    bool insertSpell(int spellId, bool newlyUnlocked);
    /// CIP .
    void setFirstVisibleButtonIdCanBeSaved(bool canBeSaved) { m_firstVisibleCanBeSaved = canBeSaved; }

    /// CIP .
    void setFirstVisibleButtonId(int v) override;

public Q_SLOTS:
    /// The current hotkey set's mappings of this bar onto its buttons.
    void onActionBarOptionsChanged() override;
    /// The character's first visible button from TActionBarOptions.
    void onCharacterConfigurationLoaded();
    /// The keys of each button's TriggerActionButton action in the current
    /// chat mode.
    void onHotkeyOptionsChanged() override;
    /// Assign/edit entries for spell, object and text; for a bar button also
    /// passive ability, multi-action and hotkey; "clear" when it holds an action.
    void onRequestContextMenu(uint ButtonId, int MultiActionChild);
    void onOpenAssignSpellActionDialog(uint ButtonId, int MultiActionChild);
    void onOpenAssignObjectActionDialog(uint ButtonId, int MultiActionChild);
    void onOpenAssignTextActionDialog(uint ButtonId, int MultiActionChild);
    void onOpenAssignPassiveAbilityActionDialog(uint ButtonId);
    /// Closes the other open multi-action buttons, toggles this one's popup
    /// and makes its mapping a multi-action one with three slots.
    void onOpenAssignMultiActionPopup(uint ButtonId);
    void onAssignHotkey(uint ButtonId);
    void onClearAction(uint ButtonId, int MultiActionChild);
    /// Closes every open multi-action button but the given one.
    void closeOpenMultiActionButtons(int ExceptBarID, int ExceptButtonID);
    /// CIP / : an object dragged onto a button is
    /// assigned with its default use (, Alt held passed on).
    void onDropped(uint ButtonId, int MultiActionIndex) override;
    /// CIP .
    void onSwapActions(uint SourceActionBarId, uint SourceButtonId, int SourceMultiActionIndex, uint DestinationActionBarId,
                       uint DestinationButtonId, int DestinationMultiActionIndex) override;
    /// CIP .
    void onSpellIconDropped(uint ButtonId, int SpellEnumId, int MultiActionIndex) override;
    void onSpellIconDropped(uint ButtonId, int SpellEnumId) override;
    void closeOpenMultiActionButtons() override { closeOpenMultiActionButtons(0, 0); }
    /// The assign dialogs' requests: the button's mapping in the hotkey options.
    void onRequestAssignSpell(uint ButtonId, int MultiActionIndex, int SpellId, const QString& Parameters, int CastType);
    void onRequestAssignObject(uint ButtonId, int MultiActionIndex, int ObjectId, int UpgradeTier, int LiquidType,
                               tibia::config::EUseType UseType, bool UseEquipSmartMode);
    void onRequestAssignText(uint ButtonId, int MultiActionIndex, const QString& Text, bool SendAutomatically);
    void onRequestAssignPassiveAbility(uint ButtonId, int PassiveAbility);
    /// The key recorder's key becomes the hotkey of the button it was opened for.
    void onAssignSelectedKeySequenceToActionButton(const QKeySequence& NewKeySequence);
    /// Closes the four assign dialogs and the key recorder.
    void closeAllDialogs() override;
    /// CIP .
    void setNumVisibleSlots(int Amount) override;

private:
    /// Shows one setting on a button or slot.
    void applySetting(TActionButtonController* button, const config::TActionSetting& setting);

    TActionBarManager* m_manager;
    /// The bar's assign dialogs and key recorder (CIP +0xf0, +0x220, +0x2a8,
    /// +0x310, +0x3d8).
    std::unique_ptr<TAssignSpellActionDialogController> m_spellDialog;
    std::unique_ptr<TAssignObjectActionDialogController> m_objectDialog;
    std::unique_ptr<TAssignTextActionDialogController> m_textDialog;
    std::unique_ptr<TAssignPassiveAbilityActionDialogController> m_passiveAbilityDialog;
    std::unique_ptr<TRecordKeyboardShortcutDialogController> m_hotkeyDialog;
    /// The button the key recorder was opened for (CIP +0x4a0).
    int m_hotkeyButtonId = 0;
    std::vector<TActionButtonController*> m_buttons;
    QPointer<QQuickItem> m_item;
    int m_numVisibleSlots = 0;
    bool m_firstVisibleCanBeSaved = true;
};

/// The client options of the action bars (CIP: config::TClientOptions
/// boolean options 0x0-0xe, read and toggled by id).
enum class EActionBarOption
{
    BottomLocked = 0,
    LeftLocked = 1,
    RightLocked = 2,
    ShowBottomBar1 = 3,
    ShowBottomBar2 = 4,
    ShowBottomBar3 = 5,
    ShowBottomBars = 6,
    ShowLeftBar1 = 7,
    ShowLeftBar2 = 8,
    ShowLeftBar3 = 9,
    ShowLeftBars = 10,
    ShowRightBar1 = 11,
    ShowRightBar2 = 12,
    ShowRightBar3 = 13,
    ShowRightBars = 14,
};

/// Host adapter: the option's value in the client options file.
bool isActionBarOptionEnabled(EActionBarOption option);
void setActionBarOptionEnabled(EActionBarOption option, bool enabled);

/// The bars of one side of the game window (CIP: TActionBarGroup). Which
/// bars exist follows the options: CIP's bimaps tie the side to its group
/// and lock options and each bar id to its show option; a bar's controller
/// lives while its option and its group's are on.
class TActionBarGroup final : public cip::tibia::gamewindow::TActionBarGroup
{
    Q_OBJECT
public:
    TActionBarGroup(TActionBarManager* manager, EActionBarPosition position, QQuickItem* container, QQmlEngine* engine,
                    QQuickItem* gameWindowItem, QQuickItem* mapWindowPane, QObject* buttonsDisplayOptions, QObject* parent = nullptr);
    ~TActionBarGroup() override;

    EActionBarPosition position() const { return m_position; }
    /// The bars that exist, by id.
    std::vector<TActionBarController*> bars() const;
    /// The bar `actionBarId`, if it exists in this group.
    TActionBarController* bar(uint8_t actionBarId) const;
    /// The ids of a side's bars (CIP bimaps , , ).
    static std::array<int, 3> barIds(EActionBarPosition position);
    /// The side whose group manages `actionBarId`; throws for an unknown id.
    static EActionBarPosition positionOfBar(uint8_t actionBarId);
    /// The show option of `actionBarId`; throws for an unknown id.
    static EActionBarOption showOptionOfBar(uint8_t actionBarId);
    /// Whether this group manages `actionBarId`.
    bool managesBar(uint8_t actionBarId) const;

    uint actionBarCount() const override { return static_cast<uint>(m_bars.size()); }
    bool locked() const override;

    /// CIP: TOGGLE_ACTION_BARS of this side and TOGGLE_ACTION_BAR of its
    /// bars toggle their options.
    void handleGameAction(const input::IGameAction& action);

public Q_SLOTS:
    void restoreLastAktionBarConfiguration() override;
    /// Creates the bars whose options are on, destroys the others.
    void onActionBarsOptionsChanged() override;
    void onLockButtonClicked() override;
    void setFirstVisibleButtonIdCanBeSaved(bool CanBeSaved) override;

private:
    void createActionBar(uint8_t actionBarId);
    void destroyActionBar(uint8_t actionBarId);
    void toggleOption(EActionBarOption option);

    TActionBarManager* m_manager;
    QPointer<QQuickItem> m_container;
    QQmlEngine* m_engine;
    QQuickItem* m_gameWindowItem;
    QQuickItem* m_mapWindowPane;
    QObject* m_buttonsDisplayOptions;
    EActionBarPosition m_position;
    bool m_firstVisibleButtonIdCanBeSaved = true;
    std::map<uint8_t, std::unique_ptr<TActionBarController>> m_bars;
};

}
}
