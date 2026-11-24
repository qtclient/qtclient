#include "clientoptions.h"

#include "config/clientoptiondefaults.h"
#include "config/clientoptionsmigration.h"
#include "config/hotkeyoptions.h"
#include "config/specialconditionsoptions.h"
#include "shared/tibiafilesystemhelper.h"

#include "jsonhelper.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QSaveFile>
#include <QStandardPaths>

#include <array>
#include <vector>

#include <algorithm>
#include <stdexcept>
#include <limits>

namespace tibia::config {

namespace {

constexpr auto VersionKey = "clientOptionsVersion";
/// Device cookies kept at most; the oldest go first.
constexpr qsizetype MaxDeviceCookies = 20;

const QHash<QString, EClientOption>& optionsByName()
{
    static const QHash<QString, EClientOption> byName = [] {
        QHash<QString, EClientOption> map;
        for (size_t i = 0; i < cip::ClientOptionNames.size(); ++i)
            map.insert(QString::fromLatin1(cip::ClientOptionNames[i].data(), qsizetype(cip::ClientOptionNames[i].size())), EClientOption(i));
        return map;
    }();
    return byName;
}

/// The int read: a number takes the stored value, then [low, high].
int readInt(const QJsonObject& json, const QString& key, const int low, const int high, const int fallback)
{
    const QJsonValue value = json.value(key);
    return std::clamp(value.isDouble() ? value.toInt() : fallback, low, high);
}

double readDouble(const QJsonObject& json, const QString& key, const double low, const double high, const double fallback)
{
    const QJsonValue value = json.value(key);
    return std::clamp(value.isDouble() ? value.toDouble() : fallback, low, high);
}

bool readBool(const QJsonObject& json, const QString& key, const bool fallback)
{
    const QJsonValue value = json.value(key);
    return value.isBool() ? value.toBool() : fallback;
}

QString readString(const QJsonObject& json, const QString& key)
{
    const QJsonValue value = json.value(key);
    return value.isString() ? value.toString() : QString();
}

/// clientWindowOptions over the current values: positions any int, sizes
/// not negative; the flags keep their value when not stored as bools.
TClientWindowOptions clientWindowOptionsFromJson(const QJsonObject& json, TClientWindowOptions options)
{
    options.screenName = readString(json, QStringLiteral("clientWindowScreenName"));
    const int x = readInt(json, QStringLiteral("clientWindowPositionX"), std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), 0);
    const int y = readInt(json, QStringLiteral("clientWindowPositionY"), std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), 0);
    const int width = readInt(json, QStringLiteral("clientWindowWidth"), 0, std::numeric_limits<int>::max(), 0);
    const int height = readInt(json, QStringLiteral("clientWindowHeight"), 0, std::numeric_limits<int>::max(), 0);
    options.geometry = QRect(x, y, width, height);
    options.maximized = readBool(json, QStringLiteral("clientWindowIsMaximized"), options.maximized);
    options.borderless = readBool(json, QStringLiteral("clientWindowIsBorderless"), options.borderless);
    return options;
}

QJsonObject clientWindowOptionsToJson(const TClientWindowOptions& options)
{
    return {
        { QStringLiteral("clientWindowScreenName"), options.screenName },
        { QStringLiteral("clientWindowPositionX"), options.geometry.left() },
        { QStringLiteral("clientWindowPositionY"), options.geometry.top() },
        { QStringLiteral("clientWindowWidth"), options.geometry.width() },
        { QStringLiteral("clientWindowHeight"), options.geometry.height() },
        { QStringLiteral("clientWindowIsMaximized"), options.maximized },
        { QStringLiteral("clientWindowIsBorderless"), options.borderless },
    };
}

QRect npcDialogOptionsFromJson(const QJsonObject& json)
{
    constexpr int max = std::numeric_limits<int>::max();
    return { readInt(json, QStringLiteral("x"), 0, max, 0), readInt(json, QStringLiteral("y"), 0, max, 0),
             readInt(json, QStringLiteral("width"), 0, max, 0), readInt(json, QStringLiteral("height"), 0, max, 0) };
}

QJsonObject npcDialogOptionsToJson(const QRect& geometry)
{
    return {
        { QStringLiteral("x"), geometry.x() },
        { QStringLiteral("y"), geometry.y() },
        { QStringLiteral("width"), geometry.width() },
        { QStringLiteral("height"), geometry.height() },
    };
}

QJsonObject controlButtonsOptionsToJson(const QJsonObject& section)
{
    return {
        { QStringLiteral("enabledButtons"), section.value(QLatin1String("enabledButtons")).toArray() },
        { QStringLiteral("disabledButtons"), section.value(QLatin1String("disabledButtons")).toArray() },
    };
}

/// A cookie for a hash replaces the hash's old one; past MaxDeviceCookies the
/// oldest go; the new one is last.
void addDeviceCookie(QList<std::pair<QString, QString>>& cookies, const QString& hash, const QString& cookie)
{
    cookies.removeIf([&hash](const std::pair<QString, QString>& entry) { return entry.first == hash; });
    while (cookies.size() > MaxDeviceCookies)
        cookies.removeFirst();
    cookies.append({ hash, cookie });
}

QList<std::pair<QString, QString>> deviceCookiesFromJson(const QJsonArray& array)
{
    QList<std::pair<QString, QString>> cookies;
    for (const QJsonValue& entry : array)
        addDeviceCookie(cookies, entry[QLatin1String("hash")].toString(), entry[QLatin1String("devicecookie")].toString());
    return cookies;
}

