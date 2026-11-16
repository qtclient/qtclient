#include "crashhandler.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

#include <algorithm>
#include <csignal>

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

/// Host stand-in for Breakpad's exception handler: catches the fatal signals
/// and runs the minidump callback (no minidump is written).
class TCrashHandler::TExceptionHandler
{
public:
    explicit TExceptionHandler(QString dumpPath)
        : m_dumpPath(std::move(dumpPath))
        , m_dumpPathUtf8(m_dumpPath.toLocal8Bit())
    {
        s_current = this;
#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_WIN)
        for (const int signal : FatalSignals) {
            struct sigaction action = {};
            action.sa_handler = &TExceptionHandler::onSignal;
            sigemptyset(&action.sa_mask);
            action.sa_flags = SA_RESETHAND;
            sigaction(signal, &action, nullptr);
        }
#endif
    }
    ~TExceptionHandler()
    {
#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_WIN)
        for (const int signal : FatalSignals)
            std::signal(signal, SIG_DFL);
#endif
        if (s_current == this)
            s_current = nullptr;
    }

    /// No minidump writer on this host.
    bool writeMinidump() { return false; }

private:
#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_WIN)
    static constexpr int FatalSignals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };
#endif

    static void onSignal(const int signal)
    {
        TExceptionHandler* handler = s_current;
        if (handler) {
            const QByteArray id = QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
            TCrashHandler::minidumpCallback(handler->m_dumpPathUtf8.constData(), id.constData(), nullptr, false);
        }
        std::raise(signal);
    }

    static inline TExceptionHandler* s_current = nullptr;
    QString m_dumpPath;
    QByteArray m_dumpPathUtf8;
};

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
