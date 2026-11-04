#include "mods/modloader.h"
#include "mods/mod_entry.h"
#include "mods/registry.h"

#include "qtclient/mod_api_v1.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QLoggingCategory>

#include <algorithm>
#include <cstring>

Q_LOGGING_CATEGORY(lcModLoader, "tibia.mods.loader")

namespace tibia::mods {
namespace {

struct PackMeta {
    QString dir;
    QString id;
    QString qmlModule;
    QString hostBuildId;
    int api = 0;
    int priority = 100;
    bool autoload = true;
    ModIdentity identity;
    ModUrls urls;
};

ModUrls readUrlsObject(const QJsonObject& urlsObj, int priority)
{
    ModUrls u;
    u.priority = priority;
    u.loginBase = urlsObj.value(QStringLiteral("loginBase")).toString().toStdString();
    u.loginPath = urlsObj.value(QStringLiteral("loginPath")).toString().toStdString();
    u.clientServicesPath = urlsObj.value(QStringLiteral("clientServicesPath")).toString().toStdString();
    u.createAccountPath = urlsObj.value(QStringLiteral("createAccountPath")).toString().toStdString();
    u.assetBase = urlsObj.value(QStringLiteral("assetBase")).toString().toStdString();
    u.paymentsBase = urlsObj.value(QStringLiteral("paymentsBase")).toString().toStdString();
    const QJsonObject pages = urlsObj.value(QStringLiteral("pages")).toObject();
    for (auto it = pages.begin(); it != pages.end(); ++it) {
        const QString v = it.value().toString();
        if (!v.isEmpty())
            u.pages.emplace(it.key().toStdString(), v.toStdString());
    }
    return u;
}

bool readMeta(const QString& packDir, PackMeta* out)
{
    QFile f(QDir(packDir).filePath(QStringLiteral("mod.json")));
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    if (root.isEmpty())
        return false;
    out->dir = packDir;
    out->id = root.value(QStringLiteral("id")).toString();
    out->api = root.value(QStringLiteral("QTCLIENT_MOD_API")).toInt(0);
    out->priority = root.value(QStringLiteral("priority")).toInt(100);
    out->autoload = root.value(QStringLiteral("autoload")).toBool(true);
    out->qmlModule = root.value(QStringLiteral("qmlModule")).toString();
    out->hostBuildId = root.value(QStringLiteral("hostBuildId")).toString();
    const QJsonObject id = root.value(QStringLiteral("identity")).toObject();
    out->identity.id = out->id.toStdString();
    out->identity.displayName = id.value(QStringLiteral("displayName")).toString().toStdString();
    out->identity.organization = id.value(QStringLiteral("organization")).toString().toStdString();
    out->identity.organizationDomain =
        id.value(QStringLiteral("organizationDomain")).toString().toStdString();
    out->identity.bundleId = id.value(QStringLiteral("bundleId")).toString().toStdString();
    out->identity.cdnBaseUrl = id.value(QStringLiteral("cdnBaseUrl")).toString().toStdString();
    out->identity.priority = out->priority;
    out->urls = readUrlsObject(root.value(QStringLiteral("urls")).toObject(), out->priority);
    return !out->id.isEmpty();
}

void registerPackQmlPaths(const PackMeta& meta)
{
    const QDir dir(meta.dir);
    const QString qmlDir = dir.filePath(QStringLiteral("qml"));
    if (QDir(qmlDir).exists()) {
        addPackQmlImportPath(qmlDir.toStdString());
        const QString moduleName =
            meta.qmlModule.isEmpty() ? meta.id : meta.qmlModule;
        const QString moduleDir = QDir(qmlDir).filePath(moduleName);
        if (!QFile::exists(QDir(moduleDir).filePath(QStringLiteral("qmldir")))) {
            qCWarning(lcModLoader) << "mod" << meta.id
                                   << "missing FS QML module"
                                   << moduleDir + QStringLiteral("/qmldir");
        }
    }
    const QString overridesDir = dir.filePath(QStringLiteral("overrides"));
    if (QDir(overridesDir).exists())
        addPackOverrideRoot(overridesDir.toStdString());
}

bool& skipLoadFlag()
{
    static bool skip = false;
    return skip;
}

bool& forceDefaultFlag()
{
    static bool force = false;
    return force;
}

QString findNamedPack(const QStringList& tipRoots, const QString& name)
{
    for (const QString& root : tipRoots) {
        const QString path = QDir(root).filePath(name);
        if (QFile::exists(QDir(path).filePath(QStringLiteral("mod.json"))))
            return path;
    }
    return {};
}

/// Collect autoload packs under tip roots. Sorted by priority descending so
/// lower priority numbers register overrides last (PackQmlInterceptor: later wins).
QStringList collectTipPacks(const QStringList& tipRoots)
{
    struct Candidate {
        QString path;
        int priority = 100;
    };
    QList<Candidate> found;
    for (const QString& root : tipRoots) {
        QDir dir(root);
        if (!dir.exists())
            continue;
        for (const QFileInfo& info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            if (info.fileName().startsWith(QLatin1Char('_')))
                continue;
            PackMeta meta;
            if (!readMeta(info.absoluteFilePath(), &meta) || !meta.autoload)
                continue;
            found.append({info.absoluteFilePath(), meta.priority});
        }
    }
    std::sort(found.begin(), found.end(), [](const Candidate& a, const Candidate& b) {
        if (a.priority != b.priority)
            return a.priority > b.priority; // higher number first
        return a.path < b.path;
    });
    QStringList out;
    out.reserve(found.size());
    for (const Candidate& c : found)
        out.append(c.path);
    return out;
}

QStringList collectEnvPacks(const QString& envPath)
{
    if (envPath.isEmpty())
        return {};
    const QString cleaned = QDir::cleanPath(envPath);
    if (QFile::exists(QDir(cleaned).filePath(QStringLiteral("mod.json"))))
        return {cleaned};
    QDir dir(cleaned);
    if (!dir.exists()) {
        qCWarning(lcModLoader) << "QTCLIENT_MODS_DIR missing" << envPath;
        return {};
    }
    return collectTipPacks({cleaned});
}

void invokeLinkedEntries(const ModContext* earlyCtx)
{
#if defined(QTCLIENT_HAS_LINKED_PACK)
    qCInfo(lcModLoader) << "invoking linked pack entries";
    qtclient_invoke_linked_mods(earlyCtx);
#else
    Q_UNUSED(earlyCtx);
#endif
}

} // namespace

void ModLoader::setSkipLoad(bool skip)
{
    skipLoadFlag() = skip;
}

bool ModLoader::skipLoad()
{
    return skipLoadFlag();
}

void ModLoader::setForceDefaultMod(bool force)
{
    forceDefaultFlag() = force;
}

bool ModLoader::forceDefaultMod()
{
    return forceDefaultFlag();
}

QStringList ModLoader::tipModsRoots()
{
    QStringList roots;
    if (QCoreApplication::instance()) {
        const QDir app(QCoreApplication::applicationDirPath());
        const QStringList raw = {
            app.filePath(QStringLiteral("mods")),
            app.absoluteFilePath(QStringLiteral("../Resources/mods")),
            app.absoluteFilePath(QStringLiteral("../mods")),
        };
        for (const QString& r : raw) {
            const QString cleaned = QDir::cleanPath(r);
            if (!cleaned.isEmpty() && !roots.contains(cleaned))
                roots << cleaned;
        }
    }
    return roots;
}

QStringList ModLoader::defaultRoots()
{
    return tipModsRoots();
}

bool ModLoader::loadFromDirectory(const QString& packDir, const ModContext* earlyCtx)
{
    Q_UNUSED(earlyCtx);
    const QString pack = QDir::cleanPath(packDir);
    PackMeta meta;
    if (!readMeta(pack, &meta)) {
        qCWarning(lcModLoader) << "invalid mod.json in" << pack;
        return false;
    }
    if (meta.api < kMinApi || meta.api > kSupportedApi) {
        qCWarning(lcModLoader) << "mod" << meta.id << "API" << meta.api
                               << "outside host range" << kMinApi << ".." << kSupportedApi;
        return false;
    }
    meta.dir = pack;

    if (!meta.hostBuildId.isEmpty()) {
        const qtclient::ModApiV1* api = qtclient_mod_host_api_v1();
        const char* hostId = api && api->hostBuildId ? api->hostBuildId : "";
        if (std::strcmp(hostId, meta.hostBuildId.toUtf8().constData()) != 0) {
            // Default strict on desktop + WASM; set QTCLIENT_MOD_STRICT_HOST=0 to relax (DEV).
            const bool strict = qEnvironmentVariableIsSet("QTCLIENT_MOD_STRICT_HOST")
                ? qEnvironmentVariableIntValue("QTCLIENT_MOD_STRICT_HOST") != 0
                : true;
            qCWarning(lcModLoader) << "mod" << meta.id << "hostBuildId" << meta.hostBuildId
                                   << "!= host" << hostId
                                   << (strict ? "(reject)" : "(continuing; set QTCLIENT_MOD_STRICT_HOST=1 to reject)");
            if (strict)
                return false;
        }
    }

#if !defined(QTCLIENT_HAS_LINKED_PACK)
    // Stock tip: FS urls/identity/QML only — leftover native/ trees fail closed.
    if (QFile::exists(QDir(pack).filePath(QStringLiteral("native")))) {
        qCCritical(lcModLoader) << "mod" << meta.id
                                << "has native/ but this tip was not built with a linked pack"
                                << "(rebuild qtclient with -DQTCLIENT_LINKED_PACK=… "
                                   "or -DQTCLIENT_LINKED_PACKS=…)";
        return false;
    }
#endif

    if (!meta.identity.displayName.empty())
        registerMod({meta.identity, nullptr});
    if (!meta.urls.empty())
        registerModUrls(meta.urls);
    registerPackQmlPaths(meta);

#if defined(QTCLIENT_HAS_LINKED_PACK)
    qCInfo(lcModLoader) << "loaded tip pack" << meta.id << "priority" << meta.priority << "from" << pack;
    return true;
#else
    if (!meta.urls.empty() || !meta.identity.displayName.empty()
        || QDir(QDir(pack).filePath(QStringLiteral("qml"))).exists()
        || QDir(QDir(pack).filePath(QStringLiteral("overrides"))).exists()) {
        qCInfo(lcModLoader) << "loaded FS pack" << meta.id << "from" << pack;
        return true;
    }
    qCWarning(lcModLoader) << "mod" << meta.id << "has no urls, identity, qml, or overrides";
    return false;
#endif
}

int ModLoader::loadAll(const QStringList& roots, const ModContext* earlyCtx)
{
    if (skipLoad())
        return 0;

    // Linked C++ once, before FS chrome (register-only when earlyCtx is null).
    invokeLinkedEntries(earlyCtx);

    const QStringList tipRoots = roots.isEmpty() ? tipModsRoots() : roots;
    QStringList packs;

    if (forceDefaultMod()) {
        const QString packDir = findNamedPack(tipRoots, QStringLiteral("default"));
        if (packDir.isEmpty()) {
            qCCritical(lcModLoader) << "--default-mod: tip mods/default/mod.json not found under"
                                    << tipRoots;
#if defined(QTCLIENT_HAS_LINKED_PACK)
            return 1; // entries already invoked
#else
            return 0;
#endif
        }
        qCInfo(lcModLoader) << "--default-mod: loading" << packDir;
        packs = {packDir};
    } else {
        const QString env = qEnvironmentVariable("QTCLIENT_MODS_DIR");
        if (!env.isEmpty()) {
            packs = collectEnvPacks(env);
            if (packs.isEmpty()) {
#if defined(QTCLIENT_HAS_LINKED_PACK)
                return 1;
#else
                return 0;
#endif
            }
            qCInfo(lcModLoader) << "QTCLIENT_MODS_DIR packs:" << packs;
        } else {
            packs = collectTipPacks(tipRoots);
        }
        // DEV: extra FS-only packs (no tip rebuild) — appended after tip packs.
        const QString extra = qEnvironmentVariable("QTCLIENT_EXTRA_MODS_DIR");
        if (!extra.isEmpty()) {
            const QStringList more = collectEnvPacks(extra);
            for (const QString& p : more) {
                if (!packs.contains(p))
                    packs.append(p);
            }
            if (!more.isEmpty())
                qCInfo(lcModLoader) << "QTCLIENT_EXTRA_MODS_DIR packs:" << more;
        }
    }

    if (packs.isEmpty()) {
#if defined(QTCLIENT_HAS_LINKED_PACK)
        qCInfo(lcModLoader) << "linked tip: no FS mods/; linked entries already invoked";
        return 1;
#else
        return 0;
#endif
    }

    int loaded = 0;
    for (const QString& packDir : packs) {
        if (loadFromDirectory(packDir, earlyCtx))
            ++loaded;
    }
    qCInfo(lcModLoader) << "loaded" << loaded << "of" << packs.size() << "tip packs";
    return loaded;
}

} // namespace tibia::mods
