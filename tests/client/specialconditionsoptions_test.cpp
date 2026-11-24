#include <gtest/gtest.h>

#include "app/clientoptions.h"
#include "config/specialconditionsoptions.h"

#include <QJsonArray>
#include <QJsonObject>

using namespace tibia::config;

// specialconditionsoptions.cpp's load() reads the options file through
// ClientOptions; the functions under test do not.
namespace tibia::app {
ClientOptions ClientOptions::load()
{
    return {};
}
}

namespace {

QJsonArray pair(int type, int value)
{
    return QJsonArray{ type, value };
}

}

TEST(SpecialConditionsOptions, EmptyOrderResetsTheWholeSectionToDefaults)
{
    const QJsonObject json{
        { QStringLiteral("showInHUD"), true },
        { QStringLiteral("showInBar"), false },
        { QStringLiteral("visibleInHUD"), QJsonArray{ pair(0, 7) } },
        { QStringLiteral("specialConditionsOrder"), QJsonArray{} },
    };
    const SpecialConditionsOptions o = SpecialConditionsOptions::fromJson(json);
    const SpecialConditionsOptions d = SpecialConditionsOptions::defaults();
    EXPECT_EQ(o.showInHUD, d.showInHUD);
    EXPECT_EQ(o.showInBar, d.showInBar);
    EXPECT_EQ(o.visibleInHUD, d.visibleInHUD);
    EXPECT_EQ(o.specialConditionsOrder, d.specialConditionsOrder);
}

TEST(SpecialConditionsOptions, OrderDropsUnknownAndAppendsMissingKnown)
{
    const QJsonObject json{
        { QStringLiteral("specialConditionsOrder"),
          QJsonArray{ pair(0, 5), pair(0, 999), QStringLiteral("x"), pair(9, 1), pair(0, 0) } },
    };
    const SpecialConditionsOptions o = SpecialConditionsOptions::fromJson(json);
    ASSERT_EQ(o.specialConditionsOrder.size(), SpecialConditionsOptions::known().size());
    EXPECT_EQ(o.specialConditionsOrder.at(0), (SpecialCondition{ 0, 5 }));
    EXPECT_EQ(o.specialConditionsOrder.at(1), (SpecialCondition{ 0, 0 }));
    // The rest follow in the default order, skipping the two already placed.
    EXPECT_EQ(o.specialConditionsOrder.at(2), (SpecialCondition{ 0, 1 }));
    EXPECT_TRUE(o.showInHUD);
    EXPECT_TRUE(o.showInBar);
}

TEST(SpecialConditionsOptions, SetsKeepAnyVariantAndCutPlayerKillerFlagsToAByte)
{
    const QJsonObject json{
        { QStringLiteral("visibleInBar"), QJsonArray{ pair(0, 999), pair(3, 0x102), pair(5, 1), 7 } },
        { QStringLiteral("specialConditionsOrder"), QJsonArray{ pair(0, 0) } },
    };
    const SpecialConditionsOptions o = SpecialConditionsOptions::fromJson(json);
    EXPECT_EQ(o.visibleInBar, (QSet<SpecialCondition>{ { 0, 999 }, { 3, 2 } }));
    EXPECT_TRUE(o.visibleInHUD.isEmpty());
}

TEST(SpecialConditionsOptions, ToJsonWritesSortedSetsAndTheOrder)
{
    SpecialConditionsOptions o;
    o.showInHUD = false;
    o.visibleInHUD = { { 4, 1 }, { 0, 3 } };
    o.specialConditionsOrder = { { 4, 1 }, { 0, 3 } };
    const QJsonObject json = o.toJson();
    EXPECT_EQ(json.value(QStringLiteral("showInHUD")).toBool(true), false);
    EXPECT_EQ(json.value(QStringLiteral("visibleInHUD")).toArray(), (QJsonArray{ pair(0, 3), pair(4, 1) }));
    EXPECT_EQ(json.value(QStringLiteral("specialConditionsOrder")).toArray(), (QJsonArray{ pair(4, 1), pair(0, 3) }));
}
