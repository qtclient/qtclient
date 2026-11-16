#pragma once

#include <QJsonObject>

namespace tibia {
namespace config {

/// TClientOptions' defaults: the "options" section a fresh options file holds
/// and the options dialog resets pages to. The control scheme default is the
/// list index of Left Smart-Click (2).
const QJsonObject& defaultClientOptions();

/// CIP's float options; every other numeric option is an int.
bool isFloatOption(const QString& name);

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
