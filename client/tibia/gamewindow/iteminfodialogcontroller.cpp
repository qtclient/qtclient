#include "iteminfodialogcontroller.h"
#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"

#include "game/gameciptypes.h"

#include "app/clientoptions.h"
#include "analysers/analyserstorages.h"
#include "analysers/itempricestorage.h"
#include "config/clientoptions.h"
#include "cyclopedia/itemtrackingstorage.h"
#include "quickloot/quicklootblackwhiteliststorage.h"
#include "gamewindow/marketcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <client/staticdata.h>
#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <QDateTime>
#include <QLocale>
#include <QStringList>

#include <algorithm>
#include <array>
#include <set>
#include <tuple>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Args = std::vector<std::any>;
using Proxy = TAppearanceTypeListModelSortFilterProxy;

/// The market's category order (CIP), copied from TMarketController.
constexpr std::array<int, 27> CategoryOrder{ 1, 2, 3, 4, 24, 5, 6, 7, 8, 9, 10, 25, 11, 12, 13, 26, 23, 14, 15, 16, 17, 18, 19, 27, 20, 21, 29 };
constexpr int TibiaCoinsCategory = 0x17;
constexpr int UnsortedCategory = 0x1c;
constexpr int GoldCategory = 0x1e;
/// CIP : at most one inspect request per 500 ms, 100 ms at the soonest.
constexpr qint64 InspectInterval = 500;
constexpr qint64 InspectMinimumDelay = 100;
/// Crystal's sendItemInspection: 1 for the Cyclopedia.
constexpr uint8_t CyclopediaInspection = 1;

QString englishNumber(const qint64 value) { return QLocale(QLocale::English).toString(value); }

TAppearanceTypePtr itemType(const uint16_t typeId)
{
    if (typeId == 0)
        return nullptr;
    return tibia::game::thingType(typeId, ThingCategoryItem);
}

/// CIP : the market category; the coins are "Gold", the rest "Unsorted".
int itemCategory(const TAppearanceTypePtr& type)
{
    switch (type->getId()) {
        case 3031:
        case 3035:
        case 3043: return GoldCategory;
        default: break;
    }
    return type->isMarketable() ? type->marketProperties()->category : UnsortedCategory;
}

/// The NPCs of one name and currency (CIP).
struct NpcOffer
{
    QString name;
    QStringList locations;
    uint32_t buyPrice = 0;
    uint32_t salePrice = 0;
    uint32_t currencyTypeId = 0;
    std::string currencyFlag;
};

std::vector<NpcOffer> npcOffers(const TAppearanceTypePtr& type)
{
    std::vector<NpcOffer> offers;
    for (const auto& npc : type->npcTradeData()) {
        offers.push_back({ npc.name, npc.locations, npc.buyPrice, npc.salePrice, npc.currencyObjectId(),
                           npc.currencyQuestFlagName().toStdString() });
    }
    return offers;
}

QString currencyString(const NpcOffer& offer)
{
    if (offer.currencyTypeId != 0) {
        const TAppearanceTypePtr currency = itemType(static_cast<uint16_t>(offer.currencyTypeId));
        return qtTrId("iteminfo_npcdata_other_currency").arg(currency ? QString::fromStdString(currency->getName()) : QString());
    }
    if (!offer.currencyFlag.empty())
        return qtTrId("iteminfo_npcdata_other_currency").arg(QString::fromStdString(offer.currencyFlag));
    return qtTrId("iteminfo_npcdata_common_currency");
}

QString npcLine(const NpcOffer& offer, const uint32_t price)
{
    const QString location = offer.locations.size() == 1 ? offer.locations.front() : qtTrId("iteminfo_npc_multiple_locations");
    return qtTrId("iteminfo_npcdata").arg(englishNumber(price), currencyString(offer), offer.name, location);
}

}

