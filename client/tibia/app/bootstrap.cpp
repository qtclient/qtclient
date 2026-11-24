#include "bootstrap.h"

#include "platform/platform.h"

#include "clientoptions.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/mousecursorcontroller.h"
#include "mods/modloader.h"
#include "mods/registry.h"
#include "overrideinterceptor.h"
#include "packqmlinterceptor.h"
#include "qmldisk.h"
#include "imageproviders/imagestripimageprovider.h"
#ifdef TIBIA_WITH_GAMECORE
#include "client/gameapplication.h"
#include "shared/dynamicresourceshelper.h"
#endif

#include <cip_registration.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QLibraryInfo>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QResource>
#include <QOffscreenSurface>
#include <QSurfaceFormat>
#include <QTranslator>
#include <QVersionNumber>
#if QT_CONFIG(vulkan)
#include <QVulkanInstance>
#endif

#include <rhi/qrhi.h>

#include <set>
#include <stdexcept>

namespace tibia::app {

namespace {

struct Renderer
{
    int index;
    const char* name;
    QSGRendererInterface::GraphicsApi api;
};

constexpr Renderer Renderers[] = {
    { 0, "auto", QSGRendererInterface::Unknown },
    { 3, "software", QSGRendererInterface::Software },
    { 6, "d3d11", QSGRendererInterface::Direct3D11 },
    { 7, "vulkan", QSGRendererInterface::Vulkan },
    { 8, "opengl", QSGRendererInterface::OpenGL },
    { 9, "metal", QSGRendererInterface::Metal },
    { 10, "d3d12", QSGRendererInterface::Direct3D12 },
};

/// CIP's strings name the client and the game "Tibia"; show the application name instead.
/// Tibia Coins and Embrace of Tibia are game content the server and store use by name.
class AppNameTranslator final : public QTranslator
{
public:
    using QTranslator::QTranslator;

