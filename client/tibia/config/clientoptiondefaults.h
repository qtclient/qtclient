#pragma once

#include <QJsonObject>

namespace tibia::config {

/// CIP's default TClientOptions (FUN_1009bb440, filled by FUN_1009ad850): the
/// "options" section the options dialog resets pages to. The control scheme
/// default is Left Smart-Click (FUN_100e543c0(3), list index 2).
const QJsonObject& defaultClientOptions();

/// Button-bar ids in CIP EButtons order (FUN_100a22cf0 / ButtonBarButtons).
QStringList defaultControlButtonIds();

/// Factory controlButtonsOptions: every shortcut enabled, in defaultControlButtonIds order.
QJsonObject defaultControlButtonsOptions();

/// Keeps a saved enabled/disabled arrangement. Ids from the code table that
/// appear in neither list are appended to enabledButtons in table order.
QJsonObject reconcileControlButtons(const QJsonObject& section);

}
