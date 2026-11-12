#pragma once

#include <QJsonObject>

namespace tibia {
namespace config {

/// CIP's default TClientOptions (, filled by): the
/// "options" section the options dialog resets pages to. The control scheme
/// default is Left Smart-Click ((3), list index 2).
const QJsonObject& defaultClientOptions();

/// Button-bar ids in CIP EButtons order (ButtonBarButtons).
QStringList defaultControlButtonIds();

/// Factory controlButtonsOptions: every shortcut enabled, in defaultControlButtonIds order.
QJsonObject defaultControlButtonsOptions();

/// Keeps a saved enabled/disabled arrangement. Ids from the code table that
/// appear in neither list are appended to enabledButtons in table order.
QJsonObject reconcileControlButtons(const QJsonObject& section);

}
}
