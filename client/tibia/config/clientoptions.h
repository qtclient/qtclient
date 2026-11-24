#pragma once

#include <cip_gen.h>
#include <cip_options.h>

#include <QByteArray>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <array>

#include <map>
#include <set>
#include <unordered_map>
#include <utility>

namespace tibia {
namespace config {

using EClientOption = cip::EClientOption;

struct THotkeySet;

/// The control schemes' localized names in list-model order (classic,
/// legacy, left smart click).
QStringList controlSchemeListModel();
/// The loot schemes' localized names in list-model order (right, shift
/// right, left).
QStringList lootSchemeListModel();
/// controlSchemeIndex → control scheme (1 classic, 2 legacy, 3 left smart
/// click), matched by the localized name; throws std::logic_error for an
/// index without a scheme.
int controlSchemeForListModelIndex(int index);
/// The list-model index of a control scheme; throws std::logic_error for an
/// unknown scheme.
int controlSchemeListModelIndex(int scheme);
/// lootSchemeIndex → loot scheme (0 right, 1 shift right, 2 left).
int lootSchemeForListModelIndex(int index);
int lootSchemeListModelIndex(int scheme);

/// clientWindowOptions: the client window's screen and geometry.
struct TClientWindowOptions
{
    QString screenName;
    QRect geometry;
    bool maximized = false;
    bool borderless = false;

    bool operator==(const TClientWindowOptions&) const = default;
};

/// chatOptions: the open channels, the chat mode and the loot channel.
struct TChatOptions
{
    QList<quint16> openChannels{ 7 };
    bool chatModeOn = false;
    bool lootChannelOpen = false;

    bool operator==(const TChatOptions&) const = default;
};

/// whiteBlacklistOptions.
struct TWhiteBlacklistOptions
{
    QStringList blacklistedNames;
    QStringList whitelistedNames;

    bool operator==(const TWhiteBlacklistOptions&) const = default;
};

/// survey: the last survey invitation and its status (0..2).
struct TSurveyInvitationOptions
{
    quint32 id = 0;
    int status = 2;

    bool operator==(const TSurveyInvitationOptions&) const = default;
};

/// The options of the client (CIP's clientoptions.json): five typed option
/// maps keyed by EClientOption, one value per section, a load and a save.
/// A setter changes nothing when the value is unchanged; otherwise it sets it
/// and emits the section's signal (optionChanged for an option).
class TClientOptions final : public cip::tibia::config::TClientOptions
{
    Q_OBJECT
public:
    /// Options without values (a working copy takes them with assignFrom,
    /// defaults with setDefaults). The client's options are instance().
    TClientOptions();

    static TClientOptions* instance();

    /// optionChanged(id) for every bool, int, float, string and date option.
    void announceAllOptions();

    bool boolOption(EClientOption id) const;
    void setBoolOption(EClientOption id, bool value);
    int intOption(EClientOption id) const;
    void setIntOption(EClientOption id, int value);
    float floatOption(EClientOption id) const;
    void setFloatOption(EClientOption id, float value);
    QString stringOption(EClientOption id) const;
    void setStringOption(EClientOption id, const QString& value);
    QDateTime dateOption(EClientOption id) const;
    void setDateOption(EClientOption id, const QDateTime& value);

