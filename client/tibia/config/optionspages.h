#pragma once

#include "shared/cip_typed.h"

#include "config/configciptypes.h"

#include "config/specialconditionsoptions.h"

#include <cip_gen.h>
#include <cip_options.h>

#include <QJsonObject>

#include <span>

namespace tibia::config {

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
        /// A 0..1 option as 0..100 (CIP: min(round(v * 100), 100), back min(p * 0.01, 1)).
        Percent,
        /// lightAttenuationCloudsIndoor as a slider step: (1 - v) / 0.005.
        CloudsIndex,
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

/// CIP 0x100a1dc30 / 0x100a1de60. The clear-bar buttons belong to the action
/// bar storage (hotkey page, FUN_100d36db0).
using TActionBarsOptionsPage = TBoundOptionsPage<TActionBarsQMLOptionsPage>;
/// CIP 0x100a1e1f0 / 0x100a1e2d0.
using TConsoleOptionsPage = TBoundOptionsPage<TConsoleQMLOptionsPage>;
/// CIP 0x100a1fce0 / 0x100a1fdc0.
using TMiscOptionsPage = TBoundOptionsPage<TMiscQMLOptionsPage>;
/// CIP 0x100a1fed0 / 0x100a1ff40.
using TGameplayOptionsPage = TBoundOptionsPage<TGameplayQMLOptionsPage>;
/// CIP 0x100a22340 / 0x100a22530.
using TScreenshotsOptionsPage = TBoundOptionsPage<TScreenshotsQMLOptionsPage>;
/// CIP 0x100a23df0 / 0x100a23f90.
using TBattleSoundsOptionsPage = TBoundOptionsPage<TBattleSoundsQMLOptionsPage>;
/// CIP 0x100a24140 / 0x100a242b0.
using TUISoundsOptionsPage = TBoundOptionsPage<TUISoundsQMLOptionsPage>;

/// The bindings of the pages above.
std::span<const TOptionBinding> actionBarsBindings();
std::span<const TOptionBinding> consoleBindings();
std::span<const TOptionBinding> miscBindings();
std::span<const TOptionBinding> gameplayBindings();
std::span<const TOptionBinding> screenshotsBindings();
std::span<const TOptionBinding> battleSoundsBindings();
std::span<const TOptionBinding> uiSoundsBindings();

/// CIP 0x100a1e3c0 / 0x100a1e4c0; the loot style list is FUN_100a1e750.
class TInterfaceOptionsPage final : public TBoundOptionsPage<TInterfaceQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit TInterfaceOptionsPage(QObject* parent = nullptr);
};

/// CIP 0x100a1eb90 / 0x100a1ed50.
class TGameWindowOptionsPage final : public TBoundOptionsPage<TGameWindowQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit TGameWindowOptionsPage(QObject* parent = nullptr);

    /// CIP 0x100a1ef30.
    Q_INVOKABLE void setShowTargetFrameHighlight(bool ShowFrame, bool ShowHighlight) override;
};

/// CIP 0x100a1f2f0 / 0x100a1f840: the bound options plus the whole
/// specialConditionsOptions section.
class THudOptionsPage final : public TBoundOptionsPage<THudQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit THudOptionsPage(QObject* parent = nullptr);

    void load(const QJsonObject& settings) override;
    void apply(QJsonObject& settings) const override;

    /// CIP 0x100a1fb70 / 0x100a1fbe0.
    void setSpecialConditionShowInHUDEnabled(int SpecialConditionIndex, bool Enabled) override;
    void setSpecialConditionShowInBarEnabled(int SpecialConditionIndex, bool Enabled) override;
    /// CIP 0x100a1fc50 (FUN_1009d6460) / 0x100a1fc70 (FUN_1009d6550).
    void moveSpecialConditionUp(int SpecialConditionIndex) override;
    void moveSpecialConditionDown(int SpecialConditionIndex) override;

private:
    /// CIP FUN_100a1f560.
    void rebuildSpecialConditionsModel();

    SpecialConditionsOptions m_specialConditions;
};

