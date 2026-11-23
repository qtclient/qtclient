#pragma once

#include "shared/cip_typed.h"

#include "config/configciptypes.h"
#include "config/hotkeyoptions.h"

#include "config/specialconditionsoptions.h"

#include <cip_gen.h>
#include <cip_options.h>

#include <QJsonObject>

#include <span>

namespace tibia {
namespace config {

class TClientOptions;

/// A page property and the option it edits (CIP: the pairs each page's load
/// reads and its apply writes).
struct TOptionBinding
{
    enum Kind
    {
        Bool,
        /// The property is the option negated (Gameplay's autoChaseOff, ...).
        InvertedBool,
        Int,
        Real,
        /// A 0..1 option as 0..100: min(round(v * 100), 100), back min(p * 0.01, 1).
        Percent,
        /// lightAttenuationCloudsIndoor as a slider step: (1 - v) / 0.005, truncated.
        CloudsIndex,
    };

    const char* property;
    cip::EClientOption option;
    Kind kind;
};

/// What the options dialog drives on each page (CIP: TOptionPageQMLWrapperBase,
/// vtable slot 12 load(const TClientOptions&) and slot 13 apply(TClientOptions*)).
/// Pages hold pending values; nothing is written before apply.
class TOptionsPage : public ::tibia::CipTyped
{
public:
    virtual ~TOptionsPage() = default;
    /// Slot 12: the pending values from `options`.
    virtual void load(const TClientOptions& options) = 0;
    /// Slot 13: the pending values into `options` (the dialog's working copy).
    virtual void apply(TClientOptions& options) const = 0;
};

/// A generated page wrapper whose properties map straight to options.
template <class Base>
class TBoundOptionsPage : public Base, public TOptionsPage
{
public:
    TBoundOptionsPage(std::span<const TOptionBinding> bindings, QObject* parent)
        : Base(parent)
        , m_bindings(bindings)
    {}

    void load(const TClientOptions& options) override;
    void apply(TClientOptions& options) const override;

private:
    std::span<const TOptionBinding> m_bindings;
};

/// The bound properties from `options`; options without a value are skipped.
void loadBindings(QObject* page, std::span<const TOptionBinding> bindings, const TClientOptions& options);
/// The bound properties into `options`; options without a value are skipped.
void applyBindings(const QObject* page, std::span<const TOptionBinding> bindings, TClientOptions& options);

template <class Base>
void TBoundOptionsPage<Base>::load(const TClientOptions& options)
{
    loadBindings(this, m_bindings, options);
}

template <class Base>
void TBoundOptionsPage<Base>::apply(TClientOptions& options) const
{
    applyBindings(this, m_bindings, options);
}

/// CIP.
using TConsoleOptionsPage = TBoundOptionsPage<TConsoleQMLOptionsPage>;
/// CIP.
using TMiscOptionsPage = TBoundOptionsPage<TMiscQMLOptionsPage>;
/// CIP.
using TGameplayOptionsPage = TBoundOptionsPage<TGameplayQMLOptionsPage>;
/// CIP.
using TScreenshotsOptionsPage = TBoundOptionsPage<TScreenshotsQMLOptionsPage>;
/// CIP.
using TBattleSoundsOptionsPage = TBoundOptionsPage<TBattleSoundsQMLOptionsPage>;
/// CIP.
using TUISoundsOptionsPage = TBoundOptionsPage<TUISoundsQMLOptionsPage>;

/// The bindings of the pages above.
std::span<const TOptionBinding> actionBarsBindings();
std::span<const TOptionBinding> consoleBindings();
std::span<const TOptionBinding> miscBindings();
std::span<const TOptionBinding> gameplayBindings();
std::span<const TOptionBinding> screenshotsBindings();
std::span<const TOptionBinding> battleSoundsBindings();
std::span<const TOptionBinding> uiSoundsBindings();

/// CIP `TActionBarsQMLOptionsPage`: the bound options plus one "clear" flag per
/// action bar. The clear buttons toggle their flag; loading the page resets
/// them. The options dialog reads the flags when it applies.
class TActionBarsOptionsPage final : public TBoundOptionsPage<TActionBarsQMLOptionsPage>
{
    Q_OBJECT
public:
    using TBoundOptionsPage<TActionBarsQMLOptionsPage>::TBoundOptionsPage;

