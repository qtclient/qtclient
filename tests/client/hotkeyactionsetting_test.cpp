#include <gtest/gtest.h>

#include "app/clientoptions.h"
#include "config/hotkeyoptions.h"

using namespace tibia::config;

// hotkeyoptions.cpp reads and writes the options files through ClientOptions;
// the functions under test do not, so empty stand-ins keep the test off the
// file system.
namespace tibia::app {
ClientOptions ClientOptions::load()
{
    return {};
}
void ClientOptions::storeAll(const QJsonObject&) {}
}

TEST(HotkeyActionSetting, EmptyMeansNoTextActionObjectPassiveOrMultiAction)
{
    TActionSetting setting;
    EXPECT_TRUE(setting.isEmpty());

    // A chat text setting without text is empty.
    setting.kind = TActionSetting::TEXT;
    EXPECT_TRUE(setting.isEmpty());
    setting.text = QStringLiteral("hi");
    EXPECT_FALSE(setting.isEmpty());

    TActionSetting object;
    object.kind = TActionSetting::OBJECT;
    EXPECT_TRUE(object.isEmpty());
    object.objectId = 3031;
    EXPECT_FALSE(object.isEmpty());

    TActionSetting passive;
    passive.kind = TActionSetting::PASSIVE_ABILITY;
    EXPECT_FALSE(passive.isEmpty());

    TActionSetting multi;
    multi.multiActions.push_back(TActionSetting{});
    EXPECT_FALSE(multi.isEmpty());
}

TEST(HotkeyActionSetting, ChatTextDropsObjectLiquidAndMultiActionsButKeepsTier)
{
    TActionSetting setting;
    setting.kind = TActionSetting::OBJECT;
    setting.objectId = 2874;
    setting.liquidType = 1;
    setting.upgradeTier = 2;
    setting.multiActions.push_back(TActionSetting{});

    setting.setChatText(QStringLiteral("exura"));
    EXPECT_EQ(setting.kind, TActionSetting::TEXT);
    EXPECT_EQ(setting.text, QStringLiteral("exura"));
    EXPECT_EQ(setting.objectId, 0);
    EXPECT_FALSE(setting.liquidType.has_value());
    EXPECT_TRUE(setting.multiActions.empty());
    EXPECT_EQ(setting.upgradeTier, std::optional<int>(2));

    // An empty text keeps the rest and leaves an empty setting.
    setting.setChatText(QString());
    EXPECT_TRUE(setting.text.isEmpty());
    EXPECT_TRUE(setting.isEmpty());
}

TEST(HotkeyActionSetting, EmptyChatTextKeepsTheObject)
{
    TActionSetting setting;
    setting.kind = TActionSetting::OBJECT;
    setting.objectId = 2874;
    setting.liquidType = 1;
    setting.setChatText(QString());
    EXPECT_EQ(setting.objectId, 2874);
    EXPECT_EQ(setting.liquidType, std::optional<int>(1));
    EXPECT_EQ(setting.kind, TActionSetting::OBJECT);
}

TEST(HotkeyOptionsData, SetNamesAreSortedCaseSensitively)
{
    THotkeyOptionsData options;
    options.hotkeySets[QStringLiteral("knight")] = {};
    options.hotkeySets[QStringLiteral("Paladin")] = {};
    options.hotkeySets[QStringLiteral("Druid")] = {};
    EXPECT_EQ(options.hotkeySetNames(),
              (QStringList{ QStringLiteral("Druid"), QStringLiteral("Paladin"), QStringLiteral("knight") }));
}

TEST(HotkeyOptionsData, CurrentSetIndexFollowsTheNameList)
{
    THotkeyOptionsData options;
    options.hotkeySets[QStringLiteral("B")] = {};
    options.hotkeySets[QStringLiteral("A")] = {};
    options.currentHotkeySetName = QStringLiteral("B");
    EXPECT_EQ(options.currentHotkeySetIndex(), 1);

    options.setCurrentHotkeySetIndex(0);
    EXPECT_EQ(options.currentHotkeySetName, QStringLiteral("A"));
    EXPECT_EQ(options.currentHotkeySetIndex(), 0);

    // An index outside the list changes nothing.
    options.setCurrentHotkeySetIndex(2);
    options.setCurrentHotkeySetIndex(-1);
    EXPECT_EQ(options.currentHotkeySetName, QStringLiteral("A"));

    options.currentHotkeySetName = QStringLiteral("missing");
    EXPECT_EQ(options.currentHotkeySetIndex(), -1);
}
