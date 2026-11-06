#include "mods/modloader.h"
#include "mods/host_api.h"
#include "mods/mod_entry.h"
#include "mods/registry.h"

#include "engine/framework/core/resourcemanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLibrary>
#include <QList>
#include <QLoggingCategory>

#include <algorithm>
#include <cstring>
#include <vector>

#if defined(__EMSCRIPTEN__)
#  include <dlfcn.h>
#endif

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

struct LoadedNative {
    QLibrary* lib = nullptr;
#if defined(__EMSCRIPTEN__)
    void* handle = nullptr;
#endif
};

std::vector<LoadedNative>& loadedNatives()
{
    static std::vector<LoadedNative> libs;
    return libs;
}

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

QString nativeModulePath(const QString& packDir)
{
    const QDir native(QDir(packDir).filePath(QStringLiteral("native")));
#if defined(__EMSCRIPTEN__)
    const QString wasm = native.filePath(QStringLiteral("mod.wasm"));
    if (QFile::exists(wasm))
        return wasm;
#elif defined(_WIN32)
    const QString dll = native.filePath(QStringLiteral("mod.dll"));
    if (QFile::exists(dll))
        return dll;
#elif defined(__APPLE__)
    const QString dylib = native.filePath(QStringLiteral("mod.dylib"));
    if (QFile::exists(dylib))
        return dylib;
#else
    const QString so = native.filePath(QStringLiteral("mod.so"));
    if (QFile::exists(so))
        return so;
#endif
    // Fallback: any libqtclient_mod_* in native/
    const QStringList entries = native.entryList(QDir::Files);
    for (const QString& name : entries) {
        if (name.startsWith(QStringLiteral("libqtclient_mod_"))
            || name.startsWith(QStringLiteral("qtclient_mod_")))
            return native.filePath(name);
    }
    return {};
}

bool invokeNativeRegister(const PackMeta& meta, const ModContext* earlyCtx)
{
    const QString path = nativeModulePath(meta.dir);
    if (path.isEmpty())
        return false;

    const QtClientHostApiV1* api = qtclient_host_api_v1();
    if (!api) {
        qCCritical(lcModLoader) << "mod" << meta.id << "host API v1 unavailable";
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

#if defined(__EMSCRIPTEN__)
    void* handle = dlopen(path.toUtf8().constData(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        qCWarning(lcModLoader) << "mod" << meta.id << "dlopen failed" << path << dlerror();
        return false;
    }
    registerFn = reinterpret_cast<QtClientModRegisterFn>(dlsym(handle, QTCLIENT_MOD_REGISTER_SYMBOL));
    if (!registerFn) {
        qCWarning(lcModLoader) << "mod" << meta.id << "missing" << QTCLIENT_MOD_REGISTER_SYMBOL;
        dlclose(handle);
        return false;
    }
    registerFn(api, &ctx);
    loadedNatives().push_back(LoadedNative{nullptr, handle});
#else
    auto* lib = new QLibrary(path);
    if (!lib->load()) {
        qCWarning(lcModLoader) << "mod" << meta.id << "QLibrary load failed" << path << lib->errorString();
        delete lib;
        return false;
    }
    registerFn = reinterpret_cast<QtClientModRegisterFn>(
        lib->resolve(QTCLIENT_MOD_REGISTER_SYMBOL));
    if (!registerFn) {
        qCWarning(lcModLoader) << "mod" << meta.id << "missing" << QTCLIENT_MOD_REGISTER_SYMBOL
                               << lib->errorString();
        lib->unload();
        delete lib;
        return false;
    }
    registerFn(api, &ctx);
    loadedNatives().push_back(LoadedNative{lib});
#endif

    qCInfo(lcModLoader) << "mod" << meta.id << "native register ok" << path;
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
        qCWarning(lcModLoader) << "QTCLIENT_MODS_DIR missing" << envPath;
        return {};
    }
    return collectTipPacks({cleaned});
}

void invokeLinkedEntries(const ModContext* earlyCtx)
{
#if defined(QTCLIENT_HAS_LINKED_PACK)
    qCInfo(lcModLoader) << "invoking linked pack entries (deprecated absorb path)";
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

    const QtClientHostApiV1* api = qtclient_host_api_v1();
    if (!meta.hostBuildId.isEmpty()) {
        const char* hostId = api && api->host_build_id ? api->host_build_id : "";
        if (std::strcmp(hostId, meta.hostBuildId.toUtf8().constData()) != 0) {
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
    qCInfo(lcModLoader) << "loaded tip pack" << meta.id << "priority" << meta.priority << "from" << pack
                        << (hasNative ? "(native+FS)" : "(FS; C++ via absorb)");
    return true;
#else
    if (hasNative || !meta.urls.empty() || !meta.identity.displayName.empty()
        || QDir(QDir(pack).filePath(QStringLiteral("qml"))).exists()
        || QDir(QDir(pack).filePath(QStringLiteral("overrides"))).exists()) {
        qCInfo(lcModLoader) << "loaded pack" << meta.id << "from" << pack
                            << (hasNative ? "(native+FS)" : "(FS)");
        return nativeOk;
    }
    qCWarning(lcModLoader) << "mod" << meta.id << "has no native, urls, identity, qml, or overrides";
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
        qCInfo(lcModLoader) << "QTCLIENT_MODS_DIR packs:" << packs;
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
        if (!more.isEmpty())
            qCInfo(lcModLoader) << "QTCLIENT_EXTRA_MODS_DIR packs:" << more;
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
