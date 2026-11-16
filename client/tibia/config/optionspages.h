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
        /// An option the page has no property for: read on load and written
        /// back unchanged on apply. `property` names the dynamic property
        /// that holds it.
        Carried,
    };

    const char* property;
    cip::EClientOption option;
    Kind kind;
};

/// Reads an option from clientoptions.json's root.
QJsonValue optionValue(const QJsonObject& settings, cip::EClientOption option);
/// Sets an option in a root-shaped object of changes.
void setOptionValue(QJsonObject& settings, cip::EClientOption option, const QJsonValue& value);

/// What the options dialog drives on each page (CIP: TOptionPageQMLWrapperBase,
/// vtable slot 12 load(const TClientOptions&) and slot 13 apply(TClientOptions*)).
/// Pages hold pending values; nothing is written before apply.
class TOptionsPage : public ::tibia::CipTyped
{
public:
    virtual ~TOptionsPage() = default;
    /// Slot 12: the pending values from `settings` (clientoptions.json's root).
    virtual void load(const QJsonObject& settings) = 0;
    /// Slot 13: the pending values into `settings` (root-shaped changes).
    virtual void apply(QJsonObject& settings) const = 0;
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

    void load(const QJsonObject& settings) override;
    void apply(QJsonObject& settings) const override;

private:
    std::span<const TOptionBinding> m_bindings;
};

void loadBindings(QObject* page, std::span<const TOptionBinding> bindings, const QJsonObject& settings);
void applyBindings(const QObject* page, std::span<const TOptionBinding> bindings, QJsonObject& settings);

template <class Base>
void TBoundOptionsPage<Base>::load(const QJsonObject& settings)
{
    loadBindings(this, m_bindings, settings);
}

template <class Base>
void TBoundOptionsPage<Base>::apply(QJsonObject& settings) const
{
    applyBindings(this, m_bindings, settings);
}

/// CIP. The clear-bar buttons belong to the action
/// bar storage (hotkey page,).
using TActionBarsOptionsPage = TBoundOptionsPage<TActionBarsQMLOptionsPage>;
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

/// CIP; the loot style list is .
class TInterfaceOptionsPage final : public TBoundOptionsPage<TInterfaceQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit TInterfaceOptionsPage(QObject* parent = nullptr);
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

    void load(const QJsonObject& settings) override;
    void apply(QJsonObject& settings) const override;

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

/// CIP.
class TGraphicsOptionsPage final : public TBoundOptionsPage<TGraphicsQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit TGraphicsOptionsPage(QObject* parent = nullptr);

    void load(const QJsonObject& settings) override;
    void apply(QJsonObject& settings) const override;

    /// The pending renderer (option 0x5c), not its list index.
    int rendererId() const { return m_rendererId; }

    /// CIP : the list index of the pending renderer.
    uint rendererIndex() const override;
    /// The renderer of that list entry.
    void setRendererIndex(uint index) override;
    /// CIP / : off and fixed while the software
    /// renderer limits the options .
    bool vSync() const override;
    void setVSync(bool enabled) override;

private:
    QList<int> m_rendererIds;
    int m_rendererId = 0;
};

/// CIP.
class TEffectsOptionsPage final : public TBoundOptionsPage<TEffectsQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit TEffectsOptionsPage(QObject* parent = nullptr);

    void load(const QJsonObject& settings) override;
    void apply(QJsonObject& settings) const override;

    /// Off while the software renderer limits the options.
    bool lightEffects() const override;
    /// Ignored while the software renderer limits the options.
    void setLightEffects(bool enabled) override;
};

/// CIP; lists.
class TControlsOptionsPage final : public TBoundOptionsPage<TControlsQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit TControlsOptionsPage(QObject* parent = nullptr);
};

/// CIP / : controlButtonsOptions, shown by name
/// and stored by id.
class TButtonBarOptionsPage final : public TBoundOptionsPage<TButtonBarQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit TButtonBarOptionsPage(QObject* parent = nullptr);

    void load(const QJsonObject& settings) override;
    void apply(QJsonObject& settings) const override;

    QStringList enabledButtons() const override;
    QStringList disabledButtons() const override;
    /// CIP / /.
    Q_INVOKABLE void enableButton(const QString& ButtonName) override;
    Q_INVOKABLE void disableButton(const QString& ButtonName) override;
    Q_INVOKABLE void moveEnabledButtonUp(const QString& ButtonName) override;
    Q_INVOKABLE void moveEnabledButtonDown(const QString& ButtonName) override;

private:
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

    void load(const QJsonObject& settings) override;
    void apply(QJsonObject& settings) const override;
    /// The sound pages are enabled while the sound engine runs and the master
    /// volume is above 0.
    bool soundEnabled() const override;

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
    void setHotkeySetNamesIndex(int v) override;
    void setNameFilter(const QString& v) override;

    void load(const QJsonObject& settings) override;
    void apply(QJsonObject& settings) const override;
    /// The current set's keys of the current chat mode back to the built-in
    /// ones of its vocation (GlobalShortcuts for other names).
    void resetToDefault();

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
    /// hotkeyModel: the rows of the hotkey mode that match the name filter.
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
