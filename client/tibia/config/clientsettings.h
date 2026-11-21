#pragma once

#include <QString>

namespace tibia::config {

/// A value of the client's settings file (`[group] key`), `defaultValue` when unset.
qint64 clientSetting(const QString& group, const QString& key, qint64 defaultValue);
/// A floating-point value of the client's settings file, `defaultValue` when unset.
double clientSettingReal(const QString& group, const QString& key, double defaultValue);
/// As clientSetting, held at `maximum`: a larger stored value is written back as `maximum`.
qint64 clientSettingAtMost(const QString& group, const QString& key, qint64 defaultValue, qint64 maximum);
/// As clientSetting, held at `minimum`: a smaller stored value is written back as `minimum`.
qint64 clientSettingAtLeast(const QString& group, const QString& key, qint64 defaultValue, qint64 minimum);

}
