#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QString>

#include <map>

namespace tibia {
namespace shared {

/// CIP `shared::TCrashReportData`: the annotations of a crash report, keyed
/// by name. The crash handler collects them and writes them beside the
/// minidump; the client reads them back, the crash report dialog completes
/// them (Comment, ReportType) and the sender posts one form field each.
class TCrashReportData : public std::map<QString, QString>
{
public:
    TCrashReportData() = default;
    /// The members of the JSON object `json`, each as a string.
    explicit TCrashReportData(const QByteArray& json);

    std::map<QString, QString>& values();
    const std::map<QString, QString>& values() const;
    /// One JSON object with a string member per annotation.
    QJsonDocument toJsonDocument() const;
    /// toJsonDocument, indented.
    QByteArray toJson() const;
    /// Replaces the character name with "xxx".
    void anonymize();
};

}
}
