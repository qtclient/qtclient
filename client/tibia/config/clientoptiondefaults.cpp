#include "clientoptiondefaults.h"

#include <QJsonArray>
#include <QSet>

namespace tibia::config {

namespace {

/// CIP EButtons / ButtonBarButtons (optionspages.cpp). Labels live there.
constexpr const char* ControlButtonIds[] = {
    "skillsWidget",
    "battleListWidget",
    "partyWidget",
    "vipWidget",
    "spellListWidget",
    "skillWheelDialog",
    "questDialog",
    "questTrackerWidget",
    "unjustifiedPoinsWidget",
    "preyDialog",
    "preyWidget",
    "rewardWallDialog",
    "analyticsSelectorWidget",
    "compendiumDialog",
    "cyclopediaDialog",
    "bosstiaryDialog",
    "bossslotsDialog",
    "bosstiaryTrackerWidget",
    "bestiaryTrackerWidget",
    "imbuementTrackerWidget",
    "weaponProficiency",
    "exaltationForgeDialog",
    "friendsDialog",
    "lenshelpFunction",
    "highscoresDialog",
    "playerGuide",
    "manageShortcuts",
    "taskboard",
};

QStringList stringList(const QJsonValue& value)
{
    QStringList list;
    for (const QJsonValue& entry : value.toArray()) {
        const QString id = entry.toString();
        if (!id.isEmpty())
            list << id;
    }
    return list;
}

}

const QJsonObject& defaultClientOptions()
{
    static const QJsonObject options {
        { QStringLiteral("actionBarBottomLocked"), false },
        { QStringLiteral("actionBarLeftLocked"), false },
        { QStringLiteral("actionBarRightLocked"), false },
        { QStringLiteral("actionBarShowBottom1"), true },
        { QStringLiteral("actionBarShowBottom2"), false },
        { QStringLiteral("actionBarShowBottom3"), false },
        { QStringLiteral("actionBarShowLeft1"), false },
        { QStringLiteral("actionBarShowLeft2"), false },
        { QStringLiteral("actionBarShowLeft3"), false },
        { QStringLiteral("actionBarShowRight1"), false },
        { QStringLiteral("actionBarShowRight2"), false },
        { QStringLiteral("actionBarShowRight3"), false },
        { QStringLiteral("actionBarsShowBottom"), true },
        { QStringLiteral("actionBarsShowLeft"), true },
        { QStringLiteral("actionBarsShowRight"), true },
        { QStringLiteral("actionButtonAllowTooltip"), true },
        { QStringLiteral("actionButtonAutoInsertSpells"), true },
        { QStringLiteral("actionButtonShowAmount"), true },
        { QStringLiteral("actionButtonShowCooldownNumbers"), true },
        { QStringLiteral("actionButtonShowGraphicalCooldown"), true },
        { QStringLiteral("actionButtonShowHotkey"), true },
        { QStringLiteral("actionButtonShowSpellParameters"), true },
        { QStringLiteral("alwaysTurnTowardsMoveDirection"), false },
        { QStringLiteral("antialiasingMode"), 2 },
        { QStringLiteral("autoChaseEnabled"), false },
        { QStringLiteral("characterSelectionShowHidden"), true },
        { QStringLiteral("characterSelectionShowOutfits"), true },
        { QStringLiteral("characterSelectionSortAscendingOrder"), true },
        { QStringLiteral("characterSelectionSortColumn"), 1 },
        { QStringLiteral("charTradePaymentRemindAgain"), 0 },
        { QStringLiteral("ChatEnabled"), true },
        { QStringLiteral("colorizeLootStyle"), 1 },
        { QStringLiteral("combatShowFrames"), true },
        { QStringLiteral("combatShowPvpFrames"), true },
        { QStringLiteral("connectionStabilityOptimizationEnabled"), true },
        { QStringLiteral("consoleShowEventMessages"), true },
        { QStringLiteral("consoleShowInfoMessages"), true },
        { QStringLiteral("consoleShowJoinLeaveMessages"), true },
        { QStringLiteral("consoleShowLevels"), true },
        { QStringLiteral("consoleShowStatusMessagesOfOthers"), true },
        { QStringLiteral("consoleShowStatusMessagesOwn"), true },
        { QStringLiteral("consoleShowTimestamps"), true },
        { QStringLiteral("consoleShowTimestampsSeconds"), false },
        { QStringLiteral("containerMoveToManagedContainerRecursive"), false },
        { QStringLiteral("containerMoveToManagedContainerRecursiveShowWarningAgain"), true },
        { QStringLiteral("containerMoveToManagedContainerRecursiveWarning"), false },
        { QStringLiteral("containerSortBackpacksFirst"), false },
        { QStringLiteral("containerSortRecursive"), false },
        { QStringLiteral("containerSortRecursiveShowWarningAgain"), true },
        { QStringLiteral("controlSchemeIndex"), 2 },
        { QStringLiteral("cooldownBarEnabled"), true },
        { QStringLiteral("creatueShowNpcIcon"), true },
        { QStringLiteral("creatureHudEnabled"), true },
        { QStringLiteral("creatureShowHealth"), true },
        { QStringLiteral("creatureShowMarks"), true },
        { QStringLiteral("creatureShowName"), true },
        { QStringLiteral("dragAndDropDefaultActionIsMoveAll"), true },
        { QStringLiteral("emergencyDialogVersion"), 0 },
        // Match uncapped classic OTC feel out of the box; enable a limit in Graphics
        // for play (display refresh) or battery. Previously true@60 with a disabled
        // checkbox left users stuck, while the HUD still counted empty presents ~150.
        { QStringLiteral("frameRateLimit"), 60 },
        { QStringLiteral("frameRateLimitEnabled"), false },
        { QStringLiteral("gameWindowLowerPaneHeight"), 216 },
        { QStringLiteral("gameWindowResizingMode"), 0 },
        { QStringLiteral("gameWindowScaleOnlyByEvenMultiples"), false },
        { QStringLiteral("gameWindowShowAttackAnimation"), true },
        { QStringLiteral("gameWindowShowBoostedCreatureMessages"), true },
        { QStringLiteral("gameWindowShowHotkeyUsageMessages"), true },
        { QStringLiteral("gameWindowShowInfoBanner"), true },
        { QStringLiteral("gameWindowShowLootHighlighting"), true },
        { QStringLiteral("gameWindowShowLootMessages"), true },
        { QStringLiteral("gameWindowShowMessages"), true },
        { QStringLiteral("gameWindowShowOfflineTrainingMessages"), true },
        { QStringLiteral("gameWindowShowOthersSpells"), true },
        { QStringLiteral("gameWindowShowOwnSpells"), true },
        { QStringLiteral("gameWindowShowPotionMessages"), true },
        { QStringLiteral("gameWindowShowPrivateMessages"), true },
        { QStringLiteral("gameWindowShowStoreMessages"), true },
        { QStringLiteral("gameWindowShowTargetHighlight"), true },
        { QStringLiteral("gameWindowShowTextualEffects"), true },
        { QStringLiteral("gameWindowShowTragetFrame"), true },
        { QStringLiteral("gameWindowUpperPaneHeight"), 362 },
        { QStringLiteral("GlobalMessagesEnabled"), true },
        { QStringLiteral("GuildMessagesEnabled"), true },
        { QStringLiteral("inspectPlayerAllowAllEnabled"), false },
        { QStringLiteral("keyboardDelayMs"), 250 },
        { QStringLiteral("keyboardDelayUseDefault"), true },
        { QStringLiteral("lightAmbientLevel"), 0.25 },
        { QStringLiteral("lightAttenuationCloudsIndoor"), 0.75 },
        { QStringLiteral("lightEffectsEnabled"), true },
        { QStringLiteral("lightLevelSeparatorLevel"), 0.8 },
        { QStringLiteral("linkCopyWarningEnabled"), true },
        { QStringLiteral("loginEmailAddress"), QString() },
        { QStringLiteral("lootSchemeIndex"), 0 },
        { QStringLiteral("marketShowLockerOnly"), false },
        { QStringLiteral("monsterBossAreaEffectsOpacity"), 1.0 },
        { QStringLiteral("monsterEffectsOpacity"), 1.0 },
        { QStringLiteral("mouseAnimatedCursor"), true },
        { QStringLiteral("mouseBigCursor"), false },
        { QStringLiteral("mouseHighlightTarget"), true },
        { QStringLiteral("mouseSystemCursor"), false },
        { QStringLiteral("NpcMessageseEnabled"), true },
        { QStringLiteral("openNewPrivateMessagesInNewTab"), false },
        { QStringLiteral("optionsShowAdvanced"), false },
        { QStringLiteral("otherPlayersEffectsOpacity"), 1.0 },
        { QStringLiteral("ownEffectsOpacity"), 1.0 },
        { QStringLiteral("PartyMessagesEnabled"), true },
        { QStringLiteral("playerHudArcDistance"), 0.25 },
        { QStringLiteral("playerHudArcHeight"), 0.35 },
        { QStringLiteral("playerHudArcOpacity"), 1.0 },
        { QStringLiteral("playerHudArcWidth"), 10 },
        { QStringLiteral("playerHudEnabled"), true },
        { QStringLiteral("playerHudShowArcs"), false },
        { QStringLiteral("playerHudShowBars"), true },
        { QStringLiteral("playerHudShowHarmonyLeft"), true },
        { QStringLiteral("playerShowHarmonySerene"), true },
        { QStringLiteral("playerShowHealth"), true },
        { QStringLiteral("playerShowMana"), true },
        { QStringLiteral("playerShowMarks"), true },
        { QStringLiteral("playerShowName"), true },
        { QStringLiteral("PrivateMessagesEnabled"), true },
        { QStringLiteral("PrivateMessagesWithoutTabEnabled"), true },
        { QStringLiteral("quickLogin"), true },
        { QStringLiteral("quickLootAllCorpsesInAreaEnabled"), true },
        { QStringLiteral("RaidMessagesEnabled"), true },
        { QStringLiteral("rememberLoginEmail"), false },
        { QStringLiteral("rendererIndex"), 0 },
        { QStringLiteral("rotateWithAlt"), false },
        { QStringLiteral("rotateWithCtrl"), true },
        { QStringLiteral("rotateWithShift"), false },
        { QStringLiteral("screenshotsAchievementEnabled"), true },
        { QStringLiteral("screenshotsAutoScreenshotsEnabled"), true },
        { QStringLiteral("screenshotsBestiaryFullEnabled"), false },
        { QStringLiteral("screenshotsBestiaryPartlyEnabled"), false },
        { QStringLiteral("screenshotsBossKillEnabled"), false },
        { QStringLiteral("screenshotsDeathPvEEnabled"), true },
        { QStringLiteral("screenshotsDeathPvPEnabled"), true },
        { QStringLiteral("screenshotsGiftOfLifeEnabled"), true },
        { QStringLiteral("screenshotsLevelUpEnabled"), true },
        { QStringLiteral("screenshotsLowHitPointsEnabled"), false },
        { QStringLiteral("screenshotsNewMaxDamageEnabled"), false },
        { QStringLiteral("screenshotsNewMaxHealingEnabled"), false },
        { QStringLiteral("screenshotsPlayerKillAssistEnabled"), false },
        { QStringLiteral("screenshotsPlayerKillFullEnabled"), false },
        { QStringLiteral("screenshotsPvPAttackEnabled"), false },
        { QStringLiteral("screenshotsRewardChestEnabled"), false },
        { QStringLiteral("screenshotsSkillUpEnabled"), true },
        { QStringLiteral("screenshotsTrackedLootEnabled"), false },
        { QStringLiteral("screenshotsUseBacklog"), false },
        { QStringLiteral("screnshotsOnlyGameWindow"), true },
        { QStringLiteral("sessionRemainLoggedIn"), true },
        { QStringLiteral("showEMailAsPlainText"), true },
        { QStringLiteral("showExpireInContainers"), false },
        { QStringLiteral("showExpireInInventory"), true },
        { QStringLiteral("showExpireWhenUnused"), true },
        { QStringLiteral("showFpsLagIndicator"), false },
        { QStringLiteral("soundAmbienceVolume"), 100 },
        { QStringLiteral("soundAnthemEnabled"), true },
        { QStringLiteral("soundBattleCreaturesAttacksEnabled"), true },
        { QStringLiteral("soundBattleCreaturesDeathEnabled"), true },
        { QStringLiteral("soundBattleCreaturesNoisesEnabled"), true },
        { QStringLiteral("soundBattleCreaturesVolume"), 100 },
        { QStringLiteral("soundBattleOthersAttackSpellsEnabled"), true },
        { QStringLiteral("soundBattleOthersHealingSpellsEnabled"), true },
        { QStringLiteral("soundBattleOthersSpellsEnabled"), true },
        { QStringLiteral("soundBattleOthersSupportSpellsEnabled"), true },
        { QStringLiteral("soundBattleOthersVolume"), 100 },
        { QStringLiteral("soundBattleOthersWeaponsEnabled"), true },
        { QStringLiteral("soundBattleOwnAttackSpellsEnabled"), true },
        { QStringLiteral("soundBattleOwnHealingSpellsEnabled"), true },
        { QStringLiteral("soundBattleOwnSpellsEnabled"), true },
        { QStringLiteral("soundBattleOwnSupportSpellsEnabled"), true },
        { QStringLiteral("soundBattleOwnVolume"), 100 },
        { QStringLiteral("soundBattleOwnWeaponsEnabled"), true },
        { QStringLiteral("soundEatingEnabled"), true },
        { QStringLiteral("soundEventsVolume"), 100 },
        { QStringLiteral("soundItemsVolume"), 100 },
        { QStringLiteral("soundMasterVolume"), 100 },
        { QStringLiteral("soundMasterVolumeOld"), 100 },
        { QStringLiteral("soundMoveItemEnabled"), true },
        { QStringLiteral("soundMusicVolume"), 50 },
        { QStringLiteral("soundPartyEnabled"), true },
        { QStringLiteral("soundUiInteractionsEnabled"), true },
        { QStringLiteral("soundUiVolume"), 100 },
        { QStringLiteral("soundVipEnabled"), true },
        { QStringLiteral("stashAskBeforeStowContainerContent"), true },
        { QStringLiteral("statusBarEnabled"), true },
        { QStringLiteral("statusPanelEnabled"), false },
        { QStringLiteral("storeAskBeforeBuyingProducts"), true },
        { QStringLiteral("SystemMessagesEnabled"), true },
        { QStringLiteral("TeamFinderMessagesEnabled"), true },
        { QStringLiteral("useFreetypeRenderer"), false },
        { QStringLiteral("usedSoundDevice"), QStringLiteral("auto") },
        { QStringLiteral("vSyncEnabled"), false },
    };
    return options;
}

QStringList defaultControlButtonIds()
{
    QStringList ids;
    for (const char* id : ControlButtonIds)
        ids << QString::fromLatin1(id);
    return ids;
}

QJsonObject defaultControlButtonsOptions()
{
    return {
        { QStringLiteral("enabledButtons"), QJsonArray::fromStringList(defaultControlButtonIds()) },
        { QStringLiteral("disabledButtons"), QJsonArray() },
    };
}

QJsonObject reconcileControlButtons(const QJsonObject& section)
{
    QStringList enabled = stringList(section.value(QLatin1String("enabledButtons")));
    const QStringList disabled = stringList(section.value(QLatin1String("disabledButtons")));
    QSet<QString> seen;
    for (const QString& id : enabled)
        seen.insert(id);
    for (const QString& id : disabled)
        seen.insert(id);
    for (const QString& id : defaultControlButtonIds()) {
        if (!seen.contains(id))
            enabled << id;
    }
    QJsonObject out = section;
    out.insert(QLatin1String("enabledButtons"), QJsonArray::fromStringList(enabled));
    out.insert(QLatin1String("disabledButtons"), QJsonArray::fromStringList(disabled));
    return out;
}

}
