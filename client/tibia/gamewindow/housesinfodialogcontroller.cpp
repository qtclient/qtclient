#include "housesinfodialogcontroller.h"

#include "cyclopedia/cyclopediaciptypes.h"
#include "cyclopedia/cyclopediamapstaticdatastorage.h"

#include "platform/platform.h"
#include "game/serversave.h"

#include "qmlcomponents/worldmapcameraviewport.h"

#include "client/gameclient.h"
#include "client/gamecore.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/texthelper.h"
#include "worldmap/ambientlightstorage.h"

#include "shared/tibiafilesystemhelper.h"
#include "assets/things.h"

#include <utility>

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
using cyclopedia::THouseInformation;

constexpr int StateAuctioned = 0;
constexpr int StateUnoccupied = 1;
constexpr int StateRented = 2;
constexpr int StateTransferPending = 3;
constexpr int StateMoveOutPending = 4;
constexpr int StateAll = 5;
constexpr int StateUnknown = 6;

QString englishNumber(const qulonglong value) { return QLocale(QLocale::English).toString(value); }

QDateTime fromSecs(const qint64 secs) { return QDateTime::fromSecsSinceEpoch(secs); }

/// "yyyy-MM-dd, HH:mm" Berlin time and its zone (CET / CEST).
QString dateBerlin(const qint64 secs)
{
    qmlcomponents::TTextHelperQmlService helper;
    return helper.formatDateTimeWithTimeZoneBerlin(static_cast<int>(secs), false);
}

/// The system locale's "MMM dd, HH:mm" Berlin time and its zone.
QString shortDateBerlin(const qint64 secs)
{
    const QString longForm = dateBerlin(secs);
    const QString zone = longForm.section(QLatin1Char(' '), -1);
    const QDateTime berlin = QDateTime::fromString(longForm.section(QLatin1Char(' '), 0, -2), QStringLiteral("yyyy-MM-dd, HH:mm"));
    return QStringLiteral("%1 %2").arg(QLocale().toString(berlin, QStringLiteral("MMM dd, HH:mm")), zone);
}

QString dateMulti(const qint64 secs)
{
    qmlcomponents::TTextHelperQmlService helper;
    return helper.createQmlMultiString(dateBerlin(secs), shortDateBerlin(secs));
}

/// The rent with thousands shortcuts.
QString shortRent(const quint32 rent)
{
    if (rent < 10000)
        return englishNumber(rent);
    if (rent < 1000000)
        return englishNumber(rent / 1000) + QStringLiteral(" k");
    return englishNumber(rent / 1000000) + QStringLiteral(" kk");
}

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

QString ownHouseReason(const int error) { return numberedId("houses_error_message_own_house", error, 28); }

QString incomingTransferReason(const int error) { return numberedId("houses_error_message_incoming_transfer", error, 18); }

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

QString shortStatus(const THouseInformation& house)
{
    QStringList parts;
    if (!house.isRentable) {
        parts << qtTrId("houses_scheduled_for_renovation");
    } else {
        switch (house.state) {
            case StateAuctioned:
                // Auctioned is green (#00f000).
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

bool isRented(const THouseInformation& house) { return house.state >= StateRented && house.state <= StateMoveOutPending; }

/// The floors of a house: its area in the static map data (z from the top
/// left, z + depth - 1); 0 and 0 without one.
struct THouseLayers
{
    int minimum = 0;
    int maximum = 0;
};

THouseLayers houseLayersOf(const quint32 houseId)
{
    const cyclopedia::TStaticHouseMapArea house = cyclopedia::houseMapArea(houseId);
    if (house.area.extent.depth <= 0)
        return {};
    return { house.area.origin.z, house.area.origin.z + house.area.extent.depth - 1 };
}

}

int THousesModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_houses.size());
}

QHash<int, QByteArray> THousesModel::roleNames() const
{
    // The default item roles plus the house roles.
    QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
    roles.insert({
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
    });
    return roles;
}