TItemInspectData::TItemInspectData(const QString& name, const uint16_t typeId,
                                   const std::vector<std::pair<std::string, std::string>>& descriptions, QObject* parent)
    : cip::tibia::gamewindow::TInspectObjectQMLData(parent)
{
    // CIP : one table row per description, the first without top padding.
    static const QString Row = QStringLiteral("<tr>  <td width=\"150\" style=\"text-align:right; padding-right:%3px; padding-top:%4px\">%1:</td>"
                                              "  <td style=\"padding-top:%4px\">%2</td></tr>");
    m_appearanceID = typeId;
    m_objectCount = 1;
    m_objectInformation = QStringLiteral("<table width=\"100%\">");
    m_clipboardReadyText = qtTrId("inspect_inspecting").arg(name) + QLatin1Char('\n');
    bool first = true;
    for (const auto& [label, value] : descriptions) {
        const QString key = QString::fromStdString(label);
        const QString text = QString::fromStdString(value);
        m_objectInformation += Row.arg(key, text).arg(5).arg(first ? 0 : 5);
        m_clipboardReadyText += QStringLiteral("%1: %2\n").arg(key, text);
        first = false;
    }
    m_objectInformation += QStringLiteral("</table>");
}

TItemInfoDialogController::TItemInfoDialogController(QObject* parent)
    : cip::tibia::gamewindow::TItemInfoDialogController(parent)
    , m_proxy(false, true, false, nullptr)
{
    m_appearanceTypeListModel = QVariant::fromValue<QObject*>(&m_proxy);
    buildCategories();
    m_indexOfSelectedClassification = -1;
    m_availableClassifications = { QString() };
    m_inspectTimer.setSingleShot(true);
    m_lastInspect = QDateTime::currentMSecsSinceEpoch();
    connect(&m_inspectTimer, &QTimer::timeout, this, &TItemInfoDialogController::onInspectRequestTimeoutReached);

    connect(prices(), &analysers::TItemPriceStorage::configurationChangedFromUserInput, this, &TItemInfoDialogController::lootValueChanged);
    connect(prices(), &analysers::TItemPriceStorage::marketPricesAvailable, this, &TItemInfoDialogController::lootValueChanged);
    if (analysers::TAnalyserStorages* container = storages()) {
        // itemTracking is null until connectToGameSession; connecting it here
        // logs QObject::connect(ICharacterConfigurationBase, TItemInfoDialogController).
        ensureItemTrackingConnection();
        connect(container, &analysers::TAnalyserStorages::premiumTimeChanged, this, [this] {
            Q_EMIT lootValueChanged();
            Q_EMIT trackingChanged();
            Q_EMIT premiumTimeChanged();
        });
        connect(container, &analysers::TAnalyserStorages::configurationLoadedForCharacter, this, [this] {
            ensureItemTrackingConnection();
            onCharacterConfigurationChanged();
            Q_EMIT premiumTimeChanged();
        });
    }
    m_hooks.emplace_back(tibia::game::kNotifyScope, "forgeData", [this](GameObject*, const Args& args) {
        const auto* data = args.empty() ? nullptr : std::any_cast<ForgeConfigData>(&args[0]);
        if (!data)
            return std::any{};
        std::set<int> classes;
        for (const ForgeClassTierPrices& classPrices : data->classPrices)
            classes.insert(classPrices.classId);
        m_classifications.assign(classes.begin(), classes.end());
        if (m_enableTierAndClassificationSelection)
            updateAvailableClassifications();
        return std::any{};
    });
    // CIP connects the option storage to the loot value (its background).
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String("colorizeLootStyle"))
            Q_EMIT lootValueChanged();
    });

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseItemDetail", [this](GameObject*, const Args& args) {
        if (const auto* data = args.empty() ? nullptr : std::any_cast<ItemInspectionData>(&args[0]))
            onInspectObjectDataReceived(*data);
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        m_entriesBuilt = false;
        resetInspect();
    });

    resetInspect();
}

TItemInfoDialogController::~TItemInfoDialogController()
{
    m_typeId = 0;
    Q_EMIT itemSelectedChanged();
    resetInspect();
}

void TItemInfoDialogController::buildCategories()
{
    for (const int identifier : categoryIdentifiers())
        m_categories << new TQmlObjectCategory(identifier, this);
}

