#include "marketcontroller.h"

#include "app/clientoptions.h"
#include "container/containerciptypes.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/texthelper.h"
#include "store/storestorage.h"

#include <client/const.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QJsonValue>
#include <QLocale>
#include <QQuickItem>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <stdexcept>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

using market::TMarketOffer;
using market::TMarketStatistics;

using market::TMarketStorage;
using market::EMarketOfferType;
using market::EMarketOfferState;
constexpr uint16_t TibiaCoinTypeId = TMarketStorage::TibiaCoinTypeId;
/// CIP : the categories with a store deeplink.
constexpr int PotionsCategory = 10;
constexpr int RunesCategory = 12;
const QLatin1StringView ShowLockerOnlyKey("marketShowLockerOnly");

/// CIP 's order.
constexpr std::array<int, 27> CategoryOrder{ 1, 2, 3, 4, 24, 5, 6, 7, 8, 9, 10, 25, 11, 12, 13, 26, 23, 14, 15, 16, 17, 18, 19, 27, 20, 21, 29 };

qmlcomponents::TTextHelperQmlService& textHelper()
{
    static qmlcomponents::TTextHelperQmlService helper;
    return helper;
}

QString multiString(const qlonglong value)
{
    return textHelper().formatNumberWithThousandSeparatorsAndThousandShortcutsAsMultiString(value);
}

QString grouped(const uint64_t value)
{
    return QLocale(QLocale::English).toString(static_cast<qulonglong>(value));
}

uint64_t playerGold()
{
    return tibia::game::hasLocalPlayer() ? tibia::game::localPlayerTotalMoney() : 0;
}

QString typeName(const uint16_t typeId)
{
    const ThingTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
    if (!type)
        return {};
    const std::string& marketName = type->getMarketData().name;
    return QString::fromStdString(marketName.empty() ? type->getName() : marketName);
}

/// "buy" or "sell"; anything else is a programming error.
int offerType(const QString& offerTypeString)
{
    if (offerTypeString == QLatin1String("buy"))
        return tibia::client::MARKETACTION_BUY;
    if (offerTypeString == QLatin1String("sell"))
        return tibia::client::MARKETACTION_SELL;
    throw std::invalid_argument(QStringLiteral("Invalid offer type %1").arg(offerTypeString).toStdString());
}

/// The fee of an offer: none without a price, and at least one piece's.
qlonglong offerFee(const int amount, const qlonglong piecePrice)
{
    if (piecePrice == 0)
        return 0;
    return static_cast<qlonglong>(TMarketStorage::marketFee(static_cast<uint32_t>(amount + (amount == 0 ? 1 : 0)), static_cast<uint64_t>(piecePrice)));
}

/// CIP .
void appendStatistics(QStringList& lines, const TMarketStatistics& statistics)
{
    const QString indent = QStringLiteral("  ");
    const auto average = statistics.transactions == 0
                             ? uint64_t{ 0 }
                             : static_cast<uint64_t>(std::floor(static_cast<double>(statistics.total) / statistics.transactions));
    lines << indent + qtTrId("market_stats_number_of_transactions").arg(QString::number(statistics.transactions));
    lines << indent + qtTrId("market_stats_highest_price").arg(grouped(statistics.highest));
    lines << indent + qtTrId("market_stats_average_price").arg(grouped(average));
    lines << indent + qtTrId("market_stats_lowest_price").arg(grouped(statistics.lowest));
}

