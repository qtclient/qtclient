#pragma once

#include "client/gamecore.h"

#include "cyclopedia/staticcontentinformation.h"

#include <client/staticdata.h>

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

#include <vector>

namespace tibia {
namespace cyclopedia {

/// One house of the Cyclopedia (CIP: cyclopedia::THouseInformation, the
/// records of THousesStorage; static part from the static data, dynamic part
/// from CurrentHouseData).
struct THouseInformation
{
    THouseInformation() = default;
    /// An "updating", rentable house of neither house list.
    explicit THouseInformation(const TStaticHouse& house);

    /// EHouseState; 6 (unknown) shows "updating" until the server answers.
    int state = 6;
    bool isRentable = true;

    quint32 id = 0;
    QString name;
    QString description;
    quint16 size = 0;
    quint16 beds = 0;
    quint32 rent = 0;
    QString town;
    bool isGuildHall = false;
    bool isShop = false;
    int x = 0;
    int y = 0;
    int z = 0;

    /// Auction (state 0).
    QString bidderName;
    bool isBidder = false;
    quint8 bidDisableReason = 0;
    quint32 bidEnd = 0;
    /// bidEnd minus the time the list arrived, in seconds.
    qint64 timeUntilAuctionEnd = 0;
    quint64 highestBid = 0;
    quint64 ownBidLimit = 0;

    /// Rented (states 2 to 4).
    QString renterName;
    quint32 paidUntil = 0;
    bool isOwner = false;
    quint8 moveOutDisableReason = 0;
    quint8 transferDisableReason = 0;

    /// Transfer pending (state 3).
    quint32 transferDate = 0;
    QString newOwnerName;
    bool newOwnerAccepted = false;
    quint64 transferPrice = 0;
    bool isNewOwner = false;
    quint8 acceptTransferDisableReason = 0;
    quint8 rejectTransferDisableReason = 0;
    quint8 cancelTransferDisableReason = 0;

    /// Move out pending (state 4).
    quint32 moveOutDate = 0;
    quint8 cancelMoveOutDisableReason = 0;

    /// 0 / 1: named in the first / second house list of the static house
    /// data, 2: in neither.
    int listing = 2;

    /// Without the auction, rent, transfer and move out data.
    void clearDynamicData();
    /// State 6: "updating" until the server answers.
    void setUnknown();
    /// State 1: rentable without an auction.
    void setNoAuction();
    /// State 0.
    void setAuction(const QString& bidderName, bool isBidder, quint8 bidDisableReason, quint32 bidEnd, qint64 timeUntilAuctionEnd,
                    quint64 highestBid, quint64 ownBidLimit);
    /// State 2.
    void setRented(const QString& renterName, quint32 paidUntil, bool isOwner, quint8 moveOutDisableReason, quint8 transferDisableReason);
    /// State 4; only a rented house can be moved out of (throws otherwise).
    void setMoveOutPending(quint32 moveOutDate, quint8 cancelMoveOutDisableReason);
};

/// The character's houses (CIP: the storage's +0x50..+0x56, StaticHouseData).
struct THouseCharacterInfo
{
    /// Excluded from auctions until this time (bid error 4).
    quint32 excludedUntil = 0;
    quint8 housesOnWorld = 0;
    quint8 housesOnAccount = 0;
    quint8 guildhalls = 0;
};

/// The house limits (CIP: the storage's +0x58..+0x5a).
struct THouseLimits
{
    quint8 perWorld = 0;
    quint8 perAccount = 0;
    quint8 guildhalls = 0;
};

/// The houses of the Cyclopedia (CIP: cyclopedia::THousesStorage). The
/// static house data and the current house data messages fill it.
class THousesStorage final : public QObject
{
    Q_OBJECT
public:
    explicit THousesStorage(QObject* parent = nullptr);

    const std::vector<THouseInformation>& houses() const { return m_houses; }
    const THouseInformation* house(quint32 id) const;
    /// The unique town names, sorted case-sensitively.
    const QStringList& towns() const { return m_towns; }
    const THouseCharacterInfo& characterInfo() const { return m_characterInfo; }
    const THouseLimits& limits() const { return m_limits; }

    void setCharacterInfo(quint32 excludedUntil, quint8 housesOnWorld, quint8 housesOnAccount, quint8 guildhalls);
    void setLimits(quint8 perWorld, quint8 perAccount, quint8 guildhalls);
    void setListing(quint32 houseId, int listing);

    /// The house setters below only run between beginUpdate and endUpdate;
    /// endUpdate announces the new house information.
    void beginUpdate();
    void setRentable(quint32 houseId, bool rentable);
    /// State 1: rentable without an auction.
    void setNoAuction(quint32 houseId);
    void setAuction(quint32 houseId, const QString& bidderName, bool isBidder, quint8 bidDisableReason, quint32 bidEnd,
                    quint64 highestBid, quint64 ownBidLimit);
    void setRented(quint32 houseId, const QString& renterName, quint32 paidUntil, bool isOwner, quint8 moveOutDisableReason,
                   quint8 transferDisableReason);
    /// A rented house whose transfer is pending.
    void setTransferPending(quint32 houseId, quint32 transferDate, const QString& newOwnerName, bool newOwnerAccepted,
                            quint64 transferPrice, bool isNewOwner, quint8 acceptTransferDisableReason,
                            quint8 rejectTransferDisableReason, quint8 cancelTransferDisableReason);
    /// A rented house whose move out is pending.
    void setMoveOutPending(quint32 houseId, quint32 moveOutDate, quint8 cancelMoveOutDisableReason);
    void endUpdate();

    /// Every house back to "updating".
    void reset();

    /// The client messages. The town
    /// "houses_my_houses_town_name" asks for the character's own houses.
    void requestTown(const QString& town);
    void bid(quint32 houseId, quint64 limit);
    void moveOut(quint32 houseId, quint32 timestamp);
    void transfer(quint32 houseId, quint32 timestamp, const QString& newOwner, quint64 price);
    void cancelMoveOut(quint32 houseId);
    void cancelTransfer(quint32 houseId);
    void acceptTransfer(quint32 houseId);
    void rejectTransfer(quint32 houseId);

    /// The static house data message: the character's houses, the limits
    /// and the two house lists (the six counts in message order).
    void applyStaticHouseData(quint32 excludedUntil, const std::vector<uint8_t>& counts, const std::vector<uint32_t>& firstList,
                              const std::vector<uint32_t>& secondList);
    /// The current house data message: the state of each listed house.
    void applyCurrentHouseData(const std::vector<CyclopediaHouseEntry>& houses);

Q_SIGNALS:
    void houseInformationChanged();
    void characterHousesInfoChanged();
    void housesLimitsChanged();

private:
    void loadStaticHouses();
    /// Throws unless an update runs.
    void ensureUpdating() const;
    /// The house, or a shared stand-in record for an id the static data does
    /// not name.
    THouseInformation& findHouse(quint32 id);
    std::vector<THouseInformation> m_houses;
    QHash<quint32, size_t> m_index;
    QStringList m_towns;
    THouseCharacterInfo m_characterInfo;
    THouseLimits m_limits;
    bool m_updating = false;
};

}
}
