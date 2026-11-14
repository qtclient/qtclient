#pragma once

#include <QString>

namespace tibia::config {

/// A value of the client's settings file (`[group] key`), `defaultValue` when unset.
qint64 clientSetting(const QString& group, const QString& key, qint64 defaultValue);
/// As clientSetting, held at `maximum`: a larger stored value is written back as `maximum`.
qint64 clientSettingAtMost(const QString& group, const QString& key, qint64 defaultValue, qint64 maximum);

}
