#include "marketcontroller.h"

#include "config/clientoptions.h"
#include "container/containerciptypes.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/texthelper.h"
#include "store/storestorage.h"

#include <client/const.h>
#include <tibia/appearances/appearancetype.h>
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
    const TAppearanceTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
    if (!type)
        return {};
    const auto* market = type->marketProperties();
    return market && !market->name.isEmpty() ? market->name : type->name();
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

/// The floored average price of the statistics (0 without transactions).
uint64_t averagePrice(const TMarketStatistics& statistics)
{
    if (statistics.transactions == 0)
        return 0;
    return static_cast<uint64_t>(std::floor(static_cast<double>(statistics.total) / statistics.transactions));
}

/// The days of the statistics that had transactions.
int daysWithTransactions(const TMarketStatistics& statistics)
{
    return static_cast<int>(std::count_if(statistics.entries.begin(), statistics.entries.end(),
                                          [](const market::TMarketTradeStatisticsEntry& entry) { return entry.transactions != 0; }));
}

/// Enough sales to judge prices: more than 9 transactions on more than 4 days.
bool hasReliableStatistics(const uint32_t transactions, const int days)
{
    return days > 4 && transactions > 9;
}

/// A sell offer at a quarter above the average, a buy offer at a quarter
/// below it.
bool isDubiousPrice(const TMarketOffer& offer, const uint64_t average)
{
    if (average == 0)
        return false;
    if (offer.type != EMarketOfferType::BUY)
        return average + (average >> 2) <= offer.price;
    return offer.price <= average - (average >> 2);
}

/// What the player can accept: the gold buys from a sell offer, the depot
/// sells to a buy offer; never more than the offer holds.
uint32_t maximumAcceptAmount(const TMarketOffer& offer, const uint64_t gold, uint32_t count)
{
    if (offer.type != EMarketOfferType::BUY)
        count = offer.price == 0 ? 0 : static_cast<uint32_t>(gold / offer.price);
    return std::min(count, offer.amount);
}

QString auctionStatus(const EMarketOfferType type, const EMarketOfferState state)
{
    switch (state) {
    case EMarketOfferState::ACTIVE:
        return qtTrId("market_status_active");
    case EMarketOfferState::CANCELLED:
        return qtTrId("market_status_canceled");
    case EMarketOfferState::EXPIRED:
        return qtTrId("expired");
    case EMarketOfferState::ACCEPTED:
        return qtTrId(type == EMarketOfferType::BUY ? "market_status_bought" : "market_status_sold");
    }
    return QStringLiteral("unknown");
}

/// CIP .
void appendStatistics(QStringList& lines, const TMarketStatistics& statistics)
{
    const QString indent = QStringLiteral("  ");
    const uint64_t average = averagePrice(statistics);
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

TDisplayMarketOffer::TDisplayMarketOffer(QObject* parent)
    : cip::tibia::market::TDisplayMarketOffer(parent)
{
}

QString TDisplayMarketOffer::itemNameWithTier() const
{
    if (m_tier < 1)
        return m_itemName;
    return qtTrId("market_name_with_tier").arg(m_itemName).arg(m_tier);
}

QString TDisplayMarketOffer::amountText() const
{
    return multiString(m_amount);
}

QString TDisplayMarketOffer::totalPriceText() const
{
    return multiString(static_cast<qlonglong>(m_amount) * static_cast<qlonglong>(m_piecePrice));
}

QString TDisplayMarketOffer::piecePriceText() const
{
    return multiString(static_cast<qlonglong>(m_piecePrice));
}

QString TDisplayMarketOffer::formatTotalValue(const int Amount)
{
    return multiString(static_cast<qlonglong>(Amount) * static_cast<qlonglong>(m_piecePrice));
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
    , m_message(new TMessageDialogController(this))
{
    m_appearanceTypeListModel = QVariant::fromValue<QObject*>(&m_proxy);
    for (const int identifier : CategoryOrder)
        m_categories << new TQmlObjectCategory(identifier, this);
    m_indexOfSelectedTier = -1;
    m_indexOfSelectedClassification = -1;
    config::TClientOptions* options = config::TClientOptions::instance();
    m_proxy.setShowLockerOnly(options->boolOption(config::EClientOption::marketShowLockerOnly));

    connect(m_message, &TMessageDialogController::acceptClicked, this, &TMarketController::onMessageDialogOkay);
    connect(options, &config::TClientOptions::marketOtpionsChanged, this, &TMarketController::onMarketOptionsChanged);
    connect(&m_proxy, &cip::tibia::gamewindow::TAppearanceTypeListModelSortFilterProxy::showLockerOnlyChanged, this,
            &TMarketController::onShowLockerOnlyChanged);
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
        if (dialog()->item())
            rebuildEntries();
        Q_EMIT availableClassificationsChanged();
        Q_EMIT availableTiersChanged();
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
            const TAppearanceTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
            tier = type && type->getClassification() > 0 ? slotTier : 0;
        }
        if (typeId == 0)
            return false;
        const TAppearanceTypePtr& browsed = tibia::game::thingType(typeId, ThingCategoryItem);
        m_messages->sendBrowseItem(typeId, browsed && browsed->getClassification() > 0 ? std::optional<uint8_t>(tier) : std::nullopt);
        return true;
    });
}

