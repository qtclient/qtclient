#pragma once

#include <QJsonObject>
#include <QKeySequence>
#include <QObject>
#include <QString>
#include <QStringList>

#include <map>
#include <optional>
#include <vector>

namespace tibia {
namespace config {

/// CIP's useType strings (order).
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
/// TActionSetting variants, read by / written by).
struct TActionSetting
{
    /// A chat text this long or longer fails the options load.
    static constexpr qsizetype MaxChatTextLength = 256;
    /// Actions a multi-action setting holds at most.
    static constexpr qsizetype MaxMultiActions = 3;

    enum EKind : int
    {
        NONE,
        /// "chatText": text said, or put in the chat input.
        TEXT,
        /// "useObject".
        OBJECT,
        /// "passiveAbility".
        PASSIVE_ABILITY,
        /// "action": a static hotkey action name (CIP's table).
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

    /// No text, action, object, passive ability or multi actions.
    bool isEmpty() const
    {
        return text.isEmpty() && action.isEmpty() && objectId == 0 && kind != PASSIVE_ABILITY && multiActions.empty();
    }
    /// A chat text; a non-empty one drops the object, its liquid and the
    /// multi actions.
    void setChatText(const QString& chatText);
    void setSendAutomatically(bool enabled) { sendAutomatically = enabled; }
    /// An object to use: drops the liquid, the tier, the passive ability and
    /// the multi actions; a non-zero object also drops the chat text.
    void setUseObject(int objectClientId);
    /// An object holding a liquid (a fluid container or splash).
    void setUseObject(int objectClientId, int liquid);
    /// An object of an upgrade tier.
    void setUseObjectWithTier(int objectClientId, int tier);
    /// A passive ability; a non-zero one drops the chat text, the object, its
    /// liquid and the multi actions (the tier stays).
    void setPassiveAbility(int ability);
    /// ESpellCastOptionalTargetType of a spell's chat text.
    void setSpellCastType(int castType) { spellCastType = castType; }
    void setUseType(EUseType type) { useType = type; }
    void setUseEquipSmartMode(bool enabled) { useEquipSmartMode = enabled; }
    static TActionSetting fromJson(const QJsonObject& json);
    QJsonObject toJson() const;
    bool operator==(const TActionSetting& other) const;
};

/// A key bound to an action setting ("chatOn"/"chatOff" entries).
struct THotkeyBinding
{
    QKeySequence keys;
    TActionSetting setting;

    bool operator==(const THotkeyBinding&) const = default;
};

/// One hotkey preset: the action bar mappings and both chat modes' keys.
struct THotkeySet
{
    std::map<std::pair<int, int>, TActionSetting> mappings;
    std::vector<THotkeyBinding> chatOff;
    std::vector<THotkeyBinding> chatOn;
    /// actionBarOptions' other keys (firstVisibleButtons), kept as read.
    QJsonObject actionBarExtra;

    bool operator==(const THotkeySet&) const = default;
};

/// The hotkeyOptions section as a value: the copy the hotkeys options page
/// edits and hands back on apply (CIP: config::THotkeyOptions).
struct THotkeyOptionsData
{
    QString currentHotkeySetName;
    bool autoSwitchHotkeyPreset = false;
    bool showStaticHotkeysFirst = true;
    std::map<QString, THotkeySet> hotkeySets;

    /// Throws std::runtime_error when a chat text is too long.
    static THotkeyOptionsData fromJson(const QJsonObject& section);
    QJsonObject toJson() const;

    /// After every load: each set gains the GlobalShortcuts bindings whose
    /// action it does not bind yet and whose keys are free in that chat mode
    /// (the arrow keys, and the keys of the movement actions, also with Ctrl,
    /// Shift or Alt when that modifier rotates). With no set at all, the
    /// vocation presets are created, Knight current and auto-switch on.
    void addGlobalShortcuts(bool rotateWithCtrl, bool rotateWithShift, bool rotateWithAlt);

    THotkeySet* set(const QString& name);
    void setCurrentSet(const QString& name);
    /// The set names in name order (case-sensitive).
    QStringList hotkeySetNames() const;
    /// The current set's index in hotkeySetNames(); -1 when it is not there.
    int currentHotkeySetIndex() const;
    /// The set at index of hotkeySetNames() becomes current; nothing for an
    /// index outside the list.
    void setCurrentHotkeySetIndex(int index);
    /// Keeps a set already named name.
    void addSet(const QString& name, const THotkeySet& set);
    void renameSet(const QString& from, const QString& to);
    void removeSet(const QString& name);
    /// The bindings of one chat mode of a set.
    void clearBindings(const QString& setName, bool chatOn);
    void addBinding(const QString& setName, bool chatOn, const QKeySequence& keys, const TActionSetting& setting);
    void setBindings(const QString& setName, bool chatOn, const std::vector<THotkeyBinding>& bindings);
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

    /// The vocation presets' set names (Knight first, CIP's default set).
    static const QStringList& vocationSetNames();
    /// CIP built-in `hotkeyOptions`: the vocation presets, Knight current.
    static QJsonObject builtInSection();
    /// CIP's default hotkey sets by name: each vocation preset with
    /// GlobalShortcuts' keys merged in, and GlobalShortcuts itself.
    static QJsonObject builtInSets();
    /// The built-in set named name: a vocation preset with GlobalShortcuts'
    /// keys, or GlobalShortcuts alone.
    static THotkeySet builtInSet(const QString& name);

    QString currentSetName() const { return m_current; }
    QStringList setNames() const;
    bool autoSwitchHotkeyPreset() const { return m_autoSwitch; }
    bool showStaticHotkeysFirst() const { return m_showStaticFirst; }
    THotkeySet& currentSet();
    const THotkeySet* set(const QString& name) const;

    /// CIP 's lookup: the bar button's setting in the current set.
    TActionSetting mapping(int actionBarId, int buttonId) const;
    void setMapping(int actionBarId, int buttonId, const TActionSetting& setting);
    /// The key bound to TriggerActionButton_<bar>.<button> (the button's label).
    QKeySequence buttonKeys(int actionBarId, int buttonId, bool chatOn) const;
    /// Every key bound to TriggerActionButton_<bar>.<button> in one chat mode
    /// (CIP via , the button tooltip's hotkeys).
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

    /// CIP / : the set after / before the current
    /// one in name order.
    void switchSet(int delta);
    void selectSet(const QString& name);
    /// Loads the sets on the first login; the switch to the set named after the
    /// character is TRY_TO_SWITCH_HOTKEYSET (CIP →).
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
}
