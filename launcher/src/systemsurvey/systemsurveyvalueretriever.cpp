#include "systemsurveyvalueretriever.h"

#include <QDebug>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOperatingSystemVersion>
#include <QProcess>
#include <QScreen>
#include <QSurfaceFormat>
#include <QSysInfo>
#include <QXmlStreamReader>

#include <cstring>
#include <future>

#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif
#ifdef Q_OS_MACOS
#include <ApplicationServices/ApplicationServices.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

namespace systemsurvey {

namespace {

#ifdef Q_OS_MACOS
/// Host: native arm64 builds have no cpuid; macOS names the CPU in sysctl.
QString sysctlString(const char* name)
{
    size_t size = 0;
    if (sysctlbyname(name, nullptr, &size, nullptr, 0) != 0 || size == 0)
        return {};
    QByteArray value(static_cast<qsizetype>(size), '\0');
    if (sysctlbyname(name, value.data(), &size, nullptr, 0) != 0)
        return {};
    return QString::fromLatin1(value.constData()).simplified();
}
#endif

bool hasCpuid()
{
#if defined(__x86_64__) || defined(__i386__)
    return true;
#else
    return false;
#endif
}

} // namespace

std::array<quint32, 4> TSystemSurveyValueRetriever::cpuid(quint32 leaf)
{
    std::array<quint32, 4> registers{};
#if defined(__x86_64__) || defined(__i386__)
    __cpuid(leaf, registers[0], registers[1], registers[2], registers[3]);
#else
    Q_UNUSED(leaf);
#endif
    return registers;
}

QString TSystemSurveyValueRetriever::manufacturerOfVendorId(const char* vendorId)
{
    static const struct {
        const char* vendorId;
        const char* manufacturer;
    } manufacturers[] = {
        {"AMDisbetter!", "AMD"},
        {"AuthenticAMD", "AMD"},
        {"CentaurHauls", "Centaur"},
        {"CyrixInstead", "Cyrix"},
        {"GenuineIntel", "Intel"},
        {"TransmetaCPU", "Transmeta"},
        {"GenuineTMx86", "Transmeta"},
        {"Geode by NSC", "National Semiconductor"},
        {"NexGenDriven", "NexGen"},
        {"RiseRiseRise", "Rise"},
        {"SiS SiS SiS ", "SiS"},
        {"UMC UMC UMC ", "UMC"},
        {"VIA VIA VIA ", "VIA"},
        {"Vortex86 SoC", " Vortex"},
        {"  Shanghai  ", " Zhaoxin"},
        {"HygonGenuine", "Hygon"},
        {"E2K MACHINE", "MCST Elbrus"},
        {"MiSTer AO486", "ao486 CPU"},
        {"bhyve bhyve ", "bhyve"},
        {" KVMKVMKVM  ", "KVM"},
        {"TCGTCGTCGTCG", "QEMU"},
        {"Microsoft Hv", "Microsoft Hyper-V or Windows Virtual PC"},
        {" lrpepyh  vr", "Parallels"},
        {"VMwareVMware", "VMware"},
        {"XenVMMXenVMM", " Xen HVM"},
        {"ACRNACRNACRN", "Project ACRN"},
        {"QNXQVMBSQG", "QNX Hypervisor"},
    };
    for (const auto& entry : manufacturers) {
        if (std::strcmp(vendorId, entry.vendorId) == 0)
            return QString::fromLatin1(entry.manufacturer);
    }
    return QStringLiteral("Unknown Manufacturer");
}

QString TSystemSurveyValueRetriever::cpuManufacturer()
{
    if (!hasCpuid()) {
#ifdef Q_OS_MACOS
        return sysctlString("machdep.cpu.vendor");
#else
        return {};
#endif
    }
    const std::array<quint32, 4> registers = cpuid(0);
    char vendor[13] = {};
    std::memcpy(vendor, &registers[1], 4);
    std::memcpy(vendor + 4, &registers[3], 4);
    std::memcpy(vendor + 8, &registers[2], 4);
    return manufacturerOfVendorId(vendor);
}

QString TSystemSurveyValueRetriever::cpuBrandName()
{
    if (!hasCpuid()) {
#ifdef Q_OS_MACOS
        const QString brand = sysctlString("machdep.cpu.brand_string");
        if (!brand.isEmpty())
            return brand;
#endif
        return QStringLiteral("Acquiring Brand Name not supported");
    }
    if (cpuid(0x80000000)[0] < 0x80000004)
        return QStringLiteral("Acquiring Brand Name not supported");
    char brand[49] = {};
    for (quint32 part = 0; part < 3; ++part) {
        const std::array<quint32, 4> registers = cpuid(0x80000002 + part);
        std::memcpy(brand + part * 16, registers.data(), 16);
    }
    return QString::fromLatin1(brand).simplified();
}

bool TSystemSurveyValueRetriever::cpuIs64Bit()
{
    if (!hasCpuid())
        return QSysInfo::WordSize == 64;
    if (cpuid(0x80000000)[0] < 0x80000001)
        return false;
    // Long mode: extended leaf edx bit 29.
    return (cpuid(0x80000001)[3] >> 29) & 1;
}

bool TSystemSurveyValueRetriever::cpuHasSse()
{
    return (cpuid(1)[3] >> 25) & 1;
}

bool TSystemSurveyValueRetriever::cpuHasSse2()
{
    return (cpuid(1)[3] >> 26) & 1;
}

std::vector<TMonitorInformation> TSystemSurveyValueRetriever::queryMonitors()
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    std::vector<TMonitorInformation> monitors(static_cast<size_t>(screens.size()));
    for (qsizetype index = 0; index < screens.size(); ++index) {
        QScreen* screen = screens[index];
        TMonitorInformation& monitor = monitors[static_cast<size_t>(index)];
        monitor.refreshRate = static_cast<qint64>(screen->refreshRate());
        const QRect geometry = screen->geometry();
        monitor.width = geometry.width();
        monitor.height = geometry.height();
        monitor.primary = screen == QGuiApplication::primaryScreen();
#ifdef Q_OS_MACOS
        // A scaled screen reports points: use the largest unscaled mode instead.
        CGDirectDisplayID display = 0;
        uint32_t count = 0;
        const CGPoint center = CGPointMake((geometry.left() + geometry.right()) / 2,
                                           (geometry.top() + geometry.bottom()) / 2);
        if (CGGetDisplaysWithPoint(center, 1, &display, &count) != kCGErrorSuccess || count == 0 || display == 0)
            continue;
        CGDisplayModeRef current = CGDisplayCopyDisplayMode(display);
        if (!current)
            continue;
        const size_t width = CGDisplayModeGetWidth(current);
        const size_t height = CGDisplayModeGetHeight(current);
        const size_t pixelWidth = CGDisplayModeGetPixelWidth(current);
        const size_t pixelHeight = CGDisplayModeGetPixelHeight(current);
        CGDisplayModeRelease(current);
        if (width == pixelWidth && height == pixelHeight)
            continue;
        const void* keys[] = {kCGDisplayShowDuplicateLowResolutionModes};
        const void* values[] = {kCFBooleanTrue};
        CFDictionaryRef options = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1,
                                                     &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFArrayRef modes = CGDisplayCopyAllDisplayModes(display, options);
        CFRelease(options);
        if (!modes)
            continue;
        size_t largest = 0;
        for (CFIndex mode = 0; mode < CFArrayGetCount(modes); ++mode) {
            auto* candidate = static_cast<CGDisplayModeRef>(const_cast<void*>(CFArrayGetValueAtIndex(modes, mode)));
            const size_t modeWidth = CGDisplayModeGetWidth(candidate);
            const size_t modeHeight = CGDisplayModeGetHeight(candidate);
            if (modeWidth == CGDisplayModeGetPixelWidth(candidate) && modeHeight == CGDisplayModeGetPixelHeight(candidate)
                && largest < modeWidth * modeHeight) {
                monitor.width = static_cast<qint64>(modeWidth);
                monitor.height = static_cast<qint64>(modeHeight);
                largest = modeWidth * modeHeight;
            }
        }
        CFRelease(modes);
#endif
    }
    return monitors;
}

