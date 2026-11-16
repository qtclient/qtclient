#include "optionspages.h"

#include "app/bootstrap.h"
#include "config/clientoptiondefaults.h"
#include "config/hotkeyoptions.h"
#include "config/specialconditionobject.h"
#include "gamewindow/actionbarcontroller.h"
#include "gamewindow/assignactiondialogs.h"
#include "gamewindow/buttonbarcontroller.h"
#include "gamewindow/genericentertextdialogcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/keyboardshortcuthandler.h"

#include "sound/soundstorage.h"

#include <QColor>
#include <QHash>
#include <QJsonArray>
#include <QKeySequence>
#include <QQuickWindow>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <utility>

namespace tibia::config {

namespace {

using O = cip::EClientOption;
using B = TOptionBinding;

QString optionName(const O option)
{
    const std::string_view name = cip::ClientOptionNames[static_cast<size_t>(option)];
    return QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size()));
}

constexpr int SoftwareRenderer = 3;
constexpr int MetalRenderer = 9;

/// : the renderer the process runs on.
int activeRenderer()
{
    switch (QQuickWindow::graphicsApi()) {
    case QSGRendererInterface::Software:
        return SoftwareRenderer;
    case QSGRendererInterface::Direct3D11:
        return 6;
    case QSGRendererInterface::Vulkan:
        return 7;
    case QSGRendererInterface::OpenGL:
        return 8;
    case QSGRendererInterface::Metal:
        return MetalRenderer;
    case QSGRendererInterface::Direct3D12:
        return 10;
    default:
        return 0;
    }
}

/// : the software renderer has no light, vsync or frame limit choice.
bool rendererLimitationsActive()
{
    return activeRenderer() == SoftwareRenderer;
}

/// .
QString rendererName(const int renderer)
{
    switch (renderer) {
    case 0:
        return qtTrId("renderer_auto");
    case SoftwareRenderer:
        return qtTrId("renderer_software");
    case 6:
        return qtTrId("renderer_directx11_hardware");
    case 7:
        return qtTrId("renderer_vulkan");
    case 8:
        return qtTrId("renderer_opengl");
    case MetalRenderer:
        return qtTrId("renderer_metal");
    case 10:
        return qtTrId("renderer_directx12_hardware");
    default:
        return qtTrId("dummy_unknown");
    }
}

/// The renderers in CIP's list order (,).
constexpr int RendererOrder[] = { 0, 6, 10, 7, 9, 8, SoftwareRenderer };

/// EButtonBarButton names and labels (, table).
/// Id order must match defaultControlButtonIds.
struct ButtonBarButton
{
    const char* id;
    const char* label;
};
constexpr ButtonBarButton ButtonBarButtons[] = {
    { "skillsWidget", "optionsmenu_buttonbar_button_skills" },
    { "battleListWidget", "optionsmenu_buttonbar_button_battle" },
    { "partyWidget", "optionsmenu_buttonbar_button_party" },
    { "vipWidget", "optionsmenu_buttonbar_button_vip" },
    { "spellListWidget", "optionsmenu_buttonbar_button_spells" },
    { "skillWheelDialog", "optionsmenu_buttonbar_button_skill_wheel_dialog" },
    { "questDialog", "optionsmenu_buttonbar_button_quests" },
    { "questTrackerWidget", "optionsmenu_buttonbar_button_questtracker" },
    { "unjustifiedPoinsWidget", "optionsmenu_buttonbar_button_unjustified" },
    { "preyDialog", "optionsmenu_buttonbar_button_prey" },
    { "preyWidget", "optionsmenu_buttonbar_button_killtracker" },
    { "rewardWallDialog", "optionsmenu_buttonbar_button_dailyreward" },
    { "analyticsSelectorWidget", "optionsmenu_buttonbar_button_analytics" },
    { "compendiumDialog", "optionsmenu_buttonbar_button_compendium" },
    { "cyclopediaDialog", "optionsmenu_buttonbar_button_cyclopedia" },
    { "bosstiaryDialog", "optionsmenu_buttonbar_button_bosstiary_dialog" },
    { "bossslotsDialog", "optionsmenu_buttonbar_button_bossslots_dialog" },
    { "bosstiaryTrackerWidget", "optionsmenu_buttonbar_button_bosstiarytracker" },
    { "bestiaryTrackerWidget", "optionsmenu_buttonbar_button_bestiarytracker" },
    { "imbuementTrackerWidget", "optionsmenu_buttonbar_button_imbuement_tracker_widget" },
    { "weaponProficiency", "optionsmenu_buttonbar_button_weapon_proficiency_dialog" },
    { "exaltationForgeDialog", "optionsmenu_buttonbar_button_exaltation_forge_dialog" },
    { "friendsDialog", "optionsmenu_buttonbar_button_social" },
    { "lenshelpFunction", "optionsmenu_buttonbar_button_lenshelp" },
    { "highscoresDialog", "optionsmenu_buttonbar_button_highscores" },
    { "playerGuide", "optionsmenu_buttonbar_button_player_guide" },
    { "manageShortcuts", "optionsmenu_buttonbar_button_manage_shortcuts" },
    { "taskboard", "optionsmenu_buttonbar_button_taskboard_dialog" },
};

QString buttonLabel(const QString& id)
{
    for (const auto& button : ButtonBarButtons)
        if (id == QLatin1String(button.id))
            return qtTrId(button.label);
    const QString product = gamewindow::TButtonBarController::productShortcutLabel(id);
    return product.isEmpty() ? id : product;
}

/// : the button a label names.
QString buttonId(const QString& label)
{
    for (const auto& button : ButtonBarButtons)
        if (label == qtTrId(button.label))
            return QString::fromLatin1(button.id);
    const QString product = gamewindow::TButtonBarController::productShortcutIdFromLabel(label);
    return product.isEmpty() ? label : product;
}

QStringList stringList(const QJsonValue& value)
{
    QStringList list;
    for (const QJsonValue& entry : value.toArray())
        list << entry.toString();
    return list;
}

constexpr B ActionBarsBindings[] = {
    { "showBottomActionBar1", O::actionBarShowBottom1, B::Bool },
    { "showBottomActionBar2", O::actionBarShowBottom2, B::Bool },
    { "showBottomActionBar3", O::actionBarShowBottom3, B::Bool },
    { "showBottomActionBars", O::actionBarsShowBottom, B::Bool },
    { "showLeftActionBar1", O::actionBarShowLeft1, B::Bool },
    { "showLeftActionBar2", O::actionBarShowLeft2, B::Bool },
    { "showLeftActionBar3", O::actionBarShowLeft3, B::Bool },
    { "showLeftActionBars", O::actionBarsShowLeft, B::Bool },
    { "showRightActionBar1", O::actionBarShowRight1, B::Bool },
    { "showRightActionBar2", O::actionBarShowRight2, B::Bool },
    { "showRightActionBar3", O::actionBarShowRight3, B::Bool },
    { "showRightActionBars", O::actionBarsShowRight, B::Bool },
    { "actionBarBottomLocked", O::actionBarBottomLocked, B::Carried },
    { "actionBarLeftLocked", O::actionBarLeftLocked, B::Carried },
    { "actionBarRightLocked", O::actionBarRightLocked, B::Carried },
    { "showHotkey", O::actionButtonShowHotkey, B::Bool },
    { "showAmount", O::actionButtonShowAmount, B::Bool },
    { "showSpellParameters", O::actionButtonShowSpellParameters, B::Bool },
    { "showCooldown", O::actionButtonShowGraphicalCooldown, B::Bool },
    { "showCooldownNumber", O::actionButtonShowCooldownNumbers, B::Bool },
    { "allowTooltip", O::actionButtonAllowTooltip, B::Bool },
    { "autoInsertSpells", O::actionButtonAutoInsertSpells, B::Bool },
};

