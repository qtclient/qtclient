#include "cloudflarecdntrace.h"
#include "package.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <gtest/gtest.h>

using namespace launcher;

TEST(LauncherPackage, ResolvesRelativeUrlsAgainstTheBasePath)
{
    const QUrl base(QStringLiteral("https://cdn.example/launcher/client/"));
    EXPECT_EQ(resolveConfigurationUrl(base, QUrl(QStringLiteral("package.json"))).toString(),
              QStringLiteral("https://cdn.example/launcher/client/package.json"));
    // No doubled separator, and the base query is dropped.
    EXPECT_EQ(resolveConfigurationUrl(QUrl(QStringLiteral("https://cdn.example/a?x=1")),
                                      QUrl(QStringLiteral("/b.json")))
                  .toString(),
              QStringLiteral("https://cdn.example/a/b.json"));
    EXPECT_EQ(resolveConfigurationUrl(QUrl(QStringLiteral("https://cdn.example/a")), QUrl(QStringLiteral("b.json")))
                  .toString(),
              QStringLiteral("https://cdn.example/a/b.json"));
    // Absolute urls stay.
    EXPECT_EQ(resolveConfigurationUrl(base, QUrl(QStringLiteral("https://other.example/x"))).toString(),
              QStringLiteral("https://other.example/x"));
}

TEST(LauncherPackage, ParsesBuildArchitecturesAsMask)
{
    TPackage package(0, QJsonObject(), QUrl());
    const QJsonObject build = QJsonDocument::fromJson(R"({"os":"macos","architecture":"x86_64;arm64","url":"p.json"})")
                                  .object();
    const TPackageBuild parsed(&package, build, QUrl(QStringLiteral("https://cdn.example/l")));
    EXPECT_EQ(parsed.os, EOperatingSystem::MacOS);
    EXPECT_EQ(parsed.architectures, ArchitectureX86_64 | ArchitectureArm64);
    EXPECT_EQ(parsed.url.toString(), QStringLiteral("https://cdn.example/l/p.json"));
    EXPECT_FALSE(parsed.versionUrl.isValid());
}

TEST(LauncherPackage, VariantsAndStatusNames)
{
    const TPackage beta(1, QJsonDocument::fromJson(R"({"name":"T","variant":"beta"})").object(), QUrl());
    EXPECT_EQ(beta.variant(), TPackage::VariantBeta);
    EXPECT_EQ(TPackage::statusName(TPackage::PartiallyInstalled), QStringLiteral("PartiallyInstalled"));
    EXPECT_EQ(TPackage::statusName(static_cast<TPackage::EStatus>(9)), QStringLiteral("Invalid PackageStatus value."));
}

TEST(LauncherPackage, CombinedVersionMarksUnknownParts)
{
    const TPackageConfiguration configuration("15.33", QByteArray(), QUrl(), QUrl());
    EXPECT_EQ(configuration.combinedVersion(), QStringLiteral("P15.33 A?"));
}

TEST(LauncherPackage, LoadsFilesSortedAndGeneration)
{
    TPackageConfiguration configuration("1", "2", QUrl(QStringLiteral("https://c/p/package.json")),
                                        QUrl(QStringLiteral("https://c/a/assets.json")));
    ASSERT_TRUE(configuration.load(
        R"({"executable":"Tibia","generation":"first","files":[{"url":"z.lzma","localfile":"z"}],"parameters":["-x"]})",
        R"({"files":[{"url":"a.lzma","localfile":"a","unpack":true}]})"));
    EXPECT_EQ(configuration.generation(), 1);
    ASSERT_EQ(configuration.files().size(), 2u);
    EXPECT_EQ(configuration.files()[0].localFile, QStringLiteral("a"));
    EXPECT_EQ(configuration.files()[0].url.toString(), QStringLiteral("https://c/a/a.lzma"));
    EXPECT_TRUE(configuration.files()[0].unpack);
    EXPECT_EQ(configuration.files()[1].url.toString(), QStringLiteral("https://c/p/z.lzma"));
    EXPECT_EQ(configuration.parameters(), QStringList{QStringLiteral("-x")});
    EXPECT_FALSE(configuration.load("not json", QByteArray()));
}

TEST(LauncherCdnTrace, ParsesAndFormatsTraceFields)
{
    const TCdnTrace trace = parseCdnTrace(QStringLiteral("fl=12f\nh=x\nip= 1.2.3.4 \nts=17.5\ncolo=FRA\nbad=a=b\n"));
    EXPECT_EQ(trace.location, QStringLiteral("12f"));
    EXPECT_EQ(trace.ip, QStringLiteral("1.2.3.4"));
    EXPECT_EQ(formatCdnTrace(trace), QStringLiteral("FL: 12f, TS: 17.5, COLO: FRA, IP: 1.2.3.4"));
}