TMarketController::~TMarketController()
{
    m_message->dialog()->close();
    for (std::vector<QObject*>* owned :
         { &m_buyOffers, &m_sellOffers, &m_historicBuyOffers, &m_historicSellOffers, &m_ownBuyOffers, &m_ownSellOffers })
        qDeleteAll(*owned);
}

QString TMarketController::selectedCategoryName() const
{
    return TQmlObjectCategory::translate(m_selectedCategory);
}

bool TMarketController::showStoreDeeplink() const
{
    return m_selectedCategory == RunesCategory || m_selectedCategory == PotionsCategory;
}

bool TMarketController::showLockerOnly() const
{
    return m_proxy.showLockerOnly();
}

QStringList TMarketController::availableClassifications() const
{
    if (!m_enableTierAndClassificationSelection)
        return { QString() };
    QStringList classifications{ qtTrId("market_filter_class_all"), qtTrId("market_filter_class_none") };
    if (m_storage) {
        for (const auto& entry : m_storage->classificationTiers())
            classifications << qtTrId("market_filter_class").arg(entry.first);
    }
    return classifications;
}

QStringList TMarketController::availableTiers() const
{
    if (!m_enableTierSelection || !m_storage)
        return { QString() };
    QStringList tiers;
    const TAppearanceTypeFilter filter = currentFilter();
    if (filter.classificationMode == 2) {
        std::set<int> allTiers;
        for (const auto& [classification, classTiers] : m_storage->classificationTiers())
            allTiers.insert(classTiers.begin(), classTiers.end());
        for (const int tier : allTiers)
            tiers << qtTrId("market_filter_tier").arg(tier);
    } else if (filter.classificationMode == 3) {
        for (const int tier : m_storage->classificationTiers().at(filter.classification))
            tiers << qtTrId("market_filter_tier").arg(tier);
    }
    return tiers;
}

void TMarketController::setEnableTierSelection(const bool v)
{
    m_enableTierSelection = v;
    Q_EMIT enableTierSelectionChanged();
}

void TMarketController::setIndexOfSelectedTier(const int v)
{
    m_indexOfSelectedTier = v;
    Q_EMIT indexOfSelectedTierChanged();
}

