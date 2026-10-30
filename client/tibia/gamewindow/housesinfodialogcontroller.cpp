#include "housesinfodialogcontroller.h"

#include "qmlcomponents/worldmapcameraviewport.h"

#include "client/gameclient.h"
#include "client/gamecore.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/texthelper.h"
#include "worldmap/ambientlightstorage.h"

#include <framework/core/resourcemanager.h>

#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QTimeZone>

#include <unordered_map>

namespace tibia::gamewindow {

namespace {

using Button = TMessageDialogController::Button;
using cyclopedia::THouseCharacterInfo;
using cyclopedia::THouseLimits;
using cyclopedia::THouseRecord;

constexpr int StateAuctioned = 0;
constexpr int StateUnoccupied = 1;
constexpr int StateRented = 2;
constexpr int StateTransferPending = 3;
constexpr int StateMoveOutPending = 4;
constexpr int StateAll = 5;
constexpr int StateUnknown = 6;

QString englishNumber(const qulonglong value) { return QLocale(QLocale::English).toString(value); }

QDateTime fromSecs(const qint64 secs) { return QDateTime::fromSecsSinceEpoch(secs); }

/// CIP 0x10095cee0: "yyyy-MM-dd, HH:mm" Berlin time and its zone.
QString dateBerlin(const qint64 secs)
{
    qmlcomponents::TTextHelperQmlService helper;
    return helper.formatDateTimeWithTimeZoneBerlin(static_cast<int>(secs), false);
}

/// CIP 0x10095d900: the system locale's "MMM dd, HH:mm" Berlin time and its zone.
QString shortDateBerlin(const qint64 secs)
{
    const QString longForm = dateBerlin(secs);
    const QString zone = longForm.section(QLatin1Char(' '), -1);
    const QDateTime berlin = QDateTime::fromString(longForm.section(QLatin1Char(' '), 0, -2), QStringLiteral("yyyy-MM-dd, HH:mm"));
    return QStringLiteral("%1 %2").arg(QLocale().toString(berlin, QStringLiteral("MMM dd, HH:mm")), zone);
}

/// CIP 0x100d8c8d0.
QString dateMulti(const qint64 secs)
{
    qmlcomponents::TTextHelperQmlService helper;
    return helper.createQmlMultiString(dateBerlin(secs), shortDateBerlin(secs));
}

/// 10:00 Berlin time on `date` (CIP: the server save, DAT_10279f210 / 0x10279f208).
QDateTime serverSave(const QDate& date)
{
#if QT_CONFIG(timezone)
    return QDateTime(date, QTime(10, 0), QTimeZone("Europe/Berlin"));
#else
    const QDate lastMarch(date.year(), 3, 31);
    const QDate lastOctober(date.year(), 10, 31);
    const bool summer = date >= lastMarch.addDays(-(lastMarch.dayOfWeek() % 7)) && date < lastOctober.addDays(-(lastOctober.dayOfWeek() % 7));
    return QDateTime(date, QTime(10, 0), QTimeZone::fromSecondsAheadOfUtc(summer ? 7200 : 3600));
#endif
}

/// CIP FUN_10101a300: today's server save, or yesterday's before it.
QDateTime lastServerSaveTime()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime today = serverSave(now.date());
    return now < today ? today.addDays(-1) : today;
}

/// CIP FUN_100d934a0: the rent with thousands shortcuts.
QString shortRent(const quint32 rent)
{
    if (rent < 10000)
        return englishNumber(rent);
    if (rent < 1000000)
        return englishNumber(rent / 1000) + QStringLiteral(" k");
    return englishNumber(rent / 1000000) + QStringLiteral(" kk");
}

/// CIP FUN_100d951d0.
QString amount(const quint8 count)
{
    return QStringLiteral("%1 %2").arg(count).arg(qtTrId(count == 1 ? "houses_amount_singular" : "houses_amount_plural"));
}

QString numberedId(const char* prefix, const int number, const int last)
{
    if (number == 0)
        return {};
    if (number > last)
        return qtTrId("error_code_internal");
    return qtTrId(QByteArray(prefix).append(QByteArray::number(number)).constData());
}

/// CIP FUN_100d8d150.
QString ownHouseReason(const int error) { return numberedId("houses_error_message_own_house", error, 28); }

/// CIP FUN_100d8d410.
QString incomingTransferReason(const int error) { return numberedId("houses_error_message_incoming_transfer", error, 18); }

/// CIP FUN_100d8cb20.
QString bidReason(const int error, const THouseCharacterInfo& info, const THouseLimits& limits, const bool guildHall)
{
    switch (error) {
        case 4: return qtTrId("houses_error_message_bid4").arg(dateBerlin(info.excludedUntil));
        case 12:
            return qtTrId("houses_error_message_bid12")
                .arg(amount(guildHall ? info.guildhalls : info.housesOnAccount))
                .arg(amount(guildHall ? limits.guildhalls : limits.perAccount));
        case 13: return qtTrId("houses_error_message_bid13").arg(amount(info.housesOnWorld)).arg(amount(limits.perWorld));
        default: return numberedId("houses_error_message_bid", error, 23);
    }
}

/// CIP 0x100abd6a0.
QString shortStatus(const THouseRecord& house)
{
    QStringList parts;
    if (!house.isRentable) {
        parts << qtTrId("houses_scheduled_for_renovation");
    } else {
        switch (house.state) {
            case StateAuctioned:
                // CIP FUN_10099b920 with the QColor at 0x1022bc1f0 (#00f000).
                parts << QStringLiteral("<font color=\"%1\">%2</font>").arg(QColor(0, 0xf0, 0).name(), qtTrId("houses_state_auctioned"));
                if (house.bidderName.isEmpty())
                    parts << qtTrId("houses_state_auctioned_no_bits");
                break;
            case StateUnoccupied: parts << qtTrId("houses_state_unoccupied"); break;
            case StateRented:
            case StateTransferPending:
            case StateMoveOutPending: parts << qtTrId("houses_state_rented_by") << house.renterName; break;
            case StateUnknown: parts << qtTrId("updating"); break;
            default: break;
        }
    }
    return parts.join(QLatin1Char(' '));
}

bool isRented(const THouseRecord& house) { return house.state >= StateRented && house.state <= StateMoveOutPending; }

/// The floors of each house: the staticmapdata MapFields extent (CIP
/// FUN_100aafa50 per house id; z from the top-left, z + depth - 1).
struct THouseLayers
{
    int minimum = 0;
    int maximum = 0;
};

quint64 readVarint(const QByteArray& data, qsizetype& at)
{
    quint64 value = 0;
    for (int shift = 0; at < data.size() && shift < 64; shift += 7) {
        const auto byte = static_cast<quint8>(data.at(at++));
        value |= quint64(byte & 0x7f) << shift;
        if (!(byte & 0x80))
            break;
    }
    return value;
}

/// The fields of one protobuf message: varints and length-delimited bodies.
template <class F>
void forEachField(const QByteArray& data, F&& visit)
{
    qsizetype at = 0;
    while (at < data.size()) {
        const quint64 key = readVarint(data, at);
        const int field = static_cast<int>(key >> 3);
        switch (key & 7) {
            case 0: visit(field, readVarint(data, at), QByteArray()); break;
            case 2: {
                const auto length = static_cast<qsizetype>(readVarint(data, at));
                visit(field, 0, data.mid(at, length));
                at += length;
                break;
            }
            case 1: at += 8; break;
            case 5: at += 4; break;
            default: return;
        }
    }
}

int coordinateZ(const QByteArray& coordinate)
{
    int z = 0;
    forEachField(coordinate, [&z](const int field, const quint64 value, const QByteArray&) {
        if (field == 3)
            z = static_cast<int>(value);
    });
    return z;
}

const std::unordered_map<quint32, THouseLayers>& houseLayers()
{
    static std::unordered_map<quint32, THouseLayers> layers;
    if (!layers.empty())
        return layers;
    try {
        const std::string dir = client::gamecore::assetsRoot(client::TGameClient::ClientVersion);
        const QJsonDocument catalog = QJsonDocument::fromJson(QByteArray::fromStdString(tibia::files::data().readFileContents(dir + "catalog-content.json")));
        QString file;
        for (const QJsonValue entry : catalog.array()) {
            if (entry.toObject().value(QStringLiteral("type")).toString() == QStringLiteral("staticmapdata"))
                file = entry.toObject().value(QStringLiteral("file")).toString();
        }
        if (file.isEmpty())
            return layers;
        const QByteArray data = QByteArray::fromStdString(tibia::files::data().readFileContents(dir + file.toStdString()));
        forEachField(data, [](const int field, quint64, const QByteArray& house) {
            if (field != 1)
                return;
            quint32 id = 0;
            THouseLayers houseLayers;
            forEachField(house, [&](const int houseField, const quint64 value, const QByteArray& fields) {
                if (houseField == 1)
                    id = static_cast<quint32>(value);
                if (houseField != 2)
                    return;
                int top = 0;
                int depth = 0;
                forEachField(fields, [&](const int mapField, quint64, const QByteArray& body) {
                    if (mapField == 1)
                        top = coordinateZ(body);
                    if (mapField == 2)
                        depth = coordinateZ(body);
                });
                if (depth > 0)
                    houseLayers = { top, top + depth - 1 };
            });
            if (id)
                layers.emplace(id, houseLayers);
        });
    } catch (const std::exception& e) {
        qWarning("houses: no staticmapdata: %s", e.what());
    }
    return layers;
}

}

int THousesModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_houses.size());
}

QHash<int, QByteArray> THousesModel::roleNames() const
{
    return {
        { HouseId, "houseId" },
        { Name, "name" },
        { Size, "size" },
        { MaxBeds, "maxBeds" },
        { Rent, "rent" },
        { State, "state" },
        { Town, "town" },
        { Description, "description" },
        { IsGuildHall, "isGuildHall" },
        { IsShop, "isShop" },
        { IsRentable, "isRentable" },
        { ShortStatusString, "shortStatusString" },
        { HasBids, "hasBids" },
        { HighestBidderName, "highestBidderName" },
        { HighestBid, "highestBid" },
        { TimeUntilAuctionEnd, "timeUntilAuctionEnd" },
        { TimeUntilAuctionEndString, "timeUntilAuctionEndString" },
        { AuctionEndString, "auctionEndString" },
        { AuctionEndStringMulti, "auctionEndStringMulti" },
        { OwnLimitSet, "ownLimitSet" },
        { OwnBidLimit, "ownBidLimit" },
        { DisableBidReasonString, "disableBidReasonString" },
        { RenterName, "renterName" },
        { PaidUntilString, "paidUntilString" },
        { PaidUntilStringMulti, "paidUntilStringMulti" },
        { IsOwnHouse, "isOwnHouse" },
        { DisableMoveOutReasonString, "disableMoveOutReasonString" },
        { DisableTransferReasonString, "disableTransferReasonString" },
        { TransferTimestampString, "transferTimestampString" },
        { TransferTimestampStringMulti, "transferTimestampStringMulti" },
        { NewOwnerName, "newOwnerName" },
        { HasNewOwnerAccepted, "hasNewOwnerAccepted" },
        { TransferPrice, "transferPrice" },
        { IsHouseOfferedToMe, "isHouseOfferedToMe" },
        { DisableAcceptTransferReasonString, "disableAcceptTransferReasonString" },
        { DisableRejectTransferReasonString, "disableRejectTransferReasonString" },
        { DisableCancelTransferReasonString, "disableCancelTransferReasonString" },
        { MoveOutTimestampString, "moveOutTimestampString" },
        { MoveOutTimestampStringMulti, "moveOutTimestampStringMulti" },
        { DisableCancelMoveOutReasonString, "disableCancelMoveOutReasonString" },
    };
}

