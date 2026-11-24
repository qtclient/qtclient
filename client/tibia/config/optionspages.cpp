#include "optionspages.h"

#include "app/bootstrap.h"
#include "config/clientoptiondefaults.h"
#include "config/clientoptions.h"
#include "config/hotkeyoptions.h"
#include "config/specialconditionobject.h"
#include "config/tibiahotkeyactionsettingqmlwrapper.h"
#include "gamewindow/actionbarcontroller.h"
#include "gamewindow/assignactiondialogs.h"
#include "gamewindow/buttonbarcontroller.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/genericentertextdialogcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/keyboardshortcuthandler.h"
#include "spells/spellinfostorage.h"
#include "appearances/appearanceinstance.h"

#include <tibia/game/thingsession_forward.h>

#include "sound/soundengine.h"

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
    { "lightAttenuationCloudsIndoor", O::lightAttenuationCloudsIndoor, B::CloudsIndex },
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
    Q_EMIT changed();
    rebuildSpecialConditionsModel();
}

void THudOptionsPage::apply(TClientOptions& options) const
{
    TBoundOptionsPage::apply(options);
    options.setSpecialConditionsOptions(m_specialConditions.toJson());
}

bool THudOptionsPage::specialConditionsShowInHUDEnabled() const
{
    return m_specialConditions.showInHUD;
}

void THudOptionsPage::setSpecialConditionsShowInHUDEnabled(const bool enabled)
{
    if (m_specialConditions.showInHUD == enabled)
        return;
    m_specialConditions.showInHUD = enabled;
    Q_EMIT changed();
}

bool THudOptionsPage::specialConditionsShowInBarEnabled() const
{
    return m_specialConditions.showInBar;
}

void THudOptionsPage::setSpecialConditionsShowInBarEnabled(const bool enabled)
{
    if (m_specialConditions.showInBar == enabled)
        return;
    m_specialConditions.showInBar = enabled;
    Q_EMIT changed();
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
    for (const auto& [member, option] : { std::pair{ &TEffectsOptionsPage::m_ambientLightLevelFraction, O::lightAmbientLevel },
                                          std::pair{ &TEffectsOptionsPage::m_levelSeparatorLevelFraction, O::lightLevelSeparatorLevel },
                                          std::pair{ &TEffectsOptionsPage::m_ownEffectsOpacityFraction, O::ownEffectsOpacity },
                                          std::pair{ &TEffectsOptionsPage::m_otherPlayersEffectsOpacityFraction, O::otherPlayersEffectsOpacity },
                                          std::pair{ &TEffectsOptionsPage::m_monsterEffectsOpacityFraction, O::monsterEffectsOpacity },
                                          std::pair{ &TEffectsOptionsPage::m_monsterBossAreaEffectsOpacityFraction,
                                                     O::monsterBossAreaEffectsOpacity } }) {
        if (options.hasOption(option))
            this->*member = options.floatOption(option);
    }
    Q_EMIT changed();
}

void TEffectsOptionsPage::apply(TClientOptions& options) const
{
    TBoundOptionsPage::apply(options);
    options.setBoolOption(O::lightEffectsEnabled, m_lightEffects);
    for (const auto& [member, option] : { std::pair{ &TEffectsOptionsPage::m_ambientLightLevelFraction, O::lightAmbientLevel },
                                          std::pair{ &TEffectsOptionsPage::m_levelSeparatorLevelFraction, O::lightLevelSeparatorLevel },
                                          std::pair{ &TEffectsOptionsPage::m_ownEffectsOpacityFraction, O::ownEffectsOpacity },
                                          std::pair{ &TEffectsOptionsPage::m_otherPlayersEffectsOpacityFraction, O::otherPlayersEffectsOpacity },
                                          std::pair{ &TEffectsOptionsPage::m_monsterEffectsOpacityFraction, O::monsterEffectsOpacity },
                                          std::pair{ &TEffectsOptionsPage::m_monsterBossAreaEffectsOpacityFraction,
                                                     O::monsterBossAreaEffectsOpacity } }) {
        if (options.hasOption(option))
            options.setFloatOption(option, this->*member);
    }
}

