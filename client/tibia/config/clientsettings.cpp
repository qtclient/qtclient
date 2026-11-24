#include "config/clientsettings.h"

#include "config/tibiaconfig.h"
#include "shared/tibiafilesystemhelper.h"

#include <QMutex>
#include <QMutexLocker>

namespace tibia::config {

namespace {

std::shared_ptr<TTibiaConfig>& installedTibiaConfig()
{
    static std::shared_ptr<TTibiaConfig> config;
    return config;
}

/// Host: the free functions are also called off the GUI thread (render
/// thread statics); the shared QSettings' group state is not thread safe.
QMutex& settingsMutex()
{
    static QMutex mutex;
    return mutex;
}

}

void setProcessTibiaConfig(std::shared_ptr<TTibiaConfig> config)
{
    const QMutexLocker lock(&settingsMutex());
    installedTibiaConfig() = std::move(config);
}

std::shared_ptr<TTibiaConfig> processTibiaConfig()
{
    const QMutexLocker lock(&settingsMutex());
    std::shared_ptr<TTibiaConfig>& config = installedTibiaConfig();
    // Read before the application made its config: the same file through a
    // file system helper of the application data location.
    if (!config)
        config = std::make_shared<TTibiaConfig>(shared::TTibiaFileSystemHelper());
    return config;
}

qint64 clientSetting(const QString& group, const QString& key, const qint64 defaultValue)
{
    const std::shared_ptr<TTibiaConfig> config = processTibiaConfig();
    const QMutexLocker lock(&settingsMutex());
    return config->clientSetting(group, key, defaultValue);
}

double clientSettingReal(const QString& group, const QString& key, const double defaultValue)
{
    const std::shared_ptr<TTibiaConfig> config = processTibiaConfig();
    const QMutexLocker lock(&settingsMutex());
    return config->clientSettingReal(group, key, defaultValue);
}

QString clientSettingString(const QString& group, const QString& key, const QString& defaultValue)
{
    const std::shared_ptr<TTibiaConfig> config = processTibiaConfig();
    const QMutexLocker lock(&settingsMutex());
    return config->clientSettingString(group, key, defaultValue);
}

qint64 clientSettingAtMost(const QString& group, const QString& key, const qint64 defaultValue, const qint64 maximum)
{
    const std::shared_ptr<TTibiaConfig> config = processTibiaConfig();
    const QMutexLocker lock(&settingsMutex());
    return config->clientSettingAtMost(group, key, defaultValue, maximum);
}

qint64 clientSettingAtLeast(const QString& group, const QString& key, const qint64 defaultValue, const qint64 minimum)
{
    const std::shared_ptr<TTibiaConfig> config = processTibiaConfig();
    const QMutexLocker lock(&settingsMutex());
    return config->clientSettingAtLeast(group, key, defaultValue, minimum);
}

}
