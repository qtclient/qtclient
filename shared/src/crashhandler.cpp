#include "crashhandler.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>

#if QTCLIENT_HAVE_BREAKPAD
#if defined(Q_OS_MACOS)
#include "client/mac/handler/exception_handler.h"
#elif defined(Q_OS_WIN)
#include "client/windows/handler/exception_handler.h"
#else
#include "client/linux/handler/exception_handler.h"
#endif
#endif

namespace tibia::shared {

namespace {

const QString LogMessagesKey = QStringLiteral("LogMessages");
const QString MessageKey = QStringLiteral("Message");
const QString TimestampKey = QStringLiteral("Timestamp");
const QString CharacterKey = QStringLiteral("Character");

/// How long a log message stays in the crash annotations, in seconds.
constexpr quint32 LogMessageLifetime = 60;

QString joinPaths(const QString& directory, const QString& fileName)
{
    return QDir(directory).filePath(fileName);
}

void writeFile(const QString& path, const QByteArray& data)
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(data);
}

}

#if QTCLIENT_HAVE_BREAKPAD
/// Google Breakpad's in-process exception handler (CipSoft links it), with
/// the platform's callback forwarded to CipSoft's minidump callback.
class TCrashHandler::TExceptionHandler
{
public:
    explicit TExceptionHandler(const QString& dumpPath)
    {
#if defined(Q_OS_MACOS)
        m_handler = std::make_unique<google_breakpad::ExceptionHandler>(
            dumpPath.toStdString(), nullptr, &TCrashHandler::minidumpCallback, nullptr, true, nullptr);
#elif defined(Q_OS_WIN)
        m_handler = std::make_unique<google_breakpad::ExceptionHandler>(dumpPath.toStdWString(), nullptr,
            &TExceptionHandler::windowsCallback, nullptr, google_breakpad::ExceptionHandler::HANDLER_ALL);
#else
        m_handler = std::make_unique<google_breakpad::ExceptionHandler>(
            google_breakpad::MinidumpDescriptor(dumpPath.toStdString()), nullptr, &TExceptionHandler::linuxCallback,
            nullptr, true, -1);
#endif
    }

    bool writeMinidump()
    {
#if defined(Q_OS_MACOS)
        return m_handler->WriteMinidump(false);
#else
        return m_handler->WriteMinidump();
#endif
    }

private:
#if defined(Q_OS_WIN)
    static bool windowsCallback(const wchar_t* dumpPath, const wchar_t* minidumpId, void* context,
        EXCEPTION_POINTERS*, MDRawAssertionInfo*, const bool succeeded)
    {
        const QByteArray directory = QString::fromWCharArray(dumpPath).toLocal8Bit();
        const QByteArray id = QString::fromWCharArray(minidumpId).toLocal8Bit();
        return TCrashHandler::minidumpCallback(directory.constData(), id.constData(), context, succeeded);
    }
#elif !defined(Q_OS_MACOS)
    /// Breakpad on Linux names the dump `<directory>/<id>.dmp`.
    static bool linuxCallback(const google_breakpad::MinidumpDescriptor& descriptor, void* context, const bool succeeded)
    {
        const QFileInfo dump(QString::fromLocal8Bit(descriptor.path()));
        const QByteArray id = dump.completeBaseName().toLocal8Bit();
        return TCrashHandler::minidumpCallback(descriptor.directory().c_str(), id.constData(), context, succeeded);
    }
#endif

    std::unique_ptr<google_breakpad::ExceptionHandler> m_handler;
};
#else
/// Host: no Breakpad on this platform (WASM, mobile); nothing catches crashes.
class TCrashHandler::TExceptionHandler
{
public:
    explicit TExceptionHandler(const QString&) {}
    bool writeMinidump() { return false; }
};
#endif

TCrashHandler& TCrashHandler::instance()
{
    static TCrashHandler crashHandler;
    return crashHandler;
}

TCrashHandler::TCrashHandler() = default;

TCrashHandler::~TCrashHandler() = default;

void TCrashHandler::initialize(const QString& productName, const QString& version, const QString& dumpPath)
{
    installHandler(dumpPath);
    m_annotations[QStringLiteral("ProductName")] = productName;
    m_annotations[QStringLiteral("Version")] = version;
}

void TCrashHandler::installHandler(const QString& dumpPath)
{
    m_exceptionHandler.reset();
    m_exceptionHandler = std::make_unique<TExceptionHandler>(dumpPath);
}

