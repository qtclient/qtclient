// Host: macOS-only platform behaviour (see platform.h).

#include "platform/platform.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLibraryInfo>
#include <QSGRendererInterface>

#include <Carbon/Carbon.h>
#include <sys/sysctl.h>

namespace tibia::platform {

std::string osName()
{
    return "mac";
}

QString clientPackageName()
{
    return QStringLiteral("tibiaclient-macos-current");
}

quint64 physicalMemory()
{
    quint64 bytes = 0;
    size_t size = sizeof(bytes);
    if (sysctlbyname("hw.memsize", &bytes, &size, nullptr, 0) == 0)
        return bytes;
    return 0;
}

bool supportsGraphicsApi(const int graphicsApi)
{
    switch (static_cast<QSGRendererInterface::GraphicsApi>(graphicsApi)) {
    case QSGRendererInterface::Unknown:
    case QSGRendererInterface::Software:
    case QSGRendererInterface::OpenGL:
    case QSGRendererInterface::Metal:
    case QSGRendererInterface::Vulkan:
        return true;
    default:
        return false;
    }
}

bool isCapsLockActive()
{
    return (GetCurrentKeyModifiers() >> alphaLockBit) & 1;
}

QStringList installRoots(const QString& applicationDir)
{
    // The .app bundle root (Contents/MacOS/../..) holds the package files.
    return { QDir(applicationDir + QStringLiteral("/../..")).canonicalPath() };
}

QStringList devLauncherCandidates(const QString& applicationDir)
{
    return { applicationDir + QStringLiteral("/../../../launcher/launcher.app/Contents/MacOS/launcher"),
             applicationDir + QStringLiteral("/../../../../launcher/src/launcher.app/Contents/MacOS/launcher") };
}

QStringList devLauncherRelativePaths()
{
    return { QStringLiteral("launcher/src/launcher.app/Contents/MacOS/launcher"), QStringLiteral("launcher/src/launcher") };
}

QString normalizeLocalPath(QString path)
{
    return path;
}

QString nativeModuleFileName()
{
    return QStringLiteral("mod.dylib");
}

void prepareGraphicsDrivers()
{
    const QString moltenIcd = QCoreApplication::applicationDirPath() + QStringLiteral("/../Frameworks/MoltenVK_icd.json");
    if (QFile::exists(moltenIcd))
        qputenv("VK_ICD_FILENAMES", QFile::encodeName(moltenIcd));
}

QStringList extraQmlPluginPaths()
{
    // macdeployqt symlinks QML plugins from Resources/qml to PlugIns/quick; the launcher
    // installs no symlinks, so load them from PlugIns/quick directly.
    const QString quickPlugins = QLibraryInfo::path(QLibraryInfo::PluginsPath) + QStringLiteral("/quick");
    if (QDir(quickPlugins).exists())
        return { quickPlugins };
    return {};
}

bool isCapsLockActive(const quint32 nativeModifiers)
{
    // NSEventModifierFlagCapsLock.
    return (nativeModifiers & (1u << 16)) != 0;
}

} // namespace tibia::platform
