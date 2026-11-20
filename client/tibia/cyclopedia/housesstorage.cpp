#include "housesstorage.h"
#include "cyclopedia/cyclopediaciptypes.h"

#include <tibia/game/gamesession.h>
#include <client/staticdata.h>
#include <tibia/assets/things.h>

#include <QDateTime>

#include <stdexcept>

#include <algorithm>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/game/thingsession_forward.h>

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

THouseInformation::THouseInformation(const TStaticHouse& house)
    : id(house.id)
    , name(house.name)
    , description(house.description)
    , size(house.size)
    , beds(house.beds)
    , rent(house.rent)
    , town(house.town)
    , isGuildHall(house.isGuildHall)
    , isShop(house.isShop)
    , x(house.position.x)
    , y(house.position.y)
    , z(house.position.z)
{
}

void THouseInformation::clearDynamicData()
{
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

void THouseInformation::setUnknown()
{
    clearDynamicData();
    state = 6;
}

void THouseInformation::setNoAuction()
{
    clearDynamicData();
    state = 1;
}

void THouseInformation::setAuction(const QString& bidderName_, const bool isBidder_, const quint8 bidDisableReason_, const quint32 bidEnd_,
                                   const qint64 timeUntilAuctionEnd_, const quint64 highestBid_, const quint64 ownBidLimit_)
{
    clearDynamicData();
    state = 0;
    bidderName = bidderName_;
    isBidder = isBidder_;
    bidDisableReason = bidDisableReason_;
    bidEnd = bidEnd_;
    timeUntilAuctionEnd = timeUntilAuctionEnd_;
    highestBid = highestBid_;
    ownBidLimit = ownBidLimit_;
}

void THouseInformation::setRented(const QString& renterName_, const quint32 paidUntil_, const bool isOwner_, const quint8 moveOutDisableReason_,
                                  const quint8 transferDisableReason_)
{
    clearDynamicData();
    state = 2;
    renterName = renterName_;
    paidUntil = paidUntil_;
    isOwner = isOwner_;
    moveOutDisableReason = moveOutDisableReason_;
    transferDisableReason = transferDisableReason_;
}

void THouseInformation::setMoveOutPending(const quint32 moveOutDate_, const quint8 cancelMoveOutDisableReason_)
{
    if (state != 2)
        throw std::runtime_error("MoveOutPending state must extend rented state");
    state = 4;
    moveOutDate = moveOutDate_;
    cancelMoveOutDisableReason = cancelMoveOutDisableReason_;
}

THousesStorage::THousesStorage(QObject* parent)
    : QObject(parent)
{
    loadStaticHouses();

    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameStarted, this, [this]() {
        loadStaticHouses();
        m_characterInfo = {};
        m_limits = {};
        Q_EMIT houseInformationChanged();
        Q_EMIT characterHousesInfoChanged();
        Q_EMIT housesLimitsChanged();
    });

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaHousesInfo", [this](GameObject*, const Args& args) {
        const auto* excludedUntil = arg<uint32_t>(args, 0);
        const auto* counts = arg<std::vector<uint8_t>>(args, 1);
        const auto* firstList = arg<std::vector<uint32_t>>(args, 2);
        const auto* secondList = arg<std::vector<uint32_t>>(args, 3);
        if (!excludedUntil || !counts || counts->size() < 6)
            return std::any{};
        applyStaticHouseData(*excludedUntil, *counts, firstList ? *firstList : std::vector<uint32_t>(),
                             secondList ? *secondList : std::vector<uint32_t>());
        // Then the handler's static house data observer.
        TCyclopediaProtocolMessageHandler::notifyStaticHouseDataObserver(*this);
        return std::any{};
    });

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaHouseList", [this](GameObject*, const Args& args) {
        if (const auto* list = arg<std::vector<CyclopediaHouseEntry>>(args, 0))
            applyCurrentHouseData(*list);
        return std::any{};
    });

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaHouseAuctionMessage", [this](GameObject*, const Args& args) {
        const auto* houseId = arg<uint32_t>(args, 0);
        const auto* action = arg<uint8_t>(args, 1);
        const auto* errorCode = arg<uint8_t>(args, 2);
        const auto* outbidden = arg<bool>(args, 3);
        if (houseId && action && errorCode)
            Q_EMIT houseActionResult(*houseId, *action, *errorCode, outbidden && *outbidden);
        return std::any{};
    });
}

void THousesStorage::applyStaticHouseData(const quint32 excludedUntil, const std::vector<uint8_t>& counts,
                                          const std::vector<uint32_t>& firstList, const std::vector<uint32_t>& secondList)
{
    setCharacterInfo(excludedUntil, counts[0], counts[1], counts[2]);
    setLimits(counts[3], counts[4], counts[5]);
    for (const uint32_t houseId : firstList)
        setListing(houseId, 0);
    for (const uint32_t houseId : secondList)
        setListing(houseId, 1);
    // The listing observer refreshes.
    Q_EMIT houseInformationChanged();
}

