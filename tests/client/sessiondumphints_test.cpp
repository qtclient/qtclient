#include <gtest/gtest.h>

#include "chat/chatchannel.h"
#include "tutorial/sessiondumphintpublishgameaction.h"
#include "tutorial/sessiondumphints.h"

#include <QFile>
#include <QJsonDocument>

#include <stdexcept>

using namespace tibia::tutorial;
using tibia::input::EGameAction;

// The test builds the hints without input/gameaction.cpp; this is its constructor.
tibia::input::TGameActionChatChannel::TGameActionChatChannel(const EGameAction type,
                                                             std::shared_ptr<const tibia::chat::TChatChannelIdentifier> identifier)
    : TGameActionSimple(type)
    , m_identifier(std::move(identifier))
{
}

namespace {

QJsonDocument document(const char* json) { return QJsonDocument::fromJson(json); }

constexpr const char* Outfits = R"("outfit": {"male": {"id": 129, "head": 115, "torso": 113, "legs": 39, "detail": 115, "addons": 3},
                                            "female": {"id": 137, "head": 1, "torso": 2, "legs": 3, "detail": 4, "addons": 0}})";

}

TEST(SessiondumpHints, ParsesOutfitsAndSortsHintsByTimestamp)
{
    const QByteArray json = QByteArray("{") + Outfits + R"(, "actions": {
        "750": [{"type": "SWITCH_CHANNEL", "channelid": 131070}, {"type": "CLEAR_OLDEST_MESSAGE"}],
        "0": [{"type": "TUTORIAL_PROGRESS", "tutorialprogress": "TutorialStart"}, {"type": "SKIP_TO", "skiptotimestamp": 12}],
        "199": [{"type": "CONDITION", "conditiontype": "SHOW_TUTORIAL_HINT", "conditiondata": {"showtutorialhintid": 14}}],
        "5": [{"type": "UNKNOWN"}]
    }})";
    TSessiondumpHints hints(QJsonDocument::fromJson(json));
    EXPECT_EQ(hints.outfit(true).lookType, 129);
    EXPECT_EQ(hints.outfit(true).torso, 113);
    EXPECT_TRUE(hints.outfit(true).addon1);
    EXPECT_TRUE(hints.outfit(true).addon2);
    EXPECT_EQ(hints.outfit(false).lookType, 137);
    EXPECT_EQ(hints.outfit(false).detail, 4);

    const auto all = hints.hints();
    ASSERT_EQ(all.size(), 4u);
    EXPECT_EQ(all[0]->timestamp(), 0);
    EXPECT_EQ(all[0]->type(), TSessiondumpHintBase::TutorialProgress);
    EXPECT_EQ(all[1]->type(), TSessiondumpHintBase::SkipTo);
    EXPECT_EQ(all[2]->timestamp(), 750);
    EXPECT_EQ(all[2]->type(), TSessiondumpHintBase::SwitchChannel);
    EXPECT_EQ(all[3]->type(), TSessiondumpHintBase::ClearOldestMessage);

    using tibia::input::gameActionAs;
    const auto progress = all[0]->execute();
    ASSERT_EQ(progress.size(), 1u);
    EXPECT_EQ(progress[0]->gameActionType(), EGameAction::TUTORIAL_PROGRESS);
    const auto* tutorial = gameActionAs<tibia::input::TGameActionTutorialProgress>(progress[0]);
    ASSERT_TRUE(tutorial);
    EXPECT_EQ(tutorial->tutorialProgress(), QStringLiteral("TutorialStart"));
    EXPECT_TRUE(all[0]->isDone());
    const auto skipActions = all[1]->execute();
    const auto* skip = gameActionAs<tibia::input::TGameActionSessiondumpSkipTo>(skipActions[0]);
    ASSERT_TRUE(skip);
    EXPECT_EQ(skip->timestamp(), 12);
    const auto channelActions = all[2]->execute();
    const auto* channel = gameActionAs<tibia::input::TGameActionChatChannel>(channelActions[0]);
    ASSERT_TRUE(channel);
    EXPECT_EQ(channel->gameActionType(), EGameAction::OPEN_CHAT_CHANNEL);
    EXPECT_EQ(channel->chatChannelIdentifier().type, tibia::chat::TChatChannelIdentifier::ServerLog);

    EXPECT_EQ(hints.firstUnfinishedHint(), all[3]);
}

TEST(SessiondumpHints, SkipsHintsBeforeATimestamp)
{
    const QByteArray json = QByteArray("{") + Outfits + R"(, "actions": {
        "10": [{"type": "CLEAR_OLDEST_MESSAGE"}], "20": [{"type": "CLEAR_OLDEST_MESSAGE"}], "30": [{"type": "CLEAR_OLDEST_MESSAGE"}]}})";
    TSessiondumpHints hints(QJsonDocument::fromJson(json));
    hints.skipHintsBefore(20);
    ASSERT_TRUE(hints.firstUnfinishedHint());
    EXPECT_EQ(hints.firstUnfinishedHint()->timestamp(), 20);
    auto extra = std::make_shared<TSessiondumpHintClearOldestMessage>();
    extra->setTimestamp(15);
    hints.addHint(extra);
    EXPECT_EQ(hints.hints()[1], extra);
    EXPECT_EQ(hints.firstUnfinishedHint(), extra);
}

TEST(SessiondumpHints, RejectsMalformedDocuments)
{
    EXPECT_THROW(TSessiondumpHints(document(R"({"actions": {}})")), std::invalid_argument);
    const QByteArray badTimestamp = QByteArray("{") + Outfits + R"(, "actions": {"x": []}})";
    EXPECT_THROW(TSessiondumpHints(QJsonDocument::fromJson(badTimestamp)), std::invalid_argument);
    const QByteArray badHint = QByteArray("{") + Outfits + R"(, "actions": {"1": [{"type": "SKIP_TO"}]}})";
    EXPECT_THROW(TSessiondumpHints(QJsonDocument::fromJson(badHint)), std::invalid_argument);
}

TEST(SessiondumpHints, ReadsTheTutorialHints)
{
    QFile file(QStringLiteral(QTCLIENT_TESTDATA_DIR "/sessiondumps/tutorial-sessiondump.hints"));
    if (!file.open(QIODevice::ReadOnly))
        GTEST_SKIP() << "no tutorial hints";
    TSessiondumpHints hints(QJsonDocument::fromJson(file.readAll()));
    EXPECT_EQ(hints.outfit(true).lookType, 129);
    // Everything but the conditions.
    EXPECT_EQ(hints.hints().size(), 24u);
}