std::vector<int> TItemInfoDialogController::categoryIdentifiers()
{
    std::vector<int> identifiers(CategoryOrder.begin(), CategoryOrder.end());
    for (const int extra : { GoldCategory, UnsortedCategory }) {
        const QString name = TQmlObjectCategory::translate(extra);
        const auto it = std::ranges::find_if(identifiers, [&name](const int identifier) {
            return TQmlObjectCategory::translate(identifier).compare(name, Qt::CaseInsensitive) >= 0;
        });
        identifiers.insert(it, extra);
    }
    std::erase(identifiers, TibiaCoinsCategory);
    return identifiers;
}

TAppearanceTypeEntry TItemInfoDialogController::cyclopediaEntry(const TAppearanceTypePtr& type)
{
    TAppearanceTypeEntry entry;
    entry.typeId = type->getId();
    entry.name = QString::fromStdString(type->getName());
    entry.marketCategory = itemCategory(type);
    entry.classification = type->getClassification();
    if (type->isMarketable()) {
        entry.requiredLevel = static_cast<int>(type->getMinimumLevel());
        entry.professions = type->restrictVocationMask();
    }
    entry.hasClothSlot = type->isCloth();
    entry.clothSlot = type->getClothSlot();
    for (const auto& npc : type->npcTradeData()) {
        if (npc.buyPrice != 0)
            entry.imbuements << npc.name;
    }
    return entry;
}

void TItemInfoDialogController::rebuildEntries()
{
    std::vector<TAppearanceTypeEntry> entries;
    for (const TAppearanceTypePtr& type : tibia::appearances::TAppearancesManager::instance().getCyclopediaObjectTypes())
        entries.push_back(cyclopediaEntry(type));
    m_proxy.resetTypes(entries);
    m_entriesBuilt = true;
    setEnableTier(false);
}

bool TItemInfoDialogController::categorySupportsTiers(const int category) const
{
    for (int row = 0; row < m_proxy.model().size(); ++row) {
        const TAppearanceTypeEntry& entry = m_proxy.model().entry(row);
        if (entry.classification == 0)
            continue;
        if (category == Proxy::AllCategories || entry.marketCategory == category)
            return true;
        if (category == Proxy::AllWeaponsCategory && Proxy::isHandWeaponCategory(entry.marketCategory))
            return true;
    }
    return false;
}

TAppearanceTypeFilter TItemInfoDialogController::currentFilter() const
{
    TAppearanceTypeFilter filter;
    if (!m_enableTierAndClassificationSelection)
        return filter;
    const std::vector<int>& classes = m_classifications;
    if (m_indexOfSelectedClassification == 1) {
        filter.classificationMode = 1;
    } else if (m_indexOfSelectedClassification == 0) {
        filter.classificationMode = 2;
    } else if (m_indexOfSelectedClassification >= 2 && m_indexOfSelectedClassification - 2 < static_cast<int>(classes.size())) {
        filter.classificationMode = 3;
        filter.classification = classes[m_indexOfSelectedClassification - 2];
    }
    return filter;
}

void TItemInfoDialogController::updateAvailableClassifications()
{
    QStringList classes;
    if (!m_enableTierAndClassificationSelection) {
        classes << QString();
    } else {
        classes << qtTrId("market_filter_class_all") << qtTrId("market_filter_class_none");
        for (const int classification : m_classifications)
            classes << qtTrId("market_filter_class").arg(classification);
    }
    m_availableClassifications = classes;
    Q_EMIT availableClassificationsChanged();
}

void TItemInfoDialogController::setEnableTier(const bool enable)
{
    m_enableTierAndClassificationSelection = enable;
    Q_EMIT enableTierAndClassificationSelectionChanged();
    updateAvailableClassifications();
}

void TItemInfoDialogController::setClassificationIndex(const int index)
{
    m_indexOfSelectedClassification = index;
    Q_EMIT indexOfSelectedClassificationChanged();
}