    void load(const TClientOptions& options) override;
    /// Every clear flag off, then changed.
    void resetClearFlags();

    void onClearBottomActionBar1Clicked() override { toggleClearFlag(m_clearBottomActionBar1); }
    void onClearBottomActionBar2Clicked() override { toggleClearFlag(m_clearBottomActionBar2); }
    void onClearBottomActionBar3Clicked() override { toggleClearFlag(m_clearBottomActionBar3); }
    void onClearLeftActionBar1Clicked() override { toggleClearFlag(m_clearLeftActionBar1); }
    void onClearLeftActionBar2Clicked() override { toggleClearFlag(m_clearLeftActionBar2); }
    void onClearLeftActionBar3Clicked() override { toggleClearFlag(m_clearLeftActionBar3); }
    void onClearRightActionBar1Clicked() override { toggleClearFlag(m_clearRightActionBar1); }
    void onClearRightActionBar2Clicked() override { toggleClearFlag(m_clearRightActionBar2); }
    void onClearRightActionBar3Clicked() override { toggleClearFlag(m_clearRightActionBar3); }

private:
    void toggleClearFlag(bool& flag)
    {
        flag = !flag;
        Q_EMIT changed();
    }
};

/// CIP; the loot style list is .
class TInterfaceOptionsPage final : public TBoundOptionsPage<TInterfaceQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit TInterfaceOptionsPage(QObject* parent = nullptr);

    /// The index of the pending colorize loot style's name in the list model (0 when absent).
    uint colorizeLootStyleIndex() const override;
    /// The style whose name the list entry shows; none past the list.
    void setColorizeLootStyleIndex(uint index) override;
    QStringList colorizeLootStyleListModel() const override;
};

/// CIP.
class TGameWindowOptionsPage final : public TBoundOptionsPage<TGameWindowQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit TGameWindowOptionsPage(QObject* parent = nullptr);

    /// CIP .
    Q_INVOKABLE void setShowTargetFrameHighlight(bool ShowFrame, bool ShowHighlight) override;
};

/// CIP / : the bound options plus the whole
/// specialConditionsOptions section.
class THudOptionsPage final : public TBoundOptionsPage<THudQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit THudOptionsPage(QObject* parent = nullptr);

    void load(const TClientOptions& options) override;
    void apply(TClientOptions& options) const override;

    /// Whether the HUD and the bar show special conditions at all (kept in the
    /// special conditions options).
    bool specialConditionsShowInHUDEnabled() const override;
    void setSpecialConditionsShowInHUDEnabled(bool enabled) override;
    bool specialConditionsShowInBarEnabled() const override;
    void setSpecialConditionsShowInBarEnabled(bool enabled) override;

    /// CIP.
    void setSpecialConditionShowInHUDEnabled(int SpecialConditionIndex, bool Enabled) override;
    void setSpecialConditionShowInBarEnabled(int SpecialConditionIndex, bool Enabled) override;
    /// CIP.
    void moveSpecialConditionUp(int SpecialConditionIndex) override;
    void moveSpecialConditionDown(int SpecialConditionIndex) override;

private:
    /// CIP .
    void rebuildSpecialConditionsModel();

    SpecialConditionsOptions m_specialConditions;
};

/// OptionsPageGraphics.qml: the borderless window, scaling, antialiasing,
/// vsync, frame-rate limit and renderer. The page keeps the options' values;
/// the list indices, list models and tooltip are computed when read.
class TGraphicsOptionsPage final : public TGraphicsQMLOptionsPage, public TOptionsPage
{
    Q_OBJECT
public:
    explicit TGraphicsOptionsPage(QObject* parent = nullptr);

