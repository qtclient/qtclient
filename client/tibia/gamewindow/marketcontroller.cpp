#include "marketcontroller.h"

#include "app/clientoptions.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/texthelper.h"

#include <client/const.h>
#include <tibia/world/game.h>
#include <client/item.h>
#include <tibia/world/localplayer.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QJsonValue>
#include <QLocale>
#include <QQuickItem>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

namespace tibia::gamewindow {

namespace {

using market::TMarketOffer;
using market::TMarketStatistics;

/// CIP DAT_1077b08f6: the Tibia coin type the market trades.
constexpr uint16_t TibiaCoinTypeId = 22118;
/// CIP 0x100e9d780: the categories with a store deeplink.
constexpr int PotionsCategory = 10;
constexpr int RunesCategory = 12;
constexpr uint64_t MaximumTotalPrice = 999999999999ULL;
const QLatin1StringView ShowLockerOnlyKey("marketShowLockerOnly");

/// CIP 0x100d5a820's order.
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

/// CIP 0x101018d20: 2.5% of the total, 20 to 2,500,000 gold.
uint64_t marketFee(const uint64_t amount, const uint64_t price)
{
    const auto fee = static_cast<uint64_t>(std::ceil(static_cast<double>(amount * price) * 0.025));
    return std::max<uint64_t>(20, std::min<uint64_t>(fee, 2500000));
}

/// CIP 0x101018da0: the most pieces whose fee `gold` covers.
uint64_t maximumAmountByFee(const uint64_t gold, const uint64_t price)
{
    if (gold < 20 || price == 0)
        return 0;
    if (gold >= 2500000)
        return 0xffffffff;
    return static_cast<uint64_t>(static_cast<double>(gold * 100) / (static_cast<double>(price) * 2.5));
}

uint64_t playerGold()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player ? player->getTotalMoney() : 0;
}

QString typeName(const uint16_t typeId)
{
    const ThingTypePtr& type = g_things.getThingType(typeId, ThingCategoryItem);
    if (!type)
        return {};
    const std::string& marketName = type->getMarketData().name;
    return QString::fromStdString(marketName.empty() ? type->getName() : marketName);
}

int offerType(const QString& offerTypeString)
{
    return offerTypeString == QLatin1String("sell") ? Otc::MARKETACTION_SELL : Otc::MARKETACTION_BUY;
}

/// CIP 0x100ea5590.
void appendStatistics(QStringList& lines, const TMarketStatistics& statistics)
{
    const QString indent = QStringLiteral("  ");
    const uint64_t average = statistics.transactions == 0 ? 0 : statistics.total / statistics.transactions;
    lines << indent + qtTrId("market_stats_number_of_transactions").arg(QString::number(statistics.transactions));
    lines << indent + qtTrId("market_stats_highest_price").arg(grouped(statistics.highest));
    lines << indent + qtTrId("market_stats_average_price").arg(grouped(average));
    lines << indent + qtTrId("market_stats_lowest_price").arg(grouped(statistics.lowest));
}

/// CIP 0x100ea42c0 joined by 0x100e9e950.
QString detailsText(const market::TMarketItemDetails& details)
{
    struct Line
    {
        int attribute;
        const char* id;
    };
    static constexpr std::array<Line, 26> Lines{ {
        { Otc::ITEM_DESC_ARMOR, "market_stats_armor" },
        { Otc::ITEM_DESC_ATTACK, "market_stats_attack" },
        { Otc::ITEM_DESC_CONTAINTER, "market_stats_capacity" },
        { Otc::ITEM_DESC_DEFENSE, "market_stats_defence" },
        { Otc::ITEM_DESC_GENERAL, "market_stats_description" },
        { Otc::ITEM_DESC_DECAY_TIME, "market_stats_expires_after" },
        { Otc::ITEM_DESC_COMBAT, "market_stats_protection" },
        { Otc::ITEM_DESC_MINLEVEL, "market_stats_minimum_level" },
        { Otc::ITEM_DESC_MINMAGICLEVEL, "market_stats_minimum_magic_level" },
        { Otc::ITEM_DESC_VOCATION, "market_stats_vocations" },
        { Otc::ITEM_DESC_RUNE, "market_stats_spell" },
        { Otc::ITEM_DESC_ABILITY, "market_stats_skill_boost" },
        { Otc::ITEM_DESC_CHARGES, "market_stats_charges" },
        { Otc::ITEM_DESC_WEAPONTYPE, "market_stats_weapon_type" },
        { Otc::ITEM_DESC_WEIGHT, "market_stats_weight" },
        { Otc::ITEM_DESC_AUGMENT, "market_stats_augments" },
        { Otc::ITEM_DESC_IMBUINGSLOTS, "market_stats_imbuement_slots" },
        { Otc::ITEM_DESC_MAGICSHIELD, "market_stats_magic_shield_capacity" },
        { Otc::ITEM_DESC_CLEAVE, "market_stats_cleave" },
        { Otc::ITEM_DESC_REFLECTION, "market_stats_damage_reflection" },
        { Otc::ITEM_DESC_PERFECT, "market_stats_maximum_range_bonus" },
        { Otc::ITEM_DESC_ELEMENTALBOND, "market_elemental_bond" },
        { Otc::ITEM_DESC_MANTRA, "market_mantra" },
        { Otc::ITEM_DESC_IMBUEMENTEFFECT, "market_imbuement_effect" },
        { Otc::ITEM_DESC_UPGRADECLASS, "market_stats_classification" },
        { Otc::ITEM_DESC_CURRENTTIER, "market_stats_tier" },
    } };
    QStringList lines;
    for (const Line& line : Lines) {
        const auto it = details.descriptions.find(line.attribute);
        if (it == details.descriptions.end() || it->second.isEmpty())
            continue;
        QString text = qtTrId(line.id).arg(it->second);
        if (line.attribute == Otc::ITEM_DESC_GENERAL)
            text += u'.';
        lines << text;
    }
    return lines.join(u'\n');
}

/// CIP 0x100ea5410 joined by 0x100e9ea30.
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
    m_itemNameWithTier = offer.tier >= 1 ? qtTrId("market_name_with_tier").arg(itemName).arg(offer.tier) : itemName;
    m_amountText = multiString(offer.amount);
    m_piecePriceText = multiString(static_cast<qlonglong>(offer.price));
    m_totalPriceText = multiString(static_cast<qlonglong>(offer.amount * offer.price));
    m_endsAtTime = textHelper().formatDateTime(static_cast<int>(offer.timestamp), true);
    switch (offer.state) {
    case Otc::OFFER_STATE_ACTIVE:
        m_auctionStatus = qtTrId("market_status_active");
        break;
    case Otc::OFFER_STATE_CANCELLED:
        m_auctionStatus = qtTrId("market_status_canceled");
        break;
    case Otc::OFFER_STATE_EXPIRED:
        m_auctionStatus = qtTrId("expired");
        break;
    case Otc::OFFER_STATE_ACCEPTED:
        m_auctionStatus = qtTrId(offer.type == Otc::MARKETACTION_BUY ? "market_status_bought" : "market_status_sold");
        break;
    default:
        m_auctionStatus = QStringLiteral("unknown");
        break;
    }

