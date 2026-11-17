#include "optionspages.h"

#include "app/bootstrap.h"
#include "config/clientoptiondefaults.h"
#include "config/clientoptions.h"
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
#include <stdexcept>
#include <utility>

namespace tibia::config {

namespace {

using O = cip::EClientOption;
using B = TOptionBinding;

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

/// The renderers in the list order.
constexpr int RendererOrder[] = { 0, 6, 10, 7, 9, 8, SoftwareRenderer };
/// The highest renderer id a name is matched against.
constexpr int MaxRenderer = 10;

constexpr int AntialiasingModeCount = 3;
constexpr int ColorizeLootStyleCount = 3;

QString colorizeLootStyleName(const int style)
{
    switch (style) {
    case 0:
        return qtTrId("colorizelootstyle_none");
    case 1:
        return qtTrId("colorizelootstyle_normal");
    case 2:
        return qtTrId("colorizelootstyle_compact");
    default:
        return qtTrId("dummy_unknown");
    }
}

QString antialiasingModeName(const int mode)
{
    switch (mode) {
    case 0:
        return qtTrId("antialiasingmode_none");
    case 1:
        return qtTrId("antialiasingmode_antialiasing");
    case 2:
        return qtTrId("antialiasingmode_retro");
    default:
        return qtTrId("dummy_unknown");
    }
}

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
    // The locks have no page property; the page keeps them as dynamic ones.
    { "actionBarBottomLocked", O::actionBarBottomLocked, B::Bool },
    { "actionBarLeftLocked", O::actionBarLeftLocked, B::Bool },
    { "actionBarRightLocked", O::actionBarRightLocked, B::Bool },
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

}

void loadBindings(QObject* page, const std::span<const TOptionBinding> bindings, const TClientOptions& options)
{
    for (const TOptionBinding& binding : bindings) {
        if (!options.hasOption(binding.option))
            continue;
        QVariant property;
        switch (binding.kind) {
        case B::Bool:
            property = options.boolOption(binding.option);
            break;
        case B::InvertedBool:
            property = !options.boolOption(binding.option);
            break;
        case B::Int:
            property = static_cast<uint>(std::max(0, options.intOption(binding.option)));
            break;
        case B::Real:
            property = static_cast<double>(options.floatOption(binding.option));
            break;
        case B::Percent:
            property = percentOf(options.floatOption(binding.option));
            break;
        case B::CloudsIndex:
            property = cloudsIndexOf(options.floatOption(binding.option));
            break;
        }
        page->setProperty(binding.property, property);
    }
}

void applyBindings(const QObject* page, const std::span<const TOptionBinding> bindings, TClientOptions& options)
{
    for (const TOptionBinding& binding : bindings) {
        if (!options.hasOption(binding.option))
            continue;
        const QVariant property = page->property(binding.property);
        switch (binding.kind) {
        case B::Bool:
            options.setBoolOption(binding.option, property.toBool());
            break;
        case B::InvertedBool:
            options.setBoolOption(binding.option, !property.toBool());
            break;
        case B::Int:
            options.setIntOption(binding.option, static_cast<int>(property.toUInt()));
            break;
        case B::Real:
            options.setFloatOption(binding.option, static_cast<float>(property.toDouble()));
            break;
        case B::Percent:
            options.setFloatOption(binding.option, fractionOf(property.toUInt()));
            break;
        case B::CloudsIndex:
            options.setFloatOption(binding.option, static_cast<float>(property.toUInt()) * -0.005f + 1.0f);
            break;
        }
    }
}

TInterfaceOptionsPage::TInterfaceOptionsPage(QObject* parent)
    : TBoundOptionsPage(InterfaceBindings, parent)
{
}

uint TInterfaceOptionsPage::colorizeLootStyleIndex() const
{
    // m_colorizeLootStyleIndex holds the style.
    const qsizetype index = colorizeLootStyleListModel().indexOf(colorizeLootStyleName(static_cast<int>(m_colorizeLootStyleIndex)));
    return index < 0 ? 0u : static_cast<uint>(index);
}

void TInterfaceOptionsPage::setColorizeLootStyleIndex(const uint index)
{
    const QStringList styles = colorizeLootStyleListModel();
    uint style = 0;
    if (index < static_cast<uint>(styles.size())) {
        for (int candidate = 0; candidate < ColorizeLootStyleCount; ++candidate) {
            if (styles.at(index) == colorizeLootStyleName(candidate)) {
                style = static_cast<uint>(candidate);
                break;
            }
        }
    }
    if (m_colorizeLootStyleIndex == style)
        return;
    m_colorizeLootStyleIndex = style;
    Q_EMIT changed();
}

QStringList TInterfaceOptionsPage::colorizeLootStyleListModel() const
{
    QStringList styles;
    for (int style = 0; style < ColorizeLootStyleCount; ++style)
        styles << colorizeLootStyleName(style);
    return styles;
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

void THudOptionsPage::load(const TClientOptions& options)
{
    TBoundOptionsPage::load(options);
    const QJsonObject& section = options.specialConditionsOptions();
    m_specialConditions = !section.isEmpty() ? SpecialConditionsOptions::fromJson(section) : SpecialConditionsOptions::defaults();
    m_specialConditionsShowInHUDEnabled = m_specialConditions.showInHUD;
    m_specialConditionsShowInBarEnabled = m_specialConditions.showInBar;
    Q_EMIT changed();
    rebuildSpecialConditionsModel();
}

void THudOptionsPage::apply(TClientOptions& options) const
{
    TBoundOptionsPage::apply(options);
    SpecialConditionsOptions conditions = m_specialConditions;
    conditions.showInHUD = m_specialConditionsShowInHUDEnabled;
    conditions.showInBar = m_specialConditionsShowInBarEnabled;
    options.setSpecialConditionsOptions(conditions.toJson());
}

void THudOptionsPage::setSpecialConditionShowInHUDEnabled(const int SpecialConditionIndex, const bool Enabled)
{
    if (SpecialConditionIndex < 0 || SpecialConditionIndex >= m_specialConditions.specialConditionsOrder.size())
        throw std::out_of_range("vector");
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
        throw std::out_of_range("vector");
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
    : TGraphicsQMLOptionsPage(parent)
{
    // product: allow toggling the CIP "no FPS limit" checkbox (was hard-disabled).
    m_canChangeFpsLimitCheckbox = !config::rendererLimitationsActive();
}

void TGraphicsOptionsPage::load(const TClientOptions& options)
{
    m_borderlessWindow = options.clientWindowOptions().borderless;
    m_scaleOnlyByEvenMultiples = options.boolOption(O::gameWindowScaleOnlyByEvenMultiples);
    m_vSync = options.boolOption(O::vSyncEnabled);
    m_frameRateLimitEnabled = options.boolOption(O::frameRateLimitEnabled);
    m_frameRateLimit = options.intOption(O::frameRateLimit);
    m_renderer = options.intOption(O::rendererIndex);
    m_antialiasingMode = options.antialiasingMode();
    Q_EMIT changed();
}

void TGraphicsOptionsPage::apply(TClientOptions& options) const
{
    TClientWindowOptions window = options.clientWindowOptions();
    window.borderless = m_borderlessWindow;
    options.setClientWindowOptions(window);
    options.setBoolOption(O::gameWindowScaleOnlyByEvenMultiples, m_scaleOnlyByEvenMultiples);
    options.setIntOption(O::antialiasingMode, m_antialiasingMode);
    options.setBoolOption(O::vSyncEnabled, m_vSync);
    options.setBoolOption(O::frameRateLimitEnabled, m_frameRateLimitEnabled);
    options.setIntOption(O::frameRateLimit, m_frameRateLimit);
    options.setRendererIndex(m_renderer);
    options.setAntialiasingMode(m_antialiasingMode);
}

uint TGraphicsOptionsPage::antialiasingModeIndex() const
{
    const qsizetype index = antialiasingModeListModel().indexOf(antialiasingModeName(m_antialiasingMode));
    return index < 0 ? 0u : static_cast<uint>(index);
}

void TGraphicsOptionsPage::setAntialiasingModeIndex(const uint index)
{
    const QStringList modes = antialiasingModeListModel();
    if (index < static_cast<uint>(modes.size())) {
        for (int mode = 0; mode < AntialiasingModeCount; ++mode) {
            if (modes.at(index) != antialiasingModeName(mode))
                continue;
            if (m_antialiasingMode != mode) {
                m_antialiasingMode = mode;
                Q_EMIT changed();
            }
            return;
        }
    }
    if (m_renderer != 0) {
        m_renderer = 0;
        Q_EMIT changed();
    }
}

QStringList TGraphicsOptionsPage::antialiasingModeListModel() const
{
    QStringList modes;
    for (int mode = 0; mode < AntialiasingModeCount; ++mode)
        modes << antialiasingModeName(mode);
    return modes;
}

bool TGraphicsOptionsPage::vSync() const
{
    return m_vSync && !config::rendererLimitationsActive();
}

void TGraphicsOptionsPage::setVSync(const bool enabled)
{
    if (m_vSync == enabled || config::rendererLimitationsActive())
        return;
    m_vSync = enabled;
    Q_EMIT changed();
}

bool TGraphicsOptionsPage::noFpsLimitEnabled() const
{
    return !m_frameRateLimitEnabled;
}

void TGraphicsOptionsPage::setNoFpsLimitEnabled(const bool enabled)
{
    if (m_frameRateLimitEnabled == !enabled)
        return;
    m_frameRateLimitEnabled = !enabled;
    Q_EMIT changed();
}

uint TGraphicsOptionsPage::rendererIndex() const
{
    const qsizetype index = rendererListModel().indexOf(rendererName(m_renderer));
    return index < 0 ? 0u : static_cast<uint>(index);
}

void TGraphicsOptionsPage::setRendererIndex(const uint index)
{
    const QStringList renderers = rendererListModel();
    int renderer = 0;
    if (index < static_cast<uint>(renderers.size())) {
        for (int id = 0; id <= MaxRenderer; ++id) {
            if (renderers.at(index) == rendererName(id)) {
                renderer = id;
                break;
            }
        }
    }
    if (m_renderer == renderer)
        return;
    m_renderer = renderer;
    Q_EMIT changed();
}

QStringList TGraphicsOptionsPage::rendererListModel() const
{
    QStringList renderers;
    for (const int renderer : RendererOrder)
        if (app::isRendererSupported(renderer))
            renderers << rendererName(renderer);
    return renderers;
}

QString TGraphicsOptionsPage::tooltipTextDisabledRenderers() const
{
    QString disabled;
    for (const int renderer : RendererOrder)
        if (renderer != SoftwareRenderer && !app::isRendererSupported(renderer))
            disabled += QLatin1Char('\n') + rendererName(renderer);
    QString tooltip;
    if (m_renderer != activeRenderer())
        tooltip += qtTrId("optionsmenu_engine_tooltip_auto").arg(rendererName(activeRenderer()));
    tooltip += (tooltip.isEmpty() ? QString() : QStringLiteral("\n\n")) + qtTrId("optionsmenu_engine_tooltip_help");
    if (!disabled.isEmpty())
        tooltip += (tooltip.isEmpty() ? QString() : QStringLiteral("\n\n")) + qtTrId("optionsmenu_engine_tooltip_disabled")
                   + QLatin1Char('\n') + disabled;
    return tooltip;
}

bool TGraphicsOptionsPage::rendererLimitationsActive() const
{
    return config::rendererLimitationsActive();
}

bool TGraphicsOptionsPage::isMetal() const
{
    return activeRenderer() == MetalRenderer;
}

TEffectsOptionsPage::TEffectsOptionsPage(QObject* parent)
    : TBoundOptionsPage(EffectsBindings, parent)
{
    m_rendererLimitationsActive = rendererLimitationsActive();
}

void TEffectsOptionsPage::load(const TClientOptions& options)
{
    TBoundOptionsPage::load(options);
    m_lightEffects = options.boolOption(O::lightEffectsEnabled);
    Q_EMIT changed();
}

void TEffectsOptionsPage::apply(TClientOptions& options) const
{
    TBoundOptionsPage::apply(options);
    options.setBoolOption(O::lightEffectsEnabled, m_lightEffects);
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
    : TControlsQMLOptionsPage(parent)
{
    m_controlSchemeListModel = controlSchemeListModel();
    m_lootSchemeListModel = lootSchemeListModel();
    m_defaultKeyboardDelayMillis = 250;
}

void TControlsOptionsPage::load(const TClientOptions& options)
{
    m_controlScheme = options.controlScheme();
    m_lootScheme = options.lootScheme();
    m_useDefaultKeyboardDelay = options.boolOption(O::keyboardDelayUseDefault);
    m_keyboardDelayMillis = static_cast<uint>(options.intOption(O::keyboardDelayMs));
    m_rotateWithAlt = options.boolOption(O::rotateWithAlt);
    m_rotateWithShift = options.boolOption(O::rotateWithShift);
    m_rotateWithCtrl = options.boolOption(O::rotateWithCtrl);
    m_dragAndDropDefaultActionIsMoveAll = options.boolOption(O::dragAndDropDefaultActionIsMoveAll);
    m_alwaysTurnTowardsMoveDirection = options.boolOption(O::alwaysTurnTowardsMoveDirection);
    Q_EMIT changed();
}

void TControlsOptionsPage::apply(TClientOptions& options) const
{
    options.setControlScheme(m_controlScheme);
    options.setLootScheme(m_lootScheme);
    options.setBoolOption(O::keyboardDelayUseDefault, m_useDefaultKeyboardDelay);
    options.setIntOption(O::keyboardDelayMs, static_cast<int>(m_keyboardDelayMillis));
    options.setBoolOption(O::rotateWithAlt, m_rotateWithAlt);
    options.setBoolOption(O::rotateWithShift, m_rotateWithShift);
    options.setBoolOption(O::rotateWithCtrl, m_rotateWithCtrl);
    options.setBoolOption(O::alwaysTurnTowardsMoveDirection, m_alwaysTurnTowardsMoveDirection);
    options.setBoolOption(O::dragAndDropDefaultActionIsMoveAll, m_dragAndDropDefaultActionIsMoveAll);
}

uint TControlsOptionsPage::controlSchemeIndex() const
{
    return static_cast<uint>(controlSchemeListModelIndex(m_controlScheme));
}

void TControlsOptionsPage::setControlSchemeIndex(const uint index)
{
    const int scheme = controlSchemeForListModelIndex(static_cast<int>(index));
    if (m_controlScheme == scheme)
        return;
    m_controlScheme = scheme;
    Q_EMIT changed();
}

uint TControlsOptionsPage::lootSchemeIndex() const
{
    return static_cast<uint>(lootSchemeListModelIndex(m_lootScheme));
}

void TControlsOptionsPage::setLootSchemeIndex(const uint index)
{
    const int scheme = lootSchemeForListModelIndex(static_cast<int>(index));
    if (m_lootScheme == scheme)
        return;
    m_lootScheme = scheme;
    Q_EMIT changed();
}

bool TControlsOptionsPage::ctrlToDragStacks() const
{
    return !m_dragAndDropDefaultActionIsMoveAll;
}

void TControlsOptionsPage::setCtrlToDragStacks(const bool enabled)
{
    if (m_dragAndDropDefaultActionIsMoveAll == !enabled)
        return;
    m_dragAndDropDefaultActionIsMoveAll = !enabled;
    Q_EMIT changed();
}

TButtonBarOptionsPage::TButtonBarOptionsPage(QObject* parent)
    : TBoundOptionsPage({}, parent)
{
}

void TButtonBarOptionsPage::load(const TClientOptions& options)
{
    const QJsonObject& section = options.controlButtonsOptions();
    if (!section.isEmpty()) {
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
    sortDisabledButtons();
    Q_EMIT changed();
}

void TButtonBarOptionsPage::sortDisabledButtons()
{
    // Button type order; pack shortcuts (no type) after them, as they were.
    const auto typeOf = [](const QString& id) {
        for (size_t type = 0; type < std::size(ButtonBarButtons); ++type)
            if (id == QLatin1String(ButtonBarButtons[type].id))
                return static_cast<int>(type);
        return static_cast<int>(std::size(ButtonBarButtons));
    };
    std::stable_sort(m_disabled.begin(), m_disabled.end(),
                     [&typeOf](const QString& a, const QString& b) { return typeOf(a) < typeOf(b); });
    m_disabled.erase(std::unique(m_disabled.begin(), m_disabled.end()), m_disabled.end());
}

void TButtonBarOptionsPage::apply(TClientOptions& options) const
{
    QJsonObject section = options.controlButtonsOptions();
    section.insert(QLatin1String("enabledButtons"), QJsonArray::fromStringList(m_enabled));
    section.insert(QLatin1String("disabledButtons"), QJsonArray::fromStringList(m_disabled));
    options.setControlButtonsOptions(section);
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
    sortDisabledButtons();
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

void TSoundOptionsPage::load(const TClientOptions& options)
{
    TBoundOptionsPage::load(options);
    const QString device = options.hasOption(O::usedSoundDevice) ? options.stringOption(O::usedSoundDevice) : QString();
    m_soundDevice = device.isEmpty() || device.compare(QLatin1String("auto"), Qt::CaseInsensitive) == 0
        ? qtTrId("optionsmenu_sound_used_sound_device_auto")
        : device;
    refreshSoundDevices();
    Q_EMIT changed();
}

void TSoundOptionsPage::apply(TClientOptions& options) const
{
    TBoundOptionsPage::apply(options);
    if (m_masterVolume != 0 && options.hasOption(O::soundMasterVolumeOld))
        options.setIntOption(O::soundMasterVolumeOld, static_cast<int>(m_masterVolume));
    const bool automatic = m_soundDevice.isEmpty() || m_soundDevice == qtTrId("optionsmenu_sound_used_sound_device_auto");
    if (options.hasOption(O::usedSoundDevice))
        options.setStringOption(O::usedSoundDevice, automatic ? QStringLiteral("auto") : m_soundDevice);
}

bool TSoundOptionsPage::soundEnabled() const
{
    return m_soundEngineInitialized && m_masterVolume != 0;
}

namespace {

/// CIP: a setting's text in the hotkeys page. A chat text shows itself; an
/// empty setting "hotkey_empty"; a static action its translated identifier
/// (its name when untranslated), an action button its bar's side, bar and
/// button; an object its use type. qtclient: a pack action its description.
QString hotkeyActionSettingDescription(const TActionSetting& setting)
{
    if (!setting.text.isEmpty())
        return setting.text;
    if (setting.action.isEmpty() && setting.objectId == 0) {
        // CipSoft also gives an empty text when the setting's +0x47 flag is set
        // (not identified yet).
        if (!setting.multiActions.empty())
            return {};
        return qtTrId("hotkey_empty");
    }
    if (setting.objectId == 0) {
        for (const auto& product : input::TKeyboardShortcutHandler::productActions()) {
            if (product.name == setting.action)
                return product.description.isEmpty() ? setting.action : product.description;
        }
        const QString id = QStringLiteral("label_gameactionidentifier_") + setting.action;
        const input::IGameActionPtr action = input::TKeyboardShortcutHandler::staticAction(setting.action);
        if (!action)
            throw std::out_of_range("map::at:  key not found");
        QString text;
        if (const auto* button = input::gameActionAs<input::TGameActionActionButtonTriggered>(action)) {
            QString side;
            switch (gamewindow::TActionBarGroup::positionOfBar(button->actionBarId())) {
            case gamewindow::EActionBarPosition::ActionBarPositionBottom: side = qtTrId("bottom"); break;
            case gamewindow::EActionBarPosition::ActionBarPositionLeft: side = qtTrId("left"); break;
            case gamewindow::EActionBarPosition::ActionBarPositionRight: side = qtTrId("right"); break;
            }
            text = side + QLatin1Char(' ') + qtTrId("actionbar") + QStringLiteral(": ")
                 + qtTrId("actionbar_button_identifier")
                       .arg(button->actionBarId())
                       .arg(button->actionButtonId(), 2, 10, QLatin1Char('0'));
        } else {
            text = qtTrId(id.toLocal8Bit().constData());
        }
        return text == id ? setting.action : text;
    }
    switch (setting.useType) {
    case EUseType::Use: return qtTrId("hotkey_use_object");
    case EUseType::UseOnYourself: return qtTrId("hotkey_use_object_on_yourself");
    case EUseType::UseOnTarget: return qtTrId("hotkey_use_object_on_target");
    case EUseType::SelectUseTarget: return qtTrId("hotkey_use_object_with_crosshair");
    case EUseType::Equip: return qtTrId("hotkey_equip_object");
    case EUseType::UseAtMouseCursor: return qtTrId("hotkey_use_object_at_mousecursor");
    }
    return QStringLiteral("invalid use object type");
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
        m_descriptionText = hotkeyActionSettingDescription(setting);
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

void THotkeysOptionsPage::load(const TClientOptions& options)
{
    QJsonObject section = options.hotkeyOptions();
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

void THotkeysOptionsPage::apply(TClientOptions& options) const
{
    auto* self = const_cast<THotkeysOptionsPage*>(this);
    self->commitRows(false);
    const QJsonObject section = m_options.toJson();
    options.setHotkeyOptions(section);
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

namespace {

/// CIP's hotkeys page sort (sub_86af86e21bc5): custom settings first, by first
/// then second key (rows without keys last), then the action buttons, then the
/// other static actions, both by description.
bool hotkeyRowLess(const THotkeyActionRow* a, const THotkeyActionRow* b)
{
    const auto isButton = [](const THotkeyActionRow* row) {
        return row->setting.kind == TActionSetting::ACTION
            && input::gameActionAs<input::TGameActionActionButtonTriggered>(input::TKeyboardShortcutHandler::staticAction(row->setting.action));
    };
    const bool aStatic = a->setting.kind == TActionSetting::ACTION;
    const bool bStatic = b->setting.kind == TActionSetting::ACTION;
    const bool aButton = aStatic && isButton(a);
    const bool bButton = bStatic && isButton(b);
    const bool aPlain = aStatic && !aButton;
    const bool bPlain = bStatic && !bButton;
    if ((!aButton || !bButton) == (aPlain && bPlain))
        return QString::compare(a->descriptionText(), b->descriptionText(), Qt::CaseSensitive) < 0;
    if (aPlain && (!bPlain || bButton))
        return false;
    if ((!aPlain || aButton) && bPlain)
        return true;
    if (!(bStatic || bButton || !aButton))
        return false;
    if (!(aStatic || aButton || !bButton))
        return true;
    if (a->first == b->first)
        return a->second < b->second;
    if (a->first.isEmpty())
        return false;
    if (b->first.isEmpty())
        return true;
    return a->first < b->first;
}

}

/// CIP sub_b4d112292d2a / sub_4ca9c7597cbe: a row per static action (its first
/// two keys of the current set and chat mode), then a row per other setting
/// (a second binding of the same setting gives it its second key), sorted.
void THotkeysOptionsPage::rebuildModel()
{
    m_hotkeyModel.clear();
    qDeleteAll(m_rows);
    m_rows.clear();
    m_editRow = nullptr;

    static const std::vector<THotkeyBinding> NoBindings;
    const THotkeySet* set = m_options.set(currentSetName());
    const std::vector<THotkeyBinding>& bindings = set ? (m_chatModeOn ? set->chatOn : set->chatOff) : NoBindings;

    std::vector<THotkeyActionRow*> rows;
    const auto addRow = [this, &rows](const EHotkeyMode mode, const TActionSetting& setting) {
        auto* row = new THotkeyActionRow(this);
        row->mode = mode;
        row->setting = setting;
        rows.push_back(row);
        return row;
    };

    // qtclient: the pack's product actions are static actions too.
    QStringList names = input::TKeyboardShortcutHandler::staticActionNames();
    for (int bar = 1; bar <= 9; ++bar)
        for (int button = 1; button <= gamewindow::TActionBarController::ButtonCount; ++button)
            names << THotkeyOptions::triggerActionButtonName(bar, button);
    for (const auto& product : input::TKeyboardShortcutHandler::productActions())
        if (!names.contains(product.name))
            names << product.name;
    for (const QString& name : std::as_const(names)) {
        TActionSetting setting;
        setting.kind = TActionSetting::ACTION;
        setting.action = name;
        const input::IGameActionPtr action = input::TKeyboardShortcutHandler::staticAction(name);
        const bool button = input::gameActionAs<input::TGameActionActionButtonTriggered>(action) != nullptr;
        THotkeyActionRow* row = addRow(button ? ActionButtons : DefaultHotkeys, setting);
        for (const THotkeyBinding& binding : bindings) {
            if (!(binding.setting == setting))
                continue;
            if (row->first.isEmpty())
                row->first = binding.keys;
            else if (row->second.isEmpty())
                row->second = binding.keys;
        }
        // qtclient: an unbound pack action shows its default keys.
        if (row->first.isEmpty()) {
            for (const auto& product : input::TKeyboardShortcutHandler::productActions())
                if (product.name == name && !product.defaultKeys.isEmpty())
                    row->first = product.defaultKeys;
        }
    }
    const size_t staticRows = rows.size();
    for (const THotkeyBinding& binding : bindings) {
        if (binding.setting.kind == TActionSetting::ACTION)
            continue;
        const auto existing = std::find_if(rows.begin() + static_cast<std::ptrdiff_t>(staticRows), rows.end(),
                                           [&](const THotkeyActionRow* row) { return row->setting == binding.setting; });
        if (existing != rows.end()) {
            (*existing)->second = binding.keys;
            continue;
        }
        addRow(CustomHotkeys, binding.setting)->first = binding.keys;
    }
    for (THotkeyActionRow* row : rows)
        row->sync();
    std::sort(rows.begin(), rows.end(), hotkeyRowLess);
    for (THotkeyActionRow* row : rows)
        m_rows.append(row);

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
