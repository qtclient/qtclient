#include "systemsurveycreator.h"
#include "systemsurveytojsonconverter.h"
#include "systemsurveyvalueretriever.h"

#include <QJsonArray>
#include <QJsonObject>

#include <gtest/gtest.h>

using namespace systemsurvey;

TEST(SystemSurvey, KeepsOnlyMonitorsWithASize)
{
    const std::vector<TMonitorInformation> monitors{{1920, 1080, true, 60}, {0, 1080, false, 60}, {800, 0, false, 0}};
    const std::vector<TMonitorInformation> valid = TSystemSurveyCreator::validMonitors(monitors);
    ASSERT_EQ(valid.size(), 1u);
    EXPECT_EQ(valid[0].width, 1920);
}

TEST(SystemSurvey, ConvertsToTheServiceDocument)
{
    TSystemSurvey survey;
    survey.monitors = {{2560, 1440, true, 120}};
    survey.operatingSystem.baseSystem = QStringLiteral("mac");
    survey.operatingSystem.name = QStringLiteral("macOS 15.1");
    survey.operatingSystem.cpuArchitecture = QStringLiteral("arm64");
    survey.mainMemorySize = 17179869184LL;
    survey.cpu.brandName = QStringLiteral("CPU");
    survey.cpu.coreNumber = 8;
    survey.cpu.clockSpeed = 2400.0;
    survey.gpus = {{QStringLiteral("GPU"), 1536LL * 1024 * 1024, 0, 0, QString()}};
    survey.openGL = {QStringLiteral("2.1"), 16384};
    const QJsonObject json = TSystemSurveyToJsonConverter().convert(survey).object();
    EXPECT_EQ(json.value(QStringLiteral("dataType")).toInt(-1), 0);
    EXPECT_EQ(json.value(QStringLiteral("clientID")).toString().size(), 40);
    EXPECT_EQ(json.value(QStringLiteral("monitors")).toArray()[0].toObject().value(QStringLiteral("refreshRate")).toInt(), 120);
    const QJsonObject system = json.value(QStringLiteral("operatingSystem")).toObject();
    EXPECT_FALSE(system.contains(QStringLiteral("release")));
    EXPECT_EQ(system.value(QStringLiteral("osArchitecture")).toString(), QStringLiteral("unknown"));
    EXPECT_EQ(json.value(QStringLiteral("mainMemorySize")).toInteger(), 17179869184LL);
    EXPECT_EQ(json.value(QStringLiteral("cpu")).toObject().value(QStringLiteral("coreNumber")).toInt(), 8);
    const QJsonObject gpu = json.value(QStringLiteral("gpu")).toArray()[0].toObject();
    EXPECT_FALSE(gpu.contains(QStringLiteral("vendorId")));
    EXPECT_FALSE(gpu.contains(QStringLiteral("driverVersion")));
    EXPECT_EQ(json.value(QStringLiteral("openGL")).toObject().value(QStringLiteral("maxTextureSize")).toInt(), 16384);
    // The client id is stable within the process.
    EXPECT_EQ(TSystemSurveyToJsonConverter::clientId(), TSystemSurveyToJsonConverter::clientId());
}

TEST(SystemSurvey, ParsesSystemProfilerDisplays)
{
    const QByteArray xml = R"(<?xml version="1.0"?><plist version="1.0"><array><dict><key>_items</key><array><dict>
<key>sppci_model</key><string>Radeon Pro 560X</string><key>spdisplays_vram</key><string>4 GB</string>
</dict></array></dict></array></plist>)";
    const std::vector<TGpuInformation> gpus = TSystemSurveyValueRetriever::parseDisplaysDataType(xml);
    ASSERT_EQ(gpus.size(), 1u);
    EXPECT_EQ(gpus[0].deviceString, QStringLiteral("Radeon Pro 560X"));
    EXPECT_EQ(gpus[0].videoMemorySize, 4LL * 1024 * 1024 * 1024);
    EXPECT_EQ(TSystemSurveyValueRetriever::parseDisplaysDataType(QByteArray())[0].deviceString,
              QStringLiteral("not found"));
}

TEST(SystemSurvey, NamesCpuManufacturers)
{
    EXPECT_EQ(TSystemSurveyValueRetriever::manufacturerOfVendorId("GenuineIntel"), QStringLiteral("Intel"));
    EXPECT_EQ(TSystemSurveyValueRetriever::manufacturerOfVendorId("AuthenticAMD"), QStringLiteral("AMD"));
    EXPECT_EQ(TSystemSurveyValueRetriever::manufacturerOfVendorId("nonsense"), QStringLiteral("Unknown Manufacturer"));
}