void THousesStorage::applyCurrentHouseData(const std::vector<CyclopediaHouseEntry>& houses)
{
    if (houses.empty())
        return;
    beginUpdate();
    for (const CyclopediaHouseEntry& entry : houses) {
        setRentable(entry.houseId, entry.isRentable);
        // Rented, transfer pending and move out pending share the rent data;
        // the disable reasons only count for the owner.
        QString renterName;
        quint32 paidUntil = 0;
        bool isOwner = false;
        quint8 moveOutDisableReason = 0;
        quint8 transferDisableReason = 0;
        if (entry.state >= 2 && entry.state <= 4) {
            renterName = fromStd(entry.ownerName);
            paidUntil = entry.paidUntil;
            isOwner = entry.isOwner;
            if (isOwner) {
                moveOutDisableReason = entry.moveOutDisableReason;
                transferDisableReason = entry.transferDisableReason;
            }
        }
        switch (entry.state) {
            case 0: {
                // No bidder: no highest bid; only the bidder knows his limit.
                const QString bidderName = fromStd(entry.bidderName);
                setAuction(entry.houseId, bidderName, entry.isBidder, entry.bidDisableReason, entry.bidEnd,
                           bidderName.isEmpty() ? 0 : entry.highestBid, entry.isBidder ? entry.bidLimit : 0);
                break;
            }
            case 1: setNoAuction(entry.houseId); break;
            case 2: setRented(entry.houseId, renterName, paidUntil, isOwner, moveOutDisableReason, transferDisableReason); break;
            case 3:
                setRented(entry.houseId, renterName, paidUntil, isOwner, moveOutDisableReason, transferDisableReason);
                setTransferPending(entry.houseId, entry.transferDate, fromStd(entry.newOwnerName), entry.newOwnerAccepted,
                                   entry.transferPrice, entry.isNewOwner, entry.acceptTransferDisableReason,
                                   entry.rejectTransferDisableReason, entry.cancelTransferDisableReason);
                break;
            case 4:
                setRented(entry.houseId, renterName, paidUntil, isOwner, moveOutDisableReason, transferDisableReason);
                setMoveOutPending(entry.houseId, entry.moveOutDate, entry.cancelMoveOutDisableReason);
                break;
            default: break;
        }
    }
    endUpdate();
}

void THousesStorage::setCharacterInfo(const quint32 excludedUntil, const quint8 housesOnWorld, const quint8 housesOnAccount,
                                      const quint8 guildhalls)
{
    m_characterInfo = { excludedUntil, housesOnWorld, housesOnAccount, guildhalls };
    Q_EMIT characterHousesInfoChanged();
}

void THousesStorage::setLimits(const quint8 perWorld, const quint8 perAccount, const quint8 guildhalls)
{
    m_limits = { perWorld, perAccount, guildhalls };
    Q_EMIT housesLimitsChanged();
}

void THousesStorage::setListing(const quint32 houseId, const int listing)
{
    findHouse(houseId).listing = listing;
}

void THousesStorage::beginUpdate() { m_updating = true; }

void THousesStorage::ensureUpdating() const
{
    if (!m_updating)
        throw std::runtime_error("House update mode not started while updating houses");
}

void THousesStorage::setRentable(const quint32 houseId, const bool rentable)
{
    ensureUpdating();
    findHouse(houseId).isRentable = rentable;
}

void THousesStorage::setNoAuction(const quint32 houseId)
{
    ensureUpdating();
    findHouse(houseId).setNoAuction();
}

void THousesStorage::setAuction(const quint32 houseId, const QString& bidderName, const bool isBidder, const quint8 bidDisableReason,
                                const quint32 bidEnd, const quint64 highestBid, const quint64 ownBidLimit)
{
    ensureUpdating();
    const qint64 now = QDateTime::currentMSecsSinceEpoch() / 1000;
    findHouse(houseId).setAuction(bidderName, isBidder, bidDisableReason, bidEnd, static_cast<qint64>(bidEnd) - now, highestBid, ownBidLimit);
}

void THousesStorage::setRented(const quint32 houseId, const QString& renterName, const quint32 paidUntil, const bool isOwner,
                               const quint8 moveOutDisableReason, const quint8 transferDisableReason)
{
    ensureUpdating();
    findHouse(houseId).setRented(renterName, paidUntil, isOwner, moveOutDisableReason, transferDisableReason);
}