QJsonArray deviceCookiesToJson(const QList<std::pair<QString, QString>>& cookies)
{
    QJsonArray array;
    for (const auto& [hash, cookie] : cookies)
        array.append(QJsonObject{ { QStringLiteral("hash"), hash }, { QStringLiteral("devicecookie"), cookie } });
    return array;
}

/// chatOptions: openChannels as read (empty when missing); the flags keep the
/// default when not stored.
TChatOptions chatOptionsFromJson(const QJsonObject& json)
{
    TChatOptions options;
    options.openChannels.clear();
    for (const QJsonValue& channel : json.value(QLatin1String("openChannels")).toArray())
        options.openChannels.append(quint16(channel.toInt()));
    options.chatModeOn = json.value(QLatin1String("chatModeOn")).toBool(options.chatModeOn);
    options.lootChannelOpen = json.value(QLatin1String("lootChannelOpen")).toBool(options.lootChannelOpen);
    return options;
}

QJsonObject chatOptionsToJson(const TChatOptions& options)
{
    QJsonArray channels;
    for (const quint16 channel : options.openChannels)
        channels.append(int(channel));
    return {
        { QStringLiteral("openChannels"), channels },
        { QStringLiteral("chatModeOn"), options.chatModeOn },
        { QStringLiteral("lootChannelOpen"), options.lootChannelOpen },
    };
}

TWhiteBlacklistOptions whiteBlacklistOptionsFromJson(const QJsonObject& json)
{
    TWhiteBlacklistOptions options;
    for (const QJsonValue& name : json.value(QLatin1String("blacklistedNames")).toArray())
        options.blacklistedNames.append(name.toString());
    for (const QJsonValue& name : json.value(QLatin1String("whitelistedNames")).toArray())
        options.whitelistedNames.append(name.toString());
    return options;
}

QJsonObject whiteBlacklistOptionsToJson(const TWhiteBlacklistOptions& options)
{
    return {
        { QStringLiteral("blacklistedNames"), QJsonArray::fromStringList(options.blacklistedNames) },
        { QStringLiteral("whitelistedNames"), QJsonArray::fromStringList(options.whitelistedNames) },
    };
}

TSurveyInvitationOptions surveyInvitationOptionsFromJson(const QJsonObject& json)
{
    TSurveyInvitationOptions options;
    options.status = 0;
    options.id = quint32(readDouble(json, QStringLiteral("id"), 0.0, double(std::numeric_limits<quint32>::max()), 0.0));
    options.status = readInt(json, QStringLiteral("status"), 0, 2, 0);
    return options;
}

QJsonObject surveyInvitationOptionsToJson(const TSurveyInvitationOptions& options)
{
    return { { QStringLiteral("id"), qint64(options.id) }, { QStringLiteral("status"), options.status } };
}

quint16 newsOptionsFromJson(const QJsonObject& json)
{
    return quint16(int(readDouble(json, QStringLiteral("maxunlocklevel"), 0.0, 65535.0, 0.0)));
}

QJsonObject newsOptionsToJson(const quint16 maxUnlockLevel)
{
    return { { QStringLiteral("maxunlocklevel"), int(maxUnlockLevel) } };
}

std::set<QByteArray> pinnedCharactersFromJson(const QJsonArray& array)
{
    std::set<QByteArray> characters;
    for (const QJsonValue& entry : array)
        characters.insert(QByteArray::fromHex(entry.toString().toLocal8Bit()));
    return characters;
}

QJsonArray pinnedCharactersToJson(const std::set<QByteArray>& characters)
{
    QJsonArray array;
    for (const QByteArray& character : characters)
        array.append(QString::fromUtf8(character.toHex()));
    return array;
}

/// The category signals and the options that raise them; an option can raise
/// several. Each is emitted once per event-loop pass however many of its
/// options changed.
struct TOptionCategory
{
    void (TClientOptions::*signal)();
    std::vector<EClientOption> options;
};