    QString translate(const char* context, const char* sourceText, const char* disambiguation, int n) const override
    {
        QString text = QTranslator::translate(context, sourceText, disambiguation, n);
        if (text.contains(QLatin1String("Tibia"))) {
            static const QRegularExpression name(QStringLiteral("(?<!Embrace of )\\bTibia\\b(?! Coin)"));
            text.replace(name, QCoreApplication::applicationName());
        }
        return text;
    }
};

}

QSGRendererInterface::GraphicsApi graphicsApiForRenderer(const int rendererIndex)
{
    for (const auto& r : Renderers)
        if (r.index == rendererIndex)
            return r.api;
    return QSGRendererInterface::Unknown;
}

int rendererIndexForGraphicsApi(const QSGRendererInterface::GraphicsApi api)
{
    // CipSoft maps the window's graphics API back through the same table; APIs it has
    // no renderer for (OpenVG, Null) report 0.
    for (const auto& r : Renderers)
        if (r.api == api)
            return r.index;
    return 0;
}

int parseRendererIndex(const QString& value, const int fallback)
{
    bool ok = false;
    const int index = value.toInt(&ok);
    if (ok)
        return index;
    for (const auto& r : Renderers)
        if (value.compare(QLatin1String(r.name), Qt::CaseInsensitive) == 0)
            return r.index;
    return fallback;
}

namespace {

/// The renderers this machine can run (probeSupportedRenderers).
std::set<int>& supportedRenderers()
{
    static std::set<int> renderers;
    return renderers;
}

/// The graphics engine the process started with (setActiveGraphicsEngine).
int s_activeGraphicsEngine = 0;

}

void probeSupportedRenderers()
{
    std::set<int>& renderers = supportedRenderers();
    renderers.insert(0);
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    QRhiMetalInitParams metalParams;
    if (QRhi::probe(QRhi::Metal, &metalParams))
        renderers.insert(9);
#endif
#if defined(__EMSCRIPTEN__)
    // Host: the browser always renders on WebGL; it has no surface to probe
    // OpenGL on before the page's canvas.
    renderers.insert(8);
#elif QT_CONFIG(opengl)
    QOffscreenSurface* fallbackSurface = QRhiGles2InitParams::newFallbackSurface(QSurfaceFormat::defaultFormat());
    QRhiGles2InitParams glParams;
    glParams.fallbackSurface = fallbackSurface;
    const bool openGl = QRhi::probe(QRhi::OpenGLES2, &glParams);
    delete fallbackSurface;
    if (openGl)
        renderers.insert(8);
#endif
#if defined(Q_OS_WIN)
    // Host: the pinned CipSoft build is the macOS client; on Windows the
    // Direct3D renderers are probed the same way.
    QRhiD3D11InitParams d3d11Params;
    if (QRhi::probe(QRhi::D3D11, &d3d11Params))
        renderers.insert(6);
    QRhiD3D12InitParams d3d12Params;
    if (QRhi::probe(QRhi::D3D12, &d3d12Params))
        renderers.insert(10);
#endif
    qputenv("MVK_CONFIG_USE_MTLHEAP", "0");
#if QT_CONFIG(vulkan)
    QVulkanInstance instance;
    instance.setApiVersion(QVersionNumber(1, 0));
    if (!instance.create())
        return;
    QRhiVulkanInitParams vulkanParams;
    vulkanParams.inst = &instance;
    const bool vulkan = QRhi::probe(QRhi::Vulkan, &vulkanParams);
    instance.destroy();
    if (vulkan)
        renderers.insert(7);
#endif
}

bool isRendererSupported(const int rendererIndex)
{
    return supportedRenderers().count(rendererIndex) != 0;
}

void setActiveGraphicsEngine(const int rendererIndex)
{
    s_activeGraphicsEngine = rendererIndex;
}

int activeGraphicsEngine()
{
    return s_activeGraphicsEngine;
}

bool isSoftwareGraphicsEngineActive()
{
    return s_activeGraphicsEngine == 3;
}

int rendererFromCommandLine()
{
    const QStringList arguments = QCoreApplication::arguments() + tibia::platform::extraArguments();
    for (qsizetype i = 1; i < arguments.size(); ++i) {
        const QString& argument = arguments.at(i);
        if (argument.startsWith(QLatin1String("--renderer=")))
            return parseRendererIndex(argument.mid(11), -1);
        if (argument == QLatin1String("--renderer") && i + 1 < arguments.size())
            return parseRendererIndex(arguments.at(i + 1), -1);
    }
    return parseRendererIndex(qEnvironmentVariable("QTCLIENT_RENDERER"), -1);
}

void removeOldQtQmlFolder()
{
    if (!tibia::platform::kHasInstallDirectory)
        return;
    // Qt 5 builds installed QML modules next to the binary; they clash with Qt 6's.
    QDir old(QCoreApplication::applicationDirPath() + QStringLiteral("/QtQml"));
    if (old.exists()) {
        qInfo() << "Found old QtQml folder, deleting to prevent conflict with Qt6 files";
        old.removeRecursively();
    }
}

void applyBundledMoltenVkIcd()
{
    tibia::platform::prepareGraphicsDrivers();
}

QString mountGraphicsResources(const QStringList& dirs)
{
    // Buffer must outlive every qrc:/images reference. Path-based
    // QResource::registerResource maps the file; on wasm that is a full heap
    // copy anyway, so always QFile::readAll + registerResource(bytes).
    static QByteArray mountedBytes;
    if (!mountedBytes.isEmpty()) {
        // Already mounted in this process (re-entry).
        return QStringLiteral("<mounted-buffer>");
    }
#if defined(TIBIA_WITH_GAMECORE) && !defined(__EMSCRIPTEN__)
    // CIP: the packaged client registers graphics_resources.rcc next to the
    // executable; the host directories below serve the other layouts.
    static QString registeredFile;
    if (!registeredFile.isEmpty())
        return registeredFile;
    const QString packagedFile = shared::TDynamicResourcesHelper::getResourceFilePath(QStringLiteral("graphics"));
    if (QFile::exists(packagedFile)) {
        try {
            client::TGameApplicationBase::registerGraphicsResources();
            registeredFile = packagedFile;
            return registeredFile;
        } catch (const std::runtime_error&) {
            // Logged by the helper; the host directories are tried next.
        }
    }
#endif

    for (const QString& dir : dirs) {
        if (dir.isEmpty())
            continue;
        QStringList candidates{
            // CipSoft: next to the executable (bin/, Contents/MacOS/).
            QDir(dir).filePath(QStringLiteral("graphics_resources.rcc")),
            // Dev extract: flat CIP assets dump (data/assets/).
            QDir(dir).filePath(QStringLiteral("assets/graphics_resources.rcc")),
        };
        for (const QString& file : candidates) {
            QFile qf(file);
            if (!qf.open(QIODevice::ReadOnly))
                continue;
            QByteArray bytes = qf.readAll();
            qf.close();
            if (bytes.size() < 16)
                continue;
            mountedBytes = std::move(bytes);
            if (QResource::registerResource(
                    reinterpret_cast<const uchar*>(mountedBytes.constData()))) {
                return file;
            }
            mountedBytes.clear();
        }
    }
    return {};
}

void installUiFonts(const QStringList& dirs)
{
    if (QFontDatabase::families().contains(QStringLiteral("Verdana"), Qt::CaseInsensitive))
        return;

    const QStringList faces{
        QStringLiteral("Verdana.ttf"),
        QStringLiteral("Verdana-Bold.ttf"),
        QStringLiteral("Verdana-Italic.ttf"),
        QStringLiteral("Verdana-Bold-Italic.ttf"),
    };
    QStringList loaded;
    for (const QString& dir : dirs) {
        if (dir.isEmpty())
            continue;
        const QStringList fontDirs{
            QDir(dir).filePath(QStringLiteral("fonts")),
            // Package Resources (and the dev extract): assets/fonts.
            QDir(dir).filePath(QStringLiteral("assets/fonts")),
        };
        for (const QString& fonts : fontDirs) {
            for (const QString& face : faces) {
                if (loaded.contains(face))
                    continue;
                QFile file(QDir(fonts).filePath(face));
                if (!file.open(QIODevice::ReadOnly))
                    continue;
                if (QFontDatabase::addApplicationFontFromData(file.readAll()) >= 0)
                    loaded << face;
            }
        }
    }
}

void alignTextHinting(QQmlEngine* engine)
{
    // The platform's font engine may need its own hinting to match the desktop
    // (see tibia::platform::uiTextHinting).
    const auto hinting = tibia::platform::uiTextHinting();
    if (!hinting || !engine)
        return;
    QObject* style = engine->singletonInstance<QObject*>(QStringLiteral("qmlcomponents"), QStringLiteral("TibiaStyle"));
    if (!style) {
        return;
    }
    QFont font = style->property("defaultTextFont").value<QFont>();
    font.setHintingPreference(*hinting);
    style->setProperty("defaultTextFont", font);
}

std::unique_ptr<QTranslator> createClientTranslator()
{
    return std::make_unique<AppNameTranslator>();
}

QString clientTranslationDirectory()
{
    return QStringLiteral(":/strings");
}

void installTranslations()
{
    std::unique_ptr<QTranslator> translator = createClientTranslator();
    translator->setParent(QCoreApplication::instance());
    if (translator->load(QStringLiteral("client.en"), clientTranslationDirectory()))
        QCoreApplication::installTranslator(translator.release());
}

std::unique_ptr<QQmlEngine> createEngine()
{
    cip::registerQmlTypes();

    auto engine = std::make_unique<QQmlEngine>();
    // Always qrc baseUrl so absolute /images/… and the cmake directory qmldir
    // stay on resources. Disk mode remaps .qml.js via the interceptor only —
    // never addImportPath(cip), which surfaces CIP's plugin qmldir on file:/.
    engine->setBaseUrl(QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/")));
    // Runtime packs: ModLoader recorded mods/<id>/qml import roots at startup.
    for (const std::string& path : tibia::mods::packQmlImportPaths()) {
        const QString qpath = QString::fromStdString(path);
        if (QDir(qpath).exists())
            engine->addImportPath(qpath);
    }
#if defined(TIBIA_QML_DISK)
    // DEV: also import from source pack qml/ so `import <pack qml module>` + disk remap stay in sync.
    // --no-mods drops pack qml/ and pack overrides (host has no client/overrides).
    // Roots are in-tree mods/<id>/qml and QTCLIENT_LINKED_PACK qml/ (colon list).
    QString diskModDirs = QStringLiteral(TIBIA_QML_MOD_DIR);
    QString diskOverrideDirs = QStringLiteral(TIBIA_QML_OVERRIDES_DIR);
    if (tibia::mods::ModLoader::skipLoad()) {
        diskModDirs.clear();
        diskOverrideDirs.clear();
    } else {
        for (const QString& root : diskModDirs.split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
            if (QDir(root).exists())
                engine->addImportPath(root);
        }
    }
    engine->addUrlInterceptor(new DiskQmlInterceptor(QStringLiteral(TIBIA_QML_CIP_DIR),
                                                     diskModDirs,
                                                     diskOverrideDirs));
    installQmlDiskHotReload(engine.get(), engine.get());
#endif
    // Production / fallback: qrc:/qt/qml/<module>/… → staged mods/<id>/qml/<module>/…
    engine->addUrlInterceptor(new PackQmlInterceptor());
    for (const QString& path : tibia::platform::extraQmlPluginPaths())
        engine->addPluginPath(path);
    imageproviders::registerImageProviders(engine.get());

    auto* cursor = new gamewindow::TMouseCursorController(ClientOptions::load().options(), engine.get());
    engine->rootContext()->setContextProperty(QStringLiteral("tibiaMouseCursorController"), cursor);
    new gamewindow::TTextCopyController(engine.get());
    // Root context objects CIP's components test for null before use.
    engine->rootContext()->setContextProperty(QStringLiteral("tibiaContextMenuController"),
                                              QVariant::fromValue<QObject*>(nullptr));
    // CIP : panels never cache into layers.
    engine->rootContext()->setContextProperty(QStringLiteral("UICachingEnabled"), false);

#if !defined(TIBIA_QML_DISK)
    auto interceptor = std::make_unique<OverrideInterceptor>();
    if (!interceptor->isEmpty())
        engine->addUrlInterceptor(interceptor.release());
#endif
    return engine;
}

}