QVariant THousesModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};
    const THouseRecord& house = m_houses[static_cast<size_t>(index.row())];
    const bool auctioned = house.state == StateAuctioned;
    const bool rented = isRented(house);
    const bool transfer = house.state == StateTransferPending;
    const bool moveOut = house.state == StateMoveOutPending;
    qmlcomponents::TTextHelperQmlService helper;
    switch (role) {
        case HouseId: return house.id;
        case Name: return house.name;
        case Size: return house.size;
        case MaxBeds: return house.beds;
        case Rent: return house.rent;
        case State: return house.state;
        case Town: return house.town;
        case Description: return house.description;
        case IsGuildHall: return house.isGuildHall;
        case IsShop: return house.isShop;
        case IsRentable: return house.isRentable;
        case ShortStatusString: return shortStatus(house);
        case HasBids: return auctioned && !house.bidderName.isEmpty();
        case HighestBidderName: return auctioned ? house.bidderName : QString();
        case HighestBid: return auctioned ? house.highestBid : 0ULL;
        case TimeUntilAuctionEnd: return auctioned ? house.timeUntilAuctionEnd : 0LL;
        case TimeUntilAuctionEndString:
            return helper.formatSecondsToShortDaysHoursMinOrHoursMinOrMinSecondsOrSecondsTimeString(
                auctioned ? static_cast<int>(house.timeUntilAuctionEnd) : 0);
        case AuctionEndString: return dateBerlin(auctioned ? house.bidEnd : 0);
        case AuctionEndStringMulti: return dateMulti(auctioned ? house.bidEnd : 0);
        case OwnLimitSet: return auctioned && house.isBidder;
        case OwnBidLimit: return auctioned ? house.ownBidLimit : 0ULL;
        case DisableBidReasonString:
            return bidReason(auctioned ? house.bidDisableReason : 0, m_characterInfo, m_limits, house.isGuildHall);
        case RenterName: return rented ? house.renterName : QString();
        case PaidUntilString: return dateBerlin(rented ? house.paidUntil : 0);
        case PaidUntilStringMulti: return dateMulti(rented ? house.paidUntil : 0);
        case IsOwnHouse: return rented && house.isOwner;
        case DisableMoveOutReasonString: return ownHouseReason(rented ? house.moveOutDisableReason : 0);
        case DisableTransferReasonString: return ownHouseReason(rented ? house.transferDisableReason : 0);
        case TransferTimestampString: return dateBerlin(transfer ? house.transferDate : 0);
        case TransferTimestampStringMulti: return dateMulti(transfer ? house.transferDate : 0);
        case NewOwnerName: return transfer ? house.newOwnerName : QString();
        case HasNewOwnerAccepted: return transfer && house.newOwnerAccepted;
        case TransferPrice: return transfer ? house.transferPrice : 0ULL;
        case IsHouseOfferedToMe: return transfer && house.isNewOwner;
        case DisableAcceptTransferReasonString: return incomingTransferReason(transfer ? house.acceptTransferDisableReason : 0);
        case DisableRejectTransferReasonString: return incomingTransferReason(transfer ? house.rejectTransferDisableReason : 0);
        case DisableCancelTransferReasonString: return ownHouseReason(transfer ? house.cancelTransferDisableReason : 0);
        case MoveOutTimestampString: return dateBerlin(moveOut ? house.moveOutDate : 0);
        case MoveOutTimestampStringMulti: return dateMulti(moveOut ? house.moveOutDate : 0);
        case DisableCancelMoveOutReasonString: return ownHouseReason(moveOut ? house.cancelMoveOutDisableReason : 0);
        default: return {};
    }
}