const std::array<TOptionCategory, TClientOptions::CategoryCount>& optionCategories()
{
    using O = EClientOption;
    static const std::array<TOptionCategory, TClientOptions::CategoryCount> categories = { {
        { &TClientOptions::actionBarsOptionsChanged, { O::actionBarShowBottom1, O::actionBarShowBottom2, O::actionBarShowBottom3, O::actionBarsShowBottom, O::actionBarShowLeft1, O::actionBarShowLeft2, O::actionBarShowLeft3, O::actionBarsShowLeft, O::actionBarShowRight1, O::actionBarShowRight2, O::actionBarShowRight3, O::actionBarsShowRight, O::actionBarBottomLocked, O::actionBarLeftLocked, O::actionBarRightLocked, O::actionButtonShowHotkey, O::actionButtonShowAmount, O::actionButtonShowSpellParameters, O::actionButtonShowGraphicalCooldown, O::actionButtonShowCooldownNumbers, O::actionButtonAllowTooltip, O::actionButtonAutoInsertSpells } },
        { &TClientOptions::chatProtocolMessageHandlerOptionsChanged, { O::openNewPrivateMessagesInNewTab, O::consoleShowJoinLeaveMessages, O::consoleShowEventMessages, O::consoleShowInfoMessages, O::consoleShowStatusMessagesOfOthers, O::consoleShowStatusMessagesOwn, O::gameWindowShowBoostedCreatureMessages, O::gameWindowShowHotkeyUsageMessages, O::gameWindowShowLootMessages, O::gameWindowShowMessages, O::gameWindowShowOfflineTrainingMessages, O::gameWindowShowOthersSpells, O::gameWindowShowOwnSpells, O::gameWindowShowPotionMessages, O::gameWindowShowPrivateMessages, O::gameWindowShowStoreMessages, O::gameWindowShowTextualEffects, O::colorizeLootStyle } },
        { &TClientOptions::chatChannelControllerOptionsChanged, { O::consoleShowLevels, O::consoleShowTimestamps, O::consoleShowTimestampsSeconds } },
        { &TClientOptions::mapWindowControllerOptionsChanged, { O::gameWindowShowTextualEffects, O::antialiasingMode, O::gameWindowScaleOnlyByEvenMultiples, O::lightEffectsEnabled } },
        { &TClientOptions::creatureHUDRenderProviderOptionsChanged, { O::playerHudEnabled, O::playerHudShowBars, O::playerHudShowArcs, O::playerHudShowHarmonyLeft, O::playerHudArcOpacity, O::playerHudArcDistance, O::playerHudArcHeight, O::playerHudArcWidth, O::playerShowName, O::playerShowHealth, O::playerShowMana, O::playerShowHarmonySerene, O::playerShowMarks, O::creatureHudEnabled, O::creatureShowName, O::creatureShowHealth, O::creatureShowMarks, O::creatueShowNpcIcon } },
        { &TClientOptions::creatureMarksOptionsChanged, { O::combatShowPvpFrames, O::combatShowFrames, O::gameWindowShowTragetFrame } },
        { &TClientOptions::controlOrLootSchemeOptionsChanged, { O::controlSchemeIndex, O::lootSchemeIndex } },
        { &TClientOptions::rotateKeyOptionsChanged, { O::rotateWithAlt, O::rotateWithCtrl, O::rotateWithShift } },
        { &TClientOptions::screenshotMangerOptionsChanged, { O::screnshotsOnlyGameWindow, O::screenshotsUseBacklog } },
        { &TClientOptions::statusBarEnabledChanged, { O::statusBarEnabled } },
        { &TClientOptions::statusPanelEnabledChanged, { O::statusPanelEnabled } },
        { &TClientOptions::gameWindowControllerOptionsChanged, { O::antialiasingMode, O::cooldownBarEnabled, O::gameWindowUpperPaneHeight, O::gameWindowLowerPaneHeight, O::gameWindowResizingMode } },
        { &TClientOptions::itemTrackingStorageOptionsChanged, { O::colorizeLootStyle } },
        { &TClientOptions::containerStorageControllerOptionsChanged, { O::colorizeLootStyle, O::showExpireInInventory, O::showExpireWhenUnused } },
        { &TClientOptions::characterSelectionControllerOptionsChanged, { O::characterSelectionShowHidden, O::characterSelectionShowOutfits, O::characterSelectionSortAscendingOrder, O::characterSelectionSortColumn } },
        { &TClientOptions::appearanceTypeListModelOptionsChanged, { O::colorizeLootStyle, O::showExpireInContainers, O::showExpireWhenUnused } },
        { &TClientOptions::marketOtpionsChanged, { O::marketShowLockerOnly } },
        { &TClientOptions::mouseCursorControllerOptionsChanged, { O::mouseAnimatedCursor, O::mouseBigCursor, O::mouseSystemCursor } },
        { &TClientOptions::soundOptionsChanged, { O::soundAmbienceVolume, O::soundBattleCreaturesAttacksEnabled, O::soundBattleCreaturesDeathEnabled, O::soundBattleCreaturesNoisesEnabled, O::soundBattleCreaturesVolume, O::soundBattleOthersAttackSpellsEnabled, O::soundBattleOthersHealingSpellsEnabled, O::soundBattleOthersSpellsEnabled, O::soundBattleOthersSupportSpellsEnabled, O::soundBattleOthersVolume, O::soundBattleOthersWeaponsEnabled, O::soundBattleOwnAttackSpellsEnabled, O::soundBattleOwnHealingSpellsEnabled, O::soundBattleOwnSpellsEnabled, O::soundBattleOwnSupportSpellsEnabled, O::soundBattleOwnVolume, O::soundBattleOwnWeaponsEnabled, O::ChatEnabled, O::soundEatingEnabled, O::soundEventsVolume, O::GlobalMessagesEnabled, O::GuildMessagesEnabled, O::soundItemsVolume, O::soundAnthemEnabled, O::soundMasterVolume, O::soundMoveItemEnabled, O::soundMusicVolume, O::NpcMessageseEnabled, O::soundPartyEnabled, O::PartyMessagesEnabled, O::PrivateMessagesEnabled, O::PrivateMessagesWithoutTabEnabled, O::RaidMessagesEnabled, O::SystemMessagesEnabled, O::TeamFinderMessagesEnabled, O::soundUiInteractionsEnabled, O::soundUiVolume, O::soundVipEnabled, O::usedSoundDevice } },
        { &TClientOptions::fpsLagIndicatorOptionsChanged, { O::showFpsLagIndicator } },
        { &TClientOptions::containerOptionsChanged, { O::containerSortBackpacksFirst, O::containerSortRecursive, O::containerSortRecursiveShowWarningAgain, O::containerMoveToManagedContainerRecursive, O::containerMoveToManagedContainerRecursiveWarning } },
        { &TClientOptions::stashOptionsChanged, { O::stashSortOrder } },
    } };
    return categories;
}

}

