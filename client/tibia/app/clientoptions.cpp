#include "clientoptions.h"

#include "config/clientoptiondefaults.h"
#include "config/clientoptions.h"
#include "config/clientoptionsmigration.h"

#include <QFile>
#include <QJsonDocument>

#include <QStringList>
#include <QTimer>

namespace tibia::app {

namespace {

/// Set while ClientOptions::store / storeAll write: they announce and save themselves.
bool g_storing = false;

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

ClientOptions ClientOptions::readStoredFile()
{
    ClientOptions o;
    QFile file(userFilePath());
    if (file.open(QIODevice::ReadOnly))
        o.m_root = config::clientoptionsmigration::migrate(QJsonDocument::fromJson(file.readAll()).object());
    return o;
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
    g_storing = true;
    setOptionValue(*options, name, value);
    g_storing = false;
    if (optionValue(*options, name) == before)
        return;
    options->saveToJsonFile();
    Q_EMIT ClientOptionsNotifier::instance()->optionChanged(name);
}

void ClientOptions::storeAll(const QJsonObject& changes)
{
    config::TClientOptions* options = config::TClientOptions::instance();
    const QJsonObject current = options->toJson().object();
    QStringList changed;
    g_storing = true;
    for (auto section = changes.begin(); section != changes.end(); ++section) {
        if (section.key() == QLatin1String("options")) {
            const QJsonObject values = section->toObject();
            for (auto it = values.begin(); it != values.end(); ++it) {
                const QJsonValue before = optionValue(*options, it.key());
                setOptionValue(*options, it.key(), *it);
                if (optionValue(*options, it.key()) != before)
                    changed << it.key();
            }
            continue;
        }
        const QJsonValue value = section->isObject() && current.value(section.key()).isObject()
                                     ? QJsonValue(merged(current.value(section.key()).toObject(), section->toObject()))
                                     : *section;
        options->fromJson({ { section.key(), value } });
        if (options->toJson().object().value(section.key()) != current.value(section.key()))
            changed << section.key();
    }
    g_storing = false;
    if (changed.isEmpty())
        return;
    options->saveToJsonFile();
    for (const QString& name : std::as_const(changed))
        Q_EMIT ClientOptionsNotifier::instance()->optionChanged(name);
}

ClientOptionsNotifier::ClientOptionsNotifier()
{
    config::TClientOptions* options = config::TClientOptions::instance();
    const auto changed = [this, options](const QString& name) {
        if (g_storing)
            return;
        if (!m_saveScheduled) {
            m_saveScheduled = true;
            QTimer::singleShot(0, this, [this, options] {
                m_saveScheduled = false;
                options->saveToJsonFile();
            });
        }
        Q_EMIT optionChanged(name);
    };
    connect(options, &config::TClientOptions::optionChanged, this,
            [changed](const config::EClientOption id) { changed(config::TClientOptions::optionName(id)); });
    connect(options, &config::TClientOptions::clientWindowOptionsChanged, this,
            [changed] { changed(QStringLiteral("clientWindowOptions")); });
}

ClientOptionsNotifier* ClientOptionsNotifier::instance()
{
    static ClientOptionsNotifier notifier;
    return &notifier;
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