constexpr B ConsoleBindings[] = {
    { "showInfoMessages", O::consoleShowInfoMessages, B::Bool },
    { "showEventMessages", O::consoleShowEventMessages, B::Bool },
    { "showStatusMessages", O::consoleShowStatusMessagesOwn, B::Bool },
    { "showStatusMessagesOfOthers", O::consoleShowStatusMessagesOfOthers, B::Bool },
    { "showTimestamps", O::consoleShowTimestamps, B::Bool },
    { "showTimestampsSeconds", O::consoleShowTimestampsSeconds, B::Bool },
    { "showLevels", O::consoleShowLevels, B::Bool },
    { "openNewPrivateMessagesInNewTab", O::openNewPrivateMessagesInNewTab, B::Bool },
};

constexpr B InterfaceBindings[] = {
    { "highlightMouseTarget", O::mouseHighlightTarget, B::Bool },
    { "systemCursor", O::mouseSystemCursor, B::Bool },
    { "animatedMouseCursor", O::mouseAnimatedCursor, B::Bool },
    { "bigMouseCursor", O::mouseBigCursor, B::Bool },
    { "showCooldownBar", O::cooldownBarEnabled, B::Bool },
    { "showLinkCopyWarning", O::linkCopyWarningEnabled, B::Bool },
    { "colorizeLootStyleIndex", O::colorizeLootStyle, B::Int },
    { "showExpireInInventory", O::showExpireInInventory, B::Bool },
    { "showExpireInContainers", O::showExpireInContainers, B::Bool },
    { "showExpireWhenUnused", O::showExpireWhenUnused, B::Bool },
};

constexpr B GameWindowBindings[] = {
    { "showTextualEffects", O::gameWindowShowTextualEffects, B::Bool },
    { "showMessages", O::gameWindowShowMessages, B::Bool },
    { "showPrivateMessages", O::gameWindowShowPrivateMessages, B::Bool },
    { "showBoostedCreatureMessages", O::gameWindowShowBoostedCreatureMessages, B::Bool },
    { "showOfflineTrainingMessages", O::gameWindowShowOfflineTrainingMessages, B::Bool },
    { "showStoreMessages", O::gameWindowShowStoreMessages, B::Bool },
    { "showPotionMessages", O::gameWindowShowPotionMessages, B::Bool },
    { "showHotkeyUsageMessages", O::gameWindowShowHotkeyUsageMessages, B::Bool },
    { "showLootMessages", O::gameWindowShowLootMessages, B::Bool },
    { "showLootHighlighting", O::gameWindowShowLootHighlighting, B::Bool },
    { "showOwnSpells", O::gameWindowShowOwnSpells, B::Bool },
    { "showOthersSpells", O::gameWindowShowOthersSpells, B::Bool },
    { "showFrames", O::combatShowFrames, B::Bool },
    { "showFramesPvp", O::combatShowPvpFrames, B::Bool },
    { "showTargetFrame", O::gameWindowShowTragetFrame, B::Bool },
    { "showTargetHighlight", O::gameWindowShowTargetHighlight, B::Bool },
    { "showAttackAnimation", O::gameWindowShowAttackAnimation, B::Bool },
    { "showInfoBanner", O::gameWindowShowInfoBanner, B::Bool },
};

constexpr B HudBindings[] = {
    { "playerHudEnabled", O::playerHudEnabled, B::Bool },
    { "playerHudShowBars", O::playerHudShowBars, B::Bool },
    { "playerHudShowArcs", O::playerHudShowArcs, B::Bool },
    { "playerHudShowHarmonyLeft", O::playerHudShowHarmonyLeft, B::Bool },
    { "playerHudArcOpacity", O::playerHudArcOpacity, B::Real },
    { "playerHudArcDistance", O::playerHudArcDistance, B::Real },
    { "playerHudArcHeight", O::playerHudArcHeight, B::Real },
    { "playerHudArcWidth", O::playerHudArcWidth, B::Int },
    { "playerShowName", O::playerShowName, B::Bool },
    { "playerShowHealth", O::playerShowHealth, B::Bool },
    { "playerShowMana", O::playerShowMana, B::Bool },
    { "playerShowHarmony", O::playerShowHarmonySerene, B::Bool },
    { "playerShowMarks", O::playerShowMarks, B::Bool },
    { "creatureHudEnabled", O::creatureHudEnabled, B::Bool },
    { "creatureShowName", O::creatureShowName, B::Bool },
    { "creatureShowHealth", O::creatureShowHealth, B::Bool },
    { "creatureShowMarks", O::creatureShowMarks, B::Bool },
    { "creatureNpcShowIcon", O::creatueShowNpcIcon, B::Bool },
    { "statusBarEnabled", O::statusBarEnabled, B::Bool },
    { "statusPanelEnabled", O::statusPanelEnabled, B::Bool },
};

constexpr B MiscBindings[] = {
    { "askBeforeBuyingStoreProducts", O::storeAskBeforeBuyingProducts, B::Bool },
    { "askBeforeStowContainerContent", O::stashAskBeforeStowContainerContent, B::Bool },
    { "askBeforeSortContainerRecursive", O::containerSortRecursiveShowWarningAgain, B::Bool },
    { "askBeforeMoveContainerContentToManagedContainersRecursive", O::containerMoveToManagedContainerRecursiveShowWarningAgain, B::Bool },
    { "stayLoggedInByDefault", O::sessionRemainLoggedIn, B::Bool },
    { "optimizeConnectionStability", O::connectionStabilityOptimizationEnabled, B::Bool },
    { "quickLogin", O::quickLogin, B::Bool },
    { "useFreetypeFontRenderer", O::useFreetypeRenderer, B::Bool },
};

constexpr B GameplayBindings[] = {
    { "allowAllToInspectPlayer", O::inspectPlayerAllowAllEnabled, B::Bool },
    { "autoChaseOff", O::autoChaseEnabled, B::InvertedBool },
    { "quickLootAllCorpsesInArea", O::quickLootAllCorpsesInAreaEnabled, B::Bool },
};

constexpr B GraphicsBindings[] = {
    { "scaleOnlyByEvenMultiples", O::gameWindowScaleOnlyByEvenMultiples, B::Bool },
    { "antialiasingModeIndex", O::antialiasingMode, B::Int },
    { "vSync", O::vSyncEnabled, B::Bool },
    { "noFpsLimitEnabled", O::frameRateLimitEnabled, B::InvertedBool },
    { "fpsLimit", O::frameRateLimit, B::Int },
};

constexpr B EffectsBindings[] = {
    { "lightEffects", O::lightEffectsEnabled, B::Bool },
    { "ambientLightLevel", O::lightAmbientLevel, B::Percent },
    { "levelSeparatorLevel", O::lightLevelSeparatorLevel, B::Percent },
    { "lightAttenuationCloudsIndoor", O::lightAttenuationCloudsIndoor, B::CloudsIndex },
    { "ownEffectsOpacity", O::ownEffectsOpacity, B::Percent },
    { "otherPlayersEffectsOpacity", O::otherPlayersEffectsOpacity, B::Percent },
    { "monsterEffectsOpacity", O::monsterEffectsOpacity, B::Percent },
    { "monsterBossAreaEffectsOpacity", O::monsterBossAreaEffectsOpacity, B::Percent },
};