/// CIP joined by .
QString detailsText(const market::TMarketItemDetails& details)
{
    struct Line
    {
        int attribute;
        const char* id;
    };
    static constexpr std::array<Line, 26> Lines{ {
        { tibia::client::ITEM_DESC_ARMOR, "market_stats_armor" },
        { tibia::client::ITEM_DESC_ATTACK, "market_stats_attack" },
        { tibia::client::ITEM_DESC_CONTAINTER, "market_stats_capacity" },
        { tibia::client::ITEM_DESC_DEFENSE, "market_stats_defence" },
        { tibia::client::ITEM_DESC_GENERAL, "market_stats_description" },
        { tibia::client::ITEM_DESC_DECAY_TIME, "market_stats_expires_after" },
        { tibia::client::ITEM_DESC_COMBAT, "market_stats_protection" },
        { tibia::client::ITEM_DESC_MINLEVEL, "market_stats_minimum_level" },
        { tibia::client::ITEM_DESC_MINMAGICLEVEL, "market_stats_minimum_magic_level" },
        { tibia::client::ITEM_DESC_VOCATION, "market_stats_vocations" },
        { tibia::client::ITEM_DESC_RUNE, "market_stats_spell" },
        { tibia::client::ITEM_DESC_ABILITY, "market_stats_skill_boost" },
        { tibia::client::ITEM_DESC_CHARGES, "market_stats_charges" },
        { tibia::client::ITEM_DESC_WEAPONTYPE, "market_stats_weapon_type" },
        { tibia::client::ITEM_DESC_WEIGHT, "market_stats_weight" },
        { tibia::client::ITEM_DESC_AUGMENT, "market_stats_augments" },
        { tibia::client::ITEM_DESC_IMBUINGSLOTS, "market_stats_imbuement_slots" },
        { tibia::client::ITEM_DESC_MAGICSHIELD, "market_stats_magic_shield_capacity" },
        { tibia::client::ITEM_DESC_CLEAVE, "market_stats_cleave" },
        { tibia::client::ITEM_DESC_REFLECTION, "market_stats_damage_reflection" },
        { tibia::client::ITEM_DESC_PERFECT, "market_stats_maximum_range_bonus" },
        { tibia::client::ITEM_DESC_ELEMENTALBOND, "market_elemental_bond" },
        { tibia::client::ITEM_DESC_MANTRA, "market_mantra" },
        { tibia::client::ITEM_DESC_IMBUEMENTEFFECT, "market_imbuement_effect" },
        { tibia::client::ITEM_DESC_UPGRADECLASS, "market_stats_classification" },
        { tibia::client::ITEM_DESC_CURRENTTIER, "market_stats_tier" },
    } };
    QStringList lines;
    for (const Line& line : Lines) {
        const QString& description = details.description(line.attribute);
        if (description.isEmpty())
            continue;
        QString text = qtTrId(line.id).arg(description);
        if (line.attribute == tibia::client::ITEM_DESC_GENERAL)
            text += u'.';
        lines << text;
    }
    return lines.join(u'\n');
}

/// CIP joined by .
QString statisticsText(const market::TMarketItemDetails& details)
{
    QStringList lines;
    lines << qtTrId("market_buy_offers_nocount");
    appendStatistics(lines, details.buy);
    lines << qtTrId("market_sell_offers_nocount");
    appendStatistics(lines, details.sell);
    return lines.join(u'\n');
}

}

TDisplayMarketOffer::TDisplayMarketOffer(const TMarketOffer& offer, const QString& itemName, const uint64_t gold, const int depotCount,
                                         const market::TMarketItemDetails& details, QObject* parent)
    : cip::tibia::market::TDisplayMarketOffer(parent)
    , m_price(offer.price)
{
    m_terminationTimestamp = static_cast<int>(offer.timestamp);
    m_terminationTimestampUniqueIdentifier = offer.counter;
    m_bidder = offer.bidder;
    m_itemName = itemName;
    m_itemNameWithTier = offer.tier.value_or(0) >= 1 ? qtTrId("market_name_with_tier").arg(itemName).arg(*offer.tier) : itemName;
    m_amountText = multiString(offer.amount);
    m_piecePriceText = multiString(static_cast<qlonglong>(offer.price));
    m_totalPriceText = multiString(static_cast<qlonglong>(offer.amount * offer.price));
    m_endsAtTime = textHelper().formatDateTime(static_cast<int>(offer.timestamp), true);
    switch (offer.state) {
    case EMarketOfferState::ACTIVE:
        m_auctionStatus = qtTrId("market_status_active");
        break;
    case EMarketOfferState::CANCELLED:
        m_auctionStatus = qtTrId("market_status_canceled");
        break;
    case EMarketOfferState::EXPIRED:
        m_auctionStatus = qtTrId("expired");
        break;
    case EMarketOfferState::ACCEPTED:
        m_auctionStatus = qtTrId(offer.type == EMarketOfferType::BUY ? "market_status_bought" : "market_status_sold");
        break;
    default:
        m_auctionStatus = QStringLiteral("unknown");
        break;
    }

    uint64_t maximum = 0;
    if (offer.type == EMarketOfferType::SELL)
        maximum = offer.price == 0 ? 0 : gold / offer.price;
    else
        maximum = static_cast<uint64_t>(std::max(depotCount, 0));
    m_maximumAcceptAmount = static_cast<int>(std::min<uint64_t>(maximum, offer.amount));

    const TMarketStatistics& sell = details.sell;
    if (details.typeId == offer.typeId && sell.transactions > 9 && sell.entries.size() > 4) {
        const uint64_t average = sell.total / sell.transactions;
        if (average != 0) {
            m_isDubious = offer.type != EMarketOfferType::BUY ? offer.price >= average + average / 4
                                                              : offer.price <= average - average / 4;
        }
    }
}

