// Host: behaviour shared by every desktop platform (Windows, macOS, Linux).
// OS-specific pieces live in platform_{mac,windows,linux}.cpp.

#include "platform/platform.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QLibrary>
#include <QNetworkInterface>
#include <QProcess>
#include <QProcessEnvironment>

#include <algorithm>

namespace tibia::platform {

int autoRenderer(const int requested)
{
    return requested;
}

std::optional<QFont::HintingPreference> uiTextHinting()
{
    // CipSoft's PreferDefaultHinting: the native font engine keeps Verdana's advances.
    return std::nullopt;
}

bool adaptMainWindow(QQuickWindow*)
{
    return false;
}

void configureNaturalCollator(QCollator& collator)
{
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
}

bool startDetached(const QString& program, const QStringList& arguments, const QHash<QString, QString>& environment,
                   const QString& workingDirectory)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    if (!workingDirectory.isEmpty())
        process.setWorkingDirectory(workingDirectory);
    if (!environment.isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        for (auto it = environment.cbegin(); it != environment.cend(); ++it)
            env.insert(it.key(), it.value());
        process.setProcessEnvironment(env);
    }
    return process.startDetached();
}

void restartApplication()
{
    QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1));
    QCoreApplication::quit();
}

QList<QByteArray> hardwareAddresses()
{
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    std::sort(interfaces.begin(), interfaces.end(),
              [](const QNetworkInterface& a, const QNetworkInterface& b) { return a.index() < b.index(); });
    QList<QByteArray> addresses;
    for (const QNetworkInterface& networkInterface : interfaces)
        addresses << networkInterface.hardwareAddress().toLocal8Bit();
    return addresses;
}

QStringList extraDataRoots()
{
    return {};
}

QStringList extraArguments()
{
    return {};
}

void* loadNativeModule(const QString& path, const char* symbol, QString& error)
{
    auto* library = new QLibrary(path);
    if (!library->load()) {
        error = library->errorString();
        delete library;
        return nullptr;
    }
    void* resolved = reinterpret_cast<void*>(library->resolve(symbol));
    if (!resolved) {
        error = library->errorString();
        library->unload();
        delete library;
        return nullptr;
    }
    // Pack modules stay loaded for the process lifetime.
    return resolved;
}

void initializeProcess()
{
}

void notifyUiReady()
{
}

void reportFatalException(const char*)
{
}

void installAudioUnlock()
{
}

void setAudioSuspended(bool)
{
}

} // namespace tibia::platform