constexpr B ControlsBindings[] = {
    { "controlSchemeIndex", O::controlSchemeIndex, B::Int },
    { "lootSchemeIndex", O::lootSchemeIndex, B::Int },
    { "useDefaultKeyboardDelay", O::keyboardDelayUseDefault, B::Bool },
    { "keyboardDelayMillis", O::keyboardDelayMs, B::Int },
    { "rotateWithAlt", O::rotateWithAlt, B::Bool },
    { "rotateWithShift", O::rotateWithShift, B::Bool },
    { "rotateWithCtrl", O::rotateWithCtrl, B::Bool },
    { "alwaysTurnTowardsMoveDirection", O::alwaysTurnTowardsMoveDirection, B::Bool },
    { "ctrlToDragStacks", O::dragAndDropDefaultActionIsMoveAll, B::InvertedBool },
};

constexpr B ScreenshotsBindings[] = {
    { "onlyGameWindow", O::screnshotsOnlyGameWindow, B::Bool },
    { "useBacklog", O::screenshotsUseBacklog, B::Bool },
    { "autoScreenshotsEnabled", O::screenshotsAutoScreenshotsEnabled, B::Bool },
    { "achievementEnabled", O::screenshotsAchievementEnabled, B::Bool },
    { "bestiaryFullEnabled", O::screenshotsBestiaryFullEnabled, B::Bool },
    { "bestiaryPartlyEnabled", O::screenshotsBestiaryPartlyEnabled, B::Bool },
    { "bossKillEnabled", O::screenshotsBossKillEnabled, B::Bool },
    { "deathPvEEnabled", O::screenshotsDeathPvEEnabled, B::Bool },
    { "deathPvPEnabled", O::screenshotsDeathPvPEnabled, B::Bool },
    { "levelUpEnabled", O::screenshotsLevelUpEnabled, B::Bool },
    { "lowHitPointsEnabled", O::screenshotsLowHitPointsEnabled, B::Bool },
    { "newMaxDamageEnabled", O::screenshotsNewMaxDamageEnabled, B::Bool },
    { "newMaxHealingEnabled", O::screenshotsNewMaxHealingEnabled, B::Bool },
    { "playerKillAssistEnabled", O::screenshotsPlayerKillAssistEnabled, B::Bool },
    { "playerKillFullEnabled", O::screenshotsPlayerKillFullEnabled, B::Bool },
    { "pvPAttackEnabled", O::screenshotsPvPAttackEnabled, B::Bool },
    { "rewardChestEnabled", O::screenshotsRewardChestEnabled, B::Bool },
    { "skillUpEnabled", O::screenshotsSkillUpEnabled, B::Bool },
    { "giftOfLifeEnabled", O::screenshotsGiftOfLifeEnabled, B::Bool },
    { "trackedLootEnabled", O::screenshotsTrackedLootEnabled, B::Bool },
};

constexpr B SoundBindings[] = {
    { "masterVolume", O::soundMasterVolume, B::Int },
    { "musicVolume", O::soundMusicVolume, B::Int },
    { "anthemEnabled", O::soundAnthemEnabled, B::Bool },
    { "ambienceVolume", O::soundAmbienceVolume, B::Int },
    { "itemsVolume", O::soundItemsVolume, B::Int },
    { "eatingEnabled", O::soundEatingEnabled, B::Bool },
    { "moveItemEnabled", O::soundMoveItemEnabled, B::Bool },
    { "eventsVolume", O::soundEventsVolume, B::Int },
};

constexpr B BattleSoundsBindings[] = {
    { "ownVolume", O::soundBattleOwnVolume, B::Int },
    { "ownSpellsEnabled", O::soundBattleOwnSpellsEnabled, B::Bool },
    { "ownAttackSpellsEnabled", O::soundBattleOwnAttackSpellsEnabled, B::Bool },
    { "ownHealingSpellsEnabled", O::soundBattleOwnHealingSpellsEnabled, B::Bool },
    { "ownSupportSpellsEnabled", O::soundBattleOwnSupportSpellsEnabled, B::Bool },
    { "ownWeaponsEnabled", O::soundBattleOwnWeaponsEnabled, B::Bool },
    { "othersVolume", O::soundBattleOthersVolume, B::Int },
    { "othersSpellsEnabled", O::soundBattleOthersSpellsEnabled, B::Bool },
    { "othersAttackSpellsEnabled", O::soundBattleOthersAttackSpellsEnabled, B::Bool },
    { "othersHealingSpellsEnabled", O::soundBattleOthersHealingSpellsEnabled, B::Bool },
    { "othersSupportSpellsEnabled", O::soundBattleOthersSupportSpellsEnabled, B::Bool },
    { "othersWeaponsEnabled", O::soundBattleOthersWeaponsEnabled, B::Bool },
    { "creaturesVolume", O::soundBattleCreaturesVolume, B::Int },
    { "creaturesNoisesEnabled", O::soundBattleCreaturesNoisesEnabled, B::Bool },
    { "creaturesDeathEnabled", O::soundBattleCreaturesDeathEnabled, B::Bool },
    { "creaturesAttacksEnabled", O::soundBattleCreaturesAttacksEnabled, B::Bool },
};

constexpr B UISoundsBindings[] = {
    { "uiVolume", O::soundUiVolume, B::Int },
    { "uiInteractionsEnabled", O::soundUiInteractionsEnabled, B::Bool },
    { "partyEnabled", O::soundPartyEnabled, B::Bool },
    { "vipEnabled", O::soundVipEnabled, B::Bool },
    { "chatEnabled", O::ChatEnabled, B::Bool },
    { "partyMessagesEnabled", O::PartyMessagesEnabled, B::Bool },
    { "guildMessagesEnabled", O::GuildMessagesEnabled, B::Bool },
    { "privateMessagesWithoutTabEnabled", O::PrivateMessagesWithoutTabEnabled, B::Bool },
    { "privateMessagesEnabled", O::PrivateMessagesEnabled, B::Bool },
    { "npcMessageseEnabled", O::NpcMessageseEnabled, B::Bool },
    { "globalMessagesEnabled", O::GlobalMessagesEnabled, B::Bool },
    { "teamFinderMessagesEnabled", O::TeamFinderMessagesEnabled, B::Bool },
    { "raidMessagesEnabled", O::RaidMessagesEnabled, B::Bool },
    { "systemMessagesEnabled", O::SystemMessagesEnabled, B::Bool },
};

/// A 0..1 float option as a slider percent: rounded, at most 100.
uint percentOf(const float value)
{
    const long percent = std::lround(value * 100.0f);
    return percent >= 0 && percent < 100 ? static_cast<uint>(percent) : 100u;
}

/// A slider percent as the 0..1 float option: at most 1, never negative.
float fractionOf(const uint percent)
{
    const float fraction = static_cast<float>(percent) * 0.01f;
    return fraction < 0.0f ? 0.0f : std::min(fraction, 1.0f);
}

/// lightAttenuationCloudsIndoor as the slider's step of 0.005, truncated.
uint cloudsIndexOf(const float value)
{
    return static_cast<uint>(static_cast<long>((1.0f - value) / 0.005f));
}

constexpr auto SpecialConditionsSection = "specialConditionsOptions";
constexpr auto ControlButtonsSection = "controlButtonsOptions";
constexpr auto ClientWindowSection = "clientWindowOptions";
constexpr auto BorderlessKey = "clientWindowIsBorderless";

}

QJsonValue optionValue(const QJsonObject& settings, const cip::EClientOption option)
{
    return settings.value(QLatin1String("options")).toObject().value(optionName(option));
}

void setOptionValue(QJsonObject& settings, const cip::EClientOption option, const QJsonValue& value)
{
    QJsonObject options = settings.value(QLatin1String("options")).toObject();
    options.insert(optionName(option), value);
    settings.insert(QLatin1String("options"), options);
}