QString TDisplayMarketOffer::formatTotalValue(const int Amount)
{
    return multiString(static_cast<qlonglong>(Amount) * static_cast<qlonglong>(m_price));
}

TQmlObjectCategory::TQmlObjectCategory(const int identifier, QObject* parent)
    : cip::tibia::gamewindow::TQmlObjectCategory(parent)
{
    m_categoryIdentifier = identifier;
    m_text = translate(identifier);
}

QString TQmlObjectCategory::translate(const int identifier)
{
    static const std::array<const char*, 32> Ids{
        "market_category_unknown",
        "market_category_armors",
        "market_category_amulets",
        "market_category_boots",
        "market_category_containers",
        "market_category_decoration",
        "market_category_food",
        "market_category_helmets",
        "market_category_legs",
        "market_category_others",
        "market_category_potions",
        "market_category_rings",
        "market_category_runes",
        "market_category_shields",
        "market_category_tools",
        "market_category_valuables",
        "market_category_ammo",
        "market_category_axes",
        "market_category_clubs",
        "market_category_distanceweapons",
        "market_category_swords",
        "market_category_wands",
        "market_category_premiumscrolls",
        "market_category_tibiacoins",
        "market_category_creature_products",
        "market_category_quiver",
        "market_category_soul_cores",
        "market_category_fist_weapons",
        "market_category_unsorted",
        "market_category_all_weapons",
        "market_category_gold",
        "market_category_default",
    };
    if (identifier == TAppearanceTypeListModelSortFilterProxy::AllCategories)
        return qtTrId("market_category_all");
    if (identifier < 0 || identifier >= static_cast<int>(Ids.size()))
        return qtTrId("market_category_unknown");
    return qtTrId(Ids[identifier]);
}

bool TQmlObjectCategory::isHandWeaponCategory()
{
    // Category 0x10 is a hand-weapon category of the list filter but not a
    // weapon tab of its own.
    return TAppearanceTypeListModelSortFilterProxy::isHandWeaponCategory(m_categoryIdentifier) && m_categoryIdentifier != 0x10;
}

