#include "systemsurveytojsonconverter.h"

#include <QCryptographicHash>
#include <QNetworkInterface>

#include <algorithm>

namespace systemsurvey {

QString TSystemSurveyToJsonConverter::clientId()
{
    static QString clientId;
    if (!clientId.isEmpty())
        return clientId;
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(QStringLiteral("91bb3378-6faa-40dc-8f8b-93b2f1d55e48").toLocal8Bit());
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    std::sort(interfaces.begin(), interfaces.end(),
              [](const QNetworkInterface& a, const QNetworkInterface& b) { return a.index() < b.index(); });
    for (const QNetworkInterface& networkInterface : interfaces)
        hash.addData(networkInterface.hardwareAddress().toLocal8Bit());
    const QByteArray hex = hash.result().toHex();
    if (hex.isEmpty())
        clientId.clear();
    else
        clientId = QString::fromLatin1(hex);
    return clientId;
}

QJsonObject TSystemSurveyToJsonConverter::monitorToJson(const TMonitorInformation& monitor)
{
    QJsonObject object;
    object.insert(QStringLiteral("width"), static_cast<int>(monitor.width));
    object.insert(QStringLiteral("height"), static_cast<int>(monitor.height));
    object.insert(QStringLiteral("primary"), monitor.primary);
    object.insert(QStringLiteral("refreshRate"), static_cast<int>(monitor.refreshRate));
    return object;
}

QJsonArray TSystemSurveyToJsonConverter::monitorsToJson(const std::vector<TMonitorInformation>& monitors)
{
    QJsonArray array;
    for (const TMonitorInformation& monitor : monitors)
        array.append(monitorToJson(monitor));
    return array;
}

QJsonObject TSystemSurveyToJsonConverter::operatingSystemToJson(const TOperatingSystemInformation& system)
{
    QJsonObject object;
    object.insert(QStringLiteral("baseSystem"), system.baseSystem);
    object.insert(QStringLiteral("name"), system.name);
    if (!system.release.isEmpty())
        object.insert(QStringLiteral("release"), system.release);
    object.insert(QStringLiteral("cpuArchitecture"), system.cpuArchitecture);
    object.insert(QStringLiteral("osArchitecture"), system.osArchitecture);
    return object;
}

QJsonObject TSystemSurveyToJsonConverter::cpuToJson(const TCpuInformation& cpu)
{
    QJsonObject object;
    object.insert(QStringLiteral("manufacturer"), cpu.manufacturer);
    object.insert(QStringLiteral("brandName"), cpu.brandName);
    object.insert(QStringLiteral("64Bit"), cpu.is64Bit);
    object.insert(QStringLiteral("sse"), cpu.sse);
    object.insert(QStringLiteral("sse2"), cpu.sse2);
    object.insert(QStringLiteral("NUMANumber"), static_cast<int>(cpu.numaNumber));
    object.insert(QStringLiteral("physicalNumber"), static_cast<int>(cpu.physicalNumber));
    object.insert(QStringLiteral("coreNumber"), static_cast<int>(cpu.coreNumber));
    object.insert(QStringLiteral("logicalCoreNumber"), static_cast<int>(cpu.logicalCoreNumber));
    object.insert(QStringLiteral("clockSpeed"), cpu.clockSpeed);
    return object;
}

QJsonObject TSystemSurveyToJsonConverter::gpuToJson(const TGpuInformation& gpu)
{
    QJsonObject object;
    object.insert(QStringLiteral("deviceString"), gpu.deviceString);
    object.insert(QStringLiteral("videoMemorySize"), gpu.videoMemorySize);
    if (gpu.vendorId != 0)
        object.insert(QStringLiteral("vendorId"), gpu.vendorId);
    if (gpu.deviceId != 0)
        object.insert(QStringLiteral("deviceId"), gpu.deviceId);
    if (!gpu.driverVersion.isEmpty())
        object.insert(QStringLiteral("driverVersion"), gpu.driverVersion);
    return object;
}

QJsonArray TSystemSurveyToJsonConverter::gpusToJson(const std::vector<TGpuInformation>& gpus)
{
    QJsonArray array;
    for (const TGpuInformation& gpu : gpus)
        array.append(gpuToJson(gpu));
    return array;
}

QJsonObject TSystemSurveyToJsonConverter::openGLToJson(const TOpenGLInformation& openGL)
{
    QJsonObject object;
    object.insert(QStringLiteral("version"), openGL.version);
    object.insert(QStringLiteral("maxTextureSize"), openGL.maxTextureSize);
    return object;
}

QJsonDocument TSystemSurveyToJsonConverter::convert(const TSystemSurvey& survey) const
{
    QJsonObject object;
    object.insert(QStringLiteral("dataType"), 0);
    object.insert(QStringLiteral("clientID"), clientId());
    object.insert(QStringLiteral("monitors"), monitorsToJson(survey.monitors));
    object.insert(QStringLiteral("operatingSystem"), operatingSystemToJson(survey.operatingSystem));
    object.insert(QStringLiteral("mainMemorySize"), survey.mainMemorySize);
    object.insert(QStringLiteral("cpu"), cpuToJson(survey.cpu));
    object.insert(QStringLiteral("gpu"), gpusToJson(survey.gpus));
    object.insert(QStringLiteral("openGL"), openGLToJson(survey.openGL));
    QJsonDocument document;
    document.setObject(object);
    return document;
}

} // namespace systemsurvey