void loadBindings(QObject* page, const std::span<const TOptionBinding> bindings, const QJsonObject& settings)
{
    const QJsonObject options = settings.value(QLatin1String("options")).toObject();
    for (const TOptionBinding& binding : bindings) {
        const QJsonValue value = options.value(optionName(binding.option));
        if (value.isUndefined())
            continue;
        QVariant property;
        switch (binding.kind) {
        case B::Bool:
            property = value.toBool();
            break;
        case B::InvertedBool:
            property = !value.toBool();
            break;
        case B::Int:
            property = static_cast<uint>(std::max(0, value.toInt()));
            break;
        case B::Real:
            // The option is a float.
            property = static_cast<double>(static_cast<float>(value.toDouble()));
            break;
        case B::Percent:
            property = percentOf(static_cast<float>(value.toDouble()));
            break;
        case B::CloudsIndex:
            property = cloudsIndexOf(static_cast<float>(value.toDouble()));
            break;
        case B::Carried:
            property = value.toVariant();
            break;
        }
        page->setProperty(binding.property, property);
    }
}

void applyBindings(const QObject* page, const std::span<const TOptionBinding> bindings, QJsonObject& settings)
{
    QJsonObject options = settings.value(QLatin1String("options")).toObject();
    for (const TOptionBinding& binding : bindings) {
        const QVariant property = page->property(binding.property);
        QJsonValue value;
        switch (binding.kind) {
        case B::Bool:
            value = property.toBool();
            break;
        case B::InvertedBool:
            value = !property.toBool();
            break;
        case B::Int:
            value = static_cast<int>(property.toUInt());
            break;
        case B::Real:
            value = static_cast<double>(static_cast<float>(property.toDouble()));
            break;
        case B::Percent:
            value = static_cast<double>(fractionOf(property.toUInt()));
            break;
        case B::CloudsIndex:
            value = static_cast<double>(static_cast<float>(property.toUInt()) * -0.005f + 1.0f);
            break;
        case B::Carried:
            if (!property.isValid())
                continue;
            value = QJsonValue::fromVariant(property);
            break;
        }
        options.insert(optionName(binding.option), value);
    }
    settings.insert(QLatin1String("options"), options);
}

TInterfaceOptionsPage::TInterfaceOptionsPage(QObject* parent)
    : TBoundOptionsPage(InterfaceBindings, parent)
{
    m_colorizeLootStyleListModel = { qtTrId("colorizelootstyle_none"), qtTrId("colorizelootstyle_normal"), qtTrId("colorizelootstyle_compact") };
}

TGameWindowOptionsPage::TGameWindowOptionsPage(QObject* parent)
    : TBoundOptionsPage(GameWindowBindings, parent)
{
}

void TGameWindowOptionsPage::setShowTargetFrameHighlight(const bool ShowFrame, const bool ShowHighlight)
{
    if (m_showTargetFrame == ShowFrame && m_showTargetHighlight == ShowHighlight)
        return;
    m_showTargetFrame = ShowFrame;
    m_showTargetHighlight = ShowHighlight;
    Q_EMIT changed();
}

THudOptionsPage::THudOptionsPage(QObject* parent)
    : TBoundOptionsPage(HudBindings, parent)
{
}

void THudOptionsPage::load(const QJsonObject& settings)
{
    TBoundOptionsPage::load(settings);
    const QJsonValue section = settings.value(QLatin1String(SpecialConditionsSection));
    m_specialConditions = section.isObject() ? SpecialConditionsOptions::fromJson(section.toObject()) : SpecialConditionsOptions::defaults();
    m_specialConditionsShowInHUDEnabled = m_specialConditions.showInHUD;
    m_specialConditionsShowInBarEnabled = m_specialConditions.showInBar;
    Q_EMIT changed();
    rebuildSpecialConditionsModel();
}

void THudOptionsPage::apply(QJsonObject& settings) const
{
    TBoundOptionsPage::apply(settings);
    SpecialConditionsOptions conditions = m_specialConditions;
    conditions.showInHUD = m_specialConditionsShowInHUDEnabled;
    conditions.showInBar = m_specialConditionsShowInBarEnabled;
    settings.insert(QLatin1String(SpecialConditionsSection), conditions.toJson());
}

void THudOptionsPage::setSpecialConditionShowInHUDEnabled(const int SpecialConditionIndex, const bool Enabled)
{
    if (SpecialConditionIndex < 0 || SpecialConditionIndex >= m_specialConditions.specialConditionsOrder.size())
        return;
    const SpecialCondition condition = m_specialConditions.specialConditionsOrder.at(SpecialConditionIndex);
    if (Enabled)
        m_specialConditions.visibleInHUD.insert(condition);
    else
        m_specialConditions.visibleInHUD.remove(condition);
    rebuildSpecialConditionsModel();
}

void THudOptionsPage::setSpecialConditionShowInBarEnabled(const int SpecialConditionIndex, const bool Enabled)
{
    if (SpecialConditionIndex < 0 || SpecialConditionIndex >= m_specialConditions.specialConditionsOrder.size())
        return;
    const SpecialCondition condition = m_specialConditions.specialConditionsOrder.at(SpecialConditionIndex);
    if (Enabled)
        m_specialConditions.visibleInBar.insert(condition);
    else
        m_specialConditions.visibleInBar.remove(condition);
    rebuildSpecialConditionsModel();
}

void THudOptionsPage::moveSpecialConditionUp(const int SpecialConditionIndex)
{
    QList<SpecialCondition>& order = m_specialConditions.specialConditionsOrder;
    if (SpecialConditionIndex > 0 && SpecialConditionIndex < order.size())
        order.swapItemsAt(SpecialConditionIndex - 1, SpecialConditionIndex);
    rebuildSpecialConditionsModel();
}

void THudOptionsPage::moveSpecialConditionDown(const int SpecialConditionIndex)
{
    QList<SpecialCondition>& order = m_specialConditions.specialConditionsOrder;
    if (SpecialConditionIndex >= 0 && SpecialConditionIndex < order.size() - 1)
        order.swapItemsAt(SpecialConditionIndex, SpecialConditionIndex + 1);
    rebuildSpecialConditionsModel();
}

void THudOptionsPage::rebuildSpecialConditionsModel()
{
    const QList<QObject*> previous = m_specialConditionsModel;
    m_specialConditionsModel.clear();
    const QList<SpecialCondition>& order = m_specialConditions.specialConditionsOrder;
    for (int i = 0; i < order.size(); ++i) {
        const SpecialCondition& condition = order.at(i);
        m_specialConditionsModel << new TSpecialConditionObject(condition, i, m_specialConditions.visibleInHUD.contains(condition),
                                                                m_specialConditions.visibleInBar.contains(condition), this);
    }
    Q_EMIT specialConditionsModelChanged();
    for (QObject* object : previous)
        object->deleteLater();
}

TGraphicsOptionsPage::TGraphicsOptionsPage(QObject* parent)
    : TBoundOptionsPage(GraphicsBindings, parent)
{
    for (const int renderer : RendererOrder) {
        if (!app::isRendererSupported(renderer))
            continue;
        m_rendererIds << renderer;
        m_rendererListModel << rendererName(renderer);
    }
    for (int mode = 0; mode < 3; ++mode)
        m_antialiasingModeListModel << qtTrId(mode == 0 ? "antialiasingmode_none" : mode == 1 ? "antialiasingmode_antialiasing" : "antialiasingmode_retro");
    m_rendererLimitationsActive = rendererLimitationsActive();
    m_isMetal = activeRenderer() == MetalRenderer;
    // product: allow toggling the CIP "no FPS limit" checkbox (was hard-disabled).
    m_canChangeFpsLimitCheckbox = !rendererLimitationsActive();
}