TMarketController::TMarketController(market::TMarketStorage* storage, market::TMarketProtocolMessageHandler* messages, QObject* parent)
    : TDialogController(QStringLiteral("market.qml"), parent)
    , m_storage(storage)
    , m_messages(messages)
    , m_model([storage](const uint16_t typeId, const uint8_t tier) { return storage ? static_cast<int>(storage->count(typeId, tier)) : 0; })
    , m_proxy(&m_model)
{
    m_appearanceTypeListModel = QVariant::fromValue<QObject*>(&m_proxy);
    for (const int identifier : CategoryOrder)
        m_categories << new TQmlObjectCategory(identifier, this);
    m_indexOfSelectedTier = -1;
    m_indexOfSelectedClassification = -1;
    m_showLockerOnly = app::ClientOptions::load().options().value(ShowLockerOnlyKey).toBool();
    updateFilterLists();

    connect(messages, &market::TMarketProtocolMessageHandler::requestOpenMarketDialog, this, &TMarketController::showDialog);
    connect(messages, &market::TMarketProtocolMessageHandler::requestCloseMarketDialog, this, &TMarketController::closeDialog);
    connect(messages, &market::TMarketProtocolMessageHandler::requestMarketMessageDialog, this, &TMarketController::showMarketMessage);
    connect(storage, &market::TMarketStorage::marketItemDetailsChanged, this, &TMarketController::onItemDetailsChanged);
    connect(storage, &market::TMarketStorage::marketOffersChanged, this, &TMarketController::onOffersChanged);
    connect(storage, &market::TMarketStorage::marketHistoryChanged, this, &TMarketController::onHistoryChanged);
    connect(storage, &market::TMarketStorage::marketOwnOffersChanged, this, &TMarketController::onOwnOffersChanged);
    connect(storage, &market::TMarketStorage::activeOfferCountChanged, this, &TMarketController::onActiveOfferCountChanged);
    connect(storage, &market::TMarketStorage::objectCountsChanged, &m_model, &TAppearanceTypeListModel::onObjectCountsChanged);
    connect(storage, &market::TMarketStorage::classificationsChanged, this, [this] {
        m_entriesBuilt = false;
        if (dialog()->item())
            rebuildEntries();
        updateFilterLists();
    });

    // CIP : browse the object's type, with its tier when it has one.
    input::setActionHandler(input::EGameAction::MARKET_BROWSE, [this](const input::TGameAction& action) {
        if (!m_messages)
            return false;
        uint16_t typeId = static_cast<uint16_t>(action.parameter);
        uint8_t tier = static_cast<uint8_t>(action.instance);
        if (action.objectClientId) {
            typeId = action.objectClientId;
            uint8_t slotTier = 0;
            if (action.position.x == UINT16_MAX) {
                if (action.position.y & 0x40) {
                    if (container::TContainerStorage* storage = container::TContainerStorage::instance()) {
                        if (const container::TStoredContainer* row = storage->containerAt(action.position.y & 0x3F))
                            slotTier = storage->slotAt(row->id, static_cast<int>(row->firstIndex) + action.stackPosition).tier;
                    }
                } else if (container::TInventoryContainer* inventory = container::TInventoryContainer::instance()) {
                    slotTier = inventory->slotAt(action.position.y).tier;
                }
            }
            const ThingTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
            tier = type && type->getClassification() > 0 ? slotTier : 0;
        }
        if (typeId == 0)
            return false;
        const ThingTypePtr& browsed = tibia::game::thingType(typeId, ThingCategoryItem);
        m_messages->sendBrowseItem(typeId, browsed && browsed->getClassification() > 0 ? std::optional<uint8_t>(tier) : std::nullopt);
        return true;
    });
}

TMarketController::~TMarketController()
{
    if (m_message)
        m_message->dialog()->close();
    for (std::vector<QObject*>* owned : { &m_offerObjects, &m_historyObjects, &m_ownOfferObjects })
        qDeleteAll(*owned);
}

void TMarketController::setItemProperty(const char* name, const QVariant& value)
{
    if (QQuickItem* item = dialog()->item())
        item->setProperty(name, value);
}

void TMarketController::rebuildEntries()
{
    std::vector<TAppearanceTypeEntry> entries;
    const auto& classTiers = m_storage ? m_storage->classificationTiers() : std::map<int, std::vector<int>>{};
    for (const ThingTypePtr& type : tibia::appearances::TAppearancesManager::instance().getMarketObjectTypes()) {
        const MarketData& market = type->getMarketData();
        // An object traded as another object is listed under that one.
        if (market.tradeAs != type->getId())
            continue;
        TAppearanceTypeEntry entry;
        entry.typeId = type->getId();
        entry.name = QString::fromStdString(market.name.empty() ? type->getName() : market.name);
        entry.marketCategory = static_cast<int>(market.category);
        entry.classification = type->getClassification();
        entry.requiredLevel = market.requiredLevel;
        entry.professions = market.restrictVocation;
        entry.hasClothSlot = type->isCloth();
        entry.clothSlot = type->getClothSlot();
        if (entry.classification == 0) {
            entries.push_back(std::move(entry));
            continue;
        }
        const auto it = classTiers.find(entry.classification);
        if (it == classTiers.end() || it->second.empty()) {
            entry.hasTier = true;
            entries.push_back(std::move(entry));
            continue;
        }
        for (const int tier : it->second) {
            TAppearanceTypeEntry tiered = entry;
            tiered.hasTier = true;
            tiered.tier = static_cast<uint8_t>(tier);
            entries.push_back(std::move(tiered));
        }
    }
    m_proxy.resetEntries(std::move(entries));
    m_entriesBuilt = true;
}

bool TMarketController::categorySupportsTiers(const int category) const
{
    for (int row = 0; row < m_model.size(); ++row) {
        const TAppearanceTypeEntry& entry = m_model.entry(row);
        if (entry.classification == 0)
            continue;
        if (category == TAppearanceTypeListModelSortFilterProxy::AllCategories || entry.marketCategory == category)
            return true;
        if (category == TAppearanceTypeListModelSortFilterProxy::AllWeaponsCategory
            && TAppearanceTypeListModelSortFilterProxy::isHandWeaponCategory(entry.marketCategory))
            return true;
    }
    return false;
}

