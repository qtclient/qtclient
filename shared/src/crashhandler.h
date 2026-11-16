#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QString>

#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace tibia {
namespace shared {

/// CIP `shared::TCrashHandler`: the process-wide crash handler. On a crash it
/// writes, next to the minidump `<id>.dmp`, the annotations as `<id>.dmp.json`
/// (product, version, time of the crash and the log messages of the last
/// minute) and the session dump as `<id>.dmp.session`.
///
/// The minidump is Google Breakpad's (as CipSoft links it) on macOS, Linux and
/// Windows; other hosts (WASM, mobile) install no handler.
class TCrashHandler
{
public:
    using TAnnotations = std::map<QString, QString>;
    using TSessiondumpProvider = std::function<QByteArray()>;

    static TCrashHandler& instance();

    virtual ~TCrashHandler();

    /// Installs the handler writing into `dumpPath` and annotates the product.
    void initialize(const QString& productName, const QString& version, const QString& dumpPath);
    /// Writes a minidump of the running process now; false when none was written.
    bool writeMinidump();
    void setAnnotation(const QString& key, const QString& value);
    void removeAnnotation(const QString& key);
    /// Keeps `message` for a minute in the LogMessages annotation.
    void addLogMessage(const QString& message);
    void clearLogMessages();
    /// Supplies the session dump written beside a minidump.
    void setSessiondumpProvider(TSessiondumpProvider provider);
    /// Whether a crash is passed on to the system after the files are written.
    void setPassExceptionOn(bool passExceptionOn);
    bool passExceptionOn() const;

    /// Replaces the character name of `annotations` with "xxx".
    static void anonymize(TAnnotations& annotations);
    static TAnnotations annotationsFromJson(const QByteArray& json);
    static QJsonDocument annotationsToJsonDocument(const TAnnotations& annotations);
    static QByteArray annotationsToJson(const TAnnotations& annotations);
    /// "<file name of minidumpPath>.json".
    static QString annotationsFileName(const QString& minidumpPath);

    /// Breakpad's minidump callback: writes the crash files; true stops the
    /// exception when the minidump succeeded and it is not passed on.
    static bool minidumpCallback(const char* dumpDirectory, const char* minidumpId, void* context, bool succeeded);

private:
    class TExceptionHandler;
    struct TLogMessage
    {
        quint32 timestamp = 0;
        QString message;
    };

    TCrashHandler();
    void installHandler(const QString& dumpPath);
    void writeCrashFiles(const QString& dumpDirectory, const QString& minidumpId);
    /// Drops the log messages older than a minute.
    void removeOldLogMessages();
    void updateLogMessagesAnnotation();

    std::unique_ptr<TExceptionHandler> m_exceptionHandler;
    TAnnotations m_annotations;
    std::vector<TLogMessage> m_logMessages;
    bool m_passExceptionOn = true;
    TSessiondumpProvider m_sessiondumpProvider;
};

}
}