void THousesModel::setHouses(std::vector<THouseRecord> houses)
{
    beginResetModel();
    m_houses = std::move(houses);
    endResetModel();
}

THousesSortProxyModel::THousesSortProxyModel(QObject* parent)
    : cip::tibia::gamewindow::THousesSortProxyModel(parent)
    , m_houses(new THousesModel(this))
{
    m_houseIdEnumValue = THousesModel::HouseId;
#if !defined(Q_OS_WASM)
    // Wasm's POSIX collator logs and ignores both of these.
    m_collator.setNumericMode(true);
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);
#endif
    setSourceModel(m_houses);
    setDynamicSortFilter(true);
    sort(0);
}

template <class F>
void THousesSortProxyModel::changeFilter(F&& change)
{
    beginFilterChange();
    change();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    invalidate();
}

void THousesSortProxyModel::setTownFilter(const QString& town)
{
    if (town == m_townFilter)
        return;
    changeFilter([&] { m_townFilter = town; });
    Q_EMIT townFilterChanged();
}

void THousesSortProxyModel::setStateFilter(const int state)
{
    if (state == m_stateFilter)
        return;
    changeFilter([&] { m_stateFilter = state; });
    Q_EMIT stateFilterChanged();
}

void THousesSortProxyModel::setHouseTypeFilter(const int type)
{
    if (type == m_houseTypeFilter)
        return;
    changeFilter([&] { m_houseTypeFilter = type; });
    Q_EMIT houseTypeFilterChanged();
}

void THousesSortProxyModel::setSortOrder(const int order)
{
    if (order == m_sortOrder)
        return;
    changeFilter([&] { m_sortOrder = order; });
    Q_EMIT sortOrderChanged();
}

void THousesSortProxyModel::clearFilters()
{
    changeFilter([this] {
        m_townFilter.clear();
        m_houseTypeFilter = 0;
        m_sortOrder = 0;
    });
}

bool THousesSortProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = m_houses->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
        return false;
    if (!m_townFilter.isEmpty()) {
        if (m_townFilter == qtTrId("houses_my_houses_town_name")) {
            if (!index.data(THousesModel::IsOwnHouse).toBool() && !index.data(THousesModel::OwnLimitSet).toBool()
                && !index.data(THousesModel::IsHouseOfferedToMe).toBool())
                return false;
        } else if (index.data(THousesModel::Town).toString() != m_townFilter) {
            return false;
        }
    }
    if (static_cast<int>(index.data(THousesModel::IsGuildHall).toBool()) != m_houseTypeFilter)
        return false;
    const int state = index.data(THousesModel::State).toInt();
    switch (m_stateFilter) {
        case StateRented: return state >= StateRented && state <= StateMoveOutPending;
        case StateAuctioned: return state == StateAuctioned;
        default: return true;
    }
}

bool THousesSortProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    // CIP DAT_1022e344c: auctioned first, then rented, empty, unknown.
    static const std::unordered_map<int, int> stateRank = { { 0, 0 }, { 2, 1 }, { 3, 1 }, { 4, 1 }, { 1, 2 }, { 6, 3 } };
    const bool nameLess = m_collator.compare(left.data(THousesModel::Name).toString(), right.data(THousesModel::Name).toString()) < 0;
    const auto compareInts = [nameLess](const qint64 a, const qint64 b) { return a == b ? nameLess : a < b; };
    switch (m_sortOrder) {
        case 1: return compareInts(left.data(THousesModel::Size).toInt(), right.data(THousesModel::Size).toInt());
        case 2: return compareInts(left.data(THousesModel::Rent).toInt(), right.data(THousesModel::Rent).toInt());
        case 3:
        case 4: {
            const int leftState = left.data(THousesModel::State).toInt();
            const int rightState = right.data(THousesModel::State).toInt();
            const auto leftRank = stateRank.find(leftState);
            const auto rightRank = stateRank.find(rightState);
            if (leftRank == stateRank.end() || rightRank == stateRank.end())
                return nameLess;
            if (leftState != StateAuctioned || rightState != StateAuctioned)
                return compareInts(leftRank->second, rightRank->second);
            const bool leftBids = left.data(THousesModel::HasBids).toBool();
            const bool rightBids = right.data(THousesModel::HasBids).toBool();
            if (!leftBids || !rightBids)
                return leftBids != rightBids ? leftBids : nameLess;
            if (m_sortOrder == 3) {
                const qint64 leftBid = left.data(THousesModel::HighestBid).toLongLong();
                const qint64 rightBid = right.data(THousesModel::HighestBid).toLongLong();
                return leftBid == rightBid ? nameLess : leftBid > rightBid;
            }
            return compareInts(left.data(THousesModel::TimeUntilAuctionEnd).toInt(), right.data(THousesModel::TimeUntilAuctionEnd).toInt());
        }
        default: return nameLess;
    }
}

