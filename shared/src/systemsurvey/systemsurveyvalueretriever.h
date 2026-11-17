#pragma once

#include "systemsurvey/systemsurvey.h"

#include <array>

namespace tibia::systemsurvey {

/// Reads the machine's values for the survey (CipSoft's macOS retriever:
/// screens via Qt + CoreGraphics, CPU via cpuid / sysctl, GPU via
/// system_profiler, OpenGL via an offscreen context).
class TSystemSurveyValueRetriever
{
public:
    virtual ~TSystemSurveyValueRetriever() = default;

    virtual std::vector<TMonitorInformation> getMonitorInformation() const;
    virtual TCpuInformation getCpuInformation() const;
    virtual TOpenGLInformation getOpenGLInformation() const;
    virtual TOperatingSystemInformation getOperatingSystemInformation() const;
    /// Bytes of physical memory (0 when unknown).
    virtual qint64 getMainMemorySize() const;
    virtual void getCpuCounts(qint64* numaNumber, qint64* physicalNumber, qint64* coreNumber,
                              qint64* logicalCoreNumber) const;
    /// MHz (0 when unknown).
    virtual double getCpuClockSpeed() const;
    virtual std::vector<TGpuInformation> getGpuInformation() const;

    /// cpuid registers of `leaf` as {eax, ebx, ecx, edx} (zeros off x86).
    static std::array<quint32, 4> cpuid(quint32 leaf);
    static QString cpuManufacturer();
    /// Manufacturer named by a cpuid vendor id ("GenuineIntel" -> "Intel").
    static QString manufacturerOfVendorId(const char* vendorId);
    static QString cpuBrandName();
    static bool cpuIs64Bit();
    static bool cpuHasSse();
    static bool cpuHasSse2();
    /// Screens with their native resolution; runs where the GUI lives.
    static std::vector<TMonitorInformation> queryMonitors();
    /// Parses `system_profiler SPDisplaysDataType -xml` output.
    static std::vector<TGpuInformation> parseDisplaysDataType(const QByteArray& xml);
};

} // namespace tibia::systemsurvey
