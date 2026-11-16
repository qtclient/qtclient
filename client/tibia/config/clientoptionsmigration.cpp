#include "clientoptionsmigration.h"

#include "config/hotkeyoptions.h"

#include <QJsonArray>
#include <QJsonValue>

namespace tibia::config::clientoptionsmigration {

namespace {

constexpr auto VersionKey = "clientOptionsVersion";

/// shared::TJsonValueValidator: the key is present and holds `type`.
bool holds(const QJsonObject& object, const QString& key, QJsonValue::Type type)
{
    return object.contains(key) && object.value(key).type() == type;
}

/// The integral-number check (a Double whose value is an int).
bool holdsInt(const QJsonObject& object, const QString& key)
{
    if (!holds(object, key, QJsonValue::Double))
        return false;
    const double value = object.value(key).toDouble();
    return value == static_cast<double>(static_cast<int>(value));
}

/// Moves a bool from `from[fromKey]` to `to[toKey]`.
void moveBool(QJsonObject& from, const QString& fromKey, QJsonObject& to, const QString& toKey)
{
    if (!holds(from, fromKey, QJsonValue::Bool))
        return;
    to.insert(toKey, from.value(fromKey).toBool(false));
    from.remove(fromKey);
}

/// Moves a bool from `from[fromKey]` to both `to[firstKey]` and `to[secondKey]`.
void moveBool(QJsonObject& from, const QString& fromKey, QJsonObject& to, const QString& firstKey, const QString& secondKey)
{
    if (!holds(from, fromKey, QJsonValue::Bool))
        return;
    to.insert(firstKey, from.value(fromKey).toBool(false));
    to.insert(secondKey, from.value(fromKey).toBool(false));
    from.remove(fromKey);
}

QString key(const char* name)
{
    return QString::fromLatin1(name);
}

}

QJsonObject migrateFromVersion0(QJsonObject root)
{
    const QJsonObject defaults = THotkeyOptions::builtInSets().value(QLatin1String("GlobalShortcuts")).toObject();
    QJsonObject hotkeyOptions = root.value(QLatin1String("hotkeyOptions")).toObject();
    const QJsonObject sets = hotkeyOptions.value(QLatin1String("hotkeySets")).toObject();
    QJsonObject migratedSets;
    for (const QString& name : sets.keys()) {
        // Old sets map key sequences to action settings.
        QJsonArray bindings;
        const QJsonObject set = sets.value(name).toObject();
        for (const QString& keys : set.keys()) {
            QJsonObject binding;
            binding.insert(QStringLiteral("keysequence"), keys);
            binding.insert(QStringLiteral("actionsetting"), set.value(keys).toObject());
            bindings.append(binding);
        }
        QJsonObject migrated;
        QJsonArray chatOff = defaults.value(QLatin1String("chatOff")).toArray();
        QJsonArray chatOn = defaults.value(QLatin1String("chatOn")).toArray();
        for (const QJsonValue& binding : std::as_const(bindings)) {
            chatOff.append(binding);
            chatOn.append(binding);
        }
        migrated.insert(QStringLiteral("chatOff"), chatOff);
        migrated.insert(QStringLiteral("chatOn"), chatOn);
        migratedSets.insert(name, migrated);
    }
    hotkeyOptions.insert(QStringLiteral("hotkeySets"), migratedSets);
    root.insert(QStringLiteral("hotkeyOptions"), hotkeyOptions);

    QJsonObject general = root.value(QLatin1String("generalOptions")).toObject();
    if (general.contains(QLatin1String("classicControl"))) {
        const bool classic = general.value(QLatin1String("classicControl")).toBool(false);
        general.insert(QStringLiteral("controlScheme"), classic ? 0 : 1);
        general.remove(QLatin1String("classicControl"));
        root.insert(QStringLiteral("generalOptions"), general);
    }
    return root;
}

QJsonObject migrateFromVersion1(QJsonObject root)
{
    QJsonObject general = root.value(QLatin1String("generalOptions")).toObject();
    if (general.contains(QLatin1String("highlightObjectUnderMouse"))) {
        general.insert(QStringLiteral("highlightMouseTarget"), general.value(QLatin1String("highlightObjectUnderMouse")).toBool(false));
        general.remove(QLatin1String("highlightObjectUnderMouse"));
    }
    root.insert(QStringLiteral("generalOptions"), general);
    return root;
}

QJsonObject migrateFromVersion2(QJsonObject root)
{
    if (!root.contains(QLatin1String("generalOptions")))
        return root;
    QJsonObject general = root.value(QLatin1String("generalOptions")).toObject();
    QJsonObject status = root.value(QLatin1String("statusOptions")).toObject();
    moveBool(general, key("showCreatureFrames"), status, key("showFrames"));
    moveBool(general, key("showCreaturePvpFrames"), status, key("showFramesPvp"));
    moveBool(general, key("showCreatureHealthBars"), status, key("playerShowHealth"), key("creatureShowHealth"));
    moveBool(general, key("showCreatureManaBars"), status, key("playerShowMana"));
    moveBool(general, key("showCreatureMarks"), status, key("playerShowMarks"), key("creatureShowMarks"));
    moveBool(general, key("showCreatureNames"), status, key("playerShowName"), key("creatureShowName"));
    moveBool(general, key("showNpcIcons"), status, key("npcShowIcon"));
    root.insert(QStringLiteral("generalOptions"), general);
    root.insert(QStringLiteral("statusOptions"), status);
    return root;
}

QJsonObject migrateFromVersion3(QJsonObject root)
{
    if (!root.contains(QLatin1String("generalOptions")))
        return root;
    QJsonObject general = root.value(QLatin1String("generalOptions")).toObject();
    QJsonObject console = root.value(QLatin1String("consoleOptions")).toObject();
    QJsonObject messages = root.value(QLatin1String("messagesOptions")).toObject();
    moveBool(general, key("showTextualEffects"), messages, key("showTextualEffects"));
    moveBool(general, key("showMessagesInGameWindow"), messages, key("showMessagesInGameWindow"));
    moveBool(general, key("showPrivateMessagesInGameWindow"), messages, key("showPrivateMessagesInGameWindow"));
    moveBool(console, key("showInfoMessages"), messages, key("showInfoMessagesInConsole"));
    moveBool(console, key("showEventMessages"), messages, key("showEventMessagesInConsole"));
    moveBool(console, key("showStatusMessages"), messages, key("showStatusMessagesInConsole"));
    moveBool(console, key("showStatusMessagesOfOthers"), messages, key("showStatusMessagesOfOthersInConsole"));
    moveBool(console, key("showTimestamps"), messages, key("showTimestampsInConsole"));
    moveBool(console, key("showLevels"), messages, key("showLevelsInConsole"));
    moveBool(console, key("showLinkCopyWarning"), messages, key("showLinkCopyWarning"));
    root.insert(QStringLiteral("generalOptions"), general);
    root.remove(QLatin1String("consoleOptions"));
    root.insert(QStringLiteral("messagesOptions"), messages);
    return root;
}

QJsonObject migrateFromVersion4(QJsonObject root)
{
    if (!root.contains(QLatin1String("sidebarWidgetsMangerOptions")))
        return root;
    QJsonObject sidebar = root.value(QLatin1String("sidebarWidgetsMangerOptions")).toObject();
    if (holds(sidebar, key("openWidgetsOrder"), QJsonValue::Array)) {
        const QJsonArray order = sidebar.value(QLatin1String("openWidgetsOrder")).toArray();
        sidebar.remove(QLatin1String("openWidgetsOrder"));
        QJsonArray perSidebar;
        perSidebar.append(order);
        sidebar.insert(QStringLiteral("openWidgetsOrderPerSidebar"), perSidebar);
    }
    root.insert(QStringLiteral("sidebarWidgetsMangerOptions"), sidebar);
    return root;
}

void moveBoolOptions(QJsonObject& section, QJsonObject& options, const std::map<QString, QString>& renames)
{
    for (const auto& [from, to] : renames) {
        if (!holds(section, from, QJsonValue::Bool))
            continue;
        options.insert(to, section.value(from).toBool(false));
        section.remove(from);
    }
}

void moveIntOptions(QJsonObject& section, QJsonObject& options, const std::map<QString, QString>& renames)
{
    for (const auto& [from, to] : renames) {
        if (!holdsInt(section, from))
            continue;
        options.insert(to, section.value(from).toInt(0));
        section.remove(from);
    }
}

void moveDoubleOptions(QJsonObject& section, QJsonObject& options, const std::map<QString, QString>& renames)
{
    for (const auto& [from, to] : renames) {
        if (!holds(section, from, QJsonValue::Double))
            continue;
        options.insert(to, section.value(from).toDouble(0.0));
        section.remove(from);
    }
}

QJsonObject migrateFromVersion5(QJsonObject root)
{
    QJsonObject options;

    if (root.contains(QLatin1String("actionBarsOptions"))) {
        QJsonObject section = root.value(QLatin1String("actionBarsOptions")).toObject();
        moveBoolOptions(section, options, {
            { key("showActionBar1"), key("actionBarShowAction1") },
            { key("showActionBar2"), key("actionBarShowAction2") },
            { key("lockActionBars"), key("actionBarLocked") },
            { key("showHotkey"), key("actionButtonShowHotkey") },
            { key("showAmount"), key("actionButtonShowAmount") },
            { key("showSpellParameters"), key("actionButtonShowSpellParameters") },
            { key("showCooldown"), key("actionButtonShowGraphicalCooldown") },
            { key("showCooldownNumber"), key("actionButtonShowCooldownNumbers") },
            { key("allowTooltip"), key("actionButtonAllowTooltip") },
        });
        root.remove(QLatin1String("actionBarsOptions"));
    }

    if (root.contains(QLatin1String("messagesOptions"))) {
        QJsonObject section = root.value(QLatin1String("messagesOptions")).toObject();
        moveBoolOptions(section, options, {
            { key("showInfoMessagesInConsole"), key("consoleShowInfoMessages") },
            { key("showEventMessagesInConsole"), key("consoleShowEventMessages") },
            { key("showStatusMessagesInConsole"), key("consoleShowStatusMessagesOwn") },
            { key("showStatusMessagesOfOthersInConsole"), key("consoleShowStatusMessagesOfOthers") },
            { key("showTimestampsInConsole"), key("consoleShowTimestamps") },
            { key("showLevelsInConsole"), key("consoleShowLevels") },
            { key("gameWindowShowTextualEffects"), key("gameWindowShowTextualEffects") },
            { key("gameWindowShowMessagesInGameWindow"), key("gameWindowShowMessages") },
            { key("gameWindowShowPrivateMessagesInGameWindow"), key("gameWindowShowPrivateMessages") },
            { key("gameWindowShowOwnSpellsInGameWindow"), key("gameWindowShowOwnSpells") },
            { key("gameWindowShowOthersSpellsInGameWindow"), key("gameWindowShowOthersSpells") },
            { key("showLinkCopyWarning"), key("linkCopyWarningEnabled") },
        });
        root.remove(QLatin1String("messagesOptions"));
    }

    if (root.contains(QLatin1String("statusOptions"))) {
        QJsonObject section = root.value(QLatin1String("statusOptions")).toObject();
        moveBoolOptions(section, options, {
            { key("playerHudEnabled"), key("playerHudEnabled") },
            { key("playerShowName"), key("playerShowName") },
            { key("playerShowHealth"), key("playerShowHealth") },
            { key("playerShowMana"), key("playerShowMana") },
            { key("playerShowMarks"), key("playerShowMarks") },
            { key("creatureHudEnabled"), key("creatureHudEnabled") },
            { key("creatureShowName"), key("creatureShowName") },
            { key("creatureShowHealth"), key("creatureShowHealth") },
            { key("creatureShowMarks"), key("creatureShowMarks") },
            { key("npcShowIcon"), key("creatueShowNpcIcon") },
            { key("showFramesPvp"), key("combatShowPvpFrames") },
            { key("showFrames"), key("combatShowFrames") },
        });
        if (holds(section, key("playerHudStyle"), QJsonValue::String)) {
            const bool bars = section.value(QLatin1String("playerHudStyle")).toString() == QLatin1String("bar");
            options.insert(QStringLiteral("playerHudShowBars"), bars);
            section.remove(QLatin1String("playerHudStyle"));
        }
        root.remove(QLatin1String("statusOptions"));
    }

    if (root.contains(QLatin1String("generalOptions"))) {
        QJsonObject section = root.value(QLatin1String("generalOptions")).toObject();
        moveBoolOptions(section, options, {
            { key("useDefaultKeyboardDelay"), key("keyboardDelayUseDefault") },
            { key("rotateWithCtrl"), key("rotateWithCtrl") },
            { key("rotateWithShift"), key("rotateWithShift") },
            { key("rotateWithAlt"), key("rotateWithAlt") },
            { key("highlightMouseTarget"), key("mouseHighlightTarget") },
            { key("animatedMouseCursor"), key("mouseAnimatedCursor") },
            { key("systemCursor"), key("mouseSystemCursor") },
            { key("autoChase"), key("autoChaseEnabled") },
            { key("quickLootAllCorpsesOnCoordinate"), key("quickLootAllCorpsesOnCoordinateEnabled") },
            { key("showCooldownBar"), key("cooldownBarEnabled") },
            { key("askBeforeBuyingStoreProducts"), key("storeAskBeforeBuyingProducts") },
            { key("stayLoggedInByDefault"), key("sessionRemainLoggedIn") },
            { key("dragAndDropDefaultActionIsMoveAll"), key("dragAndDropDefaultActionIsMoveAll") },
            { key("allowAllToInspectPlayer"), key("inspectPlayerAllowAllEnabled") },
            { key("optimizeConnectionStability"), key("connectionStabilityOptimizationEnabled") },
        });
        moveIntOptions(section, options, {
            { key("controlScheme"), key("controlSchemeIndex") },
            { key("keyboardDelay"), key("keyboardDelayMs") },
        });
        if (holds(section, key("lootScheme"), QJsonValue::String)) {
            const QString scheme = section.value(QLatin1String("lootScheme")).toString();
            int index = 0;
            if (scheme == QLatin1String("shiftright"))
                index = 1;
            else if (scheme == QLatin1String("right"))
                index = 0;
            else if (scheme == QLatin1String("left"))
                index = 2;
            options.insert(QStringLiteral("lootSchemeIndex"), index);
            section.remove(QLatin1String("lootScheme"));
        }
        root.remove(QLatin1String("generalOptions"));
    }

    if (root.contains(QLatin1String("graphicsOptions"))) {
        QJsonObject section = root.value(QLatin1String("graphicsOptions")).toObject();
        moveBoolOptions(section, options, {
            { key("antialiasing"), key("antialiasingEnabled") },
            { key("gameWindowFixedSize"), key("gameWindowScaleOnlyByEvenMultiples") },
            { key("lightEffects"), key("lightEffectsEnabled") },
            { key("quickLogin"), key("quickLogin") },
            { key("vSync"), key("vSyncEnabled") },
        });
        moveIntOptions(section, options, {
            { key("frameRateLimit"), key("frameRateLimit") },
            { key("renderer"), key("rendererIndex") },
        });
        // Both old attenuation values map to the one CIP keeps.
        moveDoubleOptions(section, options, {
            { key("lightAttenuationClouds"), key("lightAttenuationCloudsIndoor") },
            { key("lightAttenuationIndoor"), key("lightAttenuationCloudsIndoor") },
        });
        if (holds(section, key("noFrameRateLimit"), QJsonValue::Bool)) {
            options.insert(QStringLiteral("frameRateLimitEnabled"), !section.value(QLatin1String("noFrameRateLimit")).toBool(false));
            section.remove(QLatin1String("noFrameRateLimit"));
        }
        if (holdsInt(section, key("ambientLight"))) {
            options.insert(QStringLiteral("lightAmbientLevel"), section.value(QLatin1String("ambientLight")).toInt(0) * 0.01);
            section.remove(QLatin1String("ambientLight"));
        }
        if (holdsInt(section, key("levelSeparator"))) {
            options.insert(QStringLiteral("lightAttenuationCloudsIndoor"), section.value(QLatin1String("levelSeparator")).toInt(0) * 0.01);
            section.remove(QLatin1String("levelSeparator"));
        }
        if (holds(section, key("borderlessWindow"), QJsonValue::Bool)) {
            const bool borderless = section.value(QLatin1String("borderlessWindow")).toBool(false);
            if (root.contains(QLatin1String("gameWindowOptions"))) {
                QJsonObject gameWindow = root.value(QLatin1String("gameWindowOptions")).toObject();
                gameWindow.insert(QStringLiteral("clientWindowIsBorderless"), borderless);
                root.insert(QStringLiteral("gameWindowOptions"), gameWindow);
            }
            section.remove(QLatin1String("borderlessWindow"));
        }
        if (holds(section, key("rendererChangeConfirmationRequired"), QJsonValue::Bool)) {
            root.insert(QStringLiteral("rendererChangeConfirmationRequired"),
                        section.value(QLatin1String("rendererChangeConfirmationRequired")).toBool(false));
            section.remove(QLatin1String("rendererChangeConfirmationRequired"));
        }
        root.remove(QLatin1String("graphicsOptions"));
    }

    if (root.contains(QLatin1String("gameWindowOptions"))) {
        QJsonObject section = root.value(QLatin1String("gameWindowOptions")).toObject();
        if (holdsInt(section, key("mapWindowHeight"))) {
            options.insert(QStringLiteral("mapWindowHeight"), section.value(QLatin1String("mapWindowHeight")).toInt(0));
            section.remove(QLatin1String("mapWindowHeight"));
        }
        root.remove(QLatin1String("gameWindowOptions"));
        root.insert(QStringLiteral("clientWindowOptions"), section);
    }

    root.insert(QStringLiteral("options"), options);
    return root;
}

QJsonObject migrateFromVersion6(QJsonObject root)
{
    if (!root.contains(QLatin1String("options")))
        return root;
    QJsonObject options = root.value(QLatin1String("options")).toObject();
    moveBool(options, key("actionBarShowRight"), options, key("actionBarsShowBottom"), key("actionBarsShowRight"));
    moveBool(options, key("actionBarShowLeft"), options, key("actionBarsShowLeft"));
    moveBool(options, key("actionBarShowAction1"), options, key("actionBarShowBottom1"));
    moveBool(options, key("actionBarShowAction2"), options, key("actionBarShowBottom2"));
    moveBool(options, key("actionBarShowAction3"), options, key("actionBarShowBottom3"));
    root.insert(QStringLiteral("options"), options);
    return root;
}

QJsonObject migrateFromVersion7(QJsonObject root)
{
    if (!root.contains(QLatin1String("options")))
        return root;
    QJsonObject options = root.value(QLatin1String("options")).toObject();
    moveBool(options, key("quickLootAllCorpsesOnCoordinateEnabled"), options, key("quickLootAllCorpsesInAreaEnabled"));
    root.insert(QStringLiteral("options"), options);
    return root;
}

QJsonObject migrateFromVersion8(QJsonObject root)
{
    QJsonObject hotkeyOptions = root.value(QLatin1String("hotkeyOptions")).toObject();
    QJsonObject sets = hotkeyOptions.value(QLatin1String("hotkeySets")).toObject();
    sets.insert(QStringLiteral("Monk"), THotkeyOptions::builtInSets().value(QLatin1String("Monk")));
    hotkeyOptions.insert(QStringLiteral("hotkeySets"), sets);
    root.insert(QStringLiteral("hotkeyOptions"), hotkeyOptions);
    return root;
}

QJsonObject migrate(QJsonObject root)
{
    using Step = QJsonObject (*)(QJsonObject);
    static constexpr Step Steps[CurrentVersion] = {
        migrateFromVersion0, migrateFromVersion1, migrateFromVersion2,
        migrateFromVersion3, migrateFromVersion4, migrateFromVersion5,
        migrateFromVersion6, migrateFromVersion7, migrateFromVersion8,
    };
    const QString versionKey = key(VersionKey);
    for (int from = 0; from < CurrentVersion; ++from) {
        const int version = root.value(versionKey).toInt(-1);
        // A missing version counts as 0.
        if (version != from && !(from == 0 && version == -1))
            continue;
        root = Steps[from](std::move(root));
        root.insert(versionKey, from + 1);
    }
    return root;
}

}
