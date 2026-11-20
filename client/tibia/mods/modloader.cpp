#include "mods/modloader.h"

#include "platform/platform.h"
#include "mods/host_api.h"
#include "mods/mod_entry.h"
#include "mods/registry.h"

#include "shared/tibiafilesystemhelper.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>

#include <algorithm>
#include <cstring>
#include <vector>

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
    u.crashReportPath = urlsObj.value(QStringLiteral("crashReportPath")).toString().toStdString();
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

QString nativeModulePath(const QString& packDir)
{
    const QString file = QDir(QDir(packDir).filePath(QStringLiteral("native"))).filePath(tibia::platform::nativeModuleFileName());
    return QFile::exists(file) ? file : QString();
}

bool invokeNativeRegister(const PackMeta& meta, const ModContext* earlyCtx)
{
    const QString path = nativeModulePath(meta.dir);
    if (path.isEmpty())
        return false;

    const QtClientHostApiV1* api = qtclient_host_api_v1();
    if (!api) {
        return false;
    }

    QtClientModLoadCtx ctx{};
    const QByteArray idBytes = meta.id.toUtf8();
    const QByteArray dirBytes = meta.dir.toUtf8();
    ctx.pack_id = idBytes.constData();
    ctx.pack_dir = dirBytes.constData();
    ctx.host_build_id = api->host_build_id;
    ctx.api_version = api->api_version;
    ctx.early_ctx = const_cast<ModContext*>(earlyCtx);

    QtClientModRegisterFn registerFn = nullptr;

    QString loadError;
    registerFn = reinterpret_cast<QtClientModRegisterFn>(
        tibia::platform::loadNativeModule(path, QTCLIENT_MOD_REGISTER_SYMBOL, loadError));
    if (!registerFn) {
        return false;
    }
    registerFn(api, &ctx);

    return true;
}

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
            return a.priority > b.priority;
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
        return {};
    }
    return collectTipPacks({cleaned});
}

void invokeLinkedEntries(const ModContext* earlyCtx)
{
#if defined(QTCLIENT_HAS_LINKED_PACK)
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

namespace {

QStringList tipModsRootsFor(const QString& appDir)
{
    QStringList roots;
    if (!appDir.isEmpty()) {
        const QDir app(appDir);
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

} // namespace

QStringList ModLoader::tipModsRoots()
{
    return QCoreApplication::instance() ? tipModsRootsFor(QCoreApplication::applicationDirPath()) : QStringList{};
}

ModIdentity ModLoader::identityBeforeApplication(const QString& appDir)
{
    if (skipLoad())
        return {};
    const QString env = qEnvironmentVariable("QTCLIENT_MODS_DIR");
    QStringList packs = env.isEmpty() ? collectTipPacks(tipModsRootsFor(appDir)) : collectEnvPacks(env);
    packs += collectEnvPacks(qEnvironmentVariable("QTCLIENT_EXTRA_MODS_DIR"));
    ModIdentity best;
    for (const QString& pack : packs) {
        PackMeta meta;
        if (readMeta(pack, &meta) && !meta.identity.displayName.empty()
            && (best.displayName.empty() || meta.priority < best.priority))
            best = meta.identity;
    }
    return best;
}

bool ModLoader::loadFromDirectory(const QString& packDir, const ModContext* earlyCtx)
{
    const QString pack = QDir::cleanPath(packDir);
    PackMeta meta;
    if (!readMeta(pack, &meta)) {
        return false;
    }
    if (meta.api < kMinApi || meta.api > kSupportedApi) {
        return false;
    }
    meta.dir = pack;

    const QtClientHostApiV1* api = qtclient_host_api_v1();
    if (!meta.hostBuildId.isEmpty()) {
        const char* hostId = api && api->host_build_id ? api->host_build_id : "";
        if (std::strcmp(hostId, meta.hostBuildId.toUtf8().constData()) != 0) {
            const bool strict = qEnvironmentVariableIsSet("QTCLIENT_MOD_STRICT_HOST")
                ? qEnvironmentVariableIntValue("QTCLIENT_MOD_STRICT_HOST") != 0
                : true;
            if (strict)
                return false;
        }
    }

    const bool hasNative = QDir(QDir(pack).filePath(QStringLiteral("native"))).exists()
        && !nativeModulePath(pack).isEmpty();

    if (!meta.identity.displayName.empty())
        registerMod({meta.identity, nullptr});
    if (!meta.urls.empty())
        registerModUrls(meta.urls);
    registerPackQmlPaths(meta);
    tibia::files::data().addSearchPath(pack.toStdString(), true);

    bool nativeOk = true;
    if (hasNative) {
        nativeOk = invokeNativeRegister(meta, earlyCtx);
        if (!nativeOk)
            return false;
    }

#if defined(QTCLIENT_HAS_LINKED_PACK)
    return true;
#else
    if (hasNative || !meta.urls.empty() || !meta.identity.displayName.empty()
        || QDir(QDir(pack).filePath(QStringLiteral("qml"))).exists()
        || QDir(QDir(pack).filePath(QStringLiteral("overrides"))).exists()) {
        return nativeOk;
    }
    return false;
#endif
}

int ModLoader::loadAll(const QStringList& roots, const ModContext* earlyCtx)
{
    if (skipLoad())
        return 0;

    // Deprecated absorb path: linked C++ once before FS chrome.
    invokeLinkedEntries(earlyCtx);

    const QStringList tipRoots = roots.isEmpty() ? tipModsRoots() : roots;
    QStringList packs;

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
    } else {
        packs = collectTipPacks(tipRoots);
    }
    const QString extra = qEnvironmentVariable("QTCLIENT_EXTRA_MODS_DIR");
    if (!extra.isEmpty()) {
        const QStringList more = collectEnvPacks(extra);
        for (const QString& p : more) {
            if (!packs.contains(p))
                packs.append(p);
        }
    }

    if (packs.isEmpty()) {
#if defined(QTCLIENT_HAS_LINKED_PACK)
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
    return loaded;
}

} // namespace tibia::mods
