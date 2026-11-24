// CipSoft's single-value creature messages read from the tutorial session
// dump: each carries the known creature's data (its id twice, as the creature
// and the one it is stored under) with the changed field, and nothing else.

#include <gtest/gtest.h>

#include <protocol.pb.h>

#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <vector>

namespace {

using namespace ::tibia::protobuf::protocol;

std::vector<uint8_t> readDump()
{
    std::ifstream file(QTCLIENT_TESTDATA_DIR "/sessiondumps/tutorial-sessiondump.dmp", std::ios::binary);
    return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
}

/// The dump's server messages of the creature message types, in order.
std::vector<GameserverMessage> creatureMessages(const std::vector<uint8_t>& dump)
{
    std::vector<GameserverMessage> messages;
    std::size_t i = 0;
    while (i + 5 < dump.size()) {
        // A record: 'm', u16 length, the message (its type first).
        if (dump[i] != 'm' || dump[i + 3] != 0x08) {
            ++i;
            continue;
        }
        const std::size_t length = dump[i + 1] | (std::size_t(dump[i + 2]) << 8);
        GameserverMessage message;
        if (i + 3 + length > dump.size() || !message.ParseFromArray(dump.data() + i + 3, static_cast<int>(length))) {
            ++i;
            continue;
        }
        const uint8_t type = dump[i + 4];
        if (type >= 140 && type <= 146)
            messages.push_back(message);
        i += 3 + length;
    }
    return messages;
}

/// The message's creature data, or nullptr for a type the test does not read.
const CreatureData* creatureOf(const GameserverMessage& message)
{
    if (message.HasExtension(GameserverMessageCreatureHealth::gameserver_message_creature_health))
        return &message.GetExtension(GameserverMessageCreatureHealth::gameserver_message_creature_health).creature();
    if (message.HasExtension(GameserverMessageCreatureLight::gameserver_message_creature_light))
        return &message.GetExtension(GameserverMessageCreatureLight::gameserver_message_creature_light).creature();
    if (message.HasExtension(GameserverMessageCreatureOutfit::gameserver_message_creature_outfit))
        return &message.GetExtension(GameserverMessageCreatureOutfit::gameserver_message_creature_outfit).creature();
    if (message.HasExtension(GameserverMessageCreatureSpeed::gameserver_message_creature_speed))
        return &message.GetExtension(GameserverMessageCreatureSpeed::gameserver_message_creature_speed).creature();
    if (message.HasExtension(GameserverMessageCreatureUnpass::gameserver_message_creature_unpass))
        return &message.GetExtension(GameserverMessageCreatureUnpass::gameserver_message_creature_unpass).creature();
    return nullptr;
}

TEST(CreatureMessages, CarryTheKnownCreatureWithTheChangedField)
{
    const std::vector<uint8_t> dump = readDump();
    if (dump.empty())
        GTEST_SKIP() << "no tutorial dump";
    const std::vector<GameserverMessage> messages = creatureMessages(dump);
    std::map<uint32_t, int> counts;
    for (const GameserverMessage& message : messages) {
        SCOPED_TRACE(message.type());
        const CreatureData* creature = creatureOf(message);
        ASSERT_NE(creature, nullptr);
        ++counts[message.type()];
        EXPECT_TRUE(creature->has_id());
        EXPECT_EQ(creature->remove_id(), creature->id());
        EXPECT_FALSE(creature->has_name());
        switch (message.type()) {
        case 140:
            EXPECT_TRUE(creature->has_health_percent());
            break;
        case 141:
            EXPECT_TRUE(creature->has_light_intensity());
            EXPECT_TRUE(creature->has_light_color());
            break;
        case 142:
            EXPECT_TRUE(creature->has_outfit());
            EXPECT_TRUE(creature->has_mount());
            break;
        case 143:
            EXPECT_TRUE(creature->has_speed());
            EXPECT_TRUE(message.GetExtension(GameserverMessageCreatureSpeed::gameserver_message_creature_speed)
                            .has_base_speed());
            break;
        case 146:
            EXPECT_TRUE(creature->has_unpassable());
            break;
        default:
            break;
        }
    }
    // The tutorial's creature messages: health 16, light 4, outfit 7, speed 8, unpass 1.
    EXPECT_EQ(counts[140], 16);
    EXPECT_EQ(counts[141], 4);
    EXPECT_EQ(counts[142], 7);
    EXPECT_EQ(counts[143], 8);
    EXPECT_EQ(counts[146], 1);
}

TEST(CreatureMessages, FirstHealthAndSpeedOfTheTutorialCreature)
{
    const std::vector<uint8_t> dump = readDump();
    if (dump.empty())
        GTEST_SKIP() << "no tutorial dump";
    const CreatureData* health = nullptr;
    const GameserverMessageCreatureSpeed* speed = nullptr;
    const std::vector<GameserverMessage> messages = creatureMessages(dump);
    for (const GameserverMessage& message : messages) {
        if (!health && message.type() == 140)
            health = creatureOf(message);
        if (!speed && message.type() == 143)
            speed = &message.GetExtension(GameserverMessageCreatureSpeed::gameserver_message_creature_speed);
    }
    ASSERT_NE(health, nullptr);
    EXPECT_EQ(health->id(), 1073769963u);
    EXPECT_EQ(health->health_percent(), 90u);
    ASSERT_NE(speed, nullptr);
    EXPECT_EQ(speed->creature().id(), 1073769963u);
    // The first speed is the message's own, the second the creature's.
    EXPECT_EQ(speed->base_speed(), 39u);
    EXPECT_EQ(speed->creature().speed(), 40u);
}

}
