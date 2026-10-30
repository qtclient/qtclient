#include "mods/modloader.h"
#include "mods/mod_entry.h"
#include "mods/registry.h"

#include "qtclient/mod_api_v1.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

#include <algorithm>
#include <cstring>
#include <vector>

#if defined(Q_OS_WASM)
#  include <dlfcn.h>
#endif

Q_LOGGING_CATEGORY(lcModLoader, "tibia.mods.loader")

namespace tibia::mods {
namespace {

// Process-lifetime native pack handles (desktop QLibrary; WASM uses dlopen).
std::vector<std::unique_ptr<QLibrary>> g_keptLibraries;

struct PackMeta {
    QString dir;
    QString id;
    QString nativeRel;
    QString qmlModule;
    QString hostBuildId;
    int api = 0;
    int priority = 100;
    bool autoload = true;
    ModIdentity identity;
};

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
    out->nativeRel = root.value(QStringLiteral("native")).toString(QStringLiteral("native/mod"));
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
    return !out->id.isEmpty();
}

QString resolveNativePath(const PackMeta& meta)
{
    QDir dir(meta.dir);
    const QString base = dir.filePath(meta.nativeRel);
    if (QFile::exists(base))
        return base;
    if (QFile::exists(base + QStringLiteral(".so")))
        return base + QStringLiteral(".so");
    if (QFile::exists(base + QStringLiteral(".dylib")))
        return base + QStringLiteral(".dylib");
    if (QFile::exists(base + QStringLiteral(".dll")))
        return base + QStringLiteral(".dll");
    if (QFile::exists(base + QStringLiteral(".wasm")))
        return base + QStringLiteral(".wasm");
    return base;
}

void registerPackQmlPaths(const PackMeta& meta)
{
    const QDir dir(meta.dir);
    // Import root is mods/<id>/qml so `import <qmlModule>` resolves
    // mods/<id>/qml/<qmlModule>/qmldir (filesystem QML module).
    // Pack overrides/ are configure-time CIP merges only — not runtime import paths.
    // Dev F5 uses TIBIA_QML_DISK DiskQmlInterceptor for override source trees.
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
}

bool loadNativeEntry(const QString& native, const ModContext* earlyCtx)
{
    auto lib = std::make_unique<QLibrary>(native);
    if (lib->load()) {
        auto* entry = reinterpret_cast<QtClientModEntryFn>(lib->resolve(QTCLIENT_MOD_ENTRY_NAME));
        if (!entry) {
            qCWarning(lcModLoader) << "missing" << QTCLIENT_MOD_ENTRY_NAME << "in" << native;
            return false;
        }
        entry(earlyCtx);
        g_keptLibraries.push_back(std::move(lib));
        return true;
    }

#if defined(Q_OS_WASM)
    // Static Qt builds return false from QLibrary::load on wasm; use emscripten
    // dlopen for MAIN_MODULE ↔ SIDE_MODULE packs. Handles stay process-lifetime.
    void* handle = dlopen(native.toUtf8().constData(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        qCWarning(lcModLoader) << "failed to load" << native << lib->errorString()
                               << "dlopen:" << (dlerror() ? dlerror() : "");
        return false;
    }
    auto* entry = reinterpret_cast<QtClientModEntryFn>(dlsym(handle, QTCLIENT_MOD_ENTRY_NAME));
    if (!entry) {
        qCWarning(lcModLoader) << "missing" << QTCLIENT_MOD_ENTRY_NAME << "in" << native;
        return false;
    }
    entry(earlyCtx);
    return true;
#else
    qCWarning(lcModLoader) << "failed to load" << native << lib->errorString();
    return false;
#endif
}

} // namespace

std::vector<std::unique_ptr<QLibrary>>& ModLoader::keptLibraries()
{
    return g_keptLibraries;
}

QStringList ModLoader::defaultRoots()
{
    QStringList roots;
    const QString env = qEnvironmentVariable("QTCLIENT_MODS_DIR");
    if (!env.isEmpty())
        roots << env;
    if (QCoreApplication::instance()) {
        const QDir app(QCoreApplication::applicationDirPath());
        roots << app.filePath(QStringLiteral("mods"));
        roots << app.absoluteFilePath(QStringLiteral("../Resources/mods"));
        roots << app.absoluteFilePath(QStringLiteral("../mods"));
    }
    roots.removeDuplicates();
    return roots;
}

bool ModLoader::loadFromDirectory(const QString& packDir, const ModContext* earlyCtx)
{
    PackMeta meta;
    if (!readMeta(packDir, &meta)) {
        qCWarning(lcModLoader) << "invalid mod.json in" << packDir;
        return false;
    }
    if (meta.api < kMinApi || meta.api > kSupportedApi) {
        qCWarning(lcModLoader) << "mod" << meta.id << "API" << meta.api
                               << "outside host range" << kMinApi << ".." << kSupportedApi;
        return false;
    }
    if (!meta.hostBuildId.isEmpty()) {
        const qtclient::ModApiV1* api = qtclient_mod_host_api_v1();
        const char* hostId = api && api->hostBuildId ? api->hostBuildId : "";
        if (std::strcmp(hostId, meta.hostBuildId.toUtf8().constData()) != 0) {
            const bool strict = qEnvironmentVariableIntValue("QTCLIENT_MOD_STRICT_HOST");
            qCWarning(lcModLoader) << "mod" << meta.id << "hostBuildId" << meta.hostBuildId
                                   << "!= host" << hostId
                                   << (strict ? "(reject)" : "(continuing; set QTCLIENT_MOD_STRICT_HOST=1 to reject)");
            if (strict)
                return false;
        }
    }
    // Identity comes from mod.json only — packs must not re-register a second identity.
    if (!meta.identity.displayName.empty()) {
        registerMod({ meta.identity, nullptr });
    }

    registerPackQmlPaths(meta);

    const QString native = resolveNativePath(meta);
    if (!loadNativeEntry(native, earlyCtx))
        return false;

    qCInfo(lcModLoader) << "loaded mod" << meta.id << "from" << packDir;
    return true;
}

int ModLoader::loadAll(const QStringList& roots, const ModContext* earlyCtx)
{
    std::vector<PackMeta> packs;
    for (const QString& root : roots) {
        QDir dir(root);
        if (!dir.exists())
            continue;
        const QFileInfoList entries =
            dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo& info : entries) {
            PackMeta meta;
            if (!readMeta(info.absoluteFilePath(), &meta))
                continue;
            if (!meta.autoload)
                continue;
            packs.push_back(std::move(meta));
        }
    }
    std::stable_sort(packs.begin(), packs.end(),
                     [](const PackMeta& a, const PackMeta& b) { return a.priority < b.priority; });
    int loaded = 0;
    for (const PackMeta& meta : packs) {
        if (loadFromDirectory(meta.dir, earlyCtx))
            ++loaded;
    }
    return loaded;
}

} // namespace tibia::mods