QVariant THousesModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};
    const THouseInformation& house = m_houses[static_cast<size_t>(index.row())];
    const bool auctioned = house.state == StateAuctioned;
    const bool rented = isRented(house);
    const bool transfer = house.state == StateTransferPending;
    const bool moveOut = house.state == StateMoveOutPending;
    qmlcomponents::TTextHelperQmlService helper;
    switch (role) {
        case HouseId: return house.id;
        case Name: return house.name;
        case Size: return uint(house.size);
        case MaxBeds: return uint(house.beds);
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
        case HighestBid: return qlonglong(auctioned ? house.highestBid : 0);
        case TimeUntilAuctionEnd: return int(auctioned ? house.timeUntilAuctionEnd : 0);
        case TimeUntilAuctionEndString:
            return helper.formatSecondsToShortDaysHoursMinOrHoursMinOrMinSecondsOrSecondsTimeString(
                auctioned ? static_cast<int>(house.timeUntilAuctionEnd) : 0);
        case AuctionEndString: return dateBerlin(auctioned ? house.bidEnd : 0);
        case AuctionEndStringMulti: return dateMulti(auctioned ? house.bidEnd : 0);
        case OwnLimitSet: return auctioned && house.isBidder;
        case OwnBidLimit: return qlonglong(auctioned ? house.ownBidLimit : 0);
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
        case TransferPrice: return qlonglong(transfer ? house.transferPrice : 0);
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

void THousesModel::setHouses(std::vector<THouseInformation> houses)
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
    setDynamicSortFilter(true);
    sort(0);
    setSourceModel(m_houses);
    tibia::platform::configureNaturalCollator(m_collator);
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
    // Auctioned first, then rented, empty, unknown; any other state throws.
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
            const int leftRank = stateRank.at(leftState);
            const int rightRank = stateRank.at(rightState);
            if (leftState != StateAuctioned || rightState != StateAuctioned)
                return compareInts(leftRank, rightRank);
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
{
    m_model = m_proxy->houses();
    m_housesModel = m_proxy;
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
    connect(&m_townRequestTimer, &QTimer::timeout, this, &THousesInfoDialogController::onRequestDynamicDataForTownTimerTimeout);

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
    // The house data messages fill this storage; their action results come to this dialog.
    cyclopedia::TCyclopediaProtocolMessageHandler::setHousesStorage(m_storage);
    connect(cyclopedia::TCyclopediaProtocolMessageHandler::instance(),
            &cyclopedia::TCyclopediaProtocolMessageHandler::requestHouseActionResultDialog, this,
            [this](const quint32 houseId, const quint8 action, const quint8 errorCode, const bool hasBeenOutbidden) {
                onRequestHouseActionResultDialog(houseId, action, errorCode, hasBeenOutbidden);
            });
    connect(m_storage, &cyclopedia::THousesStorage::houseInformationChanged, this, [this] {
        // The towns, "Own Houses" first.
        QStringList towns = m_storage->towns();
        std::sort(towns.begin(), towns.end(), [](const QString& a, const QString& b) { return a.compare(b, Qt::CaseInsensitive) < 0; });
        towns.insert(0, qtTrId("houses_my_houses_town_name"));
        if (towns != m_townFilterModel) {
            m_townFilterModel = towns;
            Q_EMIT staticDataChanged();
        }
    });
    // Host: the Cyclopedia map's house entrances follow this storage's houses.
    connect(m_storage, &cyclopedia::THousesStorage::houseInformationChanged, this,
            [this] { cyclopedia::TCyclopediaMapStaticDataStorage::instance().setHousesStorage(m_storage); });
    cyclopedia::TCyclopediaMapStaticDataStorage::instance().setHousesStorage(m_storage);
    onHouseInformationChanged();
}

THousesInfoDialogController::~THousesInfoDialogController()
{
    disconnect(m_storage, nullptr, this, nullptr);
    cyclopedia::TCyclopediaProtocolMessageHandler::setHousesStorage(nullptr);
    cyclopedia::TCyclopediaMapStaticDataStorage::instance().setHousesStorage(nullptr);
    clearFilterModels();
}

void THousesInfoDialogController::clearFilterModels()
{
    const QList<QObject*> states = std::exchange(m_statesFitlerModel, {});
    const QList<QObject*> sortOrders = std::exchange(m_sortOrderModel, {});
    Q_EMIT staticDataChanged();
    qDeleteAll(states);
    qDeleteAll(sortOrders);
}

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

void THousesInfoDialogController::updateTimeDisplay()
{
    if (!m_ambientLight)
        return;
    m_timeOfDayNormalized = m_ambientLight->normalizedTibiaTimeOfDay();
    Q_EMIT timeOfDayChanged();
}

TCyclopediaDialogContext THousesInfoDialogController::captureContext() const
{
    THousesInfoCyclopediaContext context{ m_proxy->townFilter(), m_proxy->stateFilter(), m_proxy->houseTypeFilter(),
                                          m_proxy->sortOrder(), m_currentHouseId, {}, 1.0f };
    // The preview camera's centre and zoom, while there is one.
    if (m_houseCamera) {
        if (const auto* center = m_houseCamera->centerSubfieldCoordinate())
            context.cameraCenter = worldmap::TWorldMapSubfieldCoordinate(center->subfieldX(), center->subfieldY(), center->z());
        context.cameraZoom = m_houseCamera->scaleFactor();
    }
    return context;
}

void THousesInfoDialogController::restoreContext(const TCyclopediaDialogContext& CyclopediaContext)
{
    const auto& context = cyclopediaContextAs<THousesInfoCyclopediaContext>(CyclopediaContext);
    setDisableQmlSelect(true);
    setTownFilter(context.town);
    m_proxy->setStateFilter(context.state);
    m_proxy->setHouseTypeFilter(context.type);
    m_proxy->setSortOrder(context.sortOrder);
    selectHouse(context.houseId);
    // The preview camera where it was, after the house selection centred it.
    if (auto* viewport = qobject_cast<qmlcomponents::TWorldMapCameraViewport*>(m_houseCamera.data())) {
        viewport->centerAtSubfield(context.cameraCenter.subfieldX, context.cameraCenter.subfieldY, context.cameraCenter.z);
        viewport->setScaleFactor(context.cameraZoom);
    }
    setDisableQmlSelect(false);
}

void THousesInfoDialogController::setDisableQmlSelect(const bool disable)
{
    if (m_disableQmlSelect == disable)
        return;
    m_disableQmlSelect = disable;
    Q_EMIT disableQmlSelectChanged();
}

int THousesInfoDialogController::townFilterCurrentIndex() const
{
    return std::max<int>(0, static_cast<int>(m_townFilterModel.indexOf(m_proxy->townFilter())));
}

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

int THousesInfoDialogController::sortOrderCurrentIndex() const
{
    for (int i = 0; i < m_sortOrderModel.size(); ++i) {
        if (static_cast<int>(static_cast<THousSortOption*>(m_sortOrderModel.at(i))->sortOrder()) == m_proxy->sortOrder())
            return i;
    }
    return 0;
}

uint THousesInfoDialogController::currentHouseId() const
{
    return m_proxy->rowCount() > 0 ? m_currentHouseId : 0;
}

QDateTime THousesInfoDialogController::lastServerSave() const { return game::lastServerSave(); }

QDateTime THousesInfoDialogController::in30DaysServerSave() const { return game::lastServerSave().addDays(30); }

/// CIP is empty; product selects a town/house so the 3D preview has a row.
void THousesInfoDialogController::showDialogTab()
{
    ensureTownAndHouseSelection();
    // One refresh when the tab opens. The house-list reply must not start another.
    if (!m_proxy->townFilter().isEmpty())
        m_townRequestTimer.start();
}

void THousesInfoDialogController::hideDialogTab()
{
    m_proxy->clearFilters();
    m_storage->reset();
    m_currentHouseId = 0;
    Q_EMIT currentHouseIdChanged();
}

void THousesInfoDialogController::onHouseInformationChanged()
{
    m_proxy->changeFilter([this] { m_model->setHouses(m_storage->houses()); });
    ensureTownAndHouseSelection();
    Q_EMIT currentHouseIdChanged();
}

void THousesInfoDialogController::onCharacterHousesInfoChanged() { m_model->setCharacterInfo(m_storage->characterInfo()); }

void THousesInfoDialogController::onHousesLimitsChanged() { m_model->setLimits(m_storage->limits()); }

void THousesInfoDialogController::requestDynamicDataForTown() { m_townRequestTimer.start(); }

void THousesInfoDialogController::onRequestDynamicDataForTownTimerTimeout() { m_storage->requestTown(m_proxy->townFilter()); }

void THousesInfoDialogController::setTownFilter(const QString& Town)
{
    m_townRequestTimer.start();
    m_proxy->setTownFilter(Town);
}

void THousesInfoDialogController::setStateFilter(const int State) { m_proxy->setStateFilter(State); }

void THousesInfoDialogController::setHouseTypeFilter(const int HouseType) { m_proxy->setHouseTypeFilter(HouseType); }

void THousesInfoDialogController::setSortOrder(const int SortOrder) { m_proxy->setSortOrder(SortOrder); }

void THousesInfoDialogController::refresh()
{
    m_storage->reset();
    m_townRequestTimer.start();
}

void THousesInfoDialogController::setHouseWorldMapQuickItem(cip::tibia::qmlcomponents::TWorldMapQuickItem* pWorldMapQuickItem)
{
    m_houseWorldMap = pWorldMapQuickItem;
    onHouseViewportChanged();
}

void THousesInfoDialogController::setHouseCameraViewport(cip::tibia::qmlcomponents::TWorldMapCameraViewportQmlType* pCameraViewport)
{
    m_houseCamera = pCameraViewport;
    onHouseViewportChanged();
}

void THousesInfoDialogController::selectHouse(const uint HouseId)
{
    if (m_currentHouseId == HouseId)
        return;
    m_currentHouseId = HouseId;
    updateHouseLayers(HouseId);
    onHouseViewportChanged();
    Q_EMIT currentHouseIdChanged();
}

void THousesInfoDialogController::updateHouseLayers(const uint houseId)
{
    const THouseLayers houseLayers = houseLayersOf(houseId);
    m_minimumHouseLayer = houseLayers.minimum;
    m_maximumHouseLayer = houseLayers.maximum;
    Q_EMIT restrictingLayersChanged();
}

/// CIP: OPEN_CYCLOPEDIA's map tab at the house.
void THousesInfoDialogController::showOnCyclopediaMap()
{
    const THouseInformation* house = m_storage->house(m_currentHouseId);
    if (!house)
        return;
    auto action = std::make_shared<input::TGameActionOpenCyclopedia>(4);
    action->setCoordinate(Position(house->x, house->y, static_cast<uint8_t>(house->z)));
    action->setZoom(0.5f);
    Q_EMIT publishGameAction(action);
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
        onHouseViewportChanged();
}

void THousesInfoDialogController::centerHouse() { onHouseViewportChanged(); }

void THousesInfoDialogController::onHouseViewportChanged()
{
    const THouseInformation* house = m_storage->house(m_currentHouseId);
    if (!house || !m_houseCamera)
        return;
    if (auto* viewport = qobject_cast<qmlcomponents::TWorldMapCameraViewport*>(m_houseCamera.data())) {
        // CIP: a house under ground shows layers 8..15, else 0..7.
        const bool underground = house->z > 7;
        viewport->setTopAndBottomLayer(underground ? 8 : 0, underground ? 15 : 7);
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

void THousesInfoDialogController::onActionBidClicked(const qulonglong Limit)
{
    const THouseInformation* house = m_storage->house(m_currentHouseId);
    if (!house)
        return;
    m_pendingHouseId = m_currentHouseId;
    m_pendingAmount = Limit;
    confirmAction(1, qtTrId("houses_action_confirm_bid_message").arg(house->name, englishNumber(Limit), shortRent(house->rent)));
}

void THousesInfoDialogController::onActionMoveOutClicked(const QDate& Date)
{
    const THouseInformation* house = m_storage->house(m_currentHouseId);
    if (!house)
        return;
    m_pendingHouseId = m_currentHouseId;
    m_pendingTimestamp = static_cast<quint32>(game::serverSave(Date).toSecsSinceEpoch());
    confirmAction(2, qtTrId("houses_action_confirm_move_out_message").arg(house->name, dateBerlin(m_pendingTimestamp)));
}

void THousesInfoDialogController::onActionTransferClicked(const QDate& Date, const QString& NewOwner, const qulonglong Price)
{
    const THouseInformation* house = m_storage->house(m_currentHouseId);
    if (!house)
        return;
    m_pendingHouseId = m_currentHouseId;
    m_pendingTimestamp = static_cast<quint32>(game::serverSave(Date).toSecsSinceEpoch());
    m_pendingName = NewOwner;
    m_pendingAmount = Price;
    confirmAction(3, qtTrId("houses_action_confirm_transfer_message")
                         .arg(house->name, NewOwner, dateBerlin(m_pendingTimestamp), englishNumber(Price)));
}

void THousesInfoDialogController::onActionCancelMoveOutClicked()
{
    const THouseInformation* house = m_storage->house(m_currentHouseId);
    if (!house)
        return;
    m_pendingHouseId = m_currentHouseId;
    confirmAction(4, qtTrId("houses_action_confirm_cancel_move_out_message").arg(house->name, dateBerlin(house->moveOutDate)));
}

void THousesInfoDialogController::onActionCancelTransferClicked()
{
    const THouseInformation* house = m_storage->house(m_currentHouseId);
    if (!house)
        return;
    m_pendingHouseId = m_currentHouseId;
    confirmAction(5, qtTrId("houses_action_confirm_cancel_transfer_message")
                         .arg(house->name, dateBerlin(house->transferDate), house->newOwnerName));
}

void THousesInfoDialogController::onActionAcceptTransferClicked()
{
    const THouseInformation* house = m_storage->house(m_currentHouseId);
    if (!house)
        return;
    m_pendingHouseId = m_currentHouseId;
    confirmAction(6, qtTrId("houses_action_confirm_accept_transfer_message")
                         .arg(house->name, house->renterName, dateBerlin(house->transferDate), englishNumber(house->transferPrice)));
}

void THousesInfoDialogController::onActionRejectTransferClicked()
{
    const THouseInformation* house = m_storage->house(m_currentHouseId);
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

void THousesInfoDialogController::onRequestHouseActionResultDialog(const quint32 houseId, const int action, const int errorCode, const bool hasBeenOutbidden)
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
            const THouseInformation* house = m_storage->house(houseId);
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