void TItemInfoDialogController::setButtons(const bool level, const bool vocation, const bool oneHand, const bool twoHand)
{
    m_buttons[0] = level;
    m_buttons[1] = vocation;
    m_buttons[2] = oneHand;
    m_buttons[3] = twoHand;
}

void TItemInfoDialogController::ensureItemTrackingConnection()
{
    // The loot list is the session's: follow the current one.
    if (quickloot::TQuickLootBlackWhitelistStorage* list = lootBlackWhitelist(); list != m_connectedLootBlackWhitelist) {
        QObject::disconnect(m_lootBlackWhitelistConnection);
        m_lootBlackWhitelistConnection = {};
        m_connectedLootBlackWhitelist = list;
        if (list)
            m_lootBlackWhitelistConnection = connect(list, &quickloot::TQuickLootBlackWhitelistStorage::configurationChangedFromUserInput,
                                                     this, &TItemInfoDialogController::lootBlackWhitelistChanged);
    }
    if (m_itemTrackingConnected)
        return;
    cyclopedia::TItemTrackingStorage* tracking = itemTracking();
    if (!tracking)
        return;
    connect(tracking, &cyclopedia::TItemTrackingStorage::configurationChangedFromUserInput, this,
            &TItemInfoDialogController::trackingChanged);
    m_itemTrackingConnected = true;
}

void TItemInfoDialogController::showDialogTab()
{
    hideDialogTab();
    if (const game::TGameSessionBase* session = game::currentGameSession())
        m_proxy.connectToStorages(session->playerData(), prices(), session->itemTrackingStorage(), nullptr, nullptr);
    else
        m_proxy.disconnectFromStorages();
    if (!m_entriesBuilt)
        rebuildEntries();
    m_shown = true;
    ensureItemTrackingConnection();
    onCharacterConfigurationChanged();
    Q_EMIT premiumTimeChanged();
    setEnableTier(false);
    setClassificationIndex(-1);
}

void TItemInfoDialogController::hideDialogTab()
{
    Q_EMIT disconnectFromControllerRequest();
    m_shown = false;
    m_proxy.setCategoryFilter(-1, false, false, false, false, {}, false);
    // The list is emptied; showing the tab again reloads it.
    m_proxy.resetEntries({});
    m_entriesBuilt = false;
    m_category = -1;
    m_name.clear();
    setButtons(false, false, false, false);
    m_typeId = 0;
    Q_EMIT itemSelectedChanged();
    resetInspect();
}

void TItemInfoDialogController::requestCategorySelect(const int CategoryIdentifier, const bool FilterByLevel, const bool FilterByVocation,
                                                      const bool OnlyOneHanWeapons, const bool OnlyTwoHandWeapons)
{
    if (categorySupportsTiers(CategoryIdentifier)) {
        m_enableTierAndClassificationSelection = true;
        Q_EMIT enableTierAndClassificationSelectionChanged();
        if (CategoryIdentifier != m_category)
            setClassificationIndex(0);
        updateAvailableClassifications();
    } else {
        setEnableTier(false);
        setClassificationIndex(-1);
    }
    m_category = CategoryIdentifier;
    m_name.clear();
    setButtons(FilterByLevel, FilterByVocation, OnlyOneHanWeapons, OnlyTwoHandWeapons);
    m_proxy.setCategoryFilter(CategoryIdentifier, FilterByLevel, FilterByVocation, OnlyOneHanWeapons, OnlyTwoHandWeapons, currentFilter(), false);
}

void TItemInfoDialogController::requestNameSelect(const QString& NameFilter, const bool FilterByLevel, const bool FilterByVocation,
                                                  const bool OnlyOneHanWeapons, const bool OnlyTwoHandWeapons)
{
    m_name = NameFilter;
    m_category = -1;
    if (m_indexOfSelectedClassification < 0) {
        setEnableTier(true);
        setClassificationIndex(0);
    }
    setButtons(FilterByLevel, FilterByVocation, OnlyOneHanWeapons, OnlyTwoHandWeapons);
    m_proxy.setNameFilter(NameFilter, FilterByLevel, FilterByVocation, OnlyOneHanWeapons, OnlyTwoHandWeapons, currentFilter(), false);
}

