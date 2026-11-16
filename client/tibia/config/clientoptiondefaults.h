#pragma once

#include <QJsonObject>

namespace tibia {
namespace config {

/// TClientOptions' defaults: the "options" section a fresh options file holds
/// and the options dialog resets pages to. The control scheme default is the
/// list index of Left Smart-Click (2).
const QJsonObject& defaultClientOptions();

/// The "options" section as TClientOptions reads it over its defaults, type by
/// type: a bool, number or string option takes the stored value only when it
/// has that JSON type. Ints are clamped to [0, INT_MAX], floats to
/// [0, FLT_MAX]; then frameRateLimit to [10, 200], the light levels and effect
/// opacities to [0, 1], playerHudArcOpacity to [0.2, 1] and playerHudArcHeight
/// to [0.1, 1]. A string option not stored as a string reads as empty. Keys
/// that are no CIP option are kept as stored.
QJsonObject readClientOptions(const QJsonObject& stored);

/// Button-bar ids in EButtons order (ButtonBarButtons), then the pack's
/// default-enabled shortcuts.
QStringList defaultControlButtonIds();

/// Factory controlButtonsOptions: skills, battle list, spells, VIP, lens help,
/// quests, compendium, cyclopedia, highscores, player guide and manage
/// shortcuts enabled in that order; every other button disabled.
QJsonObject defaultControlButtonsOptions();

/// The stored button arrangement as the options load reads it: names matched
/// case-insensitively, then every table button in neither list appended to
/// enabledButtons in table order.
QJsonObject reconcileControlButtons(const QJsonObject& section);

}
}
