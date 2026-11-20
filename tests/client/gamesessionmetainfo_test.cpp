#include "game/gameservergamesessionmetainfo.h"
#include "game/sessiondumpgamesessionmetainfo.h"
#include "game/tutorialsessiondumpgamesessionmetainfo.h"

#include <gtest/gtest.h>

#include <memory>

using tibia::game::TGameserverGameSessionMetaInfo;
using tibia::game::TGameSessionMetaInfo;
using tibia::game::TSessiondumpGameSessionMetaInfo;
using tibia::game::TTutorialSessiondumpGameSessionMetaInfo;

TEST(GameSessionMetaInfo, EachKindReportsItsSessionType)
{
    const std::shared_ptr<const TGameSessionMetaInfo> gameserver = std::make_shared<TGameserverGameSessionMetaInfo>();
    const std::shared_ptr<const TGameSessionMetaInfo> sessiondump = std::make_shared<TSessiondumpGameSessionMetaInfo>();
    const std::shared_ptr<const TGameSessionMetaInfo> tutorial = std::make_shared<TTutorialSessiondumpGameSessionMetaInfo>();
    EXPECT_EQ(static_cast<int>(gameserver->getType()), 0);
    EXPECT_EQ(static_cast<int>(sessiondump->getType()), 1);
    EXPECT_EQ(static_cast<int>(tutorial->getType()), 2);
    // The tutorial is a sessiondump: a sessiondump session accepts its meta info.
    EXPECT_NE(std::dynamic_pointer_cast<const TSessiondumpGameSessionMetaInfo>(tutorial), nullptr);
    EXPECT_EQ(std::dynamic_pointer_cast<const TSessiondumpGameSessionMetaInfo>(gameserver), nullptr);
}

TEST(GameSessionMetaInfo, SessiondumpStartsEmptyWithDefaultMovement)
{
    const TSessiondumpGameSessionMetaInfo metaInfo;
    EXPECT_TRUE(metaInfo.sessiondump.url.isEmpty());
    EXPECT_TRUE(metaInfo.sessiondump.hintsUrl.isEmpty());
    EXPECT_EQ(metaInfo.sessiondump.fileSize, 0);
    EXPECT_EQ(metaInfo.sessiondump.durationMilliseconds, 0);
    EXPECT_FALSE(metaInfo.sessiondump.hasHints);
    EXPECT_FALSE(metaInfo.startedByGameAction);
    EXPECT_EQ(metaInfo.protocolMessageQueueConfiguration, nullptr);
    ASSERT_NE(metaInfo.playerMovementIntentHandlerConfiguration, nullptr);
    EXPECT_EQ(metaInfo.playerMovementIntentHandlerConfiguration->maxMillisecondsToFinishCurrentMovementBeforeSendNext, 0);
    EXPECT_DOUBLE_EQ(metaInfo.playerMovementIntentHandlerConfiguration->minPercentageOfCurrentMovementBeforeSendNext, 1.0);
}