TClientOptions::TClientOptions()
{
    for (size_t i = 0; i < m_categoryTimers.size(); ++i) {
        QTimer& timer = m_categoryTimers[i];
        timer.setInterval(0);
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, this, optionCategories()[i].signal);
    }
    connect(this, &TClientOptions::optionChanged, this, &TClientOptions::consolidateSignalsAndEnqueueOptionSetSignalsForEmit);
}

void TClientOptions::consolidateSignalsAndEnqueueOptionSetSignalsForEmit(const EClientOption id)
{
    for (size_t i = 0; i < m_categoryTimers.size(); ++i) {
        const std::vector<EClientOption>& options = optionCategories()[i].options;
        if (std::find(options.begin(), options.end(), id) != options.end())
            m_categoryTimers[i].start();
    }
}

TClientOptions* TClientOptions::instance()
{
    static TClientOptions* options = [] {
        auto* o = new TClientOptions;
        o->load();
        return o;
    }();
    return options;
}

QString TClientOptions::optionName(const EClientOption id)
{
    const std::string_view name = cip::ClientOptionNames[size_t(id)];
    return QString::fromLatin1(name.data(), qsizetype(name.size()));
}

bool TClientOptions::optionId(const QString& name, EClientOption& id)
{
    const auto it = optionsByName().find(name);
    if (it == optionsByName().end())
        return false;
    id = *it;
    return true;
}

QString TClientOptions::userFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/conf/clientoptions.json");
}

void TClientOptions::announceAllOptions()
{
    for (const auto& [id, value] : m_bools)
        Q_EMIT optionChanged(id);
    for (const auto& [id, value] : m_ints)
        Q_EMIT optionChanged(id);
    for (const auto& [id, value] : m_floats)
        Q_EMIT optionChanged(id);
    for (const auto& [id, value] : m_strings)
        Q_EMIT optionChanged(id);
}

bool TClientOptions::boolOption(const EClientOption id) const
{
    return m_bools.at(id);
}

void TClientOptions::setBoolOption(const EClientOption id, const bool value)
{
    if (m_bools.at(id) == value)
        return;
    m_bools[id] = value;
    Q_EMIT optionChanged(id);
}

int TClientOptions::intOption(const EClientOption id) const
{
    return m_ints.at(id);
}

void TClientOptions::setIntOption(const EClientOption id, const int value)
{
    if (m_ints.at(id) == value)
        return;
    m_ints[id] = value;
    Q_EMIT optionChanged(id);
}

float TClientOptions::floatOption(const EClientOption id) const
{
    return m_floats.at(id);
}

void TClientOptions::setFloatOption(const EClientOption id, const float value)
{
    if (m_floats.at(id) == value)
        return;
    m_floats[id] = value;
    Q_EMIT optionChanged(id);
}

QString TClientOptions::stringOption(const EClientOption id) const
{
    return m_strings.at(id);
}

void TClientOptions::setStringOption(const EClientOption id, const QString& value)
{
    if (m_strings.at(id) == value)
        return;
    m_strings[id] = value;
    Q_EMIT optionChanged(id);
}

QDateTime TClientOptions::dateOption(const EClientOption id) const
{
    return m_dates.at(id);
}

void TClientOptions::setDateOption(const EClientOption id, const QDateTime& value)
{
    if (m_dates.at(id) == value)
        return;
    m_dates[id] = value;
    Q_EMIT optionChanged(id);
}

QStringList controlSchemeListModel()
{
    return { qtTrId("optionsmenu_controlscheme_classic"), qtTrId("optionsmenu_controlscheme_legacy"),
             qtTrId("optionsmenu_controlscheme_leftsmartclick") };
}

QStringList lootSchemeListModel()
{
    return { qtTrId("optionsmenu_lootscheme_right"), qtTrId("optionsmenu_lootscheme_shift_right"),
             qtTrId("optionsmenu_lootscheme_left") };
}

int controlSchemeForListModelIndex(const int index)
{
    const QStringList names = controlSchemeListModel();
    if (index >= 0 && index < names.size()) {
        const QString& name = names.at(index);
        if (name == qtTrId("optionsmenu_controlscheme_classic"))
            return 1;
        if (name == qtTrId("optionsmenu_controlscheme_legacy"))
            return 2;
        if (name == qtTrId("optionsmenu_controlscheme_leftsmartclick"))
            return 3;
    }
    throw std::logic_error("bug: FindControlSchemeForListModelIndex: end of loop");
}

int controlSchemeListModelIndex(const int scheme)
{
    static constexpr const char* Names[] = { "optionsmenu_controlscheme_classic", "optionsmenu_controlscheme_legacy",
                                             "optionsmenu_controlscheme_leftsmartclick" };
    const QString name = qtTrId(scheme >= 1 && scheme <= 3 ? Names[scheme - 1] : "dummy_unknown");
    const qsizetype index = controlSchemeListModel().indexOf(name);
    if (index < 0)
        throw std::logic_error("bug: FindControlSchemeListModelIndex: end of loop");
    return int(index);
}