TAppearanceTypeFilter TMarketController::currentFilter() const
{
    TAppearanceTypeFilter filter;
    if (!m_enableTierAndClassificationSelection)
        return filter;
    std::vector<int> classes;
    std::set<int> allTiers;
    if (m_storage) {
        for (const auto& [classification, tiers] : m_storage->classificationTiers()) {
            classes.push_back(classification);
            allTiers.insert(tiers.begin(), tiers.end());
        }
    }
    if (m_indexOfSelectedClassification == 1) {
        filter.classificationMode = 1;
    } else if (m_indexOfSelectedClassification == 0) {
        filter.classificationMode = 2;
    } else if (m_indexOfSelectedClassification >= 2 && m_indexOfSelectedClassification - 2 < static_cast<int>(classes.size())) {
        filter.classificationMode = 3;
        filter.classification = classes[m_indexOfSelectedClassification - 2];
    }

    std::vector<int> tiers;
    if (filter.classificationMode == 2) {
        tiers.assign(allTiers.begin(), allTiers.end());
    } else if (filter.classificationMode == 3 && m_storage) {
        tiers = m_storage->classificationTiers().at(filter.classification);
    }
    if ((filter.classificationMode == 2 || filter.classificationMode == 3) && m_indexOfSelectedTier >= 0
        && m_indexOfSelectedTier < static_cast<int>(tiers.size())) {
        filter.tierMode = 3;
        filter.tier = tiers[m_indexOfSelectedTier];
    }
    return filter;
}

void TMarketController::updateFilterLists()
{
    QStringList classifications;
    QStringList tiers;
    if (!m_enableTierAndClassificationSelection || !m_storage) {
        classifications << QString();
        tiers << QString();
    } else {
        classifications << qtTrId("market_filter_class_all") << qtTrId("market_filter_class_none");
        std::set<int> allTiers;
        for (const auto& [classification, classTiers] : m_storage->classificationTiers()) {
            classifications << qtTrId("market_filter_class").arg(classification);
            allTiers.insert(classTiers.begin(), classTiers.end());
        }
        const TAppearanceTypeFilter filter = currentFilter();
        if (filter.classificationMode == 2) {
            for (const int tier : allTiers)
                tiers << qtTrId("market_filter_tier").arg(tier);
        } else if (filter.classificationMode == 3) {
            for (const int tier : m_storage->classificationTiers().at(filter.classification))
                tiers << qtTrId("market_filter_tier").arg(tier);
        }
        if (tiers.isEmpty())
            tiers << QString();
    }
    if (classifications != m_availableClassifications) {
        m_availableClassifications = classifications;
        Q_EMIT availableClassificationsChanged();
    }
    if (tiers != m_availableTiers) {
        m_availableTiers = tiers;
        Q_EMIT availableTiersChanged();
    }
}

void TMarketController::setEnableTierAndClassificationSelection(const bool enable)
{
    m_enableTierAndClassificationSelection = enable;
    Q_EMIT enableTierAndClassificationSelectionChanged();
}

void TMarketController::setSelectedCategory(const int category)
{
    if (m_selectedCategory == category)
        return;
    m_selectedCategory = category;
    m_selectedCategoryName = category == -1 ? QString() : TQmlObjectCategory::translate(category);
    m_showStoreDeeplink = category == RunesCategory || category == PotionsCategory;
    Q_EMIT selectedCategoryChanged();
}

void TMarketController::clearDetails()
{
    setItemProperty("buyOffersModel", QVariant::fromValue(QList<QObject*>{}));
    setItemProperty("sellOffersModel", QVariant::fromValue(QList<QObject*>{}));
    setItemProperty("itemDetailsModel", QString());
    setItemProperty("itemStatisticsModel", QString());
}

void TMarketController::reselect()
{
    m_proxy.invalidate();
    int row = -1;
    if (m_selectedType != 0)
        row = m_proxy.rowOf(m_selectedType, selectedTier());
    if (row == -1)
        onMarketCategoryItemSelected(0, 0);
    Q_EMIT itemListSelectionRequest(row);
}