void TGraphicsOptionsPage::load(const QJsonObject& settings)
{
    TBoundOptionsPage::load(settings);
    m_borderlessWindow = settings.value(QLatin1String(ClientWindowSection)).toObject().value(QLatin1String(BorderlessKey)).toBool(false);
    m_rendererId = optionValue(settings, O::rendererIndex).toInt(0);
    m_vSync = optionValue(settings, O::vSyncEnabled).toBool(m_vSync);

    QString tooltip;
    if (m_rendererId != activeRenderer())
        tooltip += qtTrId("optionsmenu_engine_tooltip_auto").arg(rendererName(activeRenderer()));
    tooltip += (tooltip.isEmpty() ? QString() : QStringLiteral("\n\n")) + qtTrId("optionsmenu_engine_tooltip_help");
    QString disabled;
    for (const int renderer : RendererOrder)
        if (renderer != SoftwareRenderer && !app::isRendererSupported(renderer))
            disabled += QLatin1Char('\n') + rendererName(renderer);
    if (!disabled.isEmpty())
        tooltip += QStringLiteral("\n\n") + qtTrId("optionsmenu_engine_tooltip_disabled") + QLatin1Char('\n') + disabled;
    m_tooltipTextDisabledRenderers = tooltip;
    Q_EMIT changed();
}

void TGraphicsOptionsPage::apply(QJsonObject& settings) const
{
    TBoundOptionsPage::apply(settings);
    setOptionValue(settings, O::rendererIndex, m_rendererId);
    setOptionValue(settings, O::vSyncEnabled, m_vSync);
    QJsonObject window = settings.value(QLatin1String(ClientWindowSection)).toObject();
    window.insert(QLatin1String(BorderlessKey), m_borderlessWindow);
    settings.insert(QLatin1String(ClientWindowSection), window);
}

uint TGraphicsOptionsPage::rendererIndex() const
{
    return static_cast<uint>(std::max<qsizetype>(0, m_rendererIds.indexOf(m_rendererId)));
}

void TGraphicsOptionsPage::setRendererIndex(const uint index)
{
    const int renderer = index < static_cast<uint>(m_rendererIds.size()) ? m_rendererIds.at(index) : 0;
    if (m_rendererId == renderer)
        return;
    m_rendererId = renderer;
    Q_EMIT changed();
}

bool TGraphicsOptionsPage::vSync() const
{
    return m_vSync && !rendererLimitationsActive();
}

void TGraphicsOptionsPage::setVSync(const bool enabled)
{
    if (m_vSync == enabled || rendererLimitationsActive())
        return;
    m_vSync = enabled;
    Q_EMIT changed();
}

TEffectsOptionsPage::TEffectsOptionsPage(QObject* parent)
    : TBoundOptionsPage(EffectsBindings, parent)
{
    m_rendererLimitationsActive = rendererLimitationsActive();
}

void TEffectsOptionsPage::load(const QJsonObject& settings)
{
    TBoundOptionsPage::load(settings);
    m_lightEffects = optionValue(settings, O::lightEffectsEnabled).toBool(m_lightEffects);
    Q_EMIT changed();
}

void TEffectsOptionsPage::apply(QJsonObject& settings) const
{
    TBoundOptionsPage::apply(settings);
    setOptionValue(settings, O::lightEffectsEnabled, m_lightEffects);
}

bool TEffectsOptionsPage::lightEffects() const
{
    return m_lightEffects && !rendererLimitationsActive();
}

void TEffectsOptionsPage::setLightEffects(const bool enabled)
{
    if (m_lightEffects == enabled || rendererLimitationsActive())
        return;
    m_lightEffects = enabled;
    Q_EMIT changed();
}

TControlsOptionsPage::TControlsOptionsPage(QObject* parent)
    : TBoundOptionsPage(ControlsBindings, parent)
{
    m_controlSchemeListModel = { qtTrId("optionsmenu_controlscheme_classic"), qtTrId("optionsmenu_controlscheme_legacy"),
                                 qtTrId("optionsmenu_controlscheme_leftsmartclick") };
    m_lootSchemeListModel = { qtTrId("optionsmenu_lootscheme_right"), qtTrId("optionsmenu_lootscheme_shift_right"), qtTrId("optionsmenu_lootscheme_left") };
    // CIP returns DAT_1022bdb0c.
    m_defaultKeyboardDelayMillis = 250;
}

TButtonBarOptionsPage::TButtonBarOptionsPage(QObject* parent)
    : TBoundOptionsPage({}, parent)
{
}

void TButtonBarOptionsPage::load(const QJsonObject& settings)
{
    const QJsonValue section = settings.value(QLatin1String(ControlButtonsSection));
    if (section.isObject()) {
        m_enabled = stringList(section[QLatin1String("enabledButtons")]);
        m_disabled = stringList(section[QLatin1String("disabledButtons")]);
    } else {
        const QJsonObject defaults = defaultControlButtonsOptions();
        m_enabled = stringList(defaults[QLatin1String("enabledButtons")]);
        m_disabled = stringList(defaults[QLatin1String("disabledButtons")]);
    }
    // Pack shortcuts registered after first ClientOptions::load — surface them here.
    QSet<QString> seen(m_enabled.begin(), m_enabled.end());
    for (const QString& id : m_disabled)
        seen.insert(id);
    for (const QString& id : gamewindow::TButtonBarController::defaultEnabledProductShortcutIds()) {
        if (!seen.contains(id))
            m_enabled << id;
    }
    for (const QString& id : gamewindow::TButtonBarController::productShortcutIds()) {
        if (seen.contains(id))
            continue;
        if (!gamewindow::TButtonBarController::defaultEnabledProductShortcutIds().contains(id))
            m_disabled << id;
    }
    Q_EMIT changed();
}

void TButtonBarOptionsPage::apply(QJsonObject& settings) const
{
    QJsonObject section = settings.value(QLatin1String(ControlButtonsSection)).toObject();
    section.insert(QLatin1String("enabledButtons"), QJsonArray::fromStringList(m_enabled));
    section.insert(QLatin1String("disabledButtons"), QJsonArray::fromStringList(m_disabled));
    settings.insert(QLatin1String(ControlButtonsSection), section);
}

QStringList TButtonBarOptionsPage::enabledButtons() const
{
    QStringList labels;
    for (const QString& id : m_enabled)
        labels << buttonLabel(id);
    return labels;
}

QStringList TButtonBarOptionsPage::disabledButtons() const
{
    QStringList labels;
    for (const QString& id : m_disabled)
        labels << buttonLabel(id);
    return labels;
}

void TButtonBarOptionsPage::enableButton(const QString& ButtonName)
{
    if (ButtonName.isEmpty())
        return;
    const QString id = buttonId(ButtonName);
    m_disabled.removeAll(id);
    m_enabled << id;
    Q_EMIT changed();
}

void TButtonBarOptionsPage::disableButton(const QString& ButtonName)
{
    if (ButtonName.isEmpty())
        return;
    const QString id = buttonId(ButtonName);
    m_enabled.removeAll(id);
    m_disabled << id;
    Q_EMIT changed();
}

void TButtonBarOptionsPage::moveEnabledButtonUp(const QString& ButtonName)
{
    if (ButtonName.isEmpty())
        return;
    const qsizetype index = m_enabled.indexOf(buttonId(ButtonName));
    if (index <= 0)
        return;
    m_enabled.swapItemsAt(index, index - 1);
    Q_EMIT changed();
}