int lootSchemeForListModelIndex(const int index)
{
    const QStringList names = lootSchemeListModel();
    if (index >= 0 && index < names.size()) {
        const QString& name = names.at(index);
        if (name == qtTrId("optionsmenu_lootscheme_right"))
            return 0;
        if (name == qtTrId("optionsmenu_lootscheme_shift_right"))
            return 1;
        if (name == qtTrId("optionsmenu_lootscheme_left"))
            return 2;
    }
    throw std::logic_error("bug: FindLootSchemeForListModelIndex: end of loop");
}

int lootSchemeListModelIndex(const int scheme)
{
    static constexpr const char* Names[] = { "optionsmenu_lootscheme_right", "optionsmenu_lootscheme_shift_right",
                                             "optionsmenu_lootscheme_left" };
    const QString name = qtTrId(scheme >= 0 && scheme <= 2 ? Names[scheme] : "dummy_unknown");
    const qsizetype index = lootSchemeListModel().indexOf(name);
    if (index < 0)
        throw std::logic_error("bug: FindLootSchemeListModelIndex: end of loop");
    return int(index);
}

int TClientOptions::controlScheme() const
{
    return controlSchemeForListModelIndex(intOption(EClientOption::controlSchemeIndex));
}

void TClientOptions::setControlScheme(const int scheme)
{
    setIntOption(EClientOption::controlSchemeIndex, controlSchemeListModelIndex(scheme));
}

void TClientOptions::setLeftSmartClickControlScheme()
{
    setControlScheme(3);
}

int TClientOptions::lootScheme() const
{
    return lootSchemeForListModelIndex(intOption(EClientOption::lootSchemeIndex));
}

void TClientOptions::setLootScheme(const int scheme)
{
    setIntOption(EClientOption::lootSchemeIndex, lootSchemeListModelIndex(scheme));
}

THotkeySet TClientOptions::currentHotkeySet() const
{
    const THotkeyOptionsData hotkeys = THotkeyOptionsData::fromJson(m_hotkeyOptions);
    const auto it = hotkeys.hotkeySets.find(hotkeys.currentHotkeySetName);
    return it != hotkeys.hotkeySets.end() ? it->second : THotkeySet{};
}

void TClientOptions::setCurrentHotkeySet(const THotkeySet& set)
{
    THotkeyOptionsData hotkeys = THotkeyOptionsData::fromJson(m_hotkeyOptions);
    const QString current = hotkeys.currentHotkeySetName;
    const auto it = hotkeys.hotkeySets.find(current);
    if ((it != hotkeys.hotkeySets.end() ? it->second : THotkeySet{}) == set)
        return;
    hotkeys.hotkeySets[current] = set;
    m_hotkeyOptions = hotkeys.toJson();
    Q_EMIT hotkeyOptionsChanged();
}

void TClientOptions::setClientWindowOptions(const TClientWindowOptions& options)
{
    if (m_clientWindow == options)
        return;
    m_clientWindow = options;
    Q_EMIT clientWindowOptionsChanged();
}

void TClientOptions::setNpcDialogOptions(const QRect& geometry)
{
    if (m_npcDialog == geometry)
        return;
    m_npcDialog = geometry;
    Q_EMIT npcDialogOptionsChanged();
}

void TClientOptions::setControlButtonsOptions(const QJsonObject& section)
{
    const QJsonObject buttons = controlButtonsOptionsToJson(section);
    if (m_controlButtons == buttons)
        return;
    m_controlButtons = buttons;
    Q_EMIT buttonBarOptionsChanged();
}

void TClientOptions::setDeviceCookies(const QList<std::pair<QString, QString>>& cookies)
{
    m_deviceCookies = cookies;
}

QString TClientOptions::deviceCookieHash(const QString& key)
{
    const QByteArray text = key.toLower().toUtf8();
    QByteArray salt = QCryptographicHash::hash(text, QCryptographicHash::Sha256);
    if (salt.size() > 4)
        salt.resize(4);
    const QString hex = QString::fromUtf8(QCryptographicHash::hash(salt + text, QCryptographicHash::Sha256).toHex());
    return hex.size() > 8 ? hex.left(8) : hex;
}

QString TClientOptions::deviceCookie(const QString& key) const
{
    const QString hash = deviceCookieHash(key);
    for (const auto& [entryHash, cookie] : m_deviceCookies)
        if (entryHash == hash)
            return cookie;
    return {};
}

void TClientOptions::setDeviceCookie(const QString& key, const QString& cookie)
{
    addDeviceCookie(m_deviceCookies, deviceCookieHash(key), cookie);
}

void TClientOptions::clearDeviceCookies()
{
    m_deviceCookies.clear();
}

void TClientOptions::setHotkeyOptions(const QJsonObject& section)
{
    if (m_hotkeyOptions == section)
        return;
    m_hotkeyOptions = section;
    Q_EMIT hotkeyOptionsChanged();
}

void TClientOptions::setChatOptions(const TChatOptions& options)
{
    if (m_chat == options)
        return;
    m_chat = options;
    Q_EMIT chatOptionsChanged();
}

void TClientOptions::setWhiteBlacklistOptions(const TWhiteBlacklistOptions& options)
{
    if (m_whiteBlacklist == options)
        return;
    m_whiteBlacklist = options;
    Q_EMIT whiteBlacklistOptionsChanged();
}