void TMarketController::setIndexOfSelectedClassification(const int v)
{
    m_indexOfSelectedClassification = v;
    Q_EMIT indexOfSelectedClassificationChanged();
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
    for (const TAppearanceTypePtr& type : tibia::appearances::TAppearancesManager::instance().getMarketObjectTypes()) {
        const auto& market = *type->marketProperties();
        TAppearanceTypeEntry entry;
        entry.typeId = type->getId();
        entry.name = market.name.isEmpty() ? type->name() : market.name;
        entry.marketCategory = market.category;
        entry.classification = type->getClassification();
        entry.requiredLevel = static_cast<int>(type->getMinimumLevel());
        entry.professions = type->restrictVocationMask();
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

void TMarketController::applyFilter(const QString& nameFilter, const bool filterByLevel, const bool filterByVocation, const bool oneHand,
                                    const bool twoHand, const bool showLockerOnly)
{
    if (nameFilter.isEmpty())
        m_proxy.setCategoryFilter(m_selectedCategory, filterByLevel, filterByVocation, oneHand, twoHand, currentFilter(), showLockerOnly);
    else
        m_proxy.setNameFilter(nameFilter, filterByLevel, filterByVocation, oneHand, twoHand, currentFilter(), showLockerOnly);
}

void TMarketController::setSelectedCategory(const int category)
{
    if (m_selectedCategory == category)
        return;
    m_selectedCategory = category;
    Q_EMIT selectedCategoryChanged();
}

void TMarketController::clearDetails()
{
    setItemProperty("itemDetailsModel", QString());
    setItemProperty("itemStatisticsModel", QString());
    setItemProperty("buyOffersModel", QVariant::fromValue(QList<QObject*>{}));
    setItemProperty("sellOffersModel", QVariant::fromValue(QList<QObject*>{}));
}

void TMarketController::reselect()
{
    m_proxy.applyFilterNow();
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
        m_enableTierAndClassificationSelection = false;
        Q_EMIT enableTierAndClassificationSelectionChanged();
        Q_EMIT availableClassificationsChanged();
        Q_EMIT availableTiersChanged();
        setEnableTierSelection(false);
        setIndexOfSelectedClassification(-1);
        setIndexOfSelectedTier(-1);
    } else {
        m_enableTierAndClassificationSelection = true;
        Q_EMIT enableTierAndClassificationSelectionChanged();
        Q_EMIT availableClassificationsChanged();
        Q_EMIT availableTiersChanged();
        if (changed) {
            setIndexOfSelectedClassification(0);
            setIndexOfSelectedTier(0);
            setEnableTierSelection(true);
        }
    }
    m_proxy.setCategoryFilter(CategoryIdentifier, FilterByLevel, FilterByVocation, OnlyOneHandWeapons, OnlyTwoHandWeapons, currentFilter(),
                              ShowLockerOnly);
    reselect();
    setSelectedCategory(CategoryIdentifier);
    Q_EMIT enableTierAndClassificationSelectionChanged();
    Q_EMIT availableTiersChanged();
    Q_EMIT availableClassificationsChanged();
}

void TMarketController::onMarketFilterSelected(const QString& NameFilter, const bool FilterByLevel, const bool FilterByVocation,
                                               const bool OnlyOneHandWeapons, const bool OnlyTwoHandWeapons, const bool ShowLockerOnly)
{
    clearDetails();
    if (NameFilter.isEmpty()) {
        onMarketCategoryItemSelected(0, 0);
    } else if (m_indexOfSelectedClassification < 0) {
        m_enableTierAndClassificationSelection = true;
        Q_EMIT enableTierAndClassificationSelectionChanged();
        Q_EMIT availableClassificationsChanged();
        Q_EMIT availableTiersChanged();
        setEnableTierSelection(true);
        Q_EMIT availableClassificationsChanged();
        setIndexOfSelectedClassification(0);
        setIndexOfSelectedTier(0);
        Q_EMIT availableTiersChanged();
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
    if (!m_messages)
        return;
    const TAppearanceTypePtr& type = tibia::game::thingType(m_selectedType, ThingCategoryItem);
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

void TMarketController::onClassificationFilterWasSelected(const int SelectedIndex, const QString& NameFilter, const bool FilterByLevel,
                                                          const bool FilterByVocation, const bool OnlyOneHandWeapons,
                                                          const bool OnlyTwoHandWeapons, const bool ShowLockerOnly)
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
    Q_EMIT availableTiersChanged();
    clearDetails();
    applyFilter(NameFilter, FilterByLevel, FilterByVocation, OnlyOneHandWeapons, OnlyTwoHandWeapons, ShowLockerOnly);
    reselect();
}

void TMarketController::onTierFilterWasSelected(const int SelectedIndex, const QString& NameFilter, const bool FilterByLevel,
                                                const bool FilterByVocation, const bool OnlyOneHandWeapons, const bool OnlyTwoHandWeapons,
                                                const bool ShowLockerOnly)
{
    setIndexOfSelectedTier(SelectedIndex);
    clearDetails();
    applyFilter(NameFilter, FilterByLevel, FilterByVocation, OnlyOneHandWeapons, OnlyTwoHandWeapons, ShowLockerOnly);
    reselect();
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
    close();
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
    close();
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
        rebuildEntries();
        createDialog();
        dialog()->show();
        return;
    }
    if (m_storage)
        onOffersChanged(m_storage->offers());
}

void TMarketController::closeDialog()
{
    close();
}

void TMarketController::close()
{
    if (m_messages)
        m_messages->sendLeave();
    destroyDialog();
    setIndexOfSelectedClassification(-1);
    setIndexOfSelectedTier(-1);
    m_enableTierAndClassificationSelection = false;
    Q_EMIT enableTierAndClassificationSelectionChanged();
    Q_EMIT availableClassificationsChanged();
    Q_EMIT availableTiersChanged();
    setEnableTierSelection(false);
    dialog()->close();
}

void TMarketController::createDialog()
{
    dialog()->create();
    if (dialog()->item() && m_storage)
        onActiveOfferCountChanged(m_storage->activeOfferCount());
}

void TMarketController::destroyDialog()
{
    Q_EMIT disconnectFromControllerRequest();
    for (const char* name : { "buyOffersModel", "sellOffersModel", "historicBuyOffersModel", "historicSellOffersModel", "ownBuyOffersModel",
                              "ownSellOffersModel" })
        setItemProperty(name, QVariant::fromValue(QList<QObject*>{}));
    // The views may still reference the rows until the next turn.
    for (std::vector<QObject*>* owned :
         { &m_buyOffers, &m_sellOffers, &m_historicBuyOffers, &m_historicSellOffers, &m_ownBuyOffers, &m_ownSellOffers }) {
        for (QObject* object : *owned)
            object->deleteLater();
        owned->clear();
    }
    m_selectedCategory = -1;
    m_selectedType = 0;
    m_proxy.resetFilters();
    dialog()->destroy();
    if (m_storage)
        m_storage->reset();
}

void TMarketController::showMarketMessage(const QString& Message)
{
    m_message->setButtons({ new TMessageDialogButton(TMessageDialogController::Button::OK, qtTrId("ok"),
                                                     TMessageDialogButton::RETURN_KEYS | TMessageDialogButton::ESCAPE_KEY) });
    m_message->showMessage(qtTrId("market_message_caption"), Message);
}

void TMarketController::onMessageDialogOkay()
{
    m_message->dialog()->close();
}

void TMarketController::onMarketOptionsChanged()
{
    m_proxy.setShowLockerOnly(config::TClientOptions::instance()->boolOption(config::EClientOption::marketShowLockerOnly));
}

void TMarketController::onShowLockerOnlyChanged()
{
    config::TClientOptions::instance()->setBoolOption(config::EClientOption::marketShowLockerOnly, m_proxy.showLockerOnly());
    Q_EMIT showLockerOnlyChanged();
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

QList<QObject*> TMarketController::displayOffers(const std::vector<TMarketOffer>& offers, std::vector<QObject*>& owned)
{
    for (QObject* object : owned)
        object->deleteLater();
    owned.clear();
    QList<QObject*> result;
    if (!m_storage)
        return result;
    const TMarketStatistics& sell = m_storage->itemDetails().sell;
    const uint64_t gold = playerGold();
    for (const TMarketOffer& offer : offers) {
        const int tier = offer.tier.value_or(0);
        auto* row = new TDisplayMarketOffer;
        row->m_terminationTimestamp = static_cast<int>(offer.timestamp);
        row->m_terminationTimestampUniqueIdentifier = offer.counter;
        row->m_bidder = offer.bidder;
        row->m_itemName = typeName(offer.typeId);
        row->m_amount = static_cast<int>(offer.amount);
        row->m_piecePrice = offer.price;
        row->m_endsAtTime = textHelper().formatDateTime(static_cast<int>(offer.timestamp), true);
        row->m_auctionStatus = auctionStatus(offer.type, offer.state);
        row->m_isDubious = hasReliableStatistics(sell.transactions, daysWithTransactions(sell)) && isDubiousPrice(offer, averagePrice(sell));
        row->m_maximumAcceptAmount = static_cast<int>(maximumAcceptAmount(offer, gold, m_storage->count(offer.typeId, static_cast<uint8_t>(tier))));
        row->m_tier = tier;
        result << row;
        owned.push_back(row);
    }
    return result;
}

void TMarketController::setOfferLists(const char* buyProperty, const char* sellProperty, const market::TMarketOfferList& offers,
                                      std::vector<QObject*>& ownedBuy, std::vector<QObject*>& ownedSell)
{
    if (!dialog()->item())
        return;
    // The views drop the old rows before the new ones are built.
    setItemProperty(buyProperty, QVariant::fromValue(QList<QObject*>{}));
    setItemProperty(sellProperty, QVariant::fromValue(QList<QObject*>{}));
    setItemProperty(buyProperty, QVariant::fromValue(displayOffers(offers.buyOffers, ownedBuy)));
    setItemProperty(sellProperty, QVariant::fromValue(displayOffers(offers.sellOffers, ownedSell)));
}

void TMarketController::onOffersChanged(const market::TMarketOfferList& NewOffers)
{
    setOfferLists("buyOffersModel", "sellOffersModel", NewOffers, m_buyOffers, m_sellOffers);
}

void TMarketController::onHistoryChanged(const market::TMarketOfferList& NewOffers)
{
    setOfferLists("historicBuyOffersModel", "historicSellOffersModel", NewOffers, m_historicBuyOffers, m_historicSellOffers);
}

void TMarketController::onOwnOffersChanged(const market::TMarketOfferList& NewOffers)
{
    setOfferLists("ownBuyOffersModel", "ownSellOffersModel", NewOffers, m_ownBuyOffers, m_ownSellOffers);
}

void TMarketController::onActiveOfferCountChanged(const uint16_t NewOfferCount)
{
    setItemProperty("canCreateNewOffer", NewOfferCount < market::TMarketStorage::MaximumActiveOffers);
}

}
