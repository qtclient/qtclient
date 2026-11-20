// Host: Linux (and Android) platform behaviour (see platform.h).

#include "platform/platform.h"

#include <QSGRendererInterface>

#include <unistd.h>

namespace tibia::platform {

std::string osName()
{
    return kOs == Os::Android ? "android" : "linux";
}

QString clientPackageName()
{
    return QStringLiteral("tibiaclient-linux-current");
}

quint64 physicalMemory()
{
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long pageSize = sysconf(_SC_PAGE_SIZE);
    return pages > 0 && pageSize > 0 ? quint64(pages) * quint64(pageSize) : 0;
}

bool supportsGraphicsApi(const int graphicsApi)
{
    switch (static_cast<QSGRendererInterface::GraphicsApi>(graphicsApi)) {
    case QSGRendererInterface::Unknown:
    case QSGRendererInterface::Software:
    case QSGRendererInterface::OpenGL:
    case QSGRendererInterface::Vulkan:
        return true;
    default:
        return false;
    }
}

bool isCapsLockActive()
{
    return false;
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
    // Do not name anything `linux` here — glibc defines linux as a macro (=1).
    return { QStringLiteral("launcher/src/launcher") };
}

QString normalizeLocalPath(QString path)
{
    return path;
}

QString nativeModuleFileName()
{
    return QStringLiteral("mod.so");
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