void TClientOptions::setSurveyInvitationOptions(const TSurveyInvitationOptions& options)
{
    if (m_survey == options)
        return;
    m_survey = options;
    Q_EMIT surveyInvitationOptionsChanged();
}

void TClientOptions::setNewsMaxUnlockLevel(const quint16 level)
{
    if (m_newsMaxUnlockLevel == level)
        return;
    m_newsMaxUnlockLevel = level;
    Q_EMIT newsOptionsChanged();
}

void TClientOptions::setSpecialConditionsOptions(const QJsonObject& section)
{
    if (m_specialConditions == section)
        return;
    m_specialConditions = section;
    Q_EMIT specialConditionsOptionsChanged();
}

void TClientOptions::setPinnedCharacters(const std::set<QByteArray>& characters)
{
    if (m_pinnedCharacters == characters)
        return;
    m_pinnedCharacters = characters;
    Q_EMIT characterSelectionControllerOptionsChanged();
}

void TClientOptions::setRendererChangeConfirmationRequired(const bool required)
{
    m_rendererChangeConfirmationRequired = required;
}

void TClientOptions::setRendererIndex(const int index)
{
    setIntOption(EClientOption::rendererIndex, index);
}

int TClientOptions::antialiasingMode() const
{
    return intOption(EClientOption::antialiasingMode);
}

void TClientOptions::setAntialiasingMode(const int mode)
{
    setIntOption(EClientOption::antialiasingMode, mode);
}

int TClientOptions::colorizeLootStyle() const
{
    return intOption(EClientOption::colorizeLootStyle);
}

void TClientOptions::setColorizeLootStyle(const int style)
{
    setIntOption(EClientOption::colorizeLootStyle, style);
}

void TClientOptions::assignOptionValues(const TClientOptions& other)
{
    if (this != &other) {
        m_bools = other.m_bools;
        m_ints = other.m_ints;
        m_floats = other.m_floats;
        m_strings = other.m_strings;
    }
    m_rendererChangeConfirmationRequired = other.m_rendererChangeConfirmationRequired;
}

bool TClientOptions::optionValuesEqual(const TClientOptions& other) const
{
    return m_bools == other.m_bools && m_ints == other.m_ints && m_floats == other.m_floats
           && m_strings == other.m_strings;
}

void TClientOptions::assignFrom(const TClientOptions& other)
{
    if (this == &other)
        return;
    m_bools = other.m_bools;
    m_ints = other.m_ints;
    m_floats = other.m_floats;
    m_strings = other.m_strings;
    m_dates = other.m_dates;
    m_rendererChangeConfirmationRequired = other.m_rendererChangeConfirmationRequired;
    m_clientWindow = other.m_clientWindow;
    m_npcDialog = other.m_npcDialog;
    m_controlButtons = other.m_controlButtons;
    m_deviceCookies = other.m_deviceCookies;
    m_hotkeyOptions = other.m_hotkeyOptions;
    m_chat = other.m_chat;
    m_whiteBlacklist = other.m_whiteBlacklist;
    m_survey = other.m_survey;
    m_newsMaxUnlockLevel = other.m_newsMaxUnlockLevel;
    m_specialConditions = other.m_specialConditions;
    m_pinnedCharacters = other.m_pinnedCharacters;
    m_extraOptions = other.m_extraOptions;
    m_extraSections = other.m_extraSections;
    m_hasStartupNotice = other.m_hasStartupNotice;
    m_startupNotice = other.m_startupNotice;
}

bool TClientOptions::hasOption(const EClientOption id) const
{
    return m_bools.contains(id) || m_ints.contains(id) || m_floats.contains(id) || m_strings.contains(id)
           || m_dates.contains(id);
}

void TClientOptions::assignOption(const EClientOption id, const TClientOptions& other)
{
    if (const auto it = other.m_bools.find(id); it != other.m_bools.end() && m_bools.contains(id))
        setBoolOption(id, it->second);
    else if (const auto it = other.m_ints.find(id); it != other.m_ints.end() && m_ints.contains(id))
        setIntOption(id, it->second);
    else if (const auto it = other.m_floats.find(id); it != other.m_floats.end() && m_floats.contains(id))
        setFloatOption(id, it->second);
    else if (const auto it = other.m_strings.find(id); it != other.m_strings.end() && m_strings.contains(id))
        setStringOption(id, it->second);
    else if (const auto it = other.m_dates.find(id); it != other.m_dates.end() && m_dates.contains(id))
        setDateOption(id, it->second);
}

QString TClientOptions::exportToBase64String() const
{
    // qCompress prefixes the zlib stream with its 4-byte length.
    const QByteArray compressed = qCompress(toJson().toJson(QJsonDocument::Compact), 6).mid(4);
    return QString::fromLatin1(compressed.toBase64());
}

void TClientOptions::setExtraOption(const QString& name, const QJsonValue& value)
{
    m_extraOptions.insert(name, value);
}

void TClientOptions::setExtraSection(const QString& name, const QJsonValue& value)
{
    m_extraSections.insert(name, value);
}

