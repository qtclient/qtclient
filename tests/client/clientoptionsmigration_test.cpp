#include <gtest/gtest.h>

#include "config/clientoptionsmigration.h"
#include "config/hotkeyoptions.h"

#include <QJsonArray>

using namespace tibia::config;

// The migration reads CIP's default sets; a fixed stand-in keeps the test off
// the resource file and the rest of THotkeyOptions.
QJsonObject THotkeyOptions::builtInSets()
{
    const QJsonObject global{ { QStringLiteral("chatOff"), QJsonArray{ QJsonObject{ { QStringLiteral("keysequence"), QStringLiteral("Esc") } } } },
                              { QStringLiteral("chatOn"), QJsonArray() } };
    return { { QStringLiteral("GlobalShortcuts"), global }, { QStringLiteral("Monk"), QJsonObject{ { QStringLiteral("chatOn"), QJsonArray() } } } };
}

TEST(ClientOptionsMigration, CurrentVersionIsUntouched)
{
    const QJsonObject root{ { QStringLiteral("clientOptionsVersion"), 9 }, { QStringLiteral("options"), QJsonObject{ { QStringLiteral("x"), 1 } } } };
    EXPECT_EQ(clientoptionsmigration::migrate(root), root);
}

TEST(ClientOptionsMigration, VersionZeroRunsTheWholeChain)
{
    const QJsonObject root{
        { QStringLiteral("hotkeyOptions"), QJsonObject{ { QStringLiteral("hotkeySets"), QJsonObject{ { QStringLiteral("Knight"), QJsonObject{ { QStringLiteral("F1"), QJsonObject{ { QStringLiteral("chatText"), QStringLiteral("hi") } } } } } } } } },
        { QStringLiteral("generalOptions"), QJsonObject{ { QStringLiteral("classicControl"), true }, { QStringLiteral("showCreatureNames"), false }, { QStringLiteral("lootScheme"), QStringLiteral("left") } } },
        { QStringLiteral("graphicsOptions"), QJsonObject{ { QStringLiteral("noFrameRateLimit"), true }, { QStringLiteral("ambientLight"), 40 } } },
    };
    const QJsonObject out = clientoptionsmigration::migrate(root);
    EXPECT_EQ(out.value(QLatin1String("clientOptionsVersion")).toInt(), 9);
    EXPECT_FALSE(out.contains(QLatin1String("generalOptions")));
    EXPECT_FALSE(out.contains(QLatin1String("graphicsOptions")));
    const QJsonObject options = out.value(QLatin1String("options")).toObject();
    EXPECT_EQ(options.value(QLatin1String("controlSchemeIndex")).toInt(-1), 0);
    EXPECT_EQ(options.value(QLatin1String("lootSchemeIndex")).toInt(-1), 2);
    EXPECT_EQ(options.value(QLatin1String("playerShowName")).toBool(true), false);
    EXPECT_EQ(options.value(QLatin1String("creatureShowName")).toBool(true), false);
    EXPECT_EQ(options.value(QLatin1String("frameRateLimitEnabled")).toBool(true), false);
    EXPECT_DOUBLE_EQ(options.value(QLatin1String("lightAmbientLevel")).toDouble(), 0.4);

    const QJsonObject sets = out.value(QLatin1String("hotkeyOptions")).toObject().value(QLatin1String("hotkeySets")).toObject();
    const QJsonArray chatOff = sets.value(QLatin1String("Knight")).toObject().value(QLatin1String("chatOff")).toArray();
    ASSERT_EQ(chatOff.size(), 2);
    EXPECT_EQ(chatOff.at(1).toObject().value(QLatin1String("keysequence")).toString(), QStringLiteral("F1"));
    EXPECT_TRUE(sets.contains(QLatin1String("Monk")));
}

TEST(ClientOptionsMigration, ActionBarRenamesAtVersionSix)
{
    const QJsonObject root{ { QStringLiteral("clientOptionsVersion"), 6 },
                            { QStringLiteral("options"), QJsonObject{ { QStringLiteral("actionBarShowRight"), true }, { QStringLiteral("quickLootAllCorpsesOnCoordinateEnabled"), true } } } };
    const QJsonObject options = clientoptionsmigration::migrate(root).value(QLatin1String("options")).toObject();
    EXPECT_TRUE(options.value(QLatin1String("actionBarsShowBottom")).toBool());
    EXPECT_TRUE(options.value(QLatin1String("actionBarsShowRight")).toBool());
    EXPECT_FALSE(options.contains(QLatin1String("actionBarShowRight")));
    EXPECT_TRUE(options.value(QLatin1String("quickLootAllCorpsesInAreaEnabled")).toBool());
}
