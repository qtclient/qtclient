// CIP network::TGameserverNetworkPacketRawDataProcessor with the XTEA helper of
// authentication::TAuthenticationAndEncryptionInfo.

#include "authentication/authenticationandencryptioninfo.h"
#include "network/gameservernetworkpacket.h"
#include "network/gameservernetworkpacketrawdataprocessor.h"

#include "compressionhelper.h"

#include <gtest/gtest.h>

#include <memory>

using tibia::authentication::TAuthenticationAndEncryptionInfo;
using tibia::network::TGameserverNetworkPacket;
using tibia::network::TGameserverNetworkPacketRawDataProcessor;

namespace {

TGameserverNetworkPacketRawDataProcessor processorFor(const TAuthenticationAndEncryptionInfo& info)
{
    return TGameserverNetworkPacketRawDataProcessor(info.xteaHelper(), std::make_shared<tibia::shared::TCompressionHelper>());
}

}

TEST(GameserverNetworkPacketRawDataProcessor, PlainPacketLosesPaddingCountAndPadding)
{
    TAuthenticationAndEncryptionInfo info;
    auto processor = processorFor(info);
    // Crystal's login challenge: one padding byte after the message.
    TGameserverNetworkPacket challenge(TGameserverNetworkPacket::Gameserver,
                                       QByteArray::fromHex("011f0102030405" "71"));
    processor.decodePacket(challenge);
    EXPECT_EQ(challenge.data(), QByteArray::fromHex("1f0102030405"));
}

TEST(GameserverNetworkPacketRawDataProcessor, EncodeWritesPaddingCountAndWholeBlocks)
{
    TAuthenticationAndEncryptionInfo info;
    auto processor = processorFor(info);
    TGameserverNetworkPacket login(TGameserverNetworkPacket::Gameclient, QByteArray(10, 'l'));
    login.setEncryptionType(TGameserverNetworkPacket::LoginRsa);
    processor.encodePacket(login);
    ASSERT_EQ(login.data().size(), 16);
    EXPECT_EQ(login.data().at(0), char(5)); // 1 + 10 bytes, padded with 5
    EXPECT_EQ(login.data().mid(1, 10), QByteArray(10, 'l'));
}

TEST(GameserverNetworkPacketRawDataProcessor, XteaPacketRoundTrips)
{
    TAuthenticationAndEncryptionInfo info;
    info.generateXteaKey();
    auto processor = processorFor(info);
    const QByteArray message = QByteArray::fromHex("0a0b0c0d0e0f101112");
    TGameserverNetworkPacket packet(TGameserverNetworkPacket::Gameclient, message);
    packet.setEncryptionType(TGameserverNetworkPacket::Xtea);
    processor.encodePacket(packet);
    ASSERT_EQ(packet.data().size() % 8, 0);
    EXPECT_NE(packet.data().mid(1, message.size()), message);
    processor.decodePacket(packet);
    EXPECT_EQ(packet.data(), message);
}
