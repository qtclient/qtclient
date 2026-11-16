#pragma once

#include <QJsonObject>
#include <QString>

#include <map>

namespace tibia {
namespace config {

/// TClientOptions' options-file upgrade: CIP files carry clientOptionsVersion
/// and the loader rewrites older layouts one version at a time.
namespace clientoptionsmigration {

/// The version current CIP clients write.
constexpr int CurrentVersion = 9;

/// Runs every migration from the file's clientOptionsVersion (missing = 0)
/// up to CurrentVersion, stamping the version after each step.
QJsonObject migrate(QJsonObject root);

/// 0 -> 1: hotkey sets become {chatOff, chatOn} binding lists seeded with
/// GlobalShortcuts; classicControl becomes controlScheme.
QJsonObject migrateFromVersion0(QJsonObject root);
/// 1 -> 2: highlightObjectUnderMouse becomes highlightMouseTarget.
QJsonObject migrateFromVersion1(QJsonObject root);
/// 2 -> 3: creature HUD flags move from generalOptions to statusOptions.
QJsonObject migrateFromVersion2(QJsonObject root);
/// 3 -> 4: message flags move into messagesOptions; consoleOptions goes.
QJsonObject migrateFromVersion3(QJsonObject root);
/// 4 -> 5: the sidebar's openWidgetsOrder becomes per-sidebar.
QJsonObject migrateFromVersion4(QJsonObject root);
/// 5 -> 6: the per-page sections fold into the flat "options" map.
QJsonObject migrateFromVersion5(QJsonObject root);
/// 6 -> 7: action bar visibility options renamed.
QJsonObject migrateFromVersion6(QJsonObject root);
/// 7 -> 8: quick loot on coordinate becomes quick loot in area.
QJsonObject migrateFromVersion7(QJsonObject root);
/// 8 -> 9: adds the default Monk hotkey set.
QJsonObject migrateFromVersion8(QJsonObject root);

/// Copies each bool `section[old]` to `options[new]` and drops it from `section`.
void moveBoolOptions(QJsonObject& section, QJsonObject& options, const std::map<QString, QString>& renames);
/// Same for integral numbers.
void moveIntOptions(QJsonObject& section, QJsonObject& options, const std::map<QString, QString>& renames);
/// Same for any number.
void moveDoubleOptions(QJsonObject& section, QJsonObject& options, const std::map<QString, QString>& renames);

}

}
}
