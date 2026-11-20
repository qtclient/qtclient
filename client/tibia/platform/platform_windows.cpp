// Host: Windows-only platform behaviour (see platform.h).

#include "platform/platform.h"

#include <QSGRendererInterface>

#include <windows.h>

namespace tibia::platform {

std::string osName()
{
    return "windows";
}

QString clientPackageName()
{
    return QStringLiteral("tibiaclient-windows-current");
}

quint64 physicalMemory()
{
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    return GlobalMemoryStatusEx(&status) ? status.ullTotalPhys : 0;
}

bool supportsGraphicsApi(const int graphicsApi)
{
    switch (static_cast<QSGRendererInterface::GraphicsApi>(graphicsApi)) {
    case QSGRendererInterface::Unknown:
    case QSGRendererInterface::Software:
    case QSGRendererInterface::OpenGL:
    case QSGRendererInterface::Direct3D11:
    case QSGRendererInterface::Direct3D12:
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
    return { QStringLiteral("launcher/src/launcher") };
}

QString normalizeLocalPath(QString path)
{
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (path.size() >= 2 && path.at(1) == QLatin1Char(':'))
        path = path.mid(2);
    return path;
}

QString nativeModuleFileName()
{
    return QStringLiteral("mod.dll");
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
    return (GetKeyState(VK_CAPITAL) & 1) != 0;
}

} // namespace tibia::platform
