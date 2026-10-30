#include "housesstorage.h"

#include <tibia/world/game.h>
#include <client/staticdata.h>
#include <tibia/assets/things.h>

#include <QDateTime>
#include <QSet>

#include <algorithm>

namespace tibia::cyclopedia {

namespace {

using Args = std::vector<std::any>;

template <class T>
const T* arg(const Args& args, const size_t index)
{
    return index < args.size() ? std::any_cast<T>(&args[index]) : nullptr;
}

QString fromStd(const std::string& text) { return QString::fromStdString(text); }

}

void THouseRecord::clearDynamicData()
{
    const THouseRecord defaults;
    state = defaults.state;
    bidderName.clear();
    isBidder = false;
    bidDisableReason = 0;
    bidEnd = 0;
    timeUntilAuctionEnd = 0;
    highestBid = 0;
    ownBidLimit = 0;
    renterName.clear();
    paidUntil = 0;
    isOwner = false;
    moveOutDisableReason = 0;
    transferDisableReason = 0;
    transferDate = 0;
    newOwnerName.clear();
    newOwnerAccepted = false;
    transferPrice = 0;
    isNewOwner = false;
    acceptTransferDisableReason = 0;
    rejectTransferDisableReason = 0;
    cancelTransferDisableReason = 0;
    moveOutDate = 0;
    cancelMoveOutDisableReason = 0;
}

THousesStorage::THousesStorage(QObject* parent)
    : QObject(parent)
{
    loadStaticHouses();

    m_hooks.emplace_back("g_game", "onGameStart", [this](LuaObject*, const Args&) {
        loadStaticHouses();
        m_characterInfo = {};
        m_limits = {};
        Q_EMIT houseInformationChanged();
        Q_EMIT characterHousesInfoChanged();
        Q_EMIT housesLimitsChanged();
        return std::any{};
    });

    // CIP 0x100a549e0 (StaticHouseData): the character's houses, the limits and
    // two house lists the house preview uses.
    m_hooks.emplace_back("g_game", "onCyclopediaHousesInfo", [this](LuaObject*, const Args& args) {
        const auto* excludedUntil = arg<uint32_t>(args, 0);
        const auto* counts = arg<std::vector<uint8_t>>(args, 1);
        if (!excludedUntil || !counts || counts->size() < 6)
            return std::any{};
        m_characterInfo = { *excludedUntil, (*counts)[0], (*counts)[1], (*counts)[2] };
        Q_EMIT characterHousesInfoChanged();
        m_limits = { (*counts)[3], (*counts)[4], (*counts)[5] };
        Q_EMIT housesLimitsChanged();
        return std::any{};
    });

    // CIP 0x100a54b40 (CurrentHouseData) into vslots 0x90 to 0xd0.
    m_hooks.emplace_back("g_game", "onCyclopediaHouseList", [this](LuaObject*, const Args& args) {
        const auto* list = arg<std::vector<CyclopediaHouseEntry>>(args, 0);
        if (!list || list->empty())
            return std::any{};
        const qint64 now = QDateTime::currentMSecsSinceEpoch() / 1000;
        for (const CyclopediaHouseEntry& entry : *list) {
            THouseRecord* house = findHouse(entry.houseId);
            if (!house)
                continue;
            house->isRentable = entry.isRentable;
            switch (entry.state) {
                case 0:
                    house->clearDynamicData();
                    house->state = 0;
                    house->bidderName = fromStd(entry.bidderName);
                    house->isBidder = entry.isBidder;
                    house->bidDisableReason = entry.bidDisableReason;
                    house->bidEnd = entry.bidEnd;
                    house->timeUntilAuctionEnd = static_cast<qint64>(entry.bidEnd) - now;
                    house->highestBid = entry.bidderName.empty() ? 0 : entry.highestBid;
                    house->ownBidLimit = entry.isBidder ? entry.bidLimit : 0;
                    break;
                case 1:
                    house->clearDynamicData();
                    house->state = 1;
                    break;
                case 2:
                case 3:
                case 4:
                    house->clearDynamicData();
                    house->state = 2;
                    house->renterName = fromStd(entry.ownerName);
                    house->paidUntil = entry.paidUntil;
                    house->isOwner = entry.isOwner;
                    house->moveOutDisableReason = entry.isOwner ? entry.moveOutDisableReason : 0;
                    house->transferDisableReason = entry.isOwner ? entry.transferDisableReason : 0;
                    if (entry.state == 3) {
                        house->state = 3;
                        house->transferDate = entry.transferDate;
                        house->newOwnerName = fromStd(entry.newOwnerName);
                        house->newOwnerAccepted = entry.newOwnerAccepted;
                        house->transferPrice = entry.transferPrice;
                        house->isNewOwner = entry.isNewOwner;
                        house->acceptTransferDisableReason = entry.acceptTransferDisableReason;
                        house->rejectTransferDisableReason = entry.rejectTransferDisableReason;
                        house->cancelTransferDisableReason = entry.cancelTransferDisableReason;
                    } else if (entry.state == 4) {
                        house->state = 4;
                        house->moveOutDate = entry.moveOutDate;
                        house->cancelMoveOutDisableReason = entry.cancelMoveOutDisableReason;
                    }
                    break;
                default:
                    break;
            }
        }
        Q_EMIT houseInformationChanged();
        return std::any{};
    });

    m_hooks.emplace_back("g_game", "onCyclopediaHouseAuctionMessage", [this](LuaObject*, const Args& args) {
        const auto* houseId = arg<uint32_t>(args, 0);
        const auto* action = arg<uint8_t>(args, 1);
        const auto* errorCode = arg<uint8_t>(args, 2);
        const auto* outbidden = arg<bool>(args, 3);
        if (houseId && action && errorCode)
            Q_EMIT houseActionResult(*houseId, *action, *errorCode, outbidden && *outbidden);
        return std::any{};
    });
}

void THousesStorage::loadStaticHouses()
{
    const std::vector<CyclopediaHouseType>& houses = g_things.getCyclopediaHouses();
    if (houses.empty() || houses.size() == m_houses.size())
        return;
    m_houses.clear();
    m_index.clear();
    m_houses.reserve(houses.size());
    QSet<QString> towns;
    for (const CyclopediaHouseType& type : houses) {
        THouseRecord house;
        house.id = type.id;
        house.name = fromStd(type.name);
        house.description = fromStd(type.description);
        house.size = static_cast<quint16>(type.size);
        house.beds = static_cast<quint16>(type.beds);
        house.rent = type.rent;
        house.town = fromStd(type.city);
        house.isGuildHall = type.isGuildHall;
        house.isShop = type.isShop;
        house.x = type.position.x;
        house.y = type.position.y;
        house.z = type.position.z;
        towns.insert(house.town);
        m_index.insert(house.id, m_houses.size());
        m_houses.push_back(std::move(house));
    }
    m_towns = QStringList(towns.begin(), towns.end());
    std::sort(m_towns.begin(), m_towns.end());
}

THouseRecord* THousesStorage::findHouse(const quint32 id)
{
    const auto it = m_index.constFind(id);
    return it == m_index.constEnd() ? nullptr : &m_houses[*it];
}

const THouseRecord* THousesStorage::house(const quint32 id) const
{
    const auto it = m_index.constFind(id);
    return it == m_index.constEnd() ? nullptr : &m_houses[*it];
}

void THousesStorage::reset()
{
    for (THouseRecord& house : m_houses)
        house.clearDynamicData();
    Q_EMIT houseInformationChanged();
}

void THousesStorage::requestTown(const QString& town)
{
    const QString name = town == qtTrId("houses_my_houses_town_name") ? QString() : town;
    g_game.requestSendCyclopediaHouseAuction(Otc::CYCLOPEDIA_HOUSE_TYPE_NONE, 0, 0, 0, name.toStdString());
}

void THousesStorage::bid(const quint32 houseId, const quint64 limit)
{
    g_game.requestSendCyclopediaHouseAuction(Otc::CYCLOPEDIA_HOUSE_TYPE_BID, houseId, 0, limit);
}

void THousesStorage::moveOut(const quint32 houseId, const quint32 timestamp)
{
    g_game.requestSendCyclopediaHouseAuction(Otc::CYCLOPEDIA_HOUSE_TYPE_MOVEOUT, houseId, timestamp);
}

void THousesStorage::transfer(const quint32 houseId, const quint32 timestamp, const QString& newOwner, const quint64 price)
{
    g_game.requestSendCyclopediaHouseAuction(Otc::CYCLOPEDIA_HOUSE_TYPE_TRANSFER, houseId, timestamp, price, newOwner.toStdString());
}

void THousesStorage::cancelMoveOut(const quint32 houseId)
{
    g_game.requestSendCyclopediaHouseAuction(Otc::CYCLOPEDIA_HOUSE_TYPE_CANCEL_MOVEOUT, houseId);
}

void THousesStorage::cancelTransfer(const quint32 houseId)
{
    g_game.requestSendCyclopediaHouseAuction(Otc::CYCLOPEDIA_HOUSE_TYPE_CANCEL_TRANSFER, houseId);
}

void THousesStorage::acceptTransfer(const quint32 houseId)
{
    g_game.requestSendCyclopediaHouseAuction(Otc::CYCLOPEDIA_HOUSE_TYPE_ACCEPT_TRANSFER, houseId);
}

void THousesStorage::rejectTransfer(const quint32 houseId)
{
    g_game.requestSendCyclopediaHouseAuction(Otc::CYCLOPEDIA_HOUSE_TYPE_REFECT_TRANSFER, houseId);
}

}