void TClientOptions::setDefaults(const int mode)
{
    // Every default is assigned over the current value; options without a
    // default keep theirs. Outside mode 0 the sound options are left as they are.
    const auto isSoundOption = [](const EClientOption id) {
        return int(id) >= int(EClientOption::soundAmbienceVolume) && int(id) <= int(EClientOption::soundVipEnabled);
    };
    const QJsonObject& defaults = defaultClientOptions();
    for (auto it = defaults.begin(); it != defaults.end(); ++it) {
        EClientOption id;
        if (!optionId(it.key(), id))
            continue;
        if (mode != 0 && isSoundOption(id))
            continue;
        if (it->isBool())
            m_bools[id] = it->toBool();
        else if (it->isString())
            m_strings[id] = it->toString();
        else if (isFloatOption(it.key()))
            m_floats[id] = float(it->toDouble());
        else
            m_ints[id] = it->toInt();
    }
    m_controlButtons = defaultControlButtonsOptions();
    m_specialConditions = SpecialConditionsOptions::defaults().toJson();
    // Host: options and sections CipSoft does not know start empty.
    m_extraOptions = {};
    m_extraSections = {};
}

void TClientOptions::readBoolOptions(const QJsonObject& options)
{
    for (auto& [id, value] : m_bools)
        value = readBool(options, optionName(id), value);
}

void TClientOptions::readIntOptions(const QJsonObject& options)
{
    for (auto& [id, value] : m_ints)
        value = readInt(options, optionName(id), 0, std::numeric_limits<int>::max(), value);
    int& frameRateLimit = m_ints.at(EClientOption::frameRateLimit);
    frameRateLimit = std::clamp(frameRateLimit, 10, 200);
}

void TClientOptions::readFloatOptions(const QJsonObject& options)
{
    for (auto& [id, value] : m_floats)
        value = float(readDouble(options, optionName(id), 0.0, double(std::numeric_limits<float>::max()), double(value)));
    for (const EClientOption id : { EClientOption::lightAmbientLevel, EClientOption::lightLevelSeparatorLevel,
                                    EClientOption::lightAttenuationCloudsIndoor, EClientOption::ownEffectsOpacity,
                                    EClientOption::otherPlayersEffectsOpacity, EClientOption::monsterEffectsOpacity,
                                    EClientOption::monsterBossAreaEffectsOpacity, EClientOption::playerHudArcDistance })
        m_floats.at(id) = std::clamp(m_floats.at(id), 0.0f, 1.0f);
    m_floats.at(EClientOption::playerHudArcOpacity) = std::clamp(m_floats.at(EClientOption::playerHudArcOpacity), 0.2f, 1.0f);
    m_floats.at(EClientOption::playerHudArcHeight) = std::clamp(m_floats.at(EClientOption::playerHudArcHeight), 0.1f, 1.0f);
}

void TClientOptions::readStringOptions(const QJsonObject& options)
{
    for (auto& [id, value] : m_strings)
        value = readString(options, optionName(id));
}

void TClientOptions::readDateOptions(const QJsonObject& options)
{
    for (auto& [id, value] : m_dates)
        value = shared::jsonDateTime(options, optionName(id));
}

void TClientOptions::writeBoolOptions(QJsonObject& options) const
{
    for (const auto& [id, value] : m_bools)
        options.insert(optionName(id), value);
}

void TClientOptions::writeIntOptions(QJsonObject& options) const
{
    for (const auto& [id, value] : m_ints)
        options.insert(optionName(id), value);
}

void TClientOptions::writeFloatOptions(QJsonObject& options) const
{
    for (const auto& [id, value] : m_floats)
        options.insert(optionName(id), double(value));
}

void TClientOptions::writeStringOptions(QJsonObject& options) const
{
    for (const auto& [id, value] : m_strings)
        options.insert(optionName(id), value);
}

void TClientOptions::writeDateOptions(QJsonObject& options) const
{
    for (const auto& [id, value] : m_dates)
        options.insert(optionName(id), int(value.toSecsSinceEpoch()));
}