void TItemInfoDialogController::onClassificationFilterWasSelected(const int SelectedIndex, const QString& NameFilter, const bool FilterByLevel,
                                                                  const bool FilterByVocation, const bool OnlyOneHandWeapons,
                                                                  const bool OnlyTwoHandWeapons, const bool ShowLockerOnly)
{
    setClassificationIndex(SelectedIndex);
    setButtons(FilterByLevel, FilterByVocation, OnlyOneHandWeapons, OnlyTwoHandWeapons);
    if (NameFilter.isEmpty())
        m_proxy.setCategoryFilter(m_category, FilterByLevel, FilterByVocation, OnlyOneHandWeapons, OnlyTwoHandWeapons, currentFilter(),
                                  ShowLockerOnly);
    else
        m_proxy.setNameFilter(NameFilter, FilterByLevel, FilterByVocation, OnlyOneHandWeapons, OnlyTwoHandWeapons, currentFilter(),
                              ShowLockerOnly);
}

void TItemInfoDialogController::resetInspect()
{
    setInspectData(new TItemInspectData(QString(), 0, { { qtTrId("status").toStdString(), qtTrId("iteminfo_loading_second_column").toStdString() } },
                                        this));
    m_npcBuyData.clear();
    m_npcSellData.clear();
    Q_EMIT npcDataChanged();
    setSelectedTypeId(0);
    m_inspectTimer.stop();
}

void TItemInfoDialogController::setSelectedTypeId(const uint16_t typeId)
{
    if (typeId == m_typeId)
        return;
    m_typeId = typeId;
    Q_EMIT lootValueChanged();
    Q_EMIT trackingChanged();
    Q_EMIT itemSelectedChanged();
    Q_EMIT lootBlackWhitelistChanged();
}

analysers::TAnalyserStorages* TItemInfoDialogController::storages() const { return analysers::TAnalyserStorages::instance(); }

analysers::TItemPriceStorage* TItemInfoDialogController::prices() const { return analysers::TItemPriceStorage::instance(); }

cyclopedia::TItemTrackingStorage* TItemInfoDialogController::itemTracking() const
{
    return storages() ? storages()->itemTracking() : nullptr;
}

quickloot::TQuickLootBlackWhitelistStorage* TItemInfoDialogController::lootBlackWhitelist() const
{
    return game::currentQuickLootBlackWhitelistStorage();
}

bool TItemInfoDialogController::hasCharacter() const { return storages() && storages()->hasCharacter(); }

void TItemInfoDialogController::setPrimaryLootValueSource(const int source)
{
    if (source == m_primaryLootValueSource)
        return;
    m_primaryLootValueSource = source;
    Q_EMIT primaryLootValueSourceChanged();
}

void TItemInfoDialogController::onCharacterConfigurationChanged()
{
    if (!hasCharacter())
        return;
    setPrimaryLootValueSource(prices()->primaryLootValueSource(m_typeId));
    Q_EMIT lootValueChanged();
    Q_EMIT trackingChanged();
    Q_EMIT lootBlackWhitelistChanged();
}

void TItemInfoDialogController::onInspectObjectDataReceived(const ItemInspectionData& data)
{
    // CIP answers only the Cyclopedia's inspections; the details are the
    // first inspected object, or none.
    if (data.inspectionType != CyclopediaInspection || !m_shown || m_typeId == 0)
        return;
    setInspectData(new TItemInspectData(QString::fromStdString(data.name), m_typeId, data.descriptions, this));
}

void TItemInfoDialogController::setInspectData(TItemInspectData* data)
{
    QObject* previous = m_inspectData;
    m_inspectData = data;
    Q_EMIT inspectDataChanged();
    if (previous)
        previous->deleteLater();
}

