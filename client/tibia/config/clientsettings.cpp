#include "config/clientsettings.h"

#include <QSettings>

namespace tibia::config {

qint64 clientSetting(const QString& group, const QString& key, const qint64 defaultValue)
{
    QSettings settings;
    settings.beginGroup(group);
    const qint64 value = settings.value(key, defaultValue).toLongLong();
    settings.endGroup();
    return value;
}

double clientSettingReal(const QString& group, const QString& key, const double defaultValue)
{
    QSettings settings;
    settings.beginGroup(group);
    const double value = settings.value(key, defaultValue).toDouble();
    settings.endGroup();
    return value;
}

qint64 clientSettingAtMost(const QString& group, const QString& key, const qint64 defaultValue, const qint64 maximum)
{
    const qint64 value = clientSetting(group, key, defaultValue);
    if (value <= maximum)
        return value;
    QSettings settings;
    settings.beginGroup(group);
    settings.setValue(key, maximum);
    settings.endGroup();
    return maximum;
}

qint64 clientSettingAtLeast(const QString& group, const QString& key, const qint64 defaultValue, const qint64 minimum)
{
    const qint64 value = clientSetting(group, key, defaultValue);
    if (value >= minimum)
        return value;
    QSettings settings;
    settings.beginGroup(group);
    settings.setValue(key, minimum);
    settings.endGroup();
    return minimum;
}

}