void TButtonBarOptionsPage::moveEnabledButtonDown(const QString& ButtonName)
{
    if (ButtonName.isEmpty())
        return;
    const qsizetype index = m_enabled.indexOf(buttonId(ButtonName));
    if (index < 0 || index + 1 >= m_enabled.size())
        return;
    m_enabled.swapItemsAt(index, index + 1);
    Q_EMIT changed();
}

void TSoundOptionsPage::refreshSoundDevices()
{
    const QString autoLabel = qtTrId("optionsmenu_sound_used_sound_device_auto");
    QStringList model{ autoLabel };
    if (auto* storage = sound::TSoundStorage::instance()) {
        m_soundEngineInitialized = storage->isEngineReady();
        model += storage->playbackDeviceNames();
    }
    if (m_soundDeviceListModel != model)
        m_soundDeviceListModel = std::move(model);
    if (!m_soundDevice.isEmpty() && !m_soundDeviceListModel.contains(m_soundDevice))
        m_soundDevice = autoLabel;
}

TSoundOptionsPage::TSoundOptionsPage(QObject* parent)
    : TBoundOptionsPage(SoundBindings, parent)
{
    m_soundDevice = qtTrId("optionsmenu_sound_used_sound_device_auto");
    refreshSoundDevices();
}

void TSoundOptionsPage::load(const QJsonObject& settings)
{
    TBoundOptionsPage::load(settings);
    const QString device = optionValue(settings, O::usedSoundDevice).toString();
    m_soundDevice = device.isEmpty() || device.compare(QLatin1String("auto"), Qt::CaseInsensitive) == 0
        ? qtTrId("optionsmenu_sound_used_sound_device_auto")
        : device;
    refreshSoundDevices();
    Q_EMIT changed();
}

void TSoundOptionsPage::apply(QJsonObject& settings) const
{
    TBoundOptionsPage::apply(settings);
    if (m_masterVolume != 0)
        setOptionValue(settings, O::soundMasterVolumeOld, static_cast<int>(m_masterVolume));
    const bool automatic = m_soundDevice.isEmpty() || m_soundDevice == qtTrId("optionsmenu_sound_used_sound_device_auto");
    setOptionValue(settings, O::usedSoundDevice, automatic ? QStringLiteral("auto") : m_soundDevice);
}

bool TSoundOptionsPage::soundEnabled() const
{
    return m_soundEngineInitialized && m_masterVolume != 0;
}

namespace {

QString actionDescription(const QString& action)
{
    if (action.startsWith(QLatin1String("TriggerActionButton_"))) {
        const auto parts = action.mid(QStringLiteral("TriggerActionButton_").size()).split(QLatin1Char('.'));
        if (parts.size() == 2)
            return qtTrId("actionbar_button_identifier").arg(parts[0]).arg(parts[1]);
    }
    for (const auto& product : input::TKeyboardShortcutHandler::productActions()) {
        if (product.name == action)
            return product.description.isEmpty() ? QStringLiteral("Misc.: %1").arg(action) : product.description;
    }
    const QString id = QStringLiteral("label_gameactionidentifier_%1").arg(action);
    const QString translated = qtTrId(id.toUtf8().constData());
    return translated == id ? QStringLiteral("Misc.: %1").arg(action) : translated;
}

/// CIP TTibiaHotkeyActionSettingQmlWrapper row for OptionsPageHotkeys.qml.
class THotkeyActionRow final : public cip::tibia::config::TTibiaHotkeyActionSettingQmlWrapper
{
public:
    THotkeysOptionsPage::EHotkeyMode mode = THotkeysOptionsPage::DefaultHotkeys;
    TActionSetting setting;
    QKeySequence first;
    QKeySequence second;
    bool matchesFilter = true;

    explicit THotkeyActionRow(QObject* parent = nullptr)
        : cip::tibia::config::TTibiaHotkeyActionSettingQmlWrapper(parent)
    {
        m_isStaticAction = true;
        m_descriptionColor = QColor(QStringLiteral("#c0c0c0"));
    }

    void sync()
    {
        m_firstHotkeyText = first.toString(QKeySequence::NativeText);
        m_secondHotkeyText = second.toString(QKeySequence::NativeText);
        m_isEmpty = setting.isEmpty();
        m_isChatText = setting.kind == TActionSetting::TEXT;
        m_isUseObject = setting.kind == TActionSetting::OBJECT;
        m_isUseWithObject = setting.kind == TActionSetting::OBJECT
            && (setting.useType == EUseType::UseOnTarget || setting.useType == EUseType::SelectUseTarget
                || setting.useType == EUseType::UseAtMouseCursor);
        m_isStaticAction = setting.kind == TActionSetting::ACTION || setting.kind == TActionSetting::NONE;
        m_chatText = setting.text;
        m_sendAutomatically = setting.sendAutomatically;
        m_objectID = uint(setting.objectId);
        m_useObjectType = int(setting.useType);
        m_smartModeEnabled = setting.useEquipSmartMode;
        m_canHaveSmartMode = setting.useType == EUseType::Equip;
        if (setting.kind == TActionSetting::ACTION)
            m_descriptionText = actionDescription(setting.action);
        else if (setting.kind == TActionSetting::TEXT)
            m_descriptionText = setting.text;
        else if (setting.kind == TActionSetting::OBJECT)
            m_descriptionText = QStringLiteral("Object %1").arg(setting.objectId);
        else
            m_descriptionText = actionDescription(setting.action);
        Q_EMIT changed();
    }
};

} // namespace

THotkeysOptionsPage::THotkeysOptionsPage(QObject* parent)
    : cip::tibia::config::THotkeysQMLOptionsPage(parent)
{
}

void THotkeysOptionsPage::setHotkeyMode(const EHotkeyMode mode)
{
    if (m_hotkeyMode == mode)
        return;
    m_hotkeyMode = mode;
    Q_EMIT hotkeyModeChanged();
    updateHotkeyListModel();
}

void THotkeysOptionsPage::setAutoSwitchHotkeyPreset(const bool v)
{
    if (m_autoSwitchHotkeyPreset == v)
        return;
    m_autoSwitchHotkeyPreset = v;
    m_options.autoSwitchHotkeyPreset = v;
    Q_EMIT autoswitchHotkeyPresetChanged();
}

void THotkeysOptionsPage::setChatModeOn(const bool v)
{
    if (m_chatModeOn == v)
        return;
    m_chatModeOn = v;
    Q_EMIT hotkeySetsChanged();
    rebuildModel();
}

void THotkeysOptionsPage::setHotkeySetNamesIndex(const int v)
{
    if (m_hotkeySetNamesIndex == v || v < 0 || v >= m_hotkeySetNamesModel.size())
        return;
    m_hotkeySetNamesIndex = v;
    m_options.setCurrentSet(m_hotkeySetNamesModel.at(v));
    Q_EMIT hotkeySetsChanged();
    rebuildModel();
}

void THotkeysOptionsPage::setNameFilter(const QString& v)
{
    if (m_nameFilter == v)
        return;
    m_nameFilter = v;
    updateHotkeyListModel();
}

void THotkeysOptionsPage::refreshSetNames()
{
    m_hotkeySetNamesModel.clear();
    for (const auto& [name, set] : m_options.hotkeySets)
        m_hotkeySetNamesModel << name;
    m_hotkeySetNamesModel.sort(Qt::CaseInsensitive);
    m_hotkeySetNamesIndex = std::max(0, int(m_hotkeySetNamesModel.indexOf(currentSetName())));
}