bool TCrashHandler::writeMinidump()
{
    return m_exceptionHandler && m_exceptionHandler->writeMinidump();
}

void TCrashHandler::setAnnotation(const QString& key, const QString& value)
{
    m_annotations[key] = value;
}

void TCrashHandler::removeAnnotation(const QString& key)
{
    m_annotations.erase(key);
}

void TCrashHandler::removeOldLogMessages()
{
    const quint32 now = quint32(QDateTime::currentSecsSinceEpoch());
    m_logMessages.erase(std::remove_if(m_logMessages.begin(), m_logMessages.end(),
                            [now](const TLogMessage& logMessage) { return now - logMessage.timestamp > LogMessageLifetime; }),
        m_logMessages.end());
}

void TCrashHandler::addLogMessage(const QString& message)
{
    removeOldLogMessages();
    m_logMessages.push_back({ quint32(QDateTime::currentSecsSinceEpoch()), message });
    updateLogMessagesAnnotation();
}

void TCrashHandler::clearLogMessages()
{
    m_logMessages.clear();
    updateLogMessagesAnnotation();
}

void TCrashHandler::updateLogMessagesAnnotation()
{
    if (m_logMessages.empty()) {
        m_annotations.erase(LogMessagesKey);
        return;
    }
    QJsonArray logMessages;
    for (const TLogMessage& logMessage : m_logMessages) {
        QJsonObject object;
        object.insert(TimestampKey, QJsonValue(qint64(logMessage.timestamp)));
        object.insert(MessageKey, QJsonValue(logMessage.message));
        logMessages.append(object);
    }
    m_annotations[LogMessagesKey] = QString::fromUtf8(QJsonDocument(logMessages).toJson(QJsonDocument::Compact));
}

void TCrashHandler::setSessiondumpProvider(TSessiondumpProvider provider)
{
    m_sessiondumpProvider = std::move(provider);
}

void TCrashHandler::setPassExceptionOn(const bool passExceptionOn)
{
    m_passExceptionOn = passExceptionOn;
}

bool TCrashHandler::passExceptionOn() const
{
    return m_passExceptionOn;
}

void TCrashHandler::anonymize(TAnnotations& annotations)
{
    if (annotations.find(CharacterKey) != annotations.end())
        annotations[CharacterKey] = QStringLiteral("xxx");
}

TCrashHandler::TAnnotations TCrashHandler::annotationsFromJson(const QByteArray& json)
{
    TAnnotations annotations;
    const QJsonObject object = QJsonDocument::fromJson(json).object();
    const QStringList keys = object.keys();
    for (const QString& key : keys)
        annotations[key] = object[key].toString(QString());
    return annotations;
}

QJsonDocument TCrashHandler::annotationsToJsonDocument(const TAnnotations& annotations)
{
    QJsonDocument document;
    QJsonObject object;
    for (const auto& [key, value] : annotations)
        object.insert(key, QJsonValue(value));
    document.setObject(object);
    return document;
}

QByteArray TCrashHandler::annotationsToJson(const TAnnotations& annotations)
{
    return annotationsToJsonDocument(annotations).toJson(QJsonDocument::Indented);
}

QString TCrashHandler::annotationsFileName(const QString& minidumpPath)
{
    return QFileInfo(minidumpPath).fileName() + QStringLiteral(".json");
}

void TCrashHandler::writeCrashFiles(const QString& dumpDirectory, const QString& minidumpId)
{
    m_annotations[TimestampKey] = QString::number(QDateTime::currentSecsSinceEpoch());
    const QString minidumpFileName = minidumpId + QStringLiteral(".dmp");
    const QString annotationsPath = joinPaths(dumpDirectory, annotationsFileName(minidumpFileName));
    removeOldLogMessages();
    writeFile(annotationsPath, annotationsToJson(m_annotations));
    if (m_sessiondumpProvider) {
        const QByteArray sessiondump = m_sessiondumpProvider();
        const QString sessiondumpPath =
            joinPaths(dumpDirectory, QFileInfo(minidumpFileName).fileName() + QStringLiteral(".session"));
        writeFile(sessiondumpPath, sessiondump);
    }
}

bool TCrashHandler::minidumpCallback(const char* dumpDirectory, const char* minidumpId, void* context, const bool succeeded)
{
    Q_UNUSED(context);
    instance().writeCrashFiles(QString::fromLocal8Bit(dumpDirectory), QString::fromLocal8Bit(minidumpId));
    return !instance().passExceptionOn() && succeeded;
}

}
