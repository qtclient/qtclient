#include "clientoptions.h"

#include "config/clientoptiondefaults.h"
#include "config/clientoptionsmigration.h"
#include "config/specialconditionsoptions.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>

namespace {

constexpr auto ControlButtonsSection = "controlButtonsOptions";
constexpr auto SpecialConditionsSection = "specialConditionsOptions";
constexpr auto VersionKey = "clientOptionsVersion";

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

/// A file that names its clientOptionsVersion is a CIP options file and is
/// upgraded to the current layout. qtclient's own user file always carries
/// the current version (see writeUserFile).
QJsonObject readOptionsFile(const QString& path)
{
    QJsonObject root = readObject(path);
    if (root.contains(QLatin1String(VersionKey)))
        root = config::clientoptionsmigration::migrate(std::move(root));
    return root;
}

bool writeUserFile(const QString& path, QJsonObject root)
{
    root.insert(QLatin1String(VersionKey), config::clientoptionsmigration::CurrentVersion);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson());
    return file.commit();
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
    // Code owns the factory defaults TClientOptions resets to: the options
    // map, the button bar and the special conditions. The user file overrides
    // them. The snapshot must not supply those sections.
    QJsonObject resource = readOptionsFile(QStringLiteral(":/conf/clientoptions.json"));
    resource.remove(QLatin1String(ControlButtonsSection));
    resource.remove(QLatin1String(SpecialConditionsSection));
    QJsonObject codeDefaults;
    codeDefaults.insert(QStringLiteral("options"), config::defaultClientOptions());
    codeDefaults.insert(QLatin1String(ControlButtonsSection), config::defaultControlButtonsOptions());
    codeDefaults.insert(QLatin1String(SpecialConditionsSection), config::SpecialConditionsOptions::defaults().toJson());
    o.m_root = merged(merged(std::move(resource), codeDefaults), readOptionsFile(userFilePath()));
    o.m_root.insert(QLatin1String(ControlButtonsSection),
                    config::reconcileControlButtons(o.m_root.value(QLatin1String(ControlButtonsSection)).toObject()));
    return o;
}

void ClientOptions::store(const QString& name, const QJsonValue& value)
{
    // Nothing happens when the option already holds the value (its default
    // included); otherwise it is set and optionChanged names it.
    if (load().options().value(name) == value)
        return;
    const QString path = userFilePath();
    QJsonObject root = readOptionsFile(path);
    QJsonObject options = root.value(QLatin1String("options")).toObject();
    options.insert(name, value);
    root.insert(QLatin1String("options"), options);
    if (!writeUserFile(path, std::move(root)))
        return;
    Q_EMIT ClientOptionsNotifier::instance()->optionChanged(name);
}

void ClientOptions::storeAll(const QJsonObject& changes)
{
    const QJsonObject current = load().root();
    const QString path = userFilePath();
    QJsonObject root = readOptionsFile(path);
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
    if (!writeUserFile(path, std::move(root)))
        return;
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
    return options().value(QLatin1String("vSyncEnabled")).toBool(false);
}

}
