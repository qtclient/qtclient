#include <gtest/gtest.h>

#include "network/dualconnectionurlinfo.h"

using tibia::network::EConnectionsUsed;
using tibia::network::TDualConnectionUrlInfo;

namespace {

const QUrl Primary(QStringLiteral("tcp://a.example:7172"));
const QUrl Secondary(QStringLiteral("tcp://b.example:7172"));

}

TEST(DualConnectionUrlInfo, NewUrlsCanBeConnectedRightAwayInUrlOrder)
{
    TDualConnectionUrlInfo info(Secondary, Primary);
    // Both start at time 0; the map order (by URL) decides.
    EXPECT_EQ(info.nextUrlToConnect(0), Primary);
    EXPECT_TRUE(info.isPrimaryUrl(Secondary));
    EXPECT_FALSE(info.isPrimaryUrl(Primary));
    EXPECT_FALSE(info.isPrimaryUrl(QUrl(QStringLiteral("tcp://c.example"))));
    EXPECT_EQ(info.connectionsUsed(), EConnectionsUsed::None);
}

TEST(DualConnectionUrlInfo, ResetGivesTheSecondaryOneMillisecondLater)
{
    TDualConnectionUrlInfo info(Secondary, Primary);
    info.resetNextConnectTimes(100);
    // The primary (b) is due at 100, the secondary (a) at 101.
    EXPECT_EQ(info.nextUrlToConnect(100), Secondary);
    EXPECT_EQ(info.nextUrlToConnect(101), Primary);
    EXPECT_EQ(info.nextUrlToConnect(99), QUrl());
}

TEST(DualConnectionUrlInfo, ConnectedUrlsAreNotOfferedAndReportedAsUsed)
{
    TDualConnectionUrlInfo info(Primary, Secondary);
    info.setConnected(Primary);
    EXPECT_EQ(info.connectionsUsed(), EConnectionsUsed::Primary);
    EXPECT_EQ(info.nextUrlToConnect(0), Secondary);
    info.setConnected(Secondary);
    EXPECT_EQ(info.connectionsUsed(), EConnectionsUsed::PrimaryAndSecondary);
    EXPECT_EQ(info.nextUrlToConnect(0), QUrl());
    info.setDisconnected(Primary, 0, false);
    EXPECT_EQ(info.connectionsUsed(), EConnectionsUsed::Secondary);
}

TEST(DualConnectionUrlInfo, BackoffDoublesUpToSixteenSeconds)
{
    TDualConnectionUrlInfo info(Primary, Secondary);
    info.setConnected(Secondary);
    const int64_t expected[] = { 1000, 2000, 4000, 8000, 16000, 16000 };
    for (const int64_t delay : expected) {
        info.setDisconnected(Primary, 0, true);
        EXPECT_EQ(info.nextUrlToConnect(delay - 1), QUrl());
        EXPECT_EQ(info.nextUrlToConnect(delay), Primary);
    }
    EXPECT_EQ(info.failedConnectAttempts(), 6u);
}

TEST(DualConnectionUrlInfo, NoBackoffAllowsAnImmediateRetryButCountsTheAttempt)
{
    TDualConnectionUrlInfo info(Primary, Secondary);
    info.setDisconnected(Primary, 500, false);
    info.setDisconnected(Secondary, 500, false);
    EXPECT_EQ(info.nextUrlToConnect(500), Primary);
    EXPECT_EQ(info.failedConnectAttempts(), 2u);
}

TEST(DualConnectionUrlInfo, ResettingFailedAttemptsSetsTheNextTime)
{
    TDualConnectionUrlInfo info(Primary, Secondary);
    info.setConnected(Secondary);
    info.setDisconnected(Primary, 0, true);
    info.setDisconnected(Primary, 0, true);
    info.resetFailedConnectAttempts(Primary, 50);
    EXPECT_EQ(info.failedConnectAttempts(), 0u);
    EXPECT_EQ(info.nextUrlToConnect(49), QUrl());
    EXPECT_EQ(info.nextUrlToConnect(50), Primary);
    // The next failure starts the backoff at one second again.
    info.setDisconnected(Primary, 0, true);
    EXPECT_EQ(info.nextUrlToConnect(1000), Primary);
}
