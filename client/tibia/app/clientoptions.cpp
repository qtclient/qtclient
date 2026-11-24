#include "clientoptions.h"

#include "config/clientoptiondefaults.h"
#include "config/clientoptions.h"
#include "config/clientoptionsmigration.h"

#include <QFile>
#include <QJsonDocument>

namespace tibia::app {

namespace {

/// The stored value of a CIP option as JSON.
QJsonValue optionValue(const config::TClientOptions& options, const QString& name)
{
    return options.toJson().object().value(QLatin1String("options")).toObject().value(name);
}

/// Sets one option: a CIP option through its typed setter, any other name as
/// a host option.
void setOptionValue(config::TClientOptions& options, const QString& name, const QJsonValue& value)
{
    config::EClientOption id;
    if (!config::TClientOptions::optionId(name, id)) {
        options.setExtraOption(name, value);
        return;
    }
    const QJsonValue current = optionValue(options, name);
    if (current.isBool())
        options.setBoolOption(id, value.toBool());
    else if (current.isString())
        options.setStringOption(id, value.toString());
    else if (config::isFloatOption(name))
        options.setFloatOption(id, float(value.toDouble()));
    else
        options.setIntOption(id, value.toInt());
}

QJsonObject merged(QJsonObject base, const QJsonObject& over)
{
    for (auto it = over.begin(); it != over.end(); ++it)
        base.insert(it.key(), *it);
    return base;
}

}

QString ClientOptions::userFilePath()
{
    return config::TClientOptions::userFilePath();
}

ClientOptions ClientOptions::load()
{
    ClientOptions o;
    o.m_root = config::TClientOptions::instance()->toJson().object();
    return o;
}

void ClientOptions::store(const QString& name, const QJsonValue& value)
{
    config::TClientOptions* options = config::TClientOptions::instance();
    const QJsonValue before = optionValue(*options, name);
    if (before == value)
        return;
    setOptionValue(*options, name, value);
    if (optionValue(*options, name) == before)
        return;
    options->saveToJsonFile();
}

void ClientOptions::storeAll(const QJsonObject& changes)
{
    config::TClientOptions* options = config::TClientOptions::instance();
    const QJsonObject current = options->toJson().object();
    bool changed = false;
    for (auto section = changes.begin(); section != changes.end(); ++section) {
        if (section.key() == QLatin1String("options")) {
            const QJsonObject values = section->toObject();
            for (auto it = values.begin(); it != values.end(); ++it) {
                const QJsonValue before = optionValue(*options, it.key());
                setOptionValue(*options, it.key(), *it);
                changed |= optionValue(*options, it.key()) != before;
            }
            continue;
        }
        const QJsonValue value = section->isObject() && current.value(section.key()).isObject()
                                     ? QJsonValue(merged(current.value(section.key()).toObject(), section->toObject()))
                                     : *section;
        options->fromJson({ { section.key(), value } });
        changed |= options->toJson().object().value(section.key()) != current.value(section.key());
    }
    if (changed)
        options->saveToJsonFile();
}

void ClientOptions::storeChanged(const config::TClientOptions& pending)
{
    const QJsonObject current = config::TClientOptions::instance()->toJson().object();
    const QJsonObject next = pending.toJson().object();
    QJsonObject changes;
    for (auto section = next.begin(); section != next.end(); ++section) {
        if (section.key() != QLatin1String("options")) {
            if (current.value(section.key()) != *section)
                changes.insert(section.key(), *section);
            continue;
        }
        const QJsonObject before = current.value(section.key()).toObject();
        const QJsonObject after = section->toObject();
        QJsonObject options;
        for (auto it = after.begin(); it != after.end(); ++it) {
            if (before.value(it.key()) != *it)
                options.insert(it.key(), *it);
        }
        if (!options.isEmpty())
            changes.insert(section.key(), options);
    }
    storeAll(changes);
}

int ClientOptions::rendererIndex() const
{
    return options().value(QLatin1String("rendererIndex")).toInt(0);
}

bool ClientOptions::vSyncEnabled() const
{
    return options().value(QLatin1String("vSyncEnabled")).toBool(false);
}

}