void TEffectsOptionsPage::setFraction(float& fraction, const uint percent)
{
    if (percentOf(fraction) == percent)
        return;
    fraction = fractionOf(percent);
    Q_EMIT changed();
}

uint TEffectsOptionsPage::ambientLightLevel() const
{
    return percentOf(m_ambientLightLevelFraction);
}

void TEffectsOptionsPage::setAmbientLightLevel(const uint percent)
{
    setFraction(m_ambientLightLevelFraction, percent);
}

uint TEffectsOptionsPage::levelSeparatorLevel() const
{
    return percentOf(m_levelSeparatorLevelFraction);
}

void TEffectsOptionsPage::setLevelSeparatorLevel(const uint percent)
{
    setFraction(m_levelSeparatorLevelFraction, percent);
}

uint TEffectsOptionsPage::ownEffectsOpacity() const
{
    return percentOf(m_ownEffectsOpacityFraction);
}

void TEffectsOptionsPage::setOwnEffectsOpacity(const uint percent)
{
    setFraction(m_ownEffectsOpacityFraction, percent);
}

uint TEffectsOptionsPage::otherPlayersEffectsOpacity() const
{
    return percentOf(m_otherPlayersEffectsOpacityFraction);
}

void TEffectsOptionsPage::setOtherPlayersEffectsOpacity(const uint percent)
{
    setFraction(m_otherPlayersEffectsOpacityFraction, percent);
}

uint TEffectsOptionsPage::monsterEffectsOpacity() const
{
    return percentOf(m_monsterEffectsOpacityFraction);
}

void TEffectsOptionsPage::setMonsterEffectsOpacity(const uint percent)
{
    setFraction(m_monsterEffectsOpacityFraction, percent);
}

uint TEffectsOptionsPage::monsterBossAreaEffectsOpacity() const
{
    return percentOf(m_monsterBossAreaEffectsOpacityFraction);
}

void TEffectsOptionsPage::setMonsterBossAreaEffectsOpacity(const uint percent)
{
    setFraction(m_monsterBossAreaEffectsOpacityFraction, percent);
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
    // Qualified: inside the class these names are the generated getters.
    m_controlSchemeListModel = config::controlSchemeListModel();
    m_lootSchemeListModel = config::lootSchemeListModel();
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
    sound::TSoundEngine::instance().updateAvailableDevices();
    if (!m_soundDevice.isEmpty() && !soundDeviceListModel().contains(m_soundDevice))
        m_soundDevice = qtTrId("optionsmenu_sound_used_sound_device_auto");
}

TSoundOptionsPage::TSoundOptionsPage(QObject* parent)
    : TBoundOptionsPage(SoundBindings, parent)
{
    m_masterVolume = 100;
    m_musicVolume = 100;
    m_anthemEnabled = true;
    m_ambienceVolume = 100;
    m_itemsVolume = 100;
    m_eatingEnabled = true;
    m_moveItemEnabled = true;
    m_eventsVolume = 100;
    connect(&sound::TSoundEngine::instance(), &sound::TSoundEngine::availableSoundDevicesChanged, this,
            &TSoundOptionsPage::changed);
}

bool TSoundOptionsPage::soundEngineInitialized() const
{
    return sound::TSoundEngine::instance().isInitialized();
}

QStringList TSoundOptionsPage::soundDeviceListModel() const
{
    QStringList devices = sound::TSoundEngine::instance().availableSoundDevices();
    devices.insert(0, qtTrId("optionsmenu_sound_used_sound_device_auto"));
    return devices;
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
    return soundEngineInitialized() && m_masterVolume != 0;
}

namespace {

/// CIP (row builder): the picture of an object setting: its liquid, else its
/// tier, else the plain type.
appearances::TAppearanceInstancePtr hotkeyObjectAppearance(const TActionSetting& setting)
{
    TAppearanceType* type = setting.objectId > 0 ? tibia::game::rawThingType(static_cast<uint16_t>(setting.objectId), ThingCategoryItem)
                                           : nullptr;
    if (!type)
        return nullptr;
    if (setting.liquidType)
        return std::make_shared<appearances::TLiquidObjectAppearanceInstance>(type, static_cast<uint32_t>(*setting.liquidType));
    appearances::TAppearanceInstancePtr instance = appearances::TAppearanceInstanceFactory::fromThingType(type);
    if (setting.upgradeTier)
        if (auto* object = dynamic_cast<appearances::TObjectAppearanceInstance*>(instance.get()))
            object->setTier(static_cast<uint8_t>(*setting.upgradeTier));
    return instance;
}

} // namespace

