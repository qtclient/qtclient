#pragma once

#include "config/clientoptions.h"
#include "config/hotkeyoptions.h"
#include "config/optionspages.h"
#include "container/objectappearanceinstanceinfostorage.h"
#include "input/gameaction.h"
#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

#include <array>
#include <map>
#include <memory>
#include <set>
#include <vector>

class QQmlEngine;
class QQuickItem;

namespace tibia {
namespace chat {
class TChatModeManager;
}
namespace config {
class TActionBarOptions;
}
namespace container {
class TInventoryContainer;
}
namespace game {
class IGameSession;
class TPlayerData;
}
namespace input {
class TGameActionActionButtonTriggered;
}
namespace gamewindow {

class TActionButtonController;
class TAssignObjectActionDialogController;
class TAssignPassiveAbilityActionDialogController;
class TAssignSpellActionDialogController;
class TAssignTextActionDialogController;
class TRecordKeyboardShortcutDialogController;

using EActionBarPosition = cip::tibia::qmlcomponents::enums::EActionBarPosition;

/// One action bar, ActionBar.qml (CIP: TActionBarController). Every bar has
/// its own button display options page, its client options and chat mode
/// manager, its five dialogs, its 50 buttons and its own game session
/// connections (object infos, inventory, player data, the character's action
/// bar options, login). The group ticks it every frame: the buttons'
/// cooldowns, then the first visible button when the character's options
/// changed. Objects whose server name is not known yet are collected while
/// the mappings are applied and asked for once logged in.
class TActionBarController final : public cip::tibia::gamewindow::TActionBarController
{
    Q_OBJECT
public:
    /// CIP's bars have 50 buttons.
    static constexpr int ButtonCount = 50;

    /// `lockedOption`: the client option that locks the bar's group.
    /// ActionBar.qml is created in `container`.
    TActionBarController(uint8_t actionBarId, EActionBarPosition position, config::EClientOption lockedOption,
                         config::TClientOptions* clientOptions, chat::TChatModeManager* chatModeManager, int buttonCount,
                         QQmlEngine* engine, QQuickItem* container, QQuickItem* gameWindowItem, QQuickItem* mapWindowPane,
                         QObject* parent = nullptr);
    /// Releases ActionBar.qml (its controller is cleared and it is hidden).
    ~TActionBarController() override;

    /// 1-based, as CIP's button ids.
    TActionButtonController* button(int buttonId) const;
    const std::vector<TActionButtonController*>& buttons() const { return m_buttons; }

    void destroyItem();
    QQuickItem* item() const { return m_item; }

    /// The button with `buttonId`, or its multi-action slot when
    /// `multiActionChild` >= 0; null when there is none.
    TActionButtonController* buttonController(int buttonId, int multiActionChild) const;

    /// The group's lock option: the bar is locked while it is on.
    bool locked() const override;

    /// The setting of a button or of one of its multi-action slots.
    config::TActionSetting settingAt(int buttonId, int multiActionIndex) const;
    /// Writes a setting to the current hotkey set (CIP: the storage's
    /// setMapping, then onHotkeyOptionsChanged).
    void assign(int buttonId, int multiActionIndex, const config::TActionSetting& setting);
    /// CIP insertSpell : puts the spell on the first empty
    /// button scrolled into view and starts its insert animation. Returns
    /// whether a button took it.
    bool insertSpell(int spellId, bool newlyUnlocked);
    void setFirstVisibleButtonIdCanBeSaved(bool canBeSaved) { m_firstVisibleButtonIdCanBeSaved = canBeSaved; }

    void setFirstVisibleButtonId(int v) override;

    /// Drops the old session's storages and connections, takes the new
    /// session's, empties the buttons and applies the mappings again.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// Per frame: every button's and slot's cooldown at `now`, then the first
    /// visible button when the character's options changed.
    void tick(qint64 now);

public Q_SLOTS:
    /// The router's game actions: assign spell, close multi-action buttons,
    /// button triggered, tutorial markers removed.
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
    /// A triggered button of this bar is no longer newly unlocked.
    void handleActionButtonTriggeredGameAction(const std::shared_ptr<const input::TGameActionActionButtonTriggered>& pGameAction);
    /// The current hotkey set's mappings of this bar onto its buttons.
    void onActionBarOptionsChanged() override;
    /// The character's action bar options changed: marks the first visible
    /// button for re-reading.
    void onActionBarCharacterOptionsChanged() override;
    /// The character's first visible button from TActionBarOptions (saved
    /// back when the group allows it); clears the mark.
    void onActionBarCharacterOptionsChangedAsync() override;
    /// The button display options page reads the client options again.
    void onActionBarsOptionsChanged() override;
    /// The keys of each button's TriggerActionButton action in the current
    /// chat mode.
    void onHotkeyOptionsChanged() override;
    /// Every object button and slot takes its object's name from the object
    /// info storage.
    void onObjectInfosChanged() override;
    /// Every button and multi-action slot: a spell's or rune's requirements
    /// against the player (mana, learned, level, promotion, vocation, an
    /// aggressive one while powerless), an object's amount from the object
    /// info storage (its expire counterpart's included; none disables it), a
    /// passive ability's cooldown or counter data and vocations.
    void onEnableConditionsChanged() override;
    /// Every equip button and slot: whether its object is worn. In smart mode
    /// an object with an expire counterpart shows the variant that is worn,
    /// else the one that is not animated.
    void onInventoryChanged() override;
    /// Once logged in, asks the server for the names of the collected objects.
    void onRequestGetObjectInfos() override;
    /// The game login succeeded: the collected objects may be asked for now.
    void onGameLoginSuccessful() override;
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
    /// An object dragged onto a button is assigned with its default use (Alt
    /// held passed on).
    void onDropped(uint ButtonId, int MultiActionIndex) override;
    void onSwapActions(uint SourceActionBarId, uint SourceButtonId, int SourceMultiActionIndex, uint DestinationActionBarId,
                       uint DestinationButtonId, int DestinationMultiActionIndex) override;
    /// A spell with parameters opens the spell dialog; any other is assigned.
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
    void setNumVisibleSlots(int Amount) override;

private:
    /// An object setting as the bar shows it: the amount of a stackable
    /// object (with its expire counterpart's) and the object instance.
    struct TObjectInstance
    {
        uint count = 0;
        appearances::TObjectAppearanceInstanceTypeID instance;
    };

