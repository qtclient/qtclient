#include "logging.h"

#include <spdlog/logger.h>
#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

namespace tibia::shared {

namespace {

/// How many rotated log files are kept besides the current one.
constexpr std::size_t MaxLogFiles = 1;

const char* const LogPattern = "[ %Y-%m-%d %H:%M:%S,%e ] %^%v%$";

std::shared_ptr<spdlog::logger> g_logger;
std::function<void(const QString&)> g_logMessageCallback;
QtMessageHandler g_currentQtMessageHandler = nullptr;

/// spdlog level of a QtMsgType; an unknown type logs as info.
spdlog::level::level_enum levelOf(const QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return spdlog::level::debug;
    case QtWarningMsg:
        return spdlog::level::warn;
    case QtCriticalMsg:
        return spdlog::level::err;
    case QtFatalMsg:
        return spdlog::level::critical;
    case QtInfoMsg:
        return spdlog::level::info;
    }
    return spdlog::level::info;
}

}

void setUpLogging(const QString& logFilePath, const std::size_t maxFileSize)
{
    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logFilePath.toStdString(), maxFileSize, MaxLogFiles, false);
    auto consoleSink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>(spdlog::color_mode::automatic);
    g_logger = std::make_shared<spdlog::logger>("tibia", spdlog::sinks_init_list{ fileSink, consoleSink });
    g_logger->set_pattern(LogPattern, spdlog::pattern_time_type::local);
    g_logger->set_level(spdlog::level::trace);
    g_logger->flush_on(spdlog::level::trace);
    installQtMessageHandler(&qtMessageHandler);
}

void setLogMessageCallback(std::function<void(const QString&)> callback)
{
    g_logMessageCallback = std::move(callback);
}

void installQtMessageHandler(const QtMessageHandler handler)
{
    if (g_currentQtMessageHandler)
        throw std::runtime_error("CurrentQtMessageHandler != nullptr");
    qInstallMessageHandler(handler);
    g_currentQtMessageHandler = handler;
}

void qtMessageHandler(const QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QByteArray localMessage = message.toLocal8Bit();
    const char* const file = context.file ? context.file : "";
    const char* const function = context.function ? context.function : "";
    if (g_logMessageCallback)
        g_logMessageCallback(message);
    if (g_logger) {
        g_logger->log(spdlog::source_loc{ file, context.line, function }, levelOf(type),
            spdlog::string_view_t(localMessage.constData(), std::size_t(localMessage.size())));
    }
    if (type == QtFatalMsg)
        std::abort();
}

}
