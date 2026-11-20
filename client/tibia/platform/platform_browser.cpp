// Host: the browser (WebAssembly) platform (see platform.h).

#include "platform/platform.h"

#include "browser/qtclient_opfs_backend.h"

#include <cstdlib>
#include <QCollator>
#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QObject>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QScreen>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>

#include <dlfcn.h>
#include <emscripten.h>
#include <emscripten/heap.h>
#include <emscripten/val.h>

namespace tibia::platform {

std::string osName()
{
    return "browser";
}

QString clientPackageName()
{
    return QStringLiteral("tibiaclient-web-current");
}

quint64 physicalMemory()
{
    // The browser exposes no RAM size; the wasm heap ceiling is this process's
    // memory, so caches size like a desktop's (they cap at 512 MB).
    return static_cast<quint64>(emscripten_get_heap_max());
}

bool supportsGraphicsApi(const int graphicsApi)
{
    // WebGL is the only accelerated API.
    switch (static_cast<QSGRendererInterface::GraphicsApi>(graphicsApi)) {
    case QSGRendererInterface::Unknown:
    case QSGRendererInterface::Software:
    case QSGRendererInterface::OpenGL:
        return true;
    default:
        return false;
    }
}

int autoRenderer(const int requested)
{
    return requested == 0 ? 8 : requested;
}

std::optional<QFont::HintingPreference> uiTextHinting()
{
    // CIP sets PreferDefaultHinting. On the desktop that is the native engine
    // (CoreText / DirectWrite), which keeps Verdana's advances, so the 1px outline
    // sits in the gap. The browser renders with FreeType, where the same setting is
    // full hinting: it rewrites those advances and one letter's outline lands on the
    // next. Vertical-only hinting is the FreeType mode that leaves advances alone.
    return QFont::PreferVerticalHinting;
}

bool adaptMainWindow(QQuickWindow* window)
{
    // clientwindow.qml pins width/height to CIP's minimum onCompleted. The screen is
    // the page's host element: keep the window filling it so the world is not
    // left-aligned with a black gutter.
    const auto fillHost = [window] {
        if (QScreen* screen = window->screen())
            window->resize(screen->size());
    };
    fillHost();
    if (QScreen* screen = window->screen())
        QObject::connect(screen, &QScreen::geometryChanged, window, fillHost);
    return true;
}

void configureNaturalCollator(QCollator&)
{
    // The POSIX collator logs and ignores numeric mode and case sensitivity.
}

bool isCapsLockActive()
{
    return false;
}

bool startDetached(const QString&, const QStringList&, const QHash<QString, QString>&)
{
    return false;
}

void restartApplication()
{
    // A page cannot relaunch itself as a process; the portal reloads on quit.
    QCoreApplication::quit();
}

QStringList installRoots(const QString&)
{
    return {};
}

QStringList devLauncherCandidates(const QString&)
{
    return {};
}

QStringList devLauncherRelativePaths()
{
    return {};
}

QString normalizeLocalPath(QString path)
{
    return path;
}

QStringList extraDataRoots()
{
    // Local wasm builds may preload the graphics RCC (WASM_PRELOAD_GRAPHICS_RCC=1).
    return { QStringLiteral("/data") };
}

QStringList extraArguments()
{
    // The page URL carries the options: tibia.html?preview&renderer=software.
    // Portal-only keys (launcher → game handoff) are not client flags.
    QStringList args;
    const auto search = QString::fromStdString(emscripten::val::global("location")["search"].as<std::string>());
    const QUrlQuery query(search.mid(1));
    for (const auto& [key, value] : query.queryItems(QUrl::FullyDecoded)) {
        if (key == QLatin1String("game"))
            continue;
        args << (value.isEmpty() ? QStringLiteral("--") + key : QStringLiteral("--%1=%2").arg(key, value));
    }
    return args;
}

QList<QByteArray> hardwareAddresses()
{
    return {};
}

QString nativeModuleFileName()
{
    return QStringLiteral("mod.wasm");
}

void* loadNativeModule(const QString& path, const char* symbol, QString& error)
{
    void* handle = dlopen(path.toUtf8().constData(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        error = QString::fromUtf8(dlerror());
        return nullptr;
    }
    void* resolved = dlsym(handle, symbol);
    if (!resolved) {
        error = QStringLiteral("missing symbol");
        dlclose(handle);
        return nullptr;
    }
    // Pack modules stay loaded for the process lifetime.
    return resolved;
}

void initializeProcess()
{
    // /home/web_user (AppData, tip packages) and /mods are the origin's OPFS, served
    // by the portal's qtclient-fs Worker. Mount before anything opens a file.
    if (!qtclientMountOpfs())
        std::abort();
    // QSettings defaults to localStorage here; keep settings as ini files in OPFS
    // (~/.config/<org>/<app>.ini), as Linux desktop does.
    QSettings::setDefaultFormat(QSettings::IniFormat);
}

void notifyUiReady()
{
    // Portal /play overlay: qtLoad never resolves under JSPI (main stays in exec).
    EM_ASM({
        try {
            if (typeof window.__qtclientGameUiReady === 'function')
                window.__qtclientGameUiReady();
        } catch (e) {}
    });
}

void reportFatalException(const char* message)
{
    EM_ASM({
        var msg = $0 ? UTF8ToString($0) : '';
        try {
            var d = window.__qtclientDiag;
            if (d && typeof d.mark === 'function')
                d.mark(msg ? 'exception escaped event loop' : 'unknown exception escaped event loop', { message: msg });
        } catch (err) {}
    }, message);
}

namespace {

void resumeWebAudio()
{
    EM_ASM({
        if (window.qtclientSoundMuted)
            return;
        var root = (typeof window !== 'undefined') ? window.miniaudio : null;
        if (!root || !root.devices)
            return;
        for (var i = 0; i < root.devices.length; i++) {
            var device = root.devices[i];
            if (device && device.webaudio && device.webaudio.state === 'suspended')
                device.webaudio.resume();
        }
    });
}

/// Browsers start the audio context suspended until a user gesture.
class TWebAudioUnlock final : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::KeyPress:
        case QEvent::TouchBegin:
            resumeWebAudio();
            break;
        default:
            break;
        }
        return false;
    }
};

} // namespace

void installAudioUnlock()
{
    if (auto* app = QGuiApplication::instance())
        app->installEventFilter(new TWebAudioUnlock(app));
}

void setAudioSuspended(const bool suspended)
{
    EM_ASM({ window.qtclientSoundMuted = $0 ? true : false; }, suspended);
}

void prepareGraphicsDrivers()
{
}

QStringList extraQmlPluginPaths()
{
    return {};
}

bool isCapsLockActive(quint32)
{
    return false;
}

} // namespace tibia::platform