    uint64_t maximum = 0;
    if (offer.type == Otc::MARKETACTION_SELL)
        maximum = offer.price == 0 ? 0 : gold / offer.price;
    else
        maximum = static_cast<uint64_t>(std::max(depotCount, 0));
    m_maximumAcceptAmount = static_cast<int>(std::min<uint64_t>(maximum, offer.amount));

    const TMarketStatistics& sell = details.sell;
    if (details.typeId == offer.typeId && sell.transactions > 9 && sell.days > 4) {
        const uint64_t average = sell.total / sell.transactions;
        if (average != 0) {
            m_isDubious = offer.type != Otc::MARKETACTION_BUY ? offer.price >= average + average / 4
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
    return TAppearanceTypeListModelSortFilterProxy::isHandWeaponCategory(m_categoryIdentifier);
}

TMarketController::TMarketController(market::TMarketStorage* storage, QObject* parent)
    : TDialogController(QStringLiteral("market.qml"), parent)
    , m_storage(storage)
    , m_model([storage](const uint16_t typeId, const uint8_t tier) { return storage ? storage->objectCount(typeId, tier) : 0; })
    , m_proxy(&m_model)
{
    m_appearanceTypeListModel = QVariant::fromValue<QObject*>(&m_proxy);
    for (const int identifier : CategoryOrder)
        m_categories << new TQmlObjectCategory(identifier, this);
    m_indexOfSelectedTier = -1;
    m_indexOfSelectedClassification = -1;
    m_showLockerOnly = app::ClientOptions::load().options().value(ShowLockerOnlyKey).toBool();
    updateFilterLists();

    connect(storage, &market::TMarketStorage::requestOpenMarketDialog, this, &TMarketController::showDialog);
    connect(storage, &market::TMarketStorage::requestCloseMarketDialog, this, &TMarketController::closeDialog);
    connect(storage, &market::TMarketStorage::requestMarketMessageDialog, this, &TMarketController::showMarketMessage);
    connect(storage, &market::TMarketStorage::itemDetailsChanged, this, &TMarketController::onItemDetailsChanged);
    connect(storage, &market::TMarketStorage::offersChanged, this, &TMarketController::onOffersChanged);
    connect(storage, &market::TMarketStorage::historyChanged, this, &TMarketController::onHistoryChanged);
    connect(storage, &market::TMarketStorage::ownOffersChanged, this, &TMarketController::onOwnOffersChanged);
    connect(storage, &market::TMarketStorage::activeOfferCountChanged, this, &TMarketController::onActiveOfferCountChanged);
    connect(storage, &market::TMarketStorage::objectCountsChanged, &m_model, &TAppearanceTypeListModel::onObjectCountsChanged);
    connect(storage, &market::TMarketStorage::classificationsChanged, this, [this] {
        m_entriesBuilt = false;
        if (dialog()->item())
            rebuildEntries();
        updateFilterLists();
    });

    // CIP 0x100ea9900: browse the object's type, with its tier when it has one.
    input::setActionHandler(input::EGameAction::MARKET_BROWSE, [this](const input::TGameAction& action) {
        if (!m_storage)
            return false;
        uint16_t typeId = static_cast<uint16_t>(action.parameter);
        uint8_t tier = static_cast<uint8_t>(action.instance);
        if (action.object && action.object->isItem()) {
            const ItemPtr item = action.object->static_self_cast<Item>();
            typeId = static_cast<uint16_t>(item->getId());
            const ThingTypePtr& type = g_things.getThingType(typeId, ThingCategoryItem);
            tier = type && type->getClassification() > 0 ? item->getTier() : 0;
        }
        if (typeId == 0)
            return false;
        m_storage->browseItem(typeId, tier);
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
    for (const ThingTypePtr& type : g_things.getThingTypes(ThingCategoryItem)) {
        if (!type || type->getId() == 0 || !type->isMarketable())
            continue;
        const MarketData& market = type->getMarketData();
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
    m_model.reset(std::move(entries));
    m_proxy.invalidate();
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
    if (m_selectedType == 0 || !m_storage)
        return;
    const ThingTypePtr& type = g_things.getThingType(m_selectedType, ThingCategoryItem);
    if (type && type->getClassification() > 0) {
        m_hasSelectedTier = true;
        m_selectedTier = static_cast<uint8_t>(QmlUpgradeTier);
    }
    m_storage->browseItem(m_selectedType, selectedTier());
    setItemProperty("tradePacketSize", m_selectedType == TibiaCoinTypeId ? m_storage->coinsPacketSize() : 1);
}

void TMarketController::onMarketOfferCancel(const int OfferTimestamp, const int OfferIdentifier)
{
    g_game.cancelMarketOffer(static_cast<uint32_t>(OfferTimestamp), static_cast<uint16_t>(OfferIdentifier));
}

void TMarketController::onMarketOfferAccept(const int OfferTimestamp, const int OfferIdentifier, const int Amount)
{
    g_game.acceptMarketOffer(static_cast<uint32_t>(OfferTimestamp), static_cast<uint16_t>(OfferIdentifier), static_cast<uint16_t>(Amount));
}

void TMarketController::onMarketOfferMaximumAmountRequest(const QString& OfferTypeString, const qlonglong PiecePrice)
{
    int maximum = 0;
    if (m_storage && m_selectedType != 0) {
        const uint64_t price = PiecePrice > 0 ? static_cast<uint64_t>(PiecePrice) : 0;
        uint64_t limit = static_cast<uint64_t>(market::TMarketStorage::maximumOfferAmount(m_selectedType));
        if (price > 0)
            limit = std::min(limit, MaximumTotalPrice / price);
        const uint64_t gold = playerGold();
        if (offerType(OfferTypeString) == Otc::MARKETACTION_SELL) {
            limit = std::min<uint64_t>(limit, static_cast<uint64_t>(std::max(m_storage->objectCount(m_selectedType, selectedTier()), 0)));
            limit = std::min(limit, maximumAmountByFee(gold, price));
        } else {
            uint64_t low = 0;
            uint64_t high = limit;
            while (low < high) {
                const uint64_t mid = (low + high + 1) / 2;
                if (mid * price + marketFee(mid, price) <= gold)
                    low = mid;
                else
                    high = mid - 1;
            }
            limit = low;
        }
        maximum = static_cast<int>(limit);
    }
    setItemProperty("maximumOfferCreationAmount", maximum);
}

void TMarketController::onMarketOfferCreate(const QString& OfferTypeString, const int Amount, const qlonglong PiecePrice,
                                            const bool CreateAnonymousOffer)
{
    if (m_selectedType == 0 || Amount <= 0 || PiecePrice <= 0)
        return;
    g_game.createMarketOffer(static_cast<uint8_t>(offerType(OfferTypeString)), m_selectedType, selectedTier(), static_cast<uint16_t>(Amount),
                             static_cast<uint64_t>(PiecePrice), CreateAnonymousOffer ? 1 : 0);
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
    if (m_storage)
        m_storage->browseHistory();
}

void TMarketController::onMarketOwnOffersOpened()
{
    if (m_storage)
        m_storage->browseOwnOffers();
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
    if (m_selectedCategory != RunesCategory && m_selectedCategory != PotionsCategory)
        return;
    close(true);
    if (m_openStore)
        m_openStore();
}

QString TMarketController::getPriceOrProfitMultistring(const int Amount, const qlonglong PiecePrice)
{
    return multiString(static_cast<qlonglong>(Amount) * PiecePrice);
}

QString TMarketController::getMarketFeeMultistring(const int Amount, const qlonglong PiecePrice)
{
    if (PiecePrice == 0)
        return multiString(0);
    return multiString(static_cast<qlonglong>(marketFee(std::max(Amount, 1), static_cast<uint64_t>(PiecePrice))));
}

QString TMarketController::getTotalPriceOrProfitMultistring(const QString& OfferTypeString, const int Amount, const qlonglong PiecePrice)
{
    qlonglong total = static_cast<qlonglong>(Amount) * PiecePrice;
    const qlonglong fee = PiecePrice == 0 ? 0 : static_cast<qlonglong>(marketFee(std::max(Amount, 1), static_cast<uint64_t>(PiecePrice)));
    total = offerType(OfferTypeString) == Otc::MARKETACTION_BUY ? total + fee : total - fee;
    return multiString(total);
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
    if (leave && m_storage)
        m_storage->leave();
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
        m_hasSelectedTier = details.hasTier;
        m_selectedTier = details.tier;
        m_proxy.setTypeFilter(m_selectedType);
        row = m_proxy.rowOf(m_selectedType, selectedTier());
        Q_EMIT itemListSelectionRequest(0);
    }
    setItemProperty("itemDetailsModel", detailsText(details));
    setItemProperty("itemStatisticsModel", statisticsText(details));
    if (row != -1)
        QMetaObject::invokeMethod(dialog()->item(), "setIndexForItemView", Q_ARG(QVariant, row));
}

void TMarketController::setOfferLists(const char* buyProperty, const char* sellProperty, const std::vector<TMarketOffer>& offers,
                                      std::vector<QObject*>& owned)
{
    if (!dialog()->item() || !m_storage)
        return;
    const uint64_t gold = playerGold();
    const market::TMarketItemDetails& details = m_storage->itemDetails();
    QList<QObject*> buy;
    QList<QObject*> sell;
    std::vector<QObject*> created;
    for (const TMarketOffer& offer : offers) {
        auto* display = new TDisplayMarketOffer(offer, typeName(offer.typeId), gold, m_storage->objectCount(offer.typeId, offer.tier), details);
        created.push_back(display);
        (offer.type == Otc::MARKETACTION_BUY ? buy : sell) << display;
    }
    setItemProperty(buyProperty, QVariant::fromValue(buy));
    setItemProperty(sellProperty, QVariant::fromValue(sell));
    for (QObject* object : owned)
        object->deleteLater();
    owned = std::move(created);
}

void TMarketController::onOffersChanged()
{
    if (!m_storage)
        return;
    std::vector<TMarketOffer> shown;
    for (const TMarketOffer& offer : m_storage->offers()) {
        if (offer.typeId == m_selectedType)
            shown.push_back(offer);
    }
    setOfferLists("buyOffersModel", "sellOffersModel", shown, m_offerObjects);
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