    const TClientWindowOptions& clientWindowOptions() const { return m_clientWindow; }
    void setClientWindowOptions(const TClientWindowOptions& options);
    const QRect& npcDialogOptions() const { return m_npcDialog; }
    void setNpcDialogOptions(const QRect& geometry);
    /// controlButtonsOptions as stored (enabledButtons, disabledButtons).
    const QJsonObject& controlButtonsOptions() const { return m_controlButtons; }
    void setControlButtonsOptions(const QJsonObject& section);
    /// deviceCookieOptions: (hash, cookie) pairs, oldest first.
    const QList<std::pair<QString, QString>>& deviceCookies() const { return m_deviceCookies; }
    /// Replaces the cookies (no signal).
    void setDeviceCookies(const QList<std::pair<QString, QString>>& cookies);
    /// The cookie stored for `key` (the account), empty when none.
    QString deviceCookie(const QString& key) const;
    /// Stores `cookie` for `key`, replacing its old one (the newest last).
    void setDeviceCookie(const QString& key, const QString& cookie);
    void clearDeviceCookies();
    /// A device cookie's key: the first 8 hex digits of
    /// SHA-256(SHA-256(k)[0..4] + k), k the lower-cased key in UTF-8.
    static QString deviceCookieHash(const QString& key);
    const QJsonObject& hotkeyOptions() const { return m_hotkeyOptions; }
    void setHotkeyOptions(const QJsonObject& section);
    /// A copy of the current hotkey set; an empty set when the current name
    /// names none.
    THotkeySet currentHotkeySet() const;
    /// Replaces the current hotkey set when `set` differs from it.
    void setCurrentHotkeySet(const THotkeySet& set);
    const TChatOptions& chatOptions() const { return m_chat; }
    void setChatOptions(const TChatOptions& options);
    const TWhiteBlacklistOptions& whiteBlacklistOptions() const { return m_whiteBlacklist; }
    void setWhiteBlacklistOptions(const TWhiteBlacklistOptions& options);
    const TSurveyInvitationOptions& surveyInvitationOptions() const { return m_survey; }
    void setSurveyInvitationOptions(const TSurveyInvitationOptions& options);
    quint16 newsMaxUnlockLevel() const { return m_newsMaxUnlockLevel; }
    void setNewsMaxUnlockLevel(quint16 level);
    const QJsonObject& specialConditionsOptions() const { return m_specialConditions; }
    void setSpecialConditionsOptions(const QJsonObject& section);
    const std::set<QByteArray>& pinnedCharacters() const { return m_pinnedCharacters; }
    void setPinnedCharacters(const std::set<QByteArray>& characters);

    bool rendererChangeConfirmationRequired() const { return m_rendererChangeConfirmationRequired; }
    void setRendererChangeConfirmationRequired(bool required);

    void setRendererIndex(int index);
    /// controlSchemeIndex as a scheme (see controlSchemeForListModelIndex).
    int controlScheme() const;
    void setControlScheme(int scheme);
    void setLeftSmartClickControlScheme();
    /// lootSchemeIndex as a scheme (see lootSchemeForListModelIndex).
    int lootScheme() const;
    void setLootScheme(int scheme);
    int antialiasingMode() const;
    void setAntialiasingMode(int mode);
    int colorizeLootStyle() const;
    void setColorizeLootStyle(int style);

    /// The option values (bool, int, float, string) and the renderer
    /// confirmation flag of `other`; the sections stay.
    void assignOptionValues(const TClientOptions& other);
    /// Whether the bool, int, float and string options equal `other`'s.
    bool optionValuesEqual(const TClientOptions& other) const;
    /// Every option, section and host value of `other`, without signals and
    /// without the options mode (the options dialog's working copy).
    void assignFrom(const TClientOptions& other);
    /// Whether an option of any type has a value (the tutorial's options
    /// lack the sound options).
    bool hasOption(EClientOption id) const;
    /// `other`'s value of one option, through its typed setter; nothing when
    /// `other` has none.
    void assignOption(EClientOption id, const TClientOptions& other);

    /// The options as compact JSON, zlib-compressed (level 6), in base64.
    QString exportToBase64String() const;

    /// 0: the user's options; 1: the tutorial's, which are not saved.
    int clientOptionsMode() const { return m_clientOptionsMode; }
    void setClientOptionsMode(int mode) { m_clientOptionsMode = mode; }
    bool isTemporaryClientOptionsMode() const { return m_clientOptionsMode != 0; }