void THotkeysOptionsPage::load(const QJsonObject& settings)
{
    QJsonObject section = settings.value(QLatin1String("hotkeyOptions")).toObject();
    if (section.value(QLatin1String("hotkeySets")).toObject().isEmpty()) {
        section = THotkeyOptions::builtInSection();
        if (section.value(QLatin1String("hotkeySets")).toObject().isEmpty())
            section = THotkeyOptions::instance()->toJson();
    }
    m_options = THotkeyOptionsData::fromJson(section);
    m_autoSwitchHotkeyPreset = m_options.autoSwitchHotkeyPreset;
    refreshSetNames();
    m_editRow = nullptr;
    Q_EMIT autoswitchHotkeyPresetChanged();
    Q_EMIT hotkeySetsChanged();
    rebuildModel();
}

void THotkeysOptionsPage::apply(QJsonObject& settings) const
{
    auto* self = const_cast<THotkeysOptionsPage*>(this);
    self->commitRows(false);
    const QJsonObject section = m_options.toJson();
    settings.insert(QStringLiteral("hotkeyOptions"), section);
    // The live hotkeys follow (CIP hands the options to TClientOptions).
    THotkeyOptions::instance()->replaceAll(section);
    self->rebuildModel();
}

void THotkeysOptionsPage::commitRows(const bool keepUnboundRows)
{
    const QString name = currentSetName();
    m_options.clearBindings(name, m_chatModeOn);
    for (QObject* object : std::as_const(m_rows)) {
        const auto* row = static_cast<THotkeyActionRow*>(object);
        if (row->setting.isEmpty())
            continue;
        if (!row->first.isEmpty())
            m_options.addBinding(name, m_chatModeOn, row->first, row->setting);
        if (!row->second.isEmpty())
            m_options.addBinding(name, m_chatModeOn, row->second, row->setting);
        else if (row->first.isEmpty() && keepUnboundRows)
            m_options.addBinding(name, m_chatModeOn, QKeySequence(), row->setting);
    }
}

void THotkeysOptionsPage::rebuildModel()
{
    m_hotkeyModel.clear();
    qDeleteAll(m_rows);
    m_rows.clear();
    m_editRow = nullptr;

    // The keys of each action, two per row; a custom setting's keys by setting.
    QHash<QString, QList<QKeySequence>> keysByAction;
    QStringList actionOrder;
    std::vector<std::pair<TActionSetting, QList<QKeySequence>>> customs;
    if (const THotkeySet* set = m_options.set(currentSetName())) {
        for (const THotkeyBinding& binding : m_chatModeOn ? set->chatOn : set->chatOff) {
            if (binding.setting.kind == TActionSetting::ACTION) {
                if (!keysByAction.contains(binding.setting.action))
                    actionOrder << binding.setting.action;
                if (!binding.keys.isEmpty())
                    keysByAction[binding.setting.action] << binding.keys;
                continue;
            }
            auto it = std::find_if(customs.begin(), customs.end(), [&](const auto& entry) { return entry.first == binding.setting; });
            if (it == customs.end())
                it = customs.insert(customs.end(), { binding.setting, {} });
            if (!binding.keys.isEmpty())
                it->second << binding.keys;
        }
    }

    const auto addRow = [this](const EHotkeyMode mode, const TActionSetting& setting, const QList<QKeySequence>& keys) {
        auto* row = new THotkeyActionRow(this);
        row->mode = mode;
        row->setting = setting;
        if (!keys.isEmpty())
            row->first = keys.at(0);
        if (keys.size() > 1)
            row->second = keys.at(1);
        row->sync();
        m_rows.append(row);
    };
    const auto actionSetting = [](const QString& name) {
        TActionSetting setting;
        setting.kind = TActionSetting::ACTION;
        setting.action = name;
        return setting;
    };

    QStringList names = input::TKeyboardShortcutHandler::staticActionNames();
    for (const auto& product : input::TKeyboardShortcutHandler::productActions())
        if (!names.contains(product.name))
            names << product.name;
    names.sort(Qt::CaseInsensitive);
    for (const QString& name : names) {
        QList<QKeySequence> keys = keysByAction.take(name);
        if (keys.isEmpty()) {
            for (const auto& product : input::TKeyboardShortcutHandler::productActions()) {
                if (product.name == name && !product.defaultKeys.isEmpty())
                    keys << product.defaultKeys;
            }
        }
        addRow(DefaultHotkeys, actionSetting(name), keys);
    }
    for (int bar = 1; bar <= 9; ++bar) {
        for (int button = 1; button <= gamewindow::TActionBarController::ButtonCount; ++button) {
            const QString name = THotkeyOptions::triggerActionButtonName(bar, button);
            addRow(ActionButtons, actionSetting(name), keysByAction.take(name));
        }
    }
    for (const QString& name : std::as_const(actionOrder)) {
        if (keysByAction.contains(name))
            addRow(CustomHotkeys, actionSetting(name), keysByAction.take(name));
    }
    for (const auto& [setting, keys] : customs)
        addRow(CustomHotkeys, setting, keys);

    updateHotkeyListModel();
}

void THotkeysOptionsPage::updateHotkeyListModel()
{
    m_hotkeyModel.clear();
    const QString filter = m_nameFilter.trimmed();
    for (QObject* object : std::as_const(m_rows)) {
        auto* row = static_cast<THotkeyActionRow*>(object);
        row->matchesFilter = filter.isEmpty() || row->descriptionText().contains(filter, Qt::CaseInsensitive)
                          || row->firstHotkeyText().contains(filter, Qt::CaseInsensitive)
                          || row->secondHotkeyText().contains(filter, Qt::CaseInsensitive);
        if (row->mode == m_hotkeyMode && row->matchesFilter)
            m_hotkeyModel.append(row);
    }
    Q_EMIT hotkeyListModelChanged();
}

void THotkeysOptionsPage::openPresetDialog(const EPresetTextMode mode, const QString& caption, const QString& description, const QString& text)
{
    m_presetTextMode = mode;
    auto* prompt = new gamewindow::TGenericEnterTextDialogController(this);
    prompt->setCaption(caption);
    prompt->setDescription(description);
    prompt->setMaxInputLength(29);
    prompt->setText(text);
    connect(prompt, &gamewindow::TGenericEnterTextDialogController::textEntered, this, &THotkeysOptionsPage::onPresetTextEntered);
    prompt->dialog()->show();
}

void THotkeysOptionsPage::requestAddPreset()
{
    openPresetDialog(EPresetTextMode::Add, qtTrId("add_hokey_preset_caption"), qtTrId("new_hokey_preset_name"), QString());
}

void THotkeysOptionsPage::requestRenameCurrentPreset()
{
    openPresetDialog(EPresetTextMode::Rename, qtTrId("rename_hokey_preset_caption"), qtTrId("rename_hokey_preset_text"), currentSetName());
}

void THotkeysOptionsPage::requestCloneCurrentPreset()
{
    openPresetDialog(EPresetTextMode::Clone, qtTrId("copy_hokey_preset_caption"), qtTrId("new_hokey_preset_name"), QString());
}

void THotkeysOptionsPage::onPresetTextEntered(const QString& EnteredText)
{
    if (EnteredText.isEmpty())
        return;
    const QString current = currentSetName();
    switch (m_presetTextMode) {
    case EPresetTextMode::Rename:
        m_options.renameSet(current, EnteredText);
        m_options.setCurrentSet(EnteredText);
        refreshSetNames();
        Q_EMIT hotkeySetsChanged();
        return;
    case EPresetTextMode::Clone: {
        const THotkeySet* source = m_options.set(current);
        m_options.addSet(EnteredText, source ? *source : THotkeySet{});
        break;
    }
    case EPresetTextMode::Add:
        m_options.addSet(EnteredText, THotkeyOptions::builtInSet(QStringLiteral("GlobalShortcuts")));
        break;
    case EPresetTextMode::None:
        return;
    }
    m_options.setCurrentSet(EnteredText);
    refreshSetNames();
    Q_EMIT hotkeySetsChanged();
    rebuildModel();
}

