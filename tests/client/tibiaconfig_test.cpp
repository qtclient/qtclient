#include "config/tibiaconfig.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QTemporaryDir>

namespace {

using tibia::config::TTibiaConfig;

std::shared_ptr<QSettings> settingsFrom(const QTemporaryDir& dir, const QByteArray& ini)
{
    const QString path = dir.filePath(QStringLiteral("config.ini"));
    QFile file(path);
    EXPECT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(ini);
    file.close();
    return std::make_shared<QSettings>(path, QSettings::IniFormat);
}

const QByteArray kIni = "[URLS]\n"
                        "loginWebService=https://www.test.tibia.com/clientservices/loginservice.php\n"
                        "wheelOfDestinyPlannerDirectUrl=https://www.test.tibia.com/community/?subtopic=wheelofdestinyplanner&code=\n"
                        "[GRAPHICS]\n"
                        ";spritesheetCacheSize=350\n"
                        "qsgAtlasWidth=4096\n"
                        "qsgAtlasHeight=100\n"
                        "maximumNumberOfEffects=900\n"
                        "renderLoopType=threaded\n"
                        "[MOVEMENT]\n"
                        "minPercentageOfCurrentMovementBeforeSendNext=0.5\n";

TEST(TTibiaConfig, ReadsGroupValuesWithDefaults)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const TTibiaConfig config(settingsFrom(dir, kIni));
    EXPECT_EQ(config.clientSetting(QStringLiteral("GRAPHICS"), QStringLiteral("qsgAtlasWidth"), 1), 4096);
    // A commented key is unset: the default.
    EXPECT_EQ(config.clientSetting(QStringLiteral("GRAPHICS"), QStringLiteral("spritesheetCacheSize"), 777), 777);
    // The key of another group is not read.
    EXPECT_EQ(config.clientSetting(QStringLiteral("NETWORK"), QStringLiteral("qsgAtlasWidth"), 5), 5);
    EXPECT_DOUBLE_EQ(config.clientSettingReal(QStringLiteral("MOVEMENT"),
                                              QStringLiteral("minPercentageOfCurrentMovementBeforeSendNext"), 1.0),
                     0.5);
    EXPECT_EQ(config.clientSettingString(QStringLiteral("GRAPHICS"), QStringLiteral("renderLoopType"), QString()),
              QStringLiteral("threaded"));
}

TEST(TTibiaConfig, HeldValuesAreWrittenBack)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const TTibiaConfig config(settingsFrom(dir, kIni));
    EXPECT_EQ(config.clientSettingAtLeast(QStringLiteral("GRAPHICS"), QStringLiteral("qsgAtlasHeight"), 4096, 512), 512);
    EXPECT_EQ(config.clientSetting(QStringLiteral("GRAPHICS"), QStringLiteral("qsgAtlasHeight"), 0), 512);
    EXPECT_EQ(config.clientSettingAtMost(QStringLiteral("GRAPHICS"), QStringLiteral("maximumNumberOfEffects"), 400, 800),
              800);
    EXPECT_EQ(config.clientSetting(QStringLiteral("GRAPHICS"), QStringLiteral("maximumNumberOfEffects"), 0), 800);
    // Within the bound the value stays.
    EXPECT_EQ(config.clientSettingAtLeast(QStringLiteral("GRAPHICS"), QStringLiteral("qsgAtlasWidth"), 4096, 512), 4096);
    // Unset, the default is held too.
    EXPECT_EQ(config.clientSettingAtLeast(QStringLiteral("GRAPHICS"), QStringLiteral("spritesheetCacheSize"), 1, 2), 2);
}

TEST(TTibiaConfig, UrlsComeFromTheUrlsGroup)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const TTibiaConfig config(settingsFrom(dir, kIni));
    EXPECT_EQ(config.loginWebServiceUrl().host(), QStringLiteral("www.test.tibia.com"));
    EXPECT_EQ(config.readUrl(QStringLiteral("hintsUrl"), QUrl(QStringLiteral("https://x.example/h.json"))),
              QUrl(QStringLiteral("https://x.example/h.json")));
    QTemporaryDir emptyDir;
    ASSERT_TRUE(emptyDir.isValid());
    const TTibiaConfig empty(settingsFrom(emptyDir, QByteArray()));
    EXPECT_TRUE(empty.loginWebServiceUrl().isEmpty());
}

TEST(TTibiaConfig, WheelPlannerUrlDefaultsToTheTibiaPage)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const TTibiaConfig config(settingsFrom(dir, kIni));
    EXPECT_EQ(config.wheelOfDestinyPlannerDirectUrl().toString(),
              QStringLiteral("https://www.test.tibia.com/community/?subtopic=wheelofdestinyplanner&code="));
    QTemporaryDir emptyDir;
    ASSERT_TRUE(emptyDir.isValid());
    const TTibiaConfig empty(settingsFrom(emptyDir, QByteArray()));
    EXPECT_EQ(empty.wheelOfDestinyPlannerDirectUrl(), QUrl(QStringLiteral("http://www.tibia.com/")));
}

}