namespace {

/// The assign dialogs of the hotkeys page belong to no action bar.
constexpr uint8_t NoActionBar = 0xff;

}

THotkeysOptionsPage::THotkeysOptionsPage(QObject* parent)
    : cip::tibia::config::THotkeysQMLOptionsPage(parent)
    , m_assignSpellDialog(std::make_unique<gamewindow::TAssignSpellActionDialogController>(NoActionBar))
    , m_assignObjectDialog(std::make_unique<gamewindow::TAssignObjectActionDialogController>(NoActionBar))
    , m_assignTextDialog(std::make_unique<gamewindow::TAssignTextActionDialogController>(NoActionBar))
{
    using namespace gamewindow;
    m_assignObjectDialog->setShowActionBarTarget(false);
    connect(m_assignObjectDialog.get(), &TAssignObjectActionDialogController::publishGameAction, this,
            &THotkeysOptionsPage::publishGameAction);
    connect(m_assignObjectDialog.get(), &TAssignObjectActionDialogController::requestAssignObject, this,
            [this](const uint ButtonId, const int MultiActionIndex, const int ObjectId, const int UpgradeTier, const int LiquidType,
                   const EUseType UseType, const bool UseEquipSmartMode) {
                onObjectAssignedToHotkeyAction(int(ButtonId), MultiActionIndex, ObjectId, UpgradeTier, LiquidType, UseType,
                                               UseEquipSmartMode);
            });
    m_assignTextDialog->setShowActionBarTarget(false);
    connect(m_assignTextDialog.get(), &TAssignTextActionDialogController::requestAssignText, this,
            [this](const uint ButtonId, const int MultiActionIndex, const QString& Text, const bool SendAutomatically) {
                onTextAssignedToHotkeyAction(int(ButtonId), MultiActionIndex, Text, SendAutomatically);
            });
    m_assignSpellDialog->setShowActionBarTarget(false);
    connect(m_assignSpellDialog.get(), &TAssignSpellActionDialogController::requestAssignSpell, this,
            [this](const uint ButtonId, const int MultiActionIndex, const int SpellId, const QString& Parameters, const ECastType CastType) {
                onSpellAssignedToHotkeyAction(int(ButtonId), MultiActionIndex, SpellId, Parameters, int(CastType));
            });
}

THotkeysOptionsPage::~THotkeysOptionsPage() = default;

TTibiaHotkeyActionSettingQmlWrapper* THotkeysOptionsPage::rowAt(const int rowIndex) const
{
    if (rowIndex < 0 || rowIndex >= m_rows.size())
        return nullptr;
    return static_cast<TTibiaHotkeyActionSettingQmlWrapper*>(m_rows.at(rowIndex));
}

void THotkeysOptionsPage::setHotkeyMode(const EHotkeyMode mode)
{
    if (m_hotkeyMode == mode)
        return;
    m_hotkeyMode = mode;
    Q_EMIT hotkeyModeChanged();
    rebuildModel();
}