/// CIP 0x100a20010 / 0x100a200c0.
class TGraphicsOptionsPage final : public TBoundOptionsPage<TGraphicsQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit TGraphicsOptionsPage(QObject* parent = nullptr);

    void load(const QJsonObject& settings) override;
    void apply(QJsonObject& settings) const override;

    /// The pending renderer (option 0x5c), not its list index.
    int rendererId() const { return m_rendererId; }

    /// CIP 0x100a202c0: the list index of the pending renderer.
    uint rendererIndex() const override;
    /// The renderer of that list entry.
    void setRendererIndex(uint index) override;
    /// CIP 0x100a20210 / 0x100a20240: off and fixed while the software
    /// renderer limits the options (FUN_100f76650).
    bool vSync() const override;
    void setVSync(bool enabled) override;

private:
    QList<int> m_rendererIds;
    int m_rendererId = 0;
};

/// CIP 0x100a21830 / 0x100a21930.
class TEffectsOptionsPage final : public TBoundOptionsPage<TEffectsQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit TEffectsOptionsPage(QObject* parent = nullptr);

    void load(const QJsonObject& settings) override;
    void apply(QJsonObject& settings) const override;

    /// Ignored while the software renderer limits the options (FUN_100f76650).
    void setLightEffects(bool enabled) override;
};

/// CIP 0x100a22060 / 0x100a22150; lists FUN_100e54220 / FUN_100e548a0.
class TControlsOptionsPage final : public TBoundOptionsPage<TControlsQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit TControlsOptionsPage(QObject* parent = nullptr);
};

/// CIP 0x100a227d0 / 0x100a22a30: controlButtonsOptions, shown by name
/// (FUN_100a22cf0) and stored by id.
class TButtonBarOptionsPage final : public TBoundOptionsPage<TButtonBarQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit TButtonBarOptionsPage(QObject* parent = nullptr);

    void load(const QJsonObject& settings) override;
    void apply(QJsonObject& settings) const override;

    QStringList enabledButtons() const override;
    QStringList disabledButtons() const override;
    /// CIP 0x100a22d70 / 0x100a22e70 / 0x100a23500 / 0x100a235f0.
    Q_INVOKABLE void enableButton(const QString& ButtonName) override;
    Q_INVOKABLE void disableButton(const QString& ButtonName) override;
    Q_INVOKABLE void moveEnabledButtonUp(const QString& ButtonName) override;
    Q_INVOKABLE void moveEnabledButtonDown(const QString& ButtonName) override;

private:
    QStringList m_enabled;
    QStringList m_disabled;
};

/// CIP 0x100a23800 / 0x100a239e0. soundEngineInitialized is true when the
/// miniaudio engine is ready.
class TSoundOptionsPage final : public TBoundOptionsPage<TSoundQMLOptionsPage>
{
    Q_OBJECT
public:
    explicit TSoundOptionsPage(QObject* parent = nullptr);

    void load(const QJsonObject& settings) override;
    void apply(QJsonObject& settings) const override;
    /// CIP: battle/UI sound pages disable when master is 0 (`soundEnabled`).
    void setMasterVolume(uint v) override;

private:
    void refreshSoundDevices();
};

/// CIP 0x100a281f0 load / 0x100a28470 apply / FUN_100a28340 rebuild. Lists
/// static + product actions (DefaultHotkeys), TriggerActionButton_* (ActionButtons),
/// and custom bindings (CustomHotkeys).
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

    Q_INVOKABLE void requestAddPreset() override;
    Q_INVOKABLE void requestRenameCurrentPreset() override;
    Q_INVOKABLE void requestCloneCurrentPreset() override;
    Q_INVOKABLE void requestRemoveCurrentPreset() override;
    Q_INVOKABLE void requestEditActionForHotkey(int HotkeyActionSettingIndex) override;
    Q_INVOKABLE void requestEnterNewHotkeyForAction(int HotkeyActionSettingIndex, int KeySequenceIndex) override;
    Q_INVOKABLE void requestAddNewCustomHotkey() override;

private:
    void rebuildModel();
    void writeWorkingBindings();
    QString currentSetName() const;
    QJsonObject m_working;
    int m_editRow = -1;
    int m_editKey = 0;
};

}