    /// The options a fresh options file holds. Mode 1 (the tutorial's
    /// options) leaves the sound options out.
    void setDefaults(int mode = 0);
    /// Reads every section `json` holds with the right JSON type over the
    /// current values; the others keep theirs.
    void fromJson(const QJsonObject& json);
    /// Every section and option, with the current options version.
    QJsonDocument toJson() const;
    /// Reads the user's options file: missing or empty, the defaults; else
    /// migrated to the current layout and read over the defaults. Then
    /// optionChanged for every option; then ensureGlobalShortcuts.
    void load();
    /// Every hotkey set gains the GlobalShortcuts it lacks; with no set, the
    /// vocation presets (THotkeyOptionsData::addGlobalShortcuts).
    void ensureGlobalShortcuts();
    /// Reads the tutorial's options (:/tutorial/clientoptions.json) like
    /// load(), with the mode-1 defaults and without ensureGlobalShortcuts.
    void loadDefaultClientOptions();
    /// A notice the game window shows on start (never set by the options
    /// themselves; false and empty).
    bool hasStartupNotice() const { return m_hasStartupNotice; }
    QString startupNotice() const { return m_startupNotice; }

    /// Host: options and top-level sections no CIP option names, kept as
    /// stored (opaque JSON, no change signal) so they survive load and save.
    QJsonObject extraOptions() const { return m_extraOptions; }
    void setExtraOption(const QString& name, const QJsonValue& value);
    QJsonValue extraSection(const QString& name) const { return m_extraSections.value(name); }
    void setExtraSection(const QString& name, const QJsonValue& value);

    static QString optionName(EClientOption id);
    /// The option named `name`, or false when no CIP option has that name.
    static bool optionId(const QString& name, EClientOption& id);

    static QString userFilePath();

    /// actionBarsOptionsChanged .. stashOptionsChanged.
    static constexpr size_t CategoryCount = 22;

public Q_SLOTS:
    void saveToJsonFile() override;
    /// Starts the timer of every category signal `id` belongs to.
    void consolidateSignalsAndEnqueueOptionSetSignalsForEmit(tibia::config::EClientOption id);

Q_SIGNALS:
    void optionChanged(tibia::config::EClientOption id);

private:
    void readBoolOptions(const QJsonObject& options);
    void readIntOptions(const QJsonObject& options);
    void readFloatOptions(const QJsonObject& options);
    void readStringOptions(const QJsonObject& options);
    void readDateOptions(const QJsonObject& options);
    void writeBoolOptions(QJsonObject& options) const;
    void writeIntOptions(QJsonObject& options) const;
    void writeFloatOptions(QJsonObject& options) const;
    void writeStringOptions(QJsonObject& options) const;
    /// Seconds since the epoch.
    void writeDateOptions(QJsonObject& options) const;

    std::unordered_map<EClientOption, bool> m_bools;
    std::unordered_map<EClientOption, int> m_ints;
    std::unordered_map<EClientOption, float> m_floats;
    std::unordered_map<EClientOption, QString> m_strings;
    std::unordered_map<EClientOption, QDateTime> m_dates;
    bool m_rendererChangeConfirmationRequired = false;
    TClientWindowOptions m_clientWindow;
    /// Unset until the NPC dialog is first placed: QRect(-1, -1, -1, -1).
    QRect m_npcDialog{ -1, -1, -1, -1 };
    QJsonObject m_controlButtons;
    QList<std::pair<QString, QString>> m_deviceCookies;
    QJsonObject m_hotkeyOptions;
    TChatOptions m_chat;
    TWhiteBlacklistOptions m_whiteBlacklist;
    TSurveyInvitationOptions m_survey;
    quint16 m_newsMaxUnlockLevel = 0;
    QJsonObject m_specialConditions;
    std::set<QByteArray> m_pinnedCharacters;

    QJsonObject m_extraOptions;
    QJsonObject m_extraSections;
    int m_clientOptionsMode = 0;
    bool m_hasStartupNotice = false;
    QString m_startupNotice;
    std::array<QTimer, CategoryCount> m_categoryTimers;
};

}
}
