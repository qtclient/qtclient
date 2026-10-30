#pragma once

#include "client/gamecore.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

#include <vector>

namespace tibia::cyclopedia {

/// One house of the Cyclopedia (CIP: the 0x140-byte records of THousesStorage
/// +0x20; static part from staticdata, dynamic part from CurrentHouseData).
struct THouseRecord
{
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

    /// CIP FUN_100abd210: forgets the dynamic data.
    void clearDynamicData();
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

/// The houses of the Cyclopedia (CIP: cyclopedia::THousesStorage, ctor
/// 0x100abde00, fed by TCyclopediaProtocolMessageHandler slots 27 to 29).
class THousesStorage final : public QObject
{
    Q_OBJECT
public:
    explicit THousesStorage(QObject* parent = nullptr);

    /// CIP vslot 0x60.
    const std::vector<THouseRecord>& houses() const { return m_houses; }
    /// CIP vslot 0xe0.
    const THouseRecord* house(quint32 id) const;
    /// The unique town names, sorted case-sensitively.
    const QStringList& towns() const { return m_towns; }
    const THouseCharacterInfo& characterInfo() const { return m_characterInfo; }
    const THouseLimits& limits() const { return m_limits; }

    /// CIP vslot 0xd8: every house back to "updating".
    void reset();

    /// The client messages (CIP 0x100a55... vslots 28 to 35). The town
    /// "houses_my_houses_town_name" asks for the character's own houses.
    void requestTown(const QString& town);
    void bid(quint32 houseId, quint64 limit);
    void moveOut(quint32 houseId, quint32 timestamp);
    void transfer(quint32 houseId, quint32 timestamp, const QString& newOwner, quint64 price);
    void cancelMoveOut(quint32 houseId);
    void cancelTransfer(quint32 houseId);
    void acceptTransfer(quint32 houseId);
    void rejectTransfer(quint32 houseId);

Q_SIGNALS:
    void houseInformationChanged();
    void characterHousesInfoChanged();
    void housesLimitsChanged();
    /// CIP: TCyclopediaProtocolMessageHandler::requestHouseActionResultDialog.
    void houseActionResult(quint32 houseId, int action, int errorCode, bool hasBeenOutbidden);

private:
    void loadStaticHouses();
    THouseRecord* findHouse(quint32 id);

    std::vector<client::gamecore::Hook> m_hooks;
    std::vector<THouseRecord> m_houses;
    QHash<quint32, size_t> m_index;
    QStringList m_towns;
    THouseCharacterInfo m_characterInfo;
    THouseLimits m_limits;
};

}