void TMarketController::onMarketCategorySelected(const int CategoryIdentifier, const bool FilterByLevel, const bool FilterByVocation,
                                                 const bool OnlyOneHandWeapons, const bool OnlyTwoHandWeapons, const bool ShowLockerOnly)
{
    clearDetails();
    const bool changed = m_selectedCategory != CategoryIdentifier;
    if (changed) {
        m_selectedType = 0;
        m_hasSelectedTier = false;
        setItemProperty("selectedAppearanceTypeID", 0);
    }
    if (!categorySupportsTiers(CategoryIdentifier)) {
        setEnableTierAndClassificationSelection(false);
        setEnableTierSelection(false);
        setIndexOfSelectedClassification(-1);
        setIndexOfSelectedTier(-1);
    } else {
        setEnableTierAndClassificationSelection(true);
        if (changed) {
            setIndexOfSelectedClassification(0);
            setIndexOfSelectedTier(0);
            setEnableTierSelection(true);
        }
    }
    updateFilterLists();
    if (ShowLockerOnly != m_showLockerOnly) {
        m_showLockerOnly = ShowLockerOnly;
        app::ClientOptions::store(ShowLockerOnlyKey, ShowLockerOnly);
        Q_EMIT showLockerOnlyChanged();
    }
    m_proxy.setCategoryFilter(CategoryIdentifier, FilterByLevel, FilterByVocation, OnlyOneHandWeapons, OnlyTwoHandWeapons, currentFilter(),
                              ShowLockerOnly);
    m_proxy.applyFilterNow();
    reselect();
    setSelectedCategory(CategoryIdentifier);
}

void TMarketController::onMarketFilterSelected(const QString& NameFilter, const bool FilterByLevel, const bool FilterByVocation,
                                               const bool OnlyOneHandWeapons, const bool OnlyTwoHandWeapons, const bool ShowLockerOnly)
{
    clearDetails();
    if (NameFilter.isEmpty()) {
        onMarketCategoryItemSelected(0, 0);
    } else if (m_indexOfSelectedClassification < 0) {
        setEnableTierAndClassificationSelection(true);
        setEnableTierSelection(true);
        setIndexOfSelectedClassification(0);
        setIndexOfSelectedTier(0);
    }
    updateFilterLists();
    if (ShowLockerOnly != m_showLockerOnly) {
        m_showLockerOnly = ShowLockerOnly;
        app::ClientOptions::store(ShowLockerOnlyKey, ShowLockerOnly);
        Q_EMIT showLockerOnlyChanged();
    }
    m_proxy.setNameFilter(NameFilter, FilterByLevel, FilterByVocation, OnlyOneHandWeapons, OnlyTwoHandWeapons, currentFilter(), ShowLockerOnly);
    m_proxy.applyFilterNow();
    reselect();
    setSelectedCategory(-1);
}

void TMarketController::onMarketCategoryItemSelected(const int AppearanceTypeID, const int QmlUpgradeTier)
{
    if (!dialog()->item())
        return;
    m_selectedType = static_cast<uint16_t>(AppearanceTypeID);
    m_hasSelectedTier = false;
    setItemProperty("selectedAppearanceTypeID", AppearanceTypeID);
    if (!m_messages)
        return;
    const ThingTypePtr& type = tibia::game::thingType(m_selectedType, ThingCategoryItem);
    if (type && type->getClassification() > 0) {
        m_hasSelectedTier = true;
        m_selectedTier = static_cast<uint8_t>(QmlUpgradeTier);
    }
    if (m_messages)
        m_messages->sendBrowseItem(m_selectedType, m_hasSelectedTier ? std::optional<uint8_t>(m_selectedTier) : std::nullopt);
    // Tibia coins trade in the store's packets.
    setItemProperty("tradePacketSize", m_selectedType == TibiaCoinTypeId && m_storage ? m_storage->coinsPacketSize() : 1);
}

void TMarketController::onMarketOfferCancel(const int OfferTimestamp, const int OfferIdentifier)
{
    if (m_messages)
        m_messages->sendCancel(static_cast<uint32_t>(OfferTimestamp), static_cast<uint16_t>(OfferIdentifier));
}

void TMarketController::onMarketOfferAccept(const int OfferTimestamp, const int OfferIdentifier, const int Amount)
{
    if (m_messages)
        m_messages->sendAccept(static_cast<uint32_t>(OfferTimestamp), static_cast<uint16_t>(OfferIdentifier), static_cast<uint32_t>(Amount));
}