void TClientOptions::fromJson(const QJsonObject& json)
{
    const auto object = [&json](const char* key) { return json.value(QLatin1String(key)); };
    if (const QJsonValue v = object("clientWindowOptions"); v.isObject())
        setClientWindowOptions(clientWindowOptionsFromJson(v.toObject(), m_clientWindow));
    if (const QJsonValue v = object("npcDialogOptions"); v.isObject())
        setNpcDialogOptions(npcDialogOptionsFromJson(v.toObject()));
    if (const QJsonValue v = object("controlButtonsOptions"); v.isObject())
        setControlButtonsOptions(reconcileControlButtons(v.toObject()));
    if (const QJsonValue v = object("deviceCookieOptions"); v.isArray())
        setDeviceCookies(deviceCookiesFromJson(v.toArray()));
    if (const QJsonValue v = object("hotkeyOptions"); v.isObject())
        setHotkeyOptions(v.toObject());
    if (const QJsonValue v = object("chatOptions"); v.isObject())
        setChatOptions(chatOptionsFromJson(v.toObject()));
    if (const QJsonValue v = object("whiteBlacklistOptions"); v.isObject())
        setWhiteBlacklistOptions(whiteBlacklistOptionsFromJson(v.toObject()));
    if (const QJsonValue v = object("survey"); v.isObject())
        setSurveyInvitationOptions(surveyInvitationOptionsFromJson(v.toObject()));
    if (const QJsonValue v = object("news"); v.isObject())
        setNewsMaxUnlockLevel(newsOptionsFromJson(v.toObject()));
    if (const QJsonValue v = object("pinnedCharacters"); v.isArray())
        setPinnedCharacters(pinnedCharactersFromJson(v.toArray()));
    if (const QJsonValue v = object("specialConditionsOptions"); v.isObject())
        setSpecialConditionsOptions(SpecialConditionsOptions::fromJson(v.toObject()).toJson());
    if (const QJsonValue v = object("options"); v.isObject()) {
        const QJsonObject options = v.toObject();
        readBoolOptions(options);
        readIntOptions(options);
        readFloatOptions(options);
        readStringOptions(options);
        readDateOptions(options);
        // Host: options no CIP option names stay as stored.
        for (auto it = options.begin(); it != options.end(); ++it) {
            EClientOption id;
            if (!optionId(it.key(), id))
                m_extraOptions.insert(it.key(), *it);
        }
    }
    if (const QJsonValue v = object("rendererChangeConfirmationRequired"); v.isBool())
        m_rendererChangeConfirmationRequired = v.toBool();
    // Host: sections CIP does not know stay as stored.
    static const QStringList known = {
        QStringLiteral("clientWindowOptions"), QStringLiteral("npcDialogOptions"), QStringLiteral("controlButtonsOptions"),
        QStringLiteral("deviceCookieOptions"), QStringLiteral("hotkeyOptions"), QStringLiteral("chatOptions"),
        QStringLiteral("whiteBlacklistOptions"), QStringLiteral("survey"), QStringLiteral("news"),
        QStringLiteral("pinnedCharacters"), QStringLiteral("specialConditionsOptions"), QStringLiteral("options"),
        QStringLiteral("rendererChangeConfirmationRequired"), QLatin1String(VersionKey),
    };
    for (auto it = json.begin(); it != json.end(); ++it)
        if (!known.contains(it.key()))
            m_extraSections.insert(it.key(), *it);
}

QJsonDocument TClientOptions::toJson() const
{
    QJsonObject root = m_extraSections;
    root.insert(QStringLiteral("clientWindowOptions"), clientWindowOptionsToJson(m_clientWindow));
    root.insert(QStringLiteral("npcDialogOptions"), npcDialogOptionsToJson(m_npcDialog));
    root.insert(QStringLiteral("controlButtonsOptions"), controlButtonsOptionsToJson(m_controlButtons));
    root.insert(QStringLiteral("deviceCookieOptions"), deviceCookiesToJson(m_deviceCookies));
    root.insert(QStringLiteral("hotkeyOptions"), m_hotkeyOptions);
    root.insert(QStringLiteral("chatOptions"), chatOptionsToJson(m_chat));
    root.insert(QStringLiteral("whiteBlacklistOptions"), whiteBlacklistOptionsToJson(m_whiteBlacklist));
    root.insert(QStringLiteral("survey"), surveyInvitationOptionsToJson(m_survey));
    root.insert(QStringLiteral("news"), newsOptionsToJson(m_newsMaxUnlockLevel));
    root.insert(QStringLiteral("pinnedCharacters"), pinnedCharactersToJson(m_pinnedCharacters));
    root.insert(QStringLiteral("specialConditionsOptions"), m_specialConditions);
    QJsonObject options = m_extraOptions;
    writeBoolOptions(options);
    writeIntOptions(options);
    writeFloatOptions(options);
    writeStringOptions(options);
    writeDateOptions(options);
    root.insert(QStringLiteral("options"), options);
    root.insert(QStringLiteral("rendererChangeConfirmationRequired"), m_rendererChangeConfirmationRequired);
    root.insert(QLatin1String(VersionKey), clientoptionsmigration::CurrentVersion);
    return QJsonDocument(root);
}

void TClientOptions::load()
{
    QJsonDocument document;
    QFile file(userFilePath());
    if (file.open(QIODevice::ReadOnly))
        document = QJsonDocument::fromJson(file.readAll());
    if (document.isEmpty()) {
        setDefaults(1);
        document = toJson();
    }
    const QJsonObject migrated = clientoptionsmigration::migrate(document.object());
    setDefaults(0);
    fromJson(migrated);
    announceAllOptions();
    ensureGlobalShortcuts();
}

void TClientOptions::loadDefaultClientOptions()
{
    QJsonDocument document = shared::TTibiaFileSystemHelper().readDefaultClientOptions();
    if (document.isEmpty()) {
        setDefaults(1);
        document = toJson();
    }
    const QJsonObject migrated = clientoptionsmigration::migrate(document.object());
    setDefaults(1);
    fromJson(migrated);
    announceAllOptions();
}

void TClientOptions::ensureGlobalShortcuts()
{
    THotkeyOptionsData hotkeys;
    try {
        hotkeys = THotkeyOptionsData::fromJson(m_hotkeyOptions);
    } catch (const std::runtime_error&) {
        // A section CIP refuses to load (a chat text too long) starts over
        // from the built-in sets.
        hotkeys = {};
    }
    hotkeys.addGlobalShortcuts(m_bools.at(EClientOption::rotateWithCtrl), m_bools.at(EClientOption::rotateWithShift),
                               m_bools.at(EClientOption::rotateWithAlt));
    setHotkeyOptions(hotkeys.toJson());
}

void TClientOptions::saveToJsonFile()
{
    if (isTemporaryClientOptionsMode())
        return;
    const QString path = userFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(toJson().toJson()) < 0 || !file.commit())
        Q_EMIT failedToSaveClientOptions(file.errorString());
}

}