void THotkeysOptionsPage::requestRemoveCurrentPreset()
{
    auto* message = new gamewindow::TMessageDialogController(this);
    message->setCloseOnButtonClicked(true);
    using Button = gamewindow::TMessageDialogController::Button;
    message->setButtons({ new gamewindow::TMessageDialogButton(qtTrId("ok"), Button::OK, QKeySequence(QStringLiteral("Return"))),
                          new gamewindow::TMessageDialogButton(qtTrId("cancel"), Button::CANCEL, QKeySequence(QStringLiteral("Esc"))) });
    connect(message, &gamewindow::TMessageDialogController::acceptClicked, this, &THotkeysOptionsPage::onMessageDialogConfirmed);
    message->showMessage(qtTrId("delete_hokey_caption"), qtTrId("delete_hokey_text").arg(currentSetName()));
}

void THotkeysOptionsPage::onMessageDialogConfirmed()
{
    m_options.removeSet(currentSetName());
    refreshSetNames();
    if (!m_hotkeySetNamesModel.isEmpty()) {
        m_hotkeySetNamesIndex = 0;
        m_options.setCurrentSet(m_hotkeySetNamesModel.first());
    }
    Q_EMIT hotkeySetsChanged();
    rebuildModel();
}

void THotkeysOptionsPage::resetToDefault()
{
    static const QStringList Vocations{ QStringLiteral("Knight"), QStringLiteral("Paladin"), QStringLiteral("Sorcerer"), QStringLiteral("Druid"),
                                        QStringLiteral("Monk") };
    const QString name = currentSetName();
    const THotkeySet defaults = THotkeyOptions::builtInSet(Vocations.contains(name) ? name : QStringLiteral("GlobalShortcuts"));
    m_options.setBindings(name, m_chatModeOn, m_chatModeOn ? defaults.chatOn : defaults.chatOff);
    rebuildModel();
}

void THotkeysOptionsPage::requestEditActionForHotkey(const int HotkeyActionSettingIndex)
{
    if (HotkeyActionSettingIndex < 0 || HotkeyActionSettingIndex >= m_hotkeyModel.size())
        return;
    auto* row = static_cast<THotkeyActionRow*>(m_hotkeyModel.at(HotkeyActionSettingIndex));
    if (!row || row->setting.kind != TActionSetting::TEXT)
        return;
    auto* prompt = new gamewindow::TGenericEnterTextDialogController(this);
    prompt->setCaption(qtTrId("optionsmenu_individual_hotkeys"));
    prompt->setText(row->setting.text);
    prompt->setMaxInputLength(255);
    const bool sendAutomatically = row->setting.sendAutomatically;
    connect(prompt, &gamewindow::TGenericEnterTextDialogController::textEntered, this, [this, HotkeyActionSettingIndex, sendAutomatically](const QString& entered) {
        onTextAssignedToHotkeyAction(HotkeyActionSettingIndex, -1, entered, sendAutomatically);
    });
    prompt->dialog()->show();
}

void THotkeysOptionsPage::requestAddNewCustomHotkey()
{
    auto* prompt = new gamewindow::TGenericEnterTextDialogController(this);
    prompt->setCaption(qtTrId("optionsmenu_individual_hotkeys"));
    prompt->setMaxInputLength(255);
    connect(prompt, &gamewindow::TGenericEnterTextDialogController::textEntered, this, [this](const QString& entered) {
        if (entered.trimmed().isEmpty())
            return;
        setHotkeyMode(CustomHotkeys);
        onTextAssignedToHotkeyAction(-1, -1, entered, false);
    });
    prompt->dialog()->show();
}

void THotkeysOptionsPage::onTextAssignedToHotkeyAction(const int HotkeyActionSettingIndex, int, const QString& Text, const bool SendAutomatically)
{
    THotkeyActionRow* row = nullptr;
    const bool added = HotkeyActionSettingIndex == -1 || HotkeyActionSettingIndex >= m_hotkeyModel.size();
    if (added) {
        row = new THotkeyActionRow(this);
        row->mode = CustomHotkeys;
        m_rows.append(row);
    } else {
        row = static_cast<THotkeyActionRow*>(m_hotkeyModel.at(HotkeyActionSettingIndex));
    }
    TActionSetting setting;
    setting.kind = TActionSetting::TEXT;
    setting.text = Text;
    setting.sendAutomatically = SendAutomatically;
    row->setting = setting;
    row->sync();
    commitRows(true);
    if (added)
        rebuildModel();
}

void THotkeysOptionsPage::requestEnterNewHotkeyForAction(const int HotkeyActionSettingIndex, const int KeySequenceIndex)
{
    if (HotkeyActionSettingIndex < 0 || HotkeyActionSettingIndex >= m_hotkeyModel.size())
        return;
    auto* row = static_cast<THotkeyActionRow*>(m_hotkeyModel.at(HotkeyActionSettingIndex));
    if (!row || row->setting.isEmpty())
        return;
    m_editRow = row;
    m_editKey = KeySequenceIndex;
    const QKeySequence current = KeySequenceIndex == 0 ? row->first : row->second;
    const QString actionName = row->setting.kind == TActionSetting::ACTION ? row->setting.action : QString();
    auto* dialog = new gamewindow::TRecordKeyboardShortcutDialogController(row->descriptionText(), actionName, m_chatModeOn, current, this);
    connect(dialog, &gamewindow::TRecordKeyboardShortcutDialogController::keySequenceSelected, this,
            &THotkeysOptionsPage::onNewKeySequenceSelectedForCurrentHotkeyActionSetting);
    dialog->dialog()->show();
}

void THotkeysOptionsPage::onNewKeySequenceSelectedForCurrentHotkeyActionSetting(const QKeySequence& NewKeySequence)
{
    auto* edited = static_cast<THotkeyActionRow*>(m_editRow);
    if (!edited || !m_rows.contains(edited))
        return;
    // A key runs one action per chat mode: other rows lose it.
    if (!NewKeySequence.isEmpty()) {
        for (QObject* object : std::as_const(m_rows)) {
            auto* row = static_cast<THotkeyActionRow*>(object);
            bool changed = false;
            if (row->first == NewKeySequence) {
                row->first = QKeySequence();
                changed = true;
            }
            if (row->second == NewKeySequence) {
                row->second = QKeySequence();
                changed = true;
            }
            if (changed)
                row->sync();
        }
    }
    if (m_editKey == 0)
        edited->first = NewKeySequence;
    else if (m_editKey == 1)
        edited->second = NewKeySequence;
    else
        return;
    edited->sync();
    commitRows(true);
}

std::span<const TOptionBinding> actionBarsBindings() { return ActionBarsBindings; }
std::span<const TOptionBinding> consoleBindings() { return ConsoleBindings; }
std::span<const TOptionBinding> miscBindings() { return MiscBindings; }
std::span<const TOptionBinding> gameplayBindings() { return GameplayBindings; }
std::span<const TOptionBinding> screenshotsBindings() { return ScreenshotsBindings; }
std::span<const TOptionBinding> battleSoundsBindings() { return BattleSoundsBindings; }
std::span<const TOptionBinding> uiSoundsBindings() { return UISoundsBindings; }

}