void TItemInfoDialogController::requestItemSelect(const int AppearanceId)
{
    resetInspect();
    setSelectedTypeId(static_cast<uint16_t>(AppearanceId));
    if (tibia::game::isOnline()) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        m_inspectTimer.start(static_cast<int>(std::max(InspectMinimumDelay, m_lastInspect + InspectInterval - now)));
        setPrimaryLootValueSource(prices()->primaryLootValueSource(m_typeId));
    }
    if (itemType(m_typeId))
        updateNpcData();
}

void TItemInfoDialogController::onInspectRequestTimeoutReached()
{
    if (m_typeId == 0 || !tibia::game::isOnline())
        return;
    m_lastInspect = QDateTime::currentMSecsSinceEpoch();
    tibia::game::inspectionObject(tibia::client::INSPECT_CYCLOPEDIA, m_typeId, 1);
}

void TItemInfoDialogController::updateNpcData()
{
    m_npcBuyData.clear();
    m_npcSellData.clear();
    if (const TAppearanceTypePtr type = itemType(m_typeId)) {
        std::vector<NpcOffer> offers = npcOffers(type);
        const auto key = [](const NpcOffer& offer) { return std::tie(offer.buyPrice, offer.locations, offer.name); };
        std::ranges::sort(offers, [](const NpcOffer& a, const NpcOffer& b) {
            return std::tie(b.buyPrice, a.locations, a.name) < std::tie(a.buyPrice, b.locations, b.name);
        });
        for (const NpcOffer& offer : offers) {
            if (offer.buyPrice != 0)
                m_npcBuyData << npcLine(offer, offer.buyPrice);
        }
        // CIP re-sorts the sellers by the price they pay, not the one they ask.
        std::ranges::sort(offers, [&key](const NpcOffer& a, const NpcOffer& b) { return key(a) < key(b); });
        for (const NpcOffer& offer : offers) {
            if (offer.salePrice != 0)
                m_npcSellData << npcLine(offer, offer.salePrice);
        }
    }
    Q_EMIT npcDataChanged();
}

bool TItemInfoDialogController::hasWeaponProficiency() const
{
    const TAppearanceTypePtr type = itemType(m_typeId);
    return type && type->getProficiencyId() != 0;
}

void TItemInfoDialogController::setPremiumTimeStorage(game::TPremiumTimeStorage* storage) { m_premiumTimeStorage = storage; }

bool TItemInfoDialogController::isPremium() const { return m_premiumTimeStorage && m_premiumTimeStorage->isPremium(); }

int TItemInfoDialogController::primaryLootValueSource() const { return m_primaryLootValueSource; }

QString TItemInfoDialogController::userDefinedValueString() const
{
    return m_typeId ? QString::number(prices()->customSalePrice(m_typeId)) : QStringLiteral("0");
}

QString TItemInfoDialogController::marketValueString() const
{
    return m_typeId ? englishNumber(static_cast<qint64>(prices()->marketPrice(m_typeId))) : QStringLiteral("0");
}

QString TItemInfoDialogController::lootValueString() const
{
    return m_typeId ? englishNumber(static_cast<qint64>(prices()->lootPrice(m_typeId))) : QStringLiteral("0");
}

QString TItemInfoDialogController::lootValueBackgroundUrlSource() const
{
    if (m_typeId == 0 || !hasCharacter())
        return {};
    if (config::TClientOptions::instance()->colorizeLootStyle() == 0)
        return {};
    const int level = prices()->lootValueLevel(m_typeId);
    if (level == 0)
        return {};
    return QStringLiteral("image://%1/%2").arg(QStringLiteral("containerslot-glowingborders")).arg(level - 1);
}

bool TItemInfoDialogController::isTracked() const { return m_typeId && itemTracking() && itemTracking()->isTracked(m_typeId); }

bool TItemInfoDialogController::canTrackAdditionalItems() const { return itemTracking() && !itemTracking()->isTrackedItemListFull(); }

bool TItemInfoDialogController::isLootBlackWhitelisted() const
{
    return m_typeId && lootBlackWhitelist() && lootBlackWhitelist()->isListed(m_typeId);
}

bool TItemInfoDialogController::canBlackWhitelistAdditionalItems() const { return lootBlackWhitelist() && !lootBlackWhitelist()->isListFull(); }