void TMarketController::onMarketOfferMaximumAmountRequest(const QString& OfferTypeString, const qlonglong PiecePrice)
{
    uint32_t maximum = 0;
    if (m_storage) {
        const auto type = offerType(OfferTypeString) == tibia::client::MARKETACTION_SELL ? EMarketOfferType::SELL : EMarketOfferType::BUY;
        maximum = m_storage->maximumOfferAmount(m_selectedType, m_hasSelectedTier ? std::optional<uint8_t>(m_selectedTier) : std::nullopt,
                                                type, PiecePrice > 0 ? static_cast<uint64_t>(PiecePrice) : 0);
    }
    setItemProperty("maximumOfferCreationAmount", static_cast<int>(maximum));
}

void TMarketController::onMarketOfferCreate(const QString& OfferTypeString, const int Amount, const qlonglong PiecePrice,
                                            const bool CreateAnonymousOffer)
{
    if (!m_messages)
        return;
    const auto type = offerType(OfferTypeString) == tibia::client::MARKETACTION_SELL ? EMarketOfferType::SELL : EMarketOfferType::BUY;
    m_messages->sendCreate(type, m_selectedType, m_hasSelectedTier ? std::optional<uint8_t>(m_selectedTier) : std::nullopt,
                           static_cast<uint32_t>(Amount), static_cast<uint64_t>(PiecePrice), CreateAnonymousOffer);
}

void TMarketController::onClassificationFilterWasSelected(const int SelectedIndex, const QString&, bool, bool, bool, bool, bool)
{
    setIndexOfSelectedClassification(SelectedIndex);
    const TAppearanceTypeFilter filter = currentFilter();
    if (filter.classificationMode == 2 || filter.classificationMode == 3) {
        setEnableTierSelection(true);
        setIndexOfSelectedTier(0);
    } else {
        setEnableTierSelection(false);
        setIndexOfSelectedTier(-1);
    }
    updateFilterLists();
    clearDetails();
}

void TMarketController::onTierFilterWasSelected(const int SelectedIndex, const QString&, bool, bool, bool, bool, bool)
{
    setIndexOfSelectedTier(SelectedIndex);
    updateFilterLists();
    clearDetails();
}

void TMarketController::onMarketHistoryOpened()
{
    if (m_messages)
        m_messages->sendBrowseHistory();
}

void TMarketController::onMarketOwnOffersOpened()
{
    if (m_messages)
        m_messages->sendBrowseOwnOffers();
}

void TMarketController::onMarketRefreshOffers()
{
    onMarketCategoryItemSelected(m_selectedType, selectedTier());
}

void TMarketController::onCloseButtonPressed()
{
    close(true);
}

void TMarketController::onGetCoinsButtonClicked()
{
    input::publish(input::makeAction(input::EGameAction::OPEN_URL_GET_COINS));
}

void TMarketController::onStoreDeeplinkClicked()
{
    // The store's boost page for runes (detail 4) or potions (detail 3).
    int detail = 0;
    if (m_selectedCategory == RunesCategory)
        detail = 4;
    else if (m_selectedCategory == PotionsCategory)
        detail = 3;
    else
        return;
    close(true);
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_STORE));
    action->parameter = store::OPEN_PREMIUM_OR_BOOSTS;
    action->instance = detail;
    input::publish(action);
}

QString TMarketController::getPriceOrProfitMultistring(const int Amount, const qlonglong PiecePrice)
{
    return multiString(static_cast<qlonglong>(Amount) * PiecePrice);
}

QString TMarketController::getMarketFeeMultistring(const int Amount, const qlonglong PiecePrice)
{
    return multiString(offerFee(Amount, PiecePrice));
}

QString TMarketController::getTotalPriceOrProfitMultistring(const QString& OfferTypeString, const int Amount, const qlonglong PiecePrice)
{
    const qlonglong total = static_cast<qlonglong>(Amount) * PiecePrice;
    if (offerType(OfferTypeString) == tibia::client::MARKETACTION_BUY)
        return multiString(total + offerFee(Amount, PiecePrice));
    return multiString(total - offerFee(Amount, PiecePrice));
}

void TMarketController::showDialog()
{
    if (!dialog()->item()) {
        if (!m_entriesBuilt)
            rebuildEntries();
        dialog()->create();
        dialog()->show();
        onActiveOfferCountChanged();
        return;
    }
    onOffersChanged();
}

void TMarketController::closeDialog()
{
    close(false);
}

