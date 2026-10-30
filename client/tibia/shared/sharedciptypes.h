#pragma once

// CIP-named shared surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::shared {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TCrashReportSender final : public cip::shared::TCrashReportSender
{
    Q_OBJECT
public:
    using cip::shared::TCrashReportSender::TCrashReportSender;
};

class TDownloadPool final : public cip::shared::TDownloadPool
{
    Q_OBJECT
public:
    using cip::shared::TDownloadPool::TDownloadPool;
};

class TDynamicResourcesHelper final : public cip::tibia::shared::TDynamicResourcesHelper
{
    Q_OBJECT
public:
    using cip::tibia::shared::TDynamicResourcesHelper::TDynamicResourcesHelper;
};

class TExecutor final : public cip::shared::TExecutor
{
    Q_OBJECT
public:
    using cip::shared::TExecutor::TExecutor;
};

class TExperienceGainChangeTracker final : public ::tibia::CipTyped {
public:
    TExperienceGainChangeTracker() = default;
};

class THighlightTimer final : public cip::shared::THighlightTimer
{
    Q_OBJECT
public:
    using cip::shared::THighlightTimer::THighlightTimer;
};

class TIODeviceReader final : public ::tibia::CipTyped {
public:
    TIODeviceReader() = default;
};

class TIODeviceWriter final : public ::tibia::CipTyped {
public:
    TIODeviceWriter() = default;
};

class TImagestripResponse final : public ::tibia::CipTyped {
public:
    TImagestripResponse() = default;
};

class TJsonDocumentHttpPostSender final : public cip::shared::TJsonDocumentHttpPostSender
{
    Q_OBJECT
public:
    using cip::shared::TJsonDocumentHttpPostSender::TJsonDocumentHttpPostSender;
};

class TJsonDocumentUploaderBase final : public cip::shared::TJsonDocumentUploaderBase
{
    Q_OBJECT
public:
    using cip::shared::TJsonDocumentUploaderBase::TJsonDocumentUploaderBase;
};

class TNetworkAccessManagerAuthenticationHandler final : public cip::shared::TNetworkAccessManagerAuthenticationHandler
{
    Q_OBJECT
public:
    using cip::shared::TNetworkAccessManagerAuthenticationHandler::TNetworkAccessManagerAuthenticationHandler;
};

class TNullExperienceGainChangeTracker final : public ::tibia::CipTyped {
public:
    TNullExperienceGainChangeTracker() = default;
};

class TNullExperiencePerHourProvider final : public ::tibia::CipTyped {
public:
    TNullExperiencePerHourProvider() = default;
};

class TOptimized1PixelBorderImageProvider final : public ::tibia::CipTyped {
public:
    TOptimized1PixelBorderImageProvider() = default;
};

class TQtConnectionWrapper final : public cip::shared::TQtConnectionWrapper
{
    Q_OBJECT
public:
    using cip::shared::TQtConnectionWrapper::TQtConnectionWrapper;
};

class TRPCServiceClient final : public cip::shared::TRPCServiceClient
{
    Q_OBJECT
public:
    using cip::shared::TRPCServiceClient::TRPCServiceClient;
};

class TRPCServiceConnection final : public cip::shared::TRPCServiceConnection
{
    Q_OBJECT
public:
    using cip::shared::TRPCServiceConnection::TRPCServiceConnection;
};

class TRPCServiceServer final : public cip::shared::TRPCServiceServer
{
    Q_OBJECT
public:
    using cip::shared::TRPCServiceServer::TRPCServiceServer;
};

class TSingleThreadedLowPriorityExecutor final : public cip::shared::TSingleThreadedLowPriorityExecutor
{
    Q_OBJECT
public:
    using cip::shared::TSingleThreadedLowPriorityExecutor::TSingleThreadedLowPriorityExecutor;
};

class TSynchronousExecutor final : public cip::shared::TSynchronousExecutor
{
    Q_OBJECT
public:
    using cip::shared::TSynchronousExecutor::TSynchronousExecutor;
};

class TTibiaClipboardHelper final : public ::tibia::CipTyped {
public:
    TTibiaClipboardHelper() = default;
};

class TTibiaFileSystemHelper final : public ::tibia::CipTyped {
public:
    TTibiaFileSystemHelper() = default;
};

} // namespace tibia::shared

namespace tibia::shared::executors_private {

class TaskExecutorObject final : public cip::shared::executors_private::TaskExecutorObject
{
    Q_OBJECT
public:
    using cip::shared::executors_private::TaskExecutorObject::TaskExecutorObject;
};

} // namespace tibia::shared::executors_private