int TItemInfoDialogController::blackWhiteListType() const { return lootBlackWhitelist() ? lootBlackWhitelist()->listType() : 0; }

void TItemInfoDialogController::requestPrimaryLootValueSelect(const int PrimaryLootValueSource)
{
    if (m_typeId == 0 || !hasCharacter())
        return;
    prices()->setPrimaryLootValueSource(m_typeId, PrimaryLootValueSource);
    Q_EMIT lootValueChanged();
}

void TItemInfoDialogController::requestCustomSaleValue(const QString& CustomSaleValueText)
{
    if (m_typeId == 0)
        return;
    bool ok = false;
    int value = CustomSaleValueText.toInt(&ok);
    if (!ok)
        value = 0;
    prices()->setCustomSalePrice(m_typeId, static_cast<quint32>(value));
    Q_EMIT lootValueChanged();
}

void TItemInfoDialogController::requestOpenStoreWithPremiumTime()
{
    auto action = std::make_shared<input::TGameActionOpenStore>(store::TStoreOpenParameters{.openAction = store::OPEN_PREMIUM_OR_BOOSTS, .premiumDetail = 0});
    Q_EMIT publishGameAction(action);
}

void TItemInfoDialogController::requestItemTracking(const bool TrackItem)
{
    if (m_typeId != 0 && itemTracking()) {
        if (TrackItem)
            itemTracking()->trackItem(m_typeId, false, true);
        else
            itemTracking()->untrackItem(m_typeId);
    }
    Q_EMIT trackingChanged();
}

void TItemInfoDialogController::requestItemBlackWhitelisting(const bool BlackWhitelistItem)
{
    // The storage's list change tells QML.
    if (m_typeId != 0 && lootBlackWhitelist())
        lootBlackWhitelist()->setListed(m_typeId, BlackWhitelistItem);
}

void TItemInfoDialogController::onWeaponProficiencyDialogButtonClicked()
{
    if (m_typeId == 0)
        return;
    auto action = std::make_shared<input::gameactions::TGameActionOpenWeaponProficiencyDialog>(static_cast<uint16_t>(m_typeId));
    Q_EMIT publishGameAction(action);
}

void TItemInfoDialogController::showType(const uint16_t typeId)
{
    m_category = -1;
    m_name.clear();
    m_proxy.setTypeFilter(typeId);
    requestItemSelect(typeId);
    Q_EMIT itemListSelectionRequest(0);
}

TCyclopediaDialogContext TItemInfoDialogController::captureContext() const
{
    TItemInfoCyclopediaContext context;
    context.category = m_category;
    context.name = m_name;
    std::copy(std::begin(m_buttons), std::end(m_buttons), std::begin(context.buttons));
    context.typeId = m_typeId;
    return context;
}

void TItemInfoDialogController::restoreContext(const TCyclopediaDialogContext& CyclopediaContext)
{
    const auto& context = cyclopediaContextAs<TItemInfoCyclopediaContext>(CyclopediaContext);
    Q_EMIT filterOptionsUpdateRequest(context.buttons[0], context.buttons[1], context.buttons[2], context.buttons[3]);
    if (context.category == -1) {
        if (context.name.isEmpty()) {
            if (context.typeId != 0)
                showType(context.typeId);
        } else {
            Q_EMIT setNameFilterRequest(context.name);
        }
    } else {
        for (int index = 0; index < m_categories.size(); ++index) {
            const auto* category = qobject_cast<cip::tibia::gamewindow::TQmlObjectCategory*>(m_categories[index]);
            if (category && category->categoryIdentifier() == context.category) {
                Q_EMIT categoryListSelectionRequest(index);
                break;
            }
        }
    }
    if (context.typeId == 0)
        return;
    m_proxy.applyFilterNow();
    for (int row = 0; row < m_proxy.rowCount(); ++row) {
        if (m_proxy.typeIdAt(row) == context.typeId) {
            Q_EMIT itemListSelectionRequest(row);
            return;
        }
    }
}

}