    void load(const TClientOptions& options) override;
    void apply(TClientOptions& options) const override;

    /// The index of the pending antialiasing mode's name in the list model (0 when absent).
    uint antialiasingModeIndex() const override;
    /// The mode whose name the list entry shows. An index past the list resets
    /// the pending renderer to auto (CipSoft's code does so).
    void setAntialiasingModeIndex(uint index) override;
    QStringList antialiasingModeListModel() const override;
    /// Off and fixed while the software renderer limits the options.
    bool vSync() const override;
    void setVSync(bool enabled) override;
    /// The inverse of frameRateLimitEnabled.
    bool noFpsLimitEnabled() const override;
    void setNoFpsLimitEnabled(bool enabled) override;
    /// The index of the pending renderer's name in the list model (0 when absent).
    uint rendererIndex() const override;
    /// The renderer whose name the list entry shows; auto past the list.
    void setRendererIndex(uint index) override;
    /// The supported renderers' names in the list order.
    QStringList rendererListModel() const override;
    /// Why the pending renderer is not the running one, the help text and the
    /// unsupported renderers.
    QString tooltipTextDisabledRenderers() const override;
    bool rendererLimitationsActive() const override;
    bool isMetal() const override;

private:
    int m_antialiasingMode = 0;
    bool m_frameRateLimitEnabled = false;
    int m_frameRateLimit = 0;
    int m_renderer = 0;
};

/// CIP.
class TEffectsOptionsPage final : public TBoundOptionsPage<TEffectsQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit TEffectsOptionsPage(QObject* parent = nullptr);

    void load(const TClientOptions& options) override;
    void apply(TClientOptions& options) const override;

    /// Off while the software renderer limits the options.
    bool lightEffects() const override;
    /// Ignored while the software renderer limits the options.
    void setLightEffects(bool enabled) override;
};

/// OptionsPageControls.qml: the control and loot schemes (kept as schemes,
/// shown as list indices), keyboard delay, rotation keys and drag-and-drop.
class TControlsOptionsPage final : public TControlsQMLOptionsPage, public TOptionsPage
{
    Q_OBJECT
public:
    explicit TControlsOptionsPage(QObject* parent = nullptr);

    void load(const TClientOptions& options) override;
    void apply(TClientOptions& options) const override;

    uint controlSchemeIndex() const override;
    void setControlSchemeIndex(uint index) override;
    uint lootSchemeIndex() const override;
    void setLootSchemeIndex(uint index) override;
    /// Ctrl drags whole stacks: the inverse of dragAndDropDefaultActionIsMoveAll.
    bool ctrlToDragStacks() const override;
    void setCtrlToDragStacks(bool enabled) override;

private:
    int m_controlScheme = 0;
    int m_lootScheme = 0;
    bool m_dragAndDropDefaultActionIsMoveAll = false;
};

/// CIP / : controlButtonsOptions, shown by name
/// and stored by id.
class TButtonBarOptionsPage final : public TBoundOptionsPage<TButtonBarQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit TButtonBarOptionsPage(QObject* parent = nullptr);

    void load(const TClientOptions& options) override;
    void apply(TClientOptions& options) const override;

    QStringList enabledButtons() const override;
    QStringList disabledButtons() const override;
    /// CIP / /.
    Q_INVOKABLE void enableButton(const QString& ButtonName) override;
    Q_INVOKABLE void disableButton(const QString& ButtonName) override;
    Q_INVOKABLE void moveEnabledButtonUp(const QString& ButtonName) override;
    Q_INVOKABLE void moveEnabledButtonDown(const QString& ButtonName) override;

private:
    /// The disabled buttons in button type order, without duplicates.
    void sortDisabledButtons();

    QStringList m_enabled;
    QStringList m_disabled;
};

