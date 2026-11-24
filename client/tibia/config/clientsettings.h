#pragma once

#include <QString>

#include <memory>

namespace tibia::config {

class TTibiaConfig;

/// Host: CipSoft reads every client setting through the application's
/// TTibiaConfig (config.ini). These free functions read the same object for
/// the callers that have no path to it; the application installs its config
/// with setProcessTibiaConfig (TGameApplicationBase's constructor). Before
/// that they read config.ini through a file system helper of the application
/// data location.
void setProcessTibiaConfig(std::shared_ptr<TTibiaConfig> config);
std::shared_ptr<TTibiaConfig> processTibiaConfig();

/// A value of the client's settings file (`[group] key`), `defaultValue` when unset.
qint64 clientSetting(const QString& group, const QString& key, qint64 defaultValue);
/// A floating-point value of the client's settings file, `defaultValue` when unset.
double clientSettingReal(const QString& group, const QString& key, double defaultValue);
/// A text value of the client's settings file, `defaultValue` when unset.
QString clientSettingString(const QString& group, const QString& key, const QString& defaultValue);
/// As clientSetting, held at `maximum`: a larger stored value is written back as `maximum`.
qint64 clientSettingAtMost(const QString& group, const QString& key, qint64 defaultValue, qint64 maximum);
/// As clientSetting, held at `minimum`: a smaller stored value is written back as `minimum`.
qint64 clientSettingAtLeast(const QString& group, const QString& key, qint64 defaultValue, qint64 minimum);

}
