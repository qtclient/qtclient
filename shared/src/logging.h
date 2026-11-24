#pragma once

#include <QString>
#include <QtGlobal>

#include <cstddef>
#include <functional>

namespace tibia {
namespace shared {

/// CipSoft's process log, shared by the client and the launcher: one spdlog
/// logger "tibia" writing every Qt message as `[ <local time,ms> ] <message>`
/// to a rotating log file (one backup) and to a coloured stdout.

/// Creates the logger on `logFilePath`, rotated at `maxFileSize` bytes, logs
/// and flushes every level, and installs `qtMessageHandler`.
void setUpLogging(const QString& logFilePath, std::size_t maxFileSize);

/// Receives the text of every message before it is logged (the client keeps
/// the last minute for crash reports).
void setLogMessageCallback(std::function<void(const QString&)> callback);

/// Installs `handler` as Qt's message handler; throws std::runtime_error when
/// one is already installed.
void installQtMessageHandler(QtMessageHandler handler);
/// Removes the installed Qt message handler; messages go to Qt's default output again.
void uninstallQtMessageHandler();

/// Passes the message to the callback, then to the logger at the level of its
/// type, and aborts on QtFatalMsg.
void qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message);

}
}