std::vector<TMonitorInformation> TSystemSurveyValueRetriever::getMonitorInformation() const
{
    return std::async(std::launch::async, &TSystemSurveyValueRetriever::queryMonitors).get();
}

TCpuInformation TSystemSurveyValueRetriever::getCpuInformation() const
{
    TCpuInformation cpu;
    cpu.manufacturer = cpuManufacturer();
    cpu.brandName = cpuBrandName();
    cpu.is64Bit = cpuIs64Bit();
    cpu.sse = cpuHasSse();
    cpu.sse2 = cpuHasSse2();
    getCpuCounts(&cpu.numaNumber, &cpu.physicalNumber, &cpu.coreNumber, &cpu.logicalCoreNumber);
    cpu.clockSpeed = getCpuClockSpeed();
    return cpu;
}

TOpenGLInformation TSystemSurveyValueRetriever::getOpenGLInformation() const
{
    TOpenGLInformation information;
    QOpenGLContext context;
    context.setFormat(QSurfaceFormat::defaultFormat());
    context.create();
    QOffscreenSurface surface;
    surface.setFormat(context.format());
    surface.create();
    if (!context.makeCurrent(&surface)) {
        qCritical() << "TOpenGLValueRetriever::getOpenGLInformation: Failed to create a GL context.";
        return information;
    }
    QOpenGLFunctions* functions = context.functions();
    information.version = QString::fromUtf8(reinterpret_cast<const char*>(functions->glGetString(GL_VERSION)));
    functions->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &information.maxTextureSize);
    context.doneCurrent();
    return information;
}