THousStateFilterOption::THousStateFilterOption(const QString& text, const int state, QObject* parent)
    : cip::tibia::gamewindow::THousStateFilterOption(parent)
{
    m_text = text;
    m_state = static_cast<cip::tibia::qmlcomponents::enums::EHouseState>(state);
}

THousSortOption::THousSortOption(const QString& text, const int order, QObject* parent)
    : cip::tibia::gamewindow::THousSortOption(parent)
{
    m_text = text;
    m_sortOrder = static_cast<cip::tibia::qmlcomponents::enums::EHousSortOrder>(order);
}

THousesInfoDialogController::THousesInfoDialogController(QObject* parent)
    : cip::tibia::gamewindow::THousesInfoDialogController(parent)
    , m_storage(new cyclopedia::THousesStorage(this))
    , m_proxy(new THousesSortProxyModel(this))
    , m_resultDialog(new TMessageDialogController(this))
    , m_confirmDialog(new TMessageDialogController(this))
    , m_houseItems(new THouseItemModel(this))
{
    m_model = m_proxy->houses();
    m_housesModel = m_proxy;
    Q_UNUSED(m_houseItems);
    for (TMessageDialogController* dialog : { m_resultDialog, m_confirmDialog }) {
        dialog->setCloseOnButtonClicked(true);
        dialog->setMessageDialogTextFormat(TMessageDialogController::RICH_TEXT);
    }
    m_confirmDialog->setYesNoButtons();
    m_resultDialog->setButtons({ new TMessageDialogButton(qtTrId("ok"), Button::OK, QKeySequence(QStringLiteral("Return"))) });
    connect(m_confirmDialog, &TMessageDialogController::acceptClicked, this, &THousesInfoDialogController::onHouseActionConfirmed);
    connect(m_confirmDialog, &TMessageDialogController::discardClicked, this, &THousesInfoDialogController::resetHouseActionData);

    // CIP ctor: each proxy filter signal is the page's filter signal and a
    // current house change (the first row may be another house).
    connect(m_proxy, &THousesSortProxyModel::townFilterChanged, this, [this] {
        Q_EMIT townFilterChanged();
        Q_EMIT currentHouseIdChanged();
    });
    connect(m_proxy, &THousesSortProxyModel::stateFilterChanged, this, [this] {
        Q_EMIT stateFilterChanged();
        Q_EMIT currentHouseIdChanged();
    });
    connect(m_proxy, &THousesSortProxyModel::houseTypeFilterChanged, this, [this] {
        Q_EMIT houseTypeFilterChanged();
        Q_EMIT currentHouseIdChanged();
    });
    connect(m_proxy, &THousesSortProxyModel::sortOrderChanged, this, [this] {
        Q_EMIT sortOrderChanged();
        Q_EMIT currentHouseIdChanged();
    });

    // CIP: a 5 ms single shot asks for the town's dynamic data.
    m_townRequestTimer.setSingleShot(true);
    m_townRequestTimer.setInterval(5);
    connect(&m_townRequestTimer, &QTimer::timeout, this, [this] { m_storage->requestTown(m_proxy->townFilter()); });

    m_statesFitlerModel = {
        new THousStateFilterOption(qtTrId("houses_filter_all_states"), StateAll, this),
        new THousStateFilterOption(qtTrId("houses_filter_auctioned"), StateAuctioned, this),
        new THousStateFilterOption(qtTrId("houses_filter_rented"), StateRented, this),
    };
    m_sortOrderModel = {
        new THousSortOption(qtTrId("houses_sort_by_name"), 0, this),
        new THousSortOption(qtTrId("houses_sort_by_size"), 1, this),
        new THousSortOption(qtTrId("houses_sort_by_rent"), 2, this),
        new THousSortOption(qtTrId("houses_sort_by_bid"), 3, this),
        new THousSortOption(qtTrId("houses_sort_by_auction_end"), 4, this),
    };

    connect(m_storage, &cyclopedia::THousesStorage::houseInformationChanged, this, &THousesInfoDialogController::onHouseInformationChanged);
    connect(m_storage, &cyclopedia::THousesStorage::characterHousesInfoChanged, this,
            &THousesInfoDialogController::onCharacterHousesInfoChanged);
    connect(m_storage, &cyclopedia::THousesStorage::housesLimitsChanged, this, &THousesInfoDialogController::onHousesLimitsChanged);
    connect(m_storage, &cyclopedia::THousesStorage::houseActionResult, this, &THousesInfoDialogController::onHouseActionResult);
    connect(m_storage, &cyclopedia::THousesStorage::houseInformationChanged, this, [this] {
        // CIP session 0x100d90430: the towns, "Own Houses" first.
        QStringList towns = m_storage->towns();
        std::sort(towns.begin(), towns.end(), [](const QString& a, const QString& b) { return a.compare(b, Qt::CaseInsensitive) < 0; });
        towns.insert(0, qtTrId("houses_my_houses_town_name"));
        if (towns != m_townFilterModel) {
            m_townFilterModel = towns;
            Q_EMIT staticDataChanged();
        }
    });
    onHouseInformationChanged();
}

THousesInfoDialogController::~THousesInfoDialogController() = default;