void THotkeysOptionsPage::setAutoSwitchHotkeyPreset(const bool v)
{
    if (m_options.autoSwitchHotkeyPreset == v)
        return;
    m_options.autoSwitchHotkeyPreset = v;
    m_autoSwitchHotkeyPreset = v;
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

int THotkeysOptionsPage::hotkeySetNamesIndex() const
{
    return m_options.currentHotkeySetIndex();
}

void THotkeysOptionsPage::setHotkeySetNamesIndex(const int v)
{
    if (m_options.currentHotkeySetIndex() == v)
        return;
    m_options.setCurrentHotkeySetIndex(v);
    refreshSetNames();
    Q_EMIT hotkeySetsChanged();
    rebuildModel();
}

void THotkeysOptionsPage::setNameFilter(const QString& v)
{
    if (m_nameFilter == v)
        return;
    m_nameFilter = v;
    rebuildModel();
}

void THotkeysOptionsPage::refreshSetNames()
{
    m_hotkeySetNamesModel = m_options.hotkeySetNames();
    m_hotkeySetNamesIndex = m_options.currentHotkeySetIndex();
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
    m_chatModeOn = options.chatOptions().chatModeOn;
    m_rotateWithAlt = options.boolOption(cip::EClientOption::rotateWithAlt);
    m_rotateWithShift = options.boolOption(cip::EClientOption::rotateWithShift);
    m_rotateWithCtrl = options.boolOption(cip::EClientOption::rotateWithCtrl);
    m_assignObjectDialog->setClientSettingSmoothFiltering(
        options.antialiasingMode() == int(cip::tibia::qmlcomponents::enums::EAntialiasingMode::AntialiasingModeAntialiasing));
    m_nameFilter.clear();
    m_editRow = nullptr;
    Q_EMIT autoswitchHotkeyPresetChanged();
    Q_EMIT hotkeySetsChanged();
    rebuildModel();
}

void THotkeysOptionsPage::setRotateModifiers(const bool rotateWithAlt, const bool rotateWithShift, const bool rotateWithCtrl)
{
    m_rotateWithAlt = rotateWithAlt;
    m_rotateWithShift = rotateWithShift;
    m_rotateWithCtrl = rotateWithCtrl;
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
        const auto* row = static_cast<TTibiaHotkeyActionSettingQmlWrapper*>(object);
        const TActionSetting& setting = row->hotkeyActionSetting();
        if (setting.isEmpty())
            continue;
        if (!row->firstKeySequence().isEmpty())
            m_options.addBinding(name, m_chatModeOn, row->firstKeySequence(), setting);
        if (!row->secondKeySequence().isEmpty())
            m_options.addBinding(name, m_chatModeOn, row->secondKeySequence(), setting);
        else if (row->firstKeySequence().isEmpty() && keepUnboundRows)
            m_options.addBinding(name, m_chatModeOn, QKeySequence(), setting);
    }
}

namespace {

/// Hotkey order: the shorter sequence first; of equally long ones, the first
/// differing key decides by its key code without modifiers.
bool hotkeySequenceLess(const QKeySequence& a, const QKeySequence& b)
{
    if (a.count() != b.count())
        return a.count() < b.count();
    for (int i = 0; i < a.count(); ++i) {
        const quint32 aKey = static_cast<quint32>(a[i].toCombined());
        const quint32 bKey = static_cast<quint32>(b[i].toCombined());
        if (aKey != bKey)
            return (aKey & 0x1ffffff) < (bKey & 0x1ffffff);
    }
    return false;
}

/// CIP's hotkeys page sort: custom settings first, by first
/// then second key (rows without keys last), then the action buttons, then the
/// other static actions, both by description.
bool hotkeyRowLess(const TTibiaHotkeyActionSettingQmlWrapper* a, const TTibiaHotkeyActionSettingQmlWrapper* b)
{
    const auto isButton = [](const TTibiaHotkeyActionSettingQmlWrapper* row) {
        return row->hotkeyActionSetting().kind == TActionSetting::ACTION
            && input::gameActionAs<input::TGameActionActionButtonTriggered>(
                input::TKeyboardShortcutHandler::staticAction(row->hotkeyActionSetting().action));
    };
    const bool aStatic = a->hotkeyActionSetting().kind == TActionSetting::ACTION;
    const bool bStatic = b->hotkeyActionSetting().kind == TActionSetting::ACTION;
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
    if (a->firstKeySequence() == b->firstKeySequence())
        return hotkeySequenceLess(a->secondKeySequence(), b->secondKeySequence());
    if (a->firstKeySequence().isEmpty())
        return false;
    if (b->firstKeySequence().isEmpty())
        return true;
    return hotkeySequenceLess(a->firstKeySequence(), b->firstKeySequence());
}

}

