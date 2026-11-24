#include <gtest/gtest.h>

#include "app/clientoptions.h"
#include "config/hotkeyoptions.h"

#include <QJsonArray>

using namespace tibia::config;

// THotkeyOptions reads and writes the options files through ClientOptions;
// the data functions under test do not, so empty stand-ins keep the test off
// the file system.
namespace tibia::app {
ClientOptions ClientOptions::load()
{
    return {};
}
void ClientOptions::storeAll(const QJsonObject&) {}
}

namespace {

QJsonObject action(const char* name)
{
    return { { QStringLiteral("action"), QString::fromLatin1(name) } };
}

QJsonObject binding(const char* keys, const QJsonObject& setting)
{
    return { { QStringLiteral("keysequence"), QString::fromLatin1(keys) }, { QStringLiteral("actionsetting"), setting } };
}

}

TEST(HotkeyOptions, ActionSettingKindIsTheFirstKeyInCipOrder)
{
    // chatText wins over action, action over useObject.
    TActionSetting text = TActionSetting::fromJson({ { QStringLiteral("action"), QStringLiteral("Look") }, { QStringLiteral("chatText"), QStringLiteral("hi") } });
    EXPECT_EQ(text.kind, TActionSetting::TEXT);
    TActionSetting named = TActionSetting::fromJson({ { QStringLiteral("useObject"), 3031 }, { QStringLiteral("action"), QStringLiteral("Look") } });
    EXPECT_EQ(named.kind, TActionSetting::ACTION);
    EXPECT_EQ(named.action, QStringLiteral("Look"));
    EXPECT_TRUE(TActionSetting::fromJson({ { QStringLiteral("foo"), 1 } }).isEmpty());
}

TEST(HotkeyOptions, UseObjectLiquidWinsAndSmartModeOnlyForEquip)
{
    const TActionSetting object = TActionSetting::fromJson({
        { QStringLiteral("useObject"), 2874 },
        { QStringLiteral("liquidType"), 7 },
        { QStringLiteral("upgradeTier"), 2 },
        { QStringLiteral("useType"), QStringLiteral("UseOnYourself") },
        { QStringLiteral("useEquipSmartMode"), true },
    });
    EXPECT_EQ(object.kind, TActionSetting::OBJECT);
    EXPECT_EQ(object.objectId, 2874);
    EXPECT_EQ(object.liquidType, 7);
    EXPECT_FALSE(object.upgradeTier.has_value());
    EXPECT_EQ(object.useType, EUseType::UseOnYourself);
    EXPECT_FALSE(object.useEquipSmartMode);

    const TActionSetting equip = TActionSetting::fromJson({
        { QStringLiteral("useObject"), -5 },
        { QStringLiteral("useType"), QStringLiteral("Equip") },
        { QStringLiteral("useEquipSmartMode"), true },
    });
    EXPECT_EQ(equip.objectId, 0);
    EXPECT_TRUE(equip.useEquipSmartMode);
}

TEST(HotkeyOptions, MultiActionsReadAtMostThree)
{
    const QJsonArray actions{ action("A"), action("B"), action("C"), action("D") };
    const TActionSetting multi = TActionSetting::fromJson({ { QStringLiteral("multiActions"), actions } });
    ASSERT_EQ(multi.multiActions.size(), 3u);
    EXPECT_EQ(multi.multiActions[2].action, QStringLiteral("C"));
}

TEST(HotkeyOptions, ChatTextOf256CharactersFailsTheLoad)
{
    EXPECT_NO_THROW(TActionSetting::fromJson({ { QStringLiteral("chatText"), QString(255, QLatin1Char('a')) } }));
    EXPECT_THROW(TActionSetting::fromJson({ { QStringLiteral("chatText"), QString(256, QLatin1Char('a')) } }), std::runtime_error);
}

TEST(HotkeyOptions, BindingsAreKeyedByKeySequence)
{
    const QJsonObject section{
        { QStringLiteral("hotkeySets"),
          QJsonObject{ { QStringLiteral("Mine"),
                         QJsonObject{ { QStringLiteral("chatOff"),
                                        QJsonArray{ binding("F2", action("A")), binding("F1", action("B")), binding("F2", action("C")),
                                                    QJsonObject{ { QStringLiteral("keysequence"), 5 } } } } } } } },
    };
    const THotkeyOptionsData data = THotkeyOptionsData::fromJson(section);
    EXPECT_EQ(data.currentHotkeySetName, QStringLiteral("Mine"));
    const std::vector<THotkeyBinding>& bindings = data.hotkeySets.at(QStringLiteral("Mine")).chatOff;
    ASSERT_EQ(bindings.size(), 2u);
    EXPECT_EQ(bindings[0].keys, QKeySequence(QStringLiteral("F1")));
    EXPECT_EQ(bindings[1].setting.action, QStringLiteral("C"));
}

TEST(HotkeyOptions, EmptySettingsAreNotWritten)
{
    THotkeyOptionsData data;
    THotkeySet set;
    set.mappings[{ 1, 2 }] = TActionSetting{};
    set.chatOn.push_back({ QKeySequence(QStringLiteral("F3")), TActionSetting{} });
    data.hotkeySets.emplace(QStringLiteral("S"), set);
    const QJsonObject written = data.toJson().value(QLatin1String("hotkeySets")).toObject().value(QLatin1String("S")).toObject();
    EXPECT_TRUE(written.value(QLatin1String("chatOn")).toArray().isEmpty());
    EXPECT_TRUE(written.value(QLatin1String("actionBarOptions")).toObject().value(QLatin1String("mappings")).toArray().isEmpty());
}

TEST(HotkeyOptions, NoSetsGiveTheVocationPresetsWithKnightCurrent)
{
    THotkeyOptionsData data;
    data.addGlobalShortcuts(true, true, true);
    EXPECT_EQ(data.hotkeySets.size(), 5u);
    EXPECT_EQ(data.currentHotkeySetName, QStringLiteral("Knight"));
    EXPECT_TRUE(data.autoSwitchHotkeyPreset);
}

TEST(HotkeyOptions, SetHotkeysMakesTheSetAndReplacesBothChatModes)
{
    THotkeyOptionsData data;
    THotkeySet existing;
    existing.chatOff = { THotkeyBinding{ QKeySequence(QStringLiteral("F1")), TActionSetting::fromJson(action("Look")) } };
    existing.mappings[{ 1, 2 }] = TActionSetting::fromJson(action("Use"));
    data.addSet(QStringLiteral("A"), existing);

    const std::vector<THotkeyBinding> off{ THotkeyBinding{ QKeySequence(QStringLiteral("F2")), TActionSetting::fromJson(action("Attack")) } };
    const std::vector<THotkeyBinding> on{ THotkeyBinding{ QKeySequence(QStringLiteral("F3")), TActionSetting::fromJson(action("Follow")) } };
    data.setHotkeys(QStringLiteral("A"), off, on);
    data.setHotkeys(QStringLiteral("B"), off, {});

    ASSERT_NE(data.set(QStringLiteral("A")), nullptr);
    EXPECT_EQ(data.set(QStringLiteral("A"))->chatOff, off);
    EXPECT_EQ(data.set(QStringLiteral("A"))->chatOn, on);
    // The action bar buttons stay.
    EXPECT_EQ(data.set(QStringLiteral("A"))->mappings.size(), 1u);
    ASSERT_NE(data.set(QStringLiteral("B")), nullptr);
    EXPECT_EQ(data.set(QStringLiteral("B"))->chatOff, off);
    EXPECT_TRUE(data.set(QStringLiteral("B"))->chatOn.empty());
}