TOperatingSystemInformation TSystemSurveyValueRetriever::getOperatingSystemInformation() const
{
    TOperatingSystemInformation system;
    system.baseSystem = QStringLiteral("mac");
    system.name = QSysInfo::prettyProductName();
    const QOperatingSystemVersion version = QOperatingSystemVersion::current();
    if (version.majorVersion() >= 0) {
        system.release = QStringLiteral("%1").arg(version.majorVersion());
        if (version.minorVersion() >= 0) {
            system.release.append(QStringLiteral(".%1").arg(version.minorVersion()));
            if (version.microVersion() >= 0)
                system.release.append(QStringLiteral(".%1").arg(version.microVersion()));
        }
    }
    system.cpuArchitecture = QSysInfo::currentCpuArchitecture();
    return system;
}

qint64 TSystemSurveyValueRetriever::getMainMemorySize() const
{
#ifdef Q_OS_MACOS
    int name[2] = {CTL_HW, HW_MEMSIZE};
    quint64 memory = 0;
    size_t size = sizeof(memory);
    if (sysctl(name, 2, &memory, &size, nullptr, 0) == 0)
        return static_cast<qint64>(memory);
#endif
    return 0;
}

void TSystemSurveyValueRetriever::getCpuCounts(qint64* numaNumber, qint64* physicalNumber, qint64* coreNumber,
                                               qint64* logicalCoreNumber) const
{
    *numaNumber = 0;
    *physicalNumber = 0;
    *coreNumber = 0;
    *logicalCoreNumber = 0;
#ifdef Q_OS_MACOS
    int value = 0;
    size_t size = sizeof(value);
    if (sysctlbyname("hw.physicalcpu", &value, &size, nullptr, 0) == 0)
        *coreNumber = value;
    if (sysctlbyname("hw.logicalcpu", &value, &size, nullptr, 0) == 0)
        *logicalCoreNumber = value;
#endif
}

double TSystemSurveyValueRetriever::getCpuClockSpeed() const
{
#ifdef Q_OS_MACOS
    quint64 frequency = 0;
    size_t size = sizeof(frequency);
    if (sysctlbyname("hw.cpufrequency", &frequency, &size, nullptr, 0) == 0)
        return static_cast<double>(frequency) / 1000000.0;
#endif
    return 0.0;
}

std::vector<TGpuInformation> TSystemSurveyValueRetriever::parseDisplaysDataType(const QByteArray& xml)
{
    QString deviceString = QStringLiteral("not found");
    qint64 videoMemorySize = 0;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        if (!reader.readNextStartElement() || reader.name() != QLatin1String("key"))
            continue;
        const QString key = reader.readElementText();
        if (key == QLatin1String("sppci_model")) {
            if (reader.readNextStartElement() && reader.name() == QLatin1String("string"))
                deviceString = reader.readElementText();
        } else if (key == QLatin1String("spdisplays_vram")) {
            if (!reader.readNextStartElement() || reader.name() != QLatin1String("string"))
                continue;
            // "1536 MB", "8 GB", …
            const QStringList parts = reader.readElementText().simplified().split(QLatin1Char(' '));
            videoMemorySize = parts.value(0).toLongLong();
            const QString unit = parts.value(1).toUpper();
            const QChar prefix = unit.isEmpty() ? QChar() : unit.at(0);
            if (prefix == QLatin1Char('G'))
                videoMemorySize *= 1024LL * 1024 * 1024;
            else if (prefix == QLatin1Char('M'))
                videoMemorySize *= 1024LL * 1024;
            else if (prefix == QLatin1Char('K'))
                videoMemorySize *= 1024LL;
        }
    }
    if (reader.hasError() && reader.error() != QXmlStreamReader::PrematureEndOfDocumentError)
        qWarning() << "Problem while reading GPU information:" << reader.errorString();
    TGpuInformation gpu;
    gpu.deviceString = deviceString;
    gpu.videoMemorySize = videoMemorySize;
    return {gpu};
}

std::vector<TGpuInformation> TSystemSurveyValueRetriever::getGpuInformation() const
{
    QByteArray output;
    QProcess process;
    process.start(QStringLiteral("system_profiler"),
                  {QStringLiteral("SPDisplaysDataType"), QStringLiteral("-xml")}, QIODevice::ReadWrite);
    if (process.waitForStarted() && process.waitForFinished())
        output = process.readAllStandardOutput();
    return parseDisplaysDataType(output);
}

} // namespace systemsurvey