/// CIP's row builder: a row per static action (its first
/// two keys of the current set and chat mode), then a row per other setting
/// (a second binding of the same setting gives it its second key), sorted.
/// A row is visible when it belongs to the hotkey mode and its description or
/// one of its key texts contains the name filter (case-insensitive).
void THotkeysOptionsPage::rebuildModel()
{
    m_hotkeyModel.clear();
    qDeleteAll(m_rows);
    m_rows.clear();
    m_editRow = nullptr;

    static const std::vector<THotkeyBinding> NoBindings;
    const THotkeySet* set = m_options.set(currentSetName());
    const std::vector<THotkeyBinding>& bindings = set ? (m_chatModeOn ? set->chatOn : set->chatOff) : NoBindings;

    std::vector<TTibiaHotkeyActionSettingQmlWrapper*> rows;
    std::vector<EHotkeyMode> modes;
    const auto addRow = [this, &rows, &modes](const EHotkeyMode mode, const TActionSetting& setting, const QKeySequence& first) {
        auto* row = new TTibiaHotkeyActionSettingQmlWrapper(first, this);
        row->hotkeyActionSetting() = setting;
        rows.push_back(row);
        modes.push_back(mode);
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
        TTibiaHotkeyActionSettingQmlWrapper* row = addRow(button ? ActionButtons : DefaultHotkeys, setting, QKeySequence());
        for (const THotkeyBinding& binding : bindings) {
            if (!(binding.setting == setting))
                continue;
            if (row->firstKeySequence().isEmpty())
                row->setFirstKeySequence(binding.keys);
            else if (row->secondKeySequence().isEmpty())
                row->setSecondKeySequence(binding.keys);
        }
        // qtclient: an unbound pack action shows its default keys.
        if (row->firstKeySequence().isEmpty()) {
            for (const auto& product : input::TKeyboardShortcutHandler::productActions())
                if (product.name == name && !product.defaultKeys.isEmpty())
                    row->setFirstKeySequence(product.defaultKeys);
        }
    }
    const size_t staticRows = rows.size();
    for (const THotkeyBinding& binding : bindings) {
        if (binding.setting.kind == TActionSetting::ACTION)
            continue;
        const auto existing = std::find_if(rows.begin() + static_cast<std::ptrdiff_t>(staticRows), rows.end(),
                                           [&](const TTibiaHotkeyActionSettingQmlWrapper* row) {
                                               return row->hotkeyActionSetting() == binding.setting;
                                           });
        if (existing != rows.end()) {
            (*existing)->setSecondKeySequence(binding.keys);
            continue;
        }
        TTibiaHotkeyActionSettingQmlWrapper* row = addRow(CustomHotkeys, binding.setting, binding.keys);
        if (binding.setting.objectId != 0)
            row->setObjectAppearanceInstance(hotkeyObjectAppearance(binding.setting));
    }
    for (size_t i = 0; i < rows.size(); ++i) {
        TTibiaHotkeyActionSettingQmlWrapper* row = rows[i];
        bool visible = modes[i] == m_hotkeyMode;
        if (visible) {
            const QString texts[] = { row->descriptionText(), row->firstHotkeyText(), row->secondHotkeyText() };
            visible = std::any_of(std::begin(texts), std::end(texts),
                                  [this](const QString& text) { return text.indexOf(m_nameFilter, 0, Qt::CaseInsensitive) != -1; });
        }
        row->setVisible(visible);
    }
    std::sort(rows.begin(), rows.end(), hotkeyRowLess);
    for (TTibiaHotkeyActionSettingQmlWrapper* row : rows)
        m_rows.append(row);

    updateHotkeyListModel();
}