    /// What a button runs: its multi-action slots, or the button itself.
    static std::vector<TActionButtonController*> actionsOf(TActionButtonController* button);
    /// Shows one setting on a button or slot.
    void applySetting(TActionButtonController* button, const config::TActionSetting& setting);
    /// The object instance of an object setting; an instance whose name the
    /// object info storage does not know yet is collected for the request.
    TObjectInstance objectInstanceOfSetting(const config::TActionSetting& setting);
    void createItem(QQmlEngine* engine, QQuickItem* container);
    /// Whether the chat mode manager has the chat on.
    bool chatOn() const;

    /// Zero-interval single shot: asks for the collected objects' infos.
    QTimer m_objectInfosTimer;
    /// Set when the character's action bar options changed; the tick reads
    /// the first visible button then.
    bool m_actionBarCharacterOptionsChanged = false;
    bool m_firstVisibleButtonIdCanBeSaved = true;
    config::EClientOption m_lockedOption;
    /// The bar's own button display options (buttonsDisplayOptions).
    config::TActionBarsOptionsPage m_buttonsDisplayOptionsPage;
    config::TClientOptions* m_clientOptions;
    QPointer<chat::TChatModeManager> m_chatModeManager;
    QPointer<QQuickItem> m_item;
    shared::TQtConnectionManager m_connections;
    shared::TQtConnectionManager m_gameSessionConnections;
    /// The bar's assign dialogs and key recorder.
    std::unique_ptr<TAssignSpellActionDialogController> m_spellDialog;
    std::unique_ptr<TAssignObjectActionDialogController> m_objectDialog;
    std::unique_ptr<TAssignTextActionDialogController> m_textDialog;
    std::unique_ptr<TAssignPassiveAbilityActionDialogController> m_passiveAbilityDialog;
    std::unique_ptr<TRecordKeyboardShortcutDialogController> m_hotkeyDialog;
    /// The button the key recorder was opened for.
    int m_hotkeyButtonId = 0;
    /// The session and the storages the bar follows.
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::shared_ptr<container::TObjectAppearanceInstanceInfoStorage> m_objectInfoStorage;
    std::shared_ptr<container::TInventoryContainer> m_inventory;
    std::shared_ptr<game::TPlayerData> m_playerData;
    QPointer<config::TActionBarOptions> m_actionBarOptions;
    std::vector<TActionButtonController*> m_buttons;
    /// The objects of the mappings whose server name is not known yet.
    std::set<appearances::TObjectAppearanceInstanceTypeID, container::TObjectAppearanceInstanceInfoStorage::TCompare> m_objectInfoRequests;
    /// Set by the game login; cleared on a new session.
    bool m_gameLoginSuccessful = false;
    int m_numVisibleSlots = 0;
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
    TActionBarGroup(EActionBarPosition position, QQuickItem* container, QQmlEngine* engine, QQuickItem* gameWindowItem,
                    QQuickItem* mapWindowPane, config::TClientOptions* clientOptions, chat::TChatModeManager* chatModeManager,
                    QObject* parent = nullptr);
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
    /// The bar whose show option is `option`; throws for an option no group manages.
    static uint8_t barOfShowOption(EActionBarOption option);
    /// Whether this group manages `actionBarId`.
    bool managesBar(uint8_t actionBarId) const;

    uint actionBarCount() const override { return static_cast<uint>(m_bars.size()); }
    bool locked() const override;

    /// TOGGLE_ACTION_BARS of this side and TOGGLE_ACTION_BAR of its bars
    /// toggle their options; every bar gets the action too.
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
    /// Keeps the session for the bars made later and hands it to every bar.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// Per frame: ticks every bar.
    void tick(qint64 now);

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

    QPointer<QQuickItem> m_container;
    QQmlEngine* m_engine;
    QQuickItem* m_gameWindowItem;
    QQuickItem* m_mapWindowPane;
    config::TClientOptions* m_clientOptions;
    QPointer<chat::TChatModeManager> m_chatModeManager;
    EActionBarPosition m_position;
    bool m_firstVisibleButtonIdCanBeSaved = true;
    std::map<uint8_t, std::unique_ptr<TActionBarController>> m_bars;
    std::shared_ptr<game::IGameSession> m_gameSession;
};

}
}