void THousesInfoDialogController::setAmbientLightStorage(worldmap::TAmbientLightStorage* ambientLight)
{
    if (m_ambientLight)
        disconnect(m_ambientLight, nullptr, this, nullptr);
    m_ambientLight = ambientLight;
    if (ambientLight) {
        connect(ambientLight, &worldmap::TAmbientLightStorage::normalizedTibiaTimeOfDayWasChanged, this,
                &THousesInfoDialogController::updateTimeDisplay);
        connect(ambientLight, &worldmap::TAmbientLightStorage::interpolatedTibiaTimeOfDayChanged, this,
                &THousesInfoDialogController::updateTimeDisplay);
    }
    updateTimeDisplay();
}

/// CIP 0x100d90fd0.
void THousesInfoDialogController::updateTimeDisplay()
{
    if (!m_ambientLight)
        return;
    m_timeOfDayNormalized = m_ambientLight->normalizedTibiaTimeOfDay();
    Q_EMIT timeOfDayChanged();
}

THousesInfoDialogController::Context THousesInfoDialogController::captureContext() const
{
    return { m_proxy->townFilter(), m_proxy->stateFilter(), m_proxy->houseTypeFilter(), m_proxy->sortOrder(), m_currentHouseId };
}

/// CIP 0x100d91d50 (without the preview camera).
void THousesInfoDialogController::restoreContext(const Context& context)
{
    setDisableQmlSelect(true);
    setTownFilter(context.town);
    m_proxy->setStateFilter(context.state);
    m_proxy->setHouseTypeFilter(context.type);
    m_proxy->setSortOrder(context.sortOrder);
    selectHouse(context.houseId);
    setDisableQmlSelect(false);
}

void THousesInfoDialogController::setDisableQmlSelect(const bool disable)
{
    if (m_disableQmlSelect == disable)
        return;
    m_disableQmlSelect = disable;
    Q_EMIT disableQmlSelectChanged();
}

/// CIP 0x100d91a20.
int THousesInfoDialogController::townFilterCurrentIndex() const
{
    return std::max<int>(0, static_cast<int>(m_townFilterModel.indexOf(m_proxy->townFilter())));
}

/// CIP 0x100d91aa0.
int THousesInfoDialogController::stateFilterCurrentIndex() const
{
    for (int i = 0; i < m_statesFitlerModel.size(); ++i) {
        if (static_cast<int>(static_cast<THousStateFilterOption*>(m_statesFitlerModel.at(i))->state()) == m_proxy->stateFilter())
            return i;
    }
    return 0;
}

cip::tibia::qmlcomponents::enums::EHouseType THousesInfoDialogController::houseType() const
{
    return static_cast<cip::tibia::qmlcomponents::enums::EHouseType>(m_proxy->houseTypeFilter());
}

/// CIP 0x100d91b20.
int THousesInfoDialogController::sortOrderCurrentIndex() const
{
    for (int i = 0; i < m_sortOrderModel.size(); ++i) {
        if (static_cast<int>(static_cast<THousSortOption*>(m_sortOrderModel.at(i))->sortOrder()) == m_proxy->sortOrder())
            return i;
    }
    return 0;
}

/// CIP 0x100d91b90.
uint THousesInfoDialogController::currentHouseId() const
{
    return m_proxy->rowCount() > 0 ? m_currentHouseId : 0;
}

/// CIP 0x100d91c10.
QDateTime THousesInfoDialogController::lastServerSave() const { return lastServerSaveTime(); }

/// CIP 0x100d91c30.
QDateTime THousesInfoDialogController::in30DaysServerSave() const { return lastServerSaveTime().addDays(30); }

/// CIP 0x100d91c90 is empty; product selects a town/house so the 3D preview has a row.
void THousesInfoDialogController::showDialogTab()
{
    ensureTownAndHouseSelection();
    // One refresh when the tab opens. The house-list reply must not start another.
    if (!m_proxy->townFilter().isEmpty())
        m_townRequestTimer.start();
}

/// CIP 0x100d91ca0.
void THousesInfoDialogController::hideDialogTab()
{
    m_proxy->clearFilters();
    m_storage->reset();
    m_currentHouseId = 0;
    Q_EMIT currentHouseIdChanged();
}

/// CIP 0x100d91010.
void THousesInfoDialogController::onHouseInformationChanged()
{
    m_proxy->changeFilter([this] { m_model->setHouses(m_storage->houses()); });
    ensureTownAndHouseSelection();
    Q_EMIT currentHouseIdChanged();
}

/// CIP 0x100d91110.
void THousesInfoDialogController::onCharacterHousesInfoChanged() { m_model->setCharacterInfo(m_storage->characterInfo()); }

/// CIP 0x100d91170.
void THousesInfoDialogController::onHousesLimitsChanged() { m_model->setLimits(m_storage->limits()); }

void THousesInfoDialogController::requestDynamicDataForTown() { m_townRequestTimer.start(); }

/// CIP 0x100d91f50.
void THousesInfoDialogController::setTownFilter(const QString& Town)
{
    m_townRequestTimer.start();
    m_proxy->setTownFilter(Town);
}

/// CIP 0x100d91ff0.
void THousesInfoDialogController::setStateFilter(const int State) { m_proxy->setStateFilter(State); }