/// hotkeyModel: the visible rows.
void THotkeysOptionsPage::updateHotkeyListModel()
{
    m_hotkeyModel.clear();
    for (QObject* object : std::as_const(m_rows)) {
        if (static_cast<TTibiaHotkeyActionSettingQmlWrapper*>(object)->isVisible())
            m_hotkeyModel.append(object);
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
    message->setButtons(gamewindow::okCancelDialogButtons());
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

void THotkeysOptionsPage::clearActionBarMappings(const uint8_t actionBarId)
{
    if (THotkeySet* set = m_options.set(currentSetName()))
        std::erase_if(set->mappings, [actionBarId](const auto& entry) { return entry.first.first == actionBarId; });
}

void THotkeysOptionsPage::closeDialogs()
{
    const auto close = [](auto* controller) {
        if (controller->dialog()->item())
            controller->dialog()->close();
    };
    for (auto* prompt : findChildren<gamewindow::TGenericEnterTextDialogController*>(Qt::FindDirectChildrenOnly))
        close(prompt);
    for (auto* message : findChildren<gamewindow::TMessageDialogController*>(Qt::FindDirectChildrenOnly))
        close(message);
    for (auto* record : findChildren<gamewindow::TRecordKeyboardShortcutDialogController*>(Qt::FindDirectChildrenOnly))
        close(record);
    close(m_assignSpellDialog.get());
    close(m_assignObjectDialog.get());
    close(m_assignTextDialog.get());
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

/// QML's index counts the visible rows: the menu opens for the row it shows.
void THotkeysOptionsPage::requestEditActionForHotkey(int HotkeyActionSettingIndex)
{
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        if (!static_cast<TTibiaHotkeyActionSettingQmlWrapper*>(m_rows.at(i))->isVisible())
            ++HotkeyActionSettingIndex;
        else if (i == HotkeyActionSettingIndex)
            break;
    }
    showEditActionMenu(HotkeyActionSettingIndex);
}

void THotkeysOptionsPage::requestAddNewCustomHotkey()
{
    showEditActionMenu(-1);
}

void THotkeysOptionsPage::showEditActionMenu(const int rowIndex)
{
    auto* menu = gamewindow::TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    bool hasObject = false;
    bool hasSpell = false;
    bool hasText = false;
    if (const TTibiaHotkeyActionSettingQmlWrapper* row = rowAt(rowIndex)) {
        const TActionSetting& setting = row->hotkeyActionSetting();
        if (!setting.text.isEmpty()) {
            const spells::TSpellInfoStorage::TSpellText spell = spells::TSpellInfoStorage::instance()->parseSpellText(setting.text);
            hasSpell = spell.spell != nullptr;
            hasText = !hasSpell;
        }
        hasObject = setting.objectId != 0;
    }
    const auto entry = [this, menu, rowIndex](const bool edit, const char* format, void (THotkeysOptionsPage::*open)(int)) {
        const QString text = qtTrId(format).arg(qtTrId(edit ? "actionbar_contextmenu_edit" : "actionbar_contextmenu_assign"));
        connect(menu->addCustomEntry(text), &gamewindow::TContextMenuEntryItem::triggered, this,
                [this, open, rowIndex] { (this->*open)(rowIndex); });
    };
    entry(hasSpell, "actionbar_contextmenu_spell", &THotkeysOptionsPage::openAssignSpellDialog);
    entry(hasObject, "actionbar_contextmenu_object", &THotkeysOptionsPage::openAssignObjectDialog);
    entry(hasText, "actionbar_contextmenu_text", &THotkeysOptionsPage::openAssignTextDialog);
    if (hasObject || hasSpell || hasText) {
        menu->addCustomSeparator();
        connect(menu->addCustomEntry(qtTrId("actionbar_contextmenu_clear_action")), &gamewindow::TContextMenuEntryItem::triggered, this,
                [this, rowIndex] { removeRow(rowIndex); });
    }
    menu->showCustomMenu();
}

void THotkeysOptionsPage::openAssignSpellDialog(const int rowIndex)
{
    if (const TTibiaHotkeyActionSettingQmlWrapper* row = rowAt(rowIndex)) {
        const TActionSetting& setting = row->hotkeyActionSetting();
        if (!setting.text.isEmpty()) {
            const spells::TSpellInfoStorage::TSpellText spell = spells::TSpellInfoStorage::instance()->parseSpellText(setting.text);
            if (spell.spell)
                m_assignSpellDialog->showDialog(uint(rowIndex), -1, spell.spell->id, spell.parameters,
                                                gamewindow::ECastType(setting.spellCastType));
            else
                m_assignSpellDialog->showDialog(uint(rowIndex), -1);
            return;
        }
    }
    m_assignSpellDialog->showDialog(uint(rowIndex), -1);
}

void THotkeysOptionsPage::openAssignObjectDialog(const int rowIndex)
{
    if (const TTibiaHotkeyActionSettingQmlWrapper* row = rowAt(rowIndex)) {
        const TActionSetting& setting = row->hotkeyActionSetting();
        if (setting.objectId != 0) {
            m_assignObjectDialog->showDialog(uint(rowIndex), -1, setting.objectId, setting.upgradeTier.value_or(0),
                                             setting.liquidType.value_or(0), setting.useType, row->canHaveSmartMode(), false);
            return;
        }
    }
    m_assignObjectDialog->showDialog(uint(rowIndex), -1);
}

void THotkeysOptionsPage::openAssignTextDialog(const int rowIndex)
{
    if (const TTibiaHotkeyActionSettingQmlWrapper* row = rowAt(rowIndex)) {
        const TActionSetting& setting = row->hotkeyActionSetting();
        if (!setting.text.isEmpty()) {
            m_assignTextDialog->showDialog(uint(rowIndex), -1, setting.text, setting.sendAutomatically);
            return;
        }
    }
    m_assignTextDialog->showDialog(uint(rowIndex), -1);
}

void THotkeysOptionsPage::removeRow(const int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= m_rows.size())
        return;
    QObject* row = m_rows.takeAt(rowIndex);
    commitRows(true);
    rebuildModel();
    row->deleteLater();
}

void THotkeysOptionsPage::onTextAssignedToHotkeyAction(const int HotkeyActionSettingIndex, int, const QString& Text, const bool SendAutomatically)
{
    TTibiaHotkeyActionSettingQmlWrapper* row = rowAt(HotkeyActionSettingIndex);
    const bool added = HotkeyActionSettingIndex == -1 || row == nullptr;
    if (added) {
        row = new TTibiaHotkeyActionSettingQmlWrapper(QKeySequence(), this);
        m_rows.append(row);
    }
    TActionSetting setting;
    setting.setChatText(Text);
    setting.setSendAutomatically(SendAutomatically);
    row->hotkeyActionSetting() = setting;
    row->notifyChanged();
    commitRows(true);
    if (added)
        rebuildModel();
}

void THotkeysOptionsPage::onObjectAssignedToHotkeyAction(const int HotkeyActionSettingIndex, int, const int ObjectId, const int UpgradeTier,
                                                         const int LiquidType, const EUseType UseType, const bool UseEquipSmartMode)
{
    TTibiaHotkeyActionSettingQmlWrapper* row = rowAt(HotkeyActionSettingIndex);
    const bool added = HotkeyActionSettingIndex == -1 || row == nullptr;
    if (added) {
        row = new TTibiaHotkeyActionSettingQmlWrapper(QKeySequence(), this);
        m_rows.append(row);
    }
    // qtclient: the dialog hands the liquid and the tier as plain values; a
    // non-zero liquid is a liquid object, else a non-zero tier a tiered one.
    TActionSetting setting;
    if (LiquidType != 0)
        setting.setUseObject(ObjectId, LiquidType);
    else if (UpgradeTier != 0)
        setting.setUseObjectWithTier(ObjectId, UpgradeTier);
    else
        setting.setUseObject(ObjectId);
    setting.setUseType(UseType);
    setting.setUseEquipSmartMode(UseEquipSmartMode);
    row->hotkeyActionSetting() = setting;
    row->setObjectAppearanceInstance(hotkeyObjectAppearance(setting));
    row->notifyChanged();
    commitRows(true);
    if (added)
        rebuildModel();
}

void THotkeysOptionsPage::onSpellAssignedToHotkeyAction(const int HotkeyActionSettingIndex, int, const int SpellId, const QString& Parameters,
                                                        const int CastType)
{
    TTibiaHotkeyActionSettingQmlWrapper* row = rowAt(HotkeyActionSettingIndex);
    const bool added = HotkeyActionSettingIndex == -1 || row == nullptr;
    if (added) {
        row = new TTibiaHotkeyActionSettingQmlWrapper(QKeySequence(), this);
        m_rows.append(row);
    }
    TActionSetting setting;
    setting.setChatText(spells::TSpellInfoStorage::instance()->spellFormula(SpellId, Parameters));
    setting.setSendAutomatically(true);
    setting.setSpellCastType(CastType);
    row->hotkeyActionSetting() = setting;
    row->notifyChanged();
    commitRows(true);
    if (added)
        rebuildModel();
}

void THotkeysOptionsPage::requestEnterNewHotkeyForAction(const int HotkeyActionSettingIndex, const int KeySequenceIndex)
{
    if (HotkeyActionSettingIndex < 0 || HotkeyActionSettingIndex >= m_hotkeyModel.size())
        return;
    auto* row = static_cast<TTibiaHotkeyActionSettingQmlWrapper*>(m_hotkeyModel.at(HotkeyActionSettingIndex));
    if (!row || row->hotkeyActionSetting().isEmpty())
        return;
    m_editRow = row;
    m_editKey = KeySequenceIndex;
    const QKeySequence current = KeySequenceIndex == 0 ? row->firstKeySequence() : row->secondKeySequence();
    const TActionSetting& setting = row->hotkeyActionSetting();
    const QString actionName = setting.kind == TActionSetting::ACTION ? setting.action : QString();
    auto* dialog = new gamewindow::TRecordKeyboardShortcutDialogController(row->descriptionText(), actionName, m_chatModeOn, current, this);
    connect(dialog, &gamewindow::TRecordKeyboardShortcutDialogController::keySequenceSelected, this,
            &THotkeysOptionsPage::onNewKeySequenceSelectedForCurrentHotkeyActionSetting);
    dialog->dialog()->show();
}

void THotkeysOptionsPage::onNewKeySequenceSelectedForCurrentHotkeyActionSetting(const QKeySequence& NewKeySequence)
{
    auto* edited = static_cast<TTibiaHotkeyActionSettingQmlWrapper*>(m_editRow);
    if (!edited || !m_rows.contains(edited))
        return;
    // A key runs one action per chat mode: other rows lose it.
    if (!NewKeySequence.isEmpty()) {
        for (QObject* object : std::as_const(m_rows)) {
            auto* row = static_cast<TTibiaHotkeyActionSettingQmlWrapper*>(object);
            bool changed = false;
            if (row->firstKeySequence() == NewKeySequence) {
                row->setFirstKeySequence(QKeySequence());
                changed = true;
            }
            if (row->secondKeySequence() == NewKeySequence) {
                row->setSecondKeySequence(QKeySequence());
                changed = true;
            }
            if (changed)
                row->notifyChanged();
        }
    }
    if (m_editKey == 0)
        edited->setFirstKeySequence(NewKeySequence);
    else if (m_editKey == 1)
        edited->setSecondKeySequence(NewKeySequence);
    else
        return;
    edited->notifyChanged();
    commitRows(true);
}

void TActionBarsOptionsPage::load(const TClientOptions& options)
{
    loadBindings(this, actionBarsBindings(), options);
    resetClearFlags();
}

void TActionBarsOptionsPage::resetClearFlags()
{
    m_clearBottomActionBar1 = m_clearBottomActionBar2 = m_clearBottomActionBar3 = false;
    m_clearLeftActionBar1 = m_clearLeftActionBar2 = m_clearLeftActionBar3 = false;
    m_clearRightActionBar1 = m_clearRightActionBar2 = m_clearRightActionBar3 = false;
    Q_EMIT changed();
}

std::span<const TOptionBinding> actionBarsBindings() { return ActionBarsBindings; }
std::span<const TOptionBinding> consoleBindings() { return ConsoleBindings; }
std::span<const TOptionBinding> miscBindings() { return MiscBindings; }
std::span<const TOptionBinding> gameplayBindings() { return GameplayBindings; }
std::span<const TOptionBinding> screenshotsBindings() { return ScreenshotsBindings; }
std::span<const TOptionBinding> battleSoundsBindings() { return BattleSoundsBindings; }
std::span<const TOptionBinding> uiSoundsBindings() { return UISoundsBindings; }

}