void TMarketController::close(const bool leave)
{
    if (leave && m_messages)
        m_messages->sendLeave();
    for (const char* name : { "buyOffersModel", "sellOffersModel", "historicBuyOffersModel", "historicSellOffersModel", "ownBuyOffersModel",
                              "ownSellOffersModel" })
        setItemProperty(name, QVariant::fromValue(QList<QObject*>{}));
    dialog()->close();
    for (std::vector<QObject*>* owned : { &m_offerObjects, &m_historyObjects, &m_ownOfferObjects }) {
        for (QObject* object : *owned)
            object->deleteLater();
        owned->clear();
    }
    m_selectedType = 0;
    m_hasSelectedTier = false;
    m_selectedCategory = -1;
    setIndexOfSelectedClassification(-1);
    setIndexOfSelectedTier(-1);
    setEnableTierAndClassificationSelection(false);
    setEnableTierSelection(false);
    updateFilterLists();
}

void TMarketController::showMarketMessage(const QString& Message)
{
    if (!m_message) {
        m_message = new TMessageDialogController(this);
        m_message->setCloseOnButtonClicked(true);
    }
    m_message->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK, QKeySequence(QStringLiteral("Return"))) });
    m_message->showMessage(qtTrId("market_message_caption"), Message);
}

void TMarketController::onItemDetailsChanged()
{
    if (!dialog()->item() || !m_storage)
        return;
    const market::TMarketItemDetails& details = m_storage->itemDetails();
    int row = -1;
    if (m_proxy.rowCount() == 0) {
        m_selectedType = details.typeId;
        m_hasSelectedTier = details.tier.has_value();
        m_selectedTier = details.tier.value_or(0);
        m_proxy.setTypeFilter(m_selectedType);
        m_proxy.applyFilterNow();
        row = m_proxy.rowOf(m_selectedType, selectedTier());
        Q_EMIT itemListSelectionRequest(0);
    }
    setItemProperty("itemDetailsModel", detailsText(details));
    setItemProperty("itemStatisticsModel", statisticsText(details));
    if (row != -1)
        QMetaObject::invokeMethod(dialog()->item(), "setIndexForItemView", Q_ARG(QVariant, row));
}

void TMarketController::setOfferLists(const char* buyProperty, const char* sellProperty, const market::TMarketOfferList& offers,
                                      std::vector<QObject*>& owned)
{
    if (!dialog()->item() || !m_storage)
        return;
    // The views drop the old rows before the new ones are built.
    setItemProperty(buyProperty, QVariant::fromValue(QList<QObject*>{}));
    setItemProperty(sellProperty, QVariant::fromValue(QList<QObject*>{}));
    const uint64_t gold = playerGold();
    const market::TMarketItemDetails& details = m_storage->itemDetails();
    std::vector<QObject*> created;
    const auto display = [&](const std::vector<TMarketOffer>& list) {
        QList<QObject*> result;
        for (const TMarketOffer& offer : list) {
            const auto depotCount = static_cast<int>(offer.tier ? m_storage->count(offer.typeId, *offer.tier) : m_storage->count(offer.typeId));
            auto* row = new TDisplayMarketOffer(offer, typeName(offer.typeId), gold, depotCount, details);
            created.push_back(row);
            result << row;
        }
        return result;
    };
    setItemProperty(buyProperty, QVariant::fromValue(display(offers.buyOffers)));
    setItemProperty(sellProperty, QVariant::fromValue(display(offers.sellOffers)));
    for (QObject* object : owned)
        object->deleteLater();
    owned = std::move(created);
}

void TMarketController::onOffersChanged()
{
    if (m_storage)
        setOfferLists("buyOffersModel", "sellOffersModel", m_storage->offers(), m_offerObjects);
}

void TMarketController::onHistoryChanged()
{
    if (m_storage)
        setOfferLists("historicBuyOffersModel", "historicSellOffersModel", m_storage->history(), m_historyObjects);
}

void TMarketController::onOwnOffersChanged()
{
    if (m_storage)
        setOfferLists("ownBuyOffersModel", "ownSellOffersModel", m_storage->ownOffers(), m_ownOfferObjects);
}

void TMarketController::onActiveOfferCountChanged()
{
    if (m_storage)
        setItemProperty("canCreateNewOffer", m_storage->activeOfferCount() < market::TMarketStorage::MaximumActiveOffers);
}

}