void THousesInfoDialogController::setHouseTypeFilter(const int HouseType) { m_proxy->setHouseTypeFilter(HouseType); }

void THousesInfoDialogController::setSortOrder(const int SortOrder) { m_proxy->setSortOrder(SortOrder); }

void THousesInfoDialogController::refresh()
{
    m_storage->reset();
    m_townRequestTimer.start();
}

/// CIP 0x100d924f0.
void THousesInfoDialogController::setHouseWorldMapQuickItem(cip::tibia::qmlcomponents::TWorldMapQuickItem* pWorldMapQuickItem)
{
    m_houseWorldMap = pWorldMapQuickItem;
    centerHousePreview();
}

/// CIP 0x100d926c0.
void THousesInfoDialogController::setHouseCameraViewport(cip::tibia::qmlcomponents::TWorldMapCameraViewportQmlType* pCameraViewport)
{
    m_houseCamera = pCameraViewport;
    centerHousePreview();
}

/// CIP 0x100d92140.
void THousesInfoDialogController::selectHouse(const uint HouseId)
{
    if (m_currentHouseId == HouseId)
        return;
    m_currentHouseId = HouseId;
    updateHouseLayers(HouseId);
    centerHousePreview();
    Q_EMIT currentHouseIdChanged();
}

void THousesInfoDialogController::updateHouseLayers(const uint houseId)
{
    const auto& layers = houseLayers();
    const auto it = layers.find(houseId);
    const THouseLayers houseLayers = it == layers.end() ? THouseLayers{} : it->second;
    m_minimumHouseLayer = houseLayers.minimum;
    m_maximumHouseLayer = houseLayers.maximum;
    Q_EMIT restrictingLayersChanged();
}

/// CIP: OPEN_CYCLOPEDIA's map tab at the house.
void THousesInfoDialogController::showOnCyclopediaMap()
{
    const THouseRecord* house = m_storage->house(m_currentHouseId);
    if (!house)
        return;
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_CYCLOPEDIA));
    action->parameter = 4;
    action->position = Position(house->x, house->y, static_cast<uint8_t>(house->z));
    input::publish(action);
}

void THousesInfoDialogController::ensureTownAndHouseSelection()
{
    if (m_proxy->townFilter().isEmpty() && !m_townFilterModel.isEmpty()) {
        // Prefer the first real town over "Own Houses" so the preview has map data.
        const QString own = qtTrId("houses_my_houses_town_name");
        QString town = m_townFilterModel.value(0);
        for (const QString& candidate : m_townFilterModel) {
            if (candidate != own) {
                town = candidate;
                break;
            }
        }
        if (!town.isEmpty())
            setTownFilter(town);
    }

    if (m_proxy->rowCount() <= 0)
        return;
    const QModelIndex first = m_proxy->index(0, 0);
    const uint firstId = first.data(THousesModel::HouseId).toUInt();
    if (firstId == 0)
        return;
    const bool currentMissing = m_currentHouseId == 0 || !m_storage->house(m_currentHouseId);
    if (currentMissing)
        selectHouse(firstId);
    else
        centerHousePreview();
}

/// CIP 0x100d92ec0 / makeHouseVisible 0x100d92a00.
void THousesInfoDialogController::centerHouse() { centerHousePreview(); }

void THousesInfoDialogController::centerHousePreview()
{
    const THouseRecord* house = m_storage->house(m_currentHouseId);
    if (!house || !m_houseCamera)
        return;
    if (auto* viewport = qobject_cast<qmlcomponents::TWorldMapCameraViewport*>(m_houseCamera.data())) {
        viewport->setTopAndBottomLayer(m_maximumHouseLayer, m_minimumHouseLayer);
        viewport->centerAtField(house->x, house->y, house->z);
    } else {
        auto* center = m_houseCamera->centerSubfieldCoordinate();
        if (!center) {
            center = new qmlcomponents::TWorldMapSubfieldCoordinateQmlType(m_houseCamera);
            m_houseCamera->setCenterSubfieldCoordinate(center);
        }
        center->setSubfieldX(house->x * 32 + 16);
        center->setSubfieldY(house->y * 32 + 16);
        center->setZ(house->z);
        m_houseCamera->setTopAndBottomLayer(m_maximumHouseLayer, m_minimumHouseLayer);
        Q_EMIT m_houseCamera->viewportChanged();
    }
}

void THousesInfoDialogController::confirmAction(const int action, const QString& message)
{
    m_pendingAction = action;
    m_confirmDialog->showMessage(qtTrId("houses_action_confirm_caption"), message);
}

/// CIP 0x100d93060.
void THousesInfoDialogController::onActionBidClicked(const qulonglong Limit)
{
    const THouseRecord* house = m_storage->house(m_currentHouseId);
    if (!house)
        return;
    m_pendingHouseId = m_currentHouseId;
    m_pendingAmount = Limit;
    confirmAction(1, qtTrId("houses_action_confirm_bid_message").arg(house->name, englishNumber(Limit), shortRent(house->rent)));
}

