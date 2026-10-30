#include "clientoptions.h"

#include "config/clientoptiondefaults.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>

namespace {

constexpr auto ControlButtonsSection = "controlButtonsOptions";

}

namespace tibia::app {

namespace {

QJsonObject readObject(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

QJsonObject merged(QJsonObject base, const QJsonObject& over)
{
    for (auto it = over.begin(); it != over.end(); ++it) {
        if (it->isObject() && base.value(it.key()).isObject())
            base.insert(it.key(), merged(base.value(it.key()).toObject(), it->toObject()));
        else
            base.insert(it.key(), *it);
    }
    return base;
}

}

QString ClientOptions::userFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/conf/clientoptions.json");
}

ClientOptions ClientOptions::load()
{
    ClientOptions o;
    // CIP's shipped clientoptions.json is a machine snapshot (hotkeys, cookies).
    // Code owns factory defaults: the options map (FUN_1009ad850) and the button
    // bar (EButtons / ButtonBarButtons). The user file overrides both. The
    // snapshot must not supply controlButtonsOptions.
    QJsonObject resource = readObject(QStringLiteral(":/conf/clientoptions.json"));
    resource.remove(QLatin1String(ControlButtonsSection));
    QJsonObject codeDefaults;
    codeDefaults.insert(QStringLiteral("options"), config::defaultClientOptions());
    codeDefaults.insert(QLatin1String(ControlButtonsSection), config::defaultControlButtonsOptions());
    o.m_root = merged(merged(std::move(resource), codeDefaults), readObject(userFilePath()));
    o.m_root.insert(QLatin1String(ControlButtonsSection),
                    config::reconcileControlButtons(o.m_root.value(QLatin1String(ControlButtonsSection)).toObject()));
    return o;
}

void ClientOptions::store(const QString& name, const QJsonValue& value)
{
    const QString path = userFilePath();
    QJsonObject root = readObject(path);
    QJsonObject options = root.value(QLatin1String("options")).toObject();
    if (options.value(name) == value)
        return;
    options.insert(name, value);
    root.insert(QLatin1String("options"), options);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(root).toJson());
    file.commit();
    Q_EMIT ClientOptionsNotifier::instance()->optionChanged(name);
}

void ClientOptions::storeAll(const QJsonObject& changes)
{
    const QJsonObject current = load().root();
    const QString path = userFilePath();
    QJsonObject root = readObject(path);
    QStringList changed;
    for (auto section = changes.begin(); section != changes.end(); ++section) {
        if (!section->isObject()) {
            if (current.value(section.key()) != *section) {
                root.insert(section.key(), *section);
                changed << section.key();
            }
            continue;
        }
        const bool perOption = section.key() == QLatin1String("options");
        const QJsonObject currentValues = current.value(section.key()).toObject();
        QJsonObject stored = root.value(section.key()).toObject();
        const QJsonObject values = section->toObject();
        bool sectionChanged = false;
        for (auto it = values.begin(); it != values.end(); ++it) {
            if (currentValues.value(it.key()) == *it)
                continue;
            stored.insert(it.key(), *it);
            sectionChanged = true;
            if (perOption)
                changed << it.key();
        }
        if (!sectionChanged)
            continue;
        root.insert(section.key(), stored);
        if (!perOption)
            changed << section.key();
    }
    if (changed.isEmpty())
        return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(root).toJson());
    file.commit();
    for (const QString& name : std::as_const(changed))
        Q_EMIT ClientOptionsNotifier::instance()->optionChanged(name);
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
    return options().value(QLatin1String("vSyncEnabled")).toBool(true);
}

}
