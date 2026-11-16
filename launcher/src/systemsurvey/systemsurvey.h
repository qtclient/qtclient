#pragma once

#include <QString>
#include <QtGlobal>

#include <vector>

namespace systemsurvey {

/// One screen: its largest native (unscaled) mode where macOS scales it.
struct TMonitorInformation {
    qint64 width = 0;
    qint64 height = 0;
    bool primary = false;
    qint64 refreshRate = 0;
};

struct TOperatingSystemInformation {
    QString baseSystem;
    QString name;
    /// "<major>[.<minor>[.<micro>]]"; left out of the JSON when empty.
    QString release;
    QString cpuArchitecture;
    QString osArchitecture = QStringLiteral("unknown");
    bool reserved = false;
};

struct TCpuInformation {
    QString manufacturer;
    QString brandName;
    bool is64Bit = false;
    bool sse = false;
    bool sse2 = false;
    qint64 numaNumber = 0;
    qint64 physicalNumber = 0;
    qint64 coreNumber = 0;
    qint64 logicalCoreNumber = 0;
    /// MHz.
    double clockSpeed = 0.0;
};

struct TGpuInformation {
    QString deviceString;
    /// Bytes.
    qint64 videoMemorySize = 0;
    int vendorId = 0;
    int deviceId = 0;
    QString driverVersion;
};

struct TOpenGLInformation {
    QString version;
    int maxTextureSize = 0;
};

/// Everything the survey reports.
struct TSystemSurvey {
    std::vector<TMonitorInformation> monitors;
    TOperatingSystemInformation operatingSystem;
    qint64 mainMemorySize = 0;
    TCpuInformation cpu;
    std::vector<TGpuInformation> gpus;
    TOpenGLInformation openGL;
};

} // namespace systemsurvey
