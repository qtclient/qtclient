#pragma once

#include <QJsonObject>
#include <QKeySequence>
#include <QObject>
#include <QString>
#include <QStringList>

#include <map>
#include <optional>
#include <vector>

namespace tibia::config {

/// CIP's useType strings (FUN_100e5f460 order).
enum class EUseType : int
{
    Use = 0,
    UseOnYourself = 1,
    UseOnTarget = 2,
    SelectUseTarget = 3,
    Equip = 4,
    UseAtMouseCursor = 5,
};

/// One "actionsetting" of clientoptions.json's hotkeyOptions (CIP:
/// TActionSetting variants, read by FUN_100e5ae00 / written by FUN_100e5b2c0).
struct TActionSetting
{
    enum EKind : int
    {
        NONE,
        /// "chatText": text said, or put in the chat input.
        TEXT,
        /// "useObject".
        OBJECT,
        /// "passiveAbility".
        PASSIVE_ABILITY,
        /// "action": a static hotkey action name (CIP's FUN_100e893b0 table).
        ACTION,
        /// "multiActions".
        MULTI_ACTION,
    };

    EKind kind = NONE;
    QString text;
    bool sendAutomatically = false;
    /// ESpellCastOptionalTargetType.
    int spellCastType = 0;
    int objectId = 0;
    EUseType useType = EUseType::Use;
    std::optional<int> upgradeTier;
    std::optional<int> liquidType;
    bool useEquipSmartMode = false;
    int passiveAbility = 0;
    QString action;
    std::vector<TActionSetting> multiActions;

    bool isEmpty() const { return kind == NONE; }
    static TActionSetting fromJson(const QJsonObject& json);
    QJsonObject toJson() const;
    bool operator==(const TActionSetting& other) const;
};

/// A key bound to an action setting ("chatOn"/"chatOff" entries).
struct THotkeyBinding
{
    QKeySequence keys;
    TActionSetting setting;
};

/// One hotkey preset: the action bar mappings and both chat modes' keys.
struct THotkeySet
{
    std::map<std::pair<int, int>, TActionSetting> mappings;
    std::vector<THotkeyBinding> chatOff;
    std::vector<THotkeyBinding> chatOn;
    /// actionBarOptions' other keys (firstVisibleButtons), kept as read.
    QJsonObject actionBarExtra;
};

/// `conf/clientoptions.json` `hotkeyOptions` (CIP: THotkeyOptions; user file
/// `<AppData>/conf/clientoptions.json`). Presets round-trip via load/save;
/// character login may auto-switch to a set named after the character
/// (TRY_TO_SWITCH_HOTKEYSET) when autoSwitchHotkeyPreset is on.
class THotkeyOptions final : public QObject
{
    Q_OBJECT
public:
    static THotkeyOptions* instance();

    /// Reads the section from the options files; CIP's built-in sets
    /// (:/config/defaulthotkeys.json, GlobalShortcuts merged into every set)
    /// when there is none.
    void load();
    void save();

    QString currentSetName() const { return m_current; }
    QStringList setNames() const;
    bool autoSwitchHotkeyPreset() const { return m_autoSwitch; }
    bool showStaticHotkeysFirst() const { return m_showStaticFirst; }
    THotkeySet& currentSet();
    const THotkeySet* set(const QString& name) const;

    /// CIP FUN_100e98280's lookup: the bar button's setting in the current set.
    TActionSetting mapping(int actionBarId, int buttonId) const;
    void setMapping(int actionBarId, int buttonId, const TActionSetting& setting);
    /// actionBarOptions.firstVisibleButtons of the current set: the 0-based
    /// index of a bar's first scrolled-in button by bar id (CIP: read by
    /// FUN_100a2ec40, written by FUN_100a2ea10).
    int firstVisibleButton(int actionBarId) const;
    void setFirstVisibleButton(int actionBarId, int index);
    /// The key bound to TriggerActionButton_<bar>.<button> (the button's label).
    QKeySequence buttonKeys(int actionBarId, int buttonId, bool chatOn) const;
    /// Every key bound to TriggerActionButton_<bar>.<button> in one chat mode
    /// (CIP FUN_1009d40d0 via FUN_1009d3eb0, the button tooltip's hotkeys).
    QList<QKeySequence> allButtonKeys(int actionBarId, int buttonId, bool chatOn) const;
    void setButtonKeys(int actionBarId, int buttonId, bool chatOn, const QKeySequence& keys);
    /// Keys bound to a static / product action name in one chat mode.
    QList<QKeySequence> actionKeys(const QString& action, bool chatOn) const;
    /// Replaces the keys for `action` in one chat mode (CIP options-page apply).
    void setActionKeys(const QString& action, bool chatOn, const QList<QKeySequence>& keys);
    /// The setting a key runs in one chat mode.
    const TActionSetting* binding(const QKeySequence& keys, bool chatOn) const;
    /// Replace the whole hotkeyOptions section (options-page Apply).
    void replaceAll(const QJsonObject& section);
    /// Current section as JSON (options-page Load working copy).
    QJsonObject toJson() const;
    void setAutoSwitchHotkeyPreset(bool enabled);
    void setShowStaticHotkeysFirst(bool enabled);

    /// CIP FUN_100e84e40 / FUN_100e852a0: the set after / before the current
    /// one in name order.
    void switchSet(int delta);
    void selectSet(const QString& name);
    /// Loads the sets on the first login; the switch to the set named after the
    /// character is TRY_TO_SWITCH_HOTKEYSET (CIP FUN_1009111d0 → FUN_100e848d0).
    void onCharacterLoggedIn();

    static QString triggerActionButtonName(int actionBarId, int buttonId);

    /// Load → edit mapping → switchSet → save → reload against
    /// `<AppData>/conf/clientoptions.json` `hotkeyOptions`. Empty on success.
    static QString selfCheckRoundTrip();

Q_SIGNALS:
    void mappingsChanged();
    void keysChanged();
    /// The current set changed (all of the above).
    void currentSetChanged();

private:
    THotkeyOptions() = default;
    void readFrom(const QJsonObject& section);

    std::map<QString, THotkeySet> m_sets;
    QString m_current;
    bool m_autoSwitch = false;
    bool m_showStaticFirst = true;
    bool m_loaded = false;
};

}