void THousesInfoDialogController::onActionMoveOutClicked(const QDate& Date)
{
    const THouseRecord* house = m_storage->house(m_currentHouseId);
    if (!house)
        return;
    m_pendingHouseId = m_currentHouseId;
    m_pendingTimestamp = static_cast<quint32>(serverSave(Date).toSecsSinceEpoch());
    confirmAction(2, qtTrId("houses_action_confirm_move_out_message").arg(house->name, dateBerlin(m_pendingTimestamp)));
}

void THousesInfoDialogController::onActionTransferClicked(const QDate& Date, const QString& NewOwner, const qulonglong Price)
{
    const THouseRecord* house = m_storage->house(m_currentHouseId);
    if (!house)
        return;
    m_pendingHouseId = m_currentHouseId;
    m_pendingTimestamp = static_cast<quint32>(serverSave(Date).toSecsSinceEpoch());
    m_pendingName = NewOwner;
    m_pendingAmount = Price;
    confirmAction(3, qtTrId("houses_action_confirm_transfer_message")
                         .arg(house->name, NewOwner, dateBerlin(m_pendingTimestamp), englishNumber(Price)));
}

void THousesInfoDialogController::onActionCancelMoveOutClicked()
{
    const THouseRecord* house = m_storage->house(m_currentHouseId);
    if (!house)
        return;
    m_pendingHouseId = m_currentHouseId;
    confirmAction(4, qtTrId("houses_action_confirm_cancel_move_out_message").arg(house->name, dateBerlin(house->moveOutDate)));
}

void THousesInfoDialogController::onActionCancelTransferClicked()
{
    const THouseRecord* house = m_storage->house(m_currentHouseId);
    if (!house)
        return;
    m_pendingHouseId = m_currentHouseId;
    confirmAction(5, qtTrId("houses_action_confirm_cancel_transfer_message")
                         .arg(house->name, dateBerlin(house->transferDate), house->newOwnerName));
}

void THousesInfoDialogController::onActionAcceptTransferClicked()
{
    const THouseRecord* house = m_storage->house(m_currentHouseId);
    if (!house)
        return;
    m_pendingHouseId = m_currentHouseId;
    confirmAction(6, qtTrId("houses_action_confirm_accept_transfer_message")
                         .arg(house->name, house->renterName, dateBerlin(house->transferDate), englishNumber(house->transferPrice)));
}

void THousesInfoDialogController::onActionRejectTransferClicked()
{
    const THouseRecord* house = m_storage->house(m_currentHouseId);
    if (!house)
        return;
    m_pendingHouseId = m_currentHouseId;
    confirmAction(7, qtTrId("houses_action_confirm_reject_transfer_message").arg(house->name, house->renterName));
}

void THousesInfoDialogController::onHouseActionConfirmed()
{
    switch (m_pendingAction) {
        case 1: m_storage->bid(m_pendingHouseId, m_pendingAmount); break;
        case 2: m_storage->moveOut(m_pendingHouseId, m_pendingTimestamp); break;
        case 3: m_storage->transfer(m_pendingHouseId, m_pendingTimestamp, m_pendingName, m_pendingAmount); break;
        case 4: m_storage->cancelMoveOut(m_pendingHouseId); break;
        case 5: m_storage->cancelTransfer(m_pendingHouseId); break;
        case 6: m_storage->acceptTransfer(m_pendingHouseId); break;
        case 7: m_storage->rejectTransfer(m_pendingHouseId); break;
        default: break;
    }
    resetHouseActionData();
    Q_EMIT houseActionConfirmed();
}

void THousesInfoDialogController::resetHouseActionData()
{
    m_pendingAction = -1;
    m_pendingHouseId = 0;
    m_pendingAmount = 0;
    m_pendingTimestamp = 0;
    m_pendingName.clear();
}

void THousesInfoDialogController::onHouseActionResult(const quint32 houseId, const int action, const int errorCode, const bool hasBeenOutbidden)
{
    static const char* const names[] = { "bid", "move_out", "transfer", "cancel_move_out", "cancel_transfer", "accept_transfer", "reject_transfer" };
    const bool known = action >= 1 && action <= 7;
    const QByteArray name = known ? QByteArray("houses_actions_").append(names[action - 1]) : QByteArray();
    QString message;
    if (errorCode == 0) {
        if (known)
            message = action == 1 && hasBeenOutbidden ? qtTrId("houses_actions_bid_success_outbidden") : qtTrId(QByteArray(name).append("_success").constData());
    } else {
        QString reason;
        if (action >= 2 && action <= 5) {
            reason = ownHouseReason(errorCode);
        } else if (action == 6 || action == 7) {
            reason = incomingTransferReason(errorCode);
        } else if (action == 1) {
            const THouseRecord* house = m_storage->house(houseId);
            reason = bidReason(errorCode, m_storage->characterInfo(), m_storage->limits(), house && house->isGuildHall);
        } else {
            reason = qtTrId("error_code_internal");
        }
        if (known)
            message = qtTrId(QByteArray(name).append("_failed").constData());
        message += QStringLiteral("<br>") + reason;
    }
    m_resultDialog->showMessage(qtTrId("houses_actions_result_caption"), message);
}

}