/// CIP. soundEngineInitialized is true when the
/// miniaudio engine is ready.
class TSoundOptionsPage final : public TBoundOptionsPage<TSoundQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit TSoundOptionsPage(QObject* parent = nullptr);

    void load(const TClientOptions& options) override;
    void apply(TClientOptions& options) const override;
    /// The sound pages are enabled while the sound engine runs and the master
    /// volume is above 0.
    bool soundEnabled() const override;
    bool soundEngineInitialized() const override;
    /// The sound engine's devices after the "auto" entry.
    QStringList soundDeviceListModel() const override;

private:
    void refreshSoundDevices();
};

/// CIP load / apply / rebuild. Lists
/// static + product actions (DefaultHotkeys), TriggerActionButton_* (ActionButtons),
/// and custom bindings (CustomHotkeys).
/// OptionsPageHotkeys.qml (CIP: THotkeysQMLOptionsPage). The page edits a
/// copy of the hotkey options (THotkeyOptionsData) and keeps one row per
/// action of the current set and chat mode; every edit writes the rows back
/// into the copy, Apply hands the copy to the client options.
class THotkeysOptionsPage final : public cip::tibia::config::THotkeysQMLOptionsPage, public TOptionsPage
{
    Q_OBJECT
public:
    explicit THotkeysOptionsPage(QObject* parent = nullptr);

    void setHotkeyMode(EHotkeyMode mode);
    void setAutoSwitchHotkeyPreset(bool v) override;
    void setChatModeOn(bool v) override;
    /// The current set's index in the name list.
    int hotkeySetNamesIndex() const override;
    /// The set at that index of the name list becomes current.
    void setHotkeySetNamesIndex(int v) override;
    void setNameFilter(const QString& v) override;

    void load(const TClientOptions& options) override;
    void apply(TClientOptions& options) const override;
    /// The current set's keys of the current chat mode back to the built-in
    /// ones of its vocation (GlobalShortcuts for other names).
    void resetToDefault();
    /// The current set loses every button mapping of the action bar (the
    /// options dialog's Apply, for the bars marked for clearing).
    void clearActionBarMappings(uint8_t actionBarId);
    /// Closes the page's text, message and key recording dialogs.
    void closeDialogs();

    Q_INVOKABLE void requestAddPreset() override;
    Q_INVOKABLE void requestRenameCurrentPreset() override;
    Q_INVOKABLE void requestCloneCurrentPreset() override;
    Q_INVOKABLE void requestRemoveCurrentPreset() override;
    Q_INVOKABLE void requestEditActionForHotkey(int HotkeyActionSettingIndex) override;
    Q_INVOKABLE void requestEnterNewHotkeyForAction(int HotkeyActionSettingIndex, int KeySequenceIndex) override;
    Q_INVOKABLE void requestAddNewCustomHotkey() override;

public Q_SLOTS:
    void onPresetTextEntered(const QString& EnteredText);
    void onMessageDialogConfirmed();
    void onNewKeySequenceSelectedForCurrentHotkeyActionSetting(const QKeySequence& NewKeySequence);
    /// HotkeyActionSettingIndex is a row of hotkeyModel; -1 adds a row.
    void onTextAssignedToHotkeyAction(int HotkeyActionSettingIndex, int MultiActionIndex, const QString& Text, bool SendAutomatically);

private:
    enum class EPresetTextMode { None = 0, Add = 1, Clone = 2, Rename = 3 };

    /// The rows of the current set and chat mode, then the list model.
    void rebuildModel();
    /// hotkeyModel: the visible rows.
    void updateHotkeyListModel();
    /// The rows' keys into the current set's chat mode; rows without keys
    /// stay when keepUnboundRows.
    void commitRows(bool keepUnboundRows);
    void refreshSetNames();
    void openPresetDialog(EPresetTextMode mode, const QString& caption, const QString& description, const QString& text);
    QString currentSetName() const { return m_options.currentHotkeySetName; }

    THotkeyOptionsData m_options;
    QList<QObject*> m_rows;
    QObject* m_editRow = nullptr;
    int m_editKey = 0;
    EPresetTextMode m_presetTextMode = EPresetTextMode::None;
};

}
}