void THousesStorage::setTransferPending(const quint32 houseId, const quint32 transferDate, const QString& newOwnerName,
                                       const bool newOwnerAccepted, const quint64 transferPrice, const bool isNewOwner,
                                       const quint8 acceptTransferDisableReason, const quint8 rejectTransferDisableReason,
                                       const quint8 cancelTransferDisableReason)
{
    ensureUpdating();
    THouseInformation& house = findHouse(houseId);
    if (house.state != 2)
        throw std::runtime_error("TransferPending state must extend rented state");
    house.state = 3;
    house.transferDate = transferDate;
    house.newOwnerName = newOwnerName;
    house.newOwnerAccepted = newOwnerAccepted;
    house.transferPrice = transferPrice;
    house.isNewOwner = isNewOwner;
    house.acceptTransferDisableReason = acceptTransferDisableReason;
    house.rejectTransferDisableReason = rejectTransferDisableReason;
    house.cancelTransferDisableReason = cancelTransferDisableReason;
}

void THousesStorage::setMoveOutPending(const quint32 houseId, const quint32 moveOutDate, const quint8 cancelMoveOutDisableReason)
{
    ensureUpdating();
    findHouse(houseId).setMoveOutPending(moveOutDate, cancelMoveOutDisableReason);
}

void THousesStorage::endUpdate()
{
    if (!m_updating)
        return;
    m_updating = false;
    Q_EMIT houseInformationChanged();
}

void THousesStorage::loadStaticHouses()
{
    const std::vector<TStaticHouse>& houses = staticHouses();
    m_houses.clear();
    m_index.clear();
    m_towns.clear();
    m_houses.reserve(houses.size());
    for (const TStaticHouse& house : houses) {
        m_index.insert(house.id, m_houses.size());
        m_houses.emplace_back(house);
        m_towns.append(house.town);
        m_towns.removeDuplicates();
    }
    m_towns.sort(Qt::CaseSensitive);
}

THouseInformation& THousesStorage::findHouse(const quint32 id)
{
    static THouseInformation unknownHouse(TStaticHouse::unknown(0));
    const auto it = m_index.constFind(id);
    return it == m_index.constEnd() ? unknownHouse : m_houses[*it];
}

const THouseInformation* THousesStorage::house(const quint32 id) const
{
    const auto it = m_index.constFind(id);
    return it == m_index.constEnd() ? nullptr : &m_houses[*it];
}

void THousesStorage::reset()
{
    for (THouseInformation& house : m_houses)
        house.setUnknown();
    Q_EMIT houseInformationChanged();
}

void THousesStorage::requestTown(const QString& town)
{
    const QString name = town == qtTrId("houses_my_houses_town_name") ? QString() : town;
    tibia::game::requestSendCyclopediaHouseAuction(tibia::client::CYCLOPEDIA_HOUSE_TYPE_NONE, 0, 0, 0, name.toStdString());
}

void THousesStorage::bid(const quint32 houseId, const quint64 limit)
{
    tibia::game::requestSendCyclopediaHouseAuction(tibia::client::CYCLOPEDIA_HOUSE_TYPE_BID, houseId, 0, limit);
}

void THousesStorage::moveOut(const quint32 houseId, const quint32 timestamp)
{
    tibia::game::requestSendCyclopediaHouseAuction(tibia::client::CYCLOPEDIA_HOUSE_TYPE_MOVEOUT, houseId, timestamp);
}

void THousesStorage::transfer(const quint32 houseId, const quint32 timestamp, const QString& newOwner, const quint64 price)
{
    tibia::game::requestSendCyclopediaHouseAuction(tibia::client::CYCLOPEDIA_HOUSE_TYPE_TRANSFER, houseId, timestamp, price, newOwner.toStdString());
}

void THousesStorage::cancelMoveOut(const quint32 houseId)
{
    tibia::game::requestSendCyclopediaHouseAuction(tibia::client::CYCLOPEDIA_HOUSE_TYPE_CANCEL_MOVEOUT, houseId);
}

void THousesStorage::cancelTransfer(const quint32 houseId)
{
    tibia::game::requestSendCyclopediaHouseAuction(tibia::client::CYCLOPEDIA_HOUSE_TYPE_CANCEL_TRANSFER, houseId);
}

void THousesStorage::acceptTransfer(const quint32 houseId)
{
    tibia::game::requestSendCyclopediaHouseAuction(tibia::client::CYCLOPEDIA_HOUSE_TYPE_ACCEPT_TRANSFER, houseId);
}

void THousesStorage::rejectTransfer(const quint32 houseId)
{
    tibia::game::requestSendCyclopediaHouseAuction(tibia::client::CYCLOPEDIA_HOUSE_TYPE_REFECT_TRANSFER, houseId);
}

}
