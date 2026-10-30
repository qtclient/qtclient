#include "iteminfodialogcontroller.h"

#include "app/clientoptions.h"
#include "cyclopedia/iteminfostorage.h"
#include "gamewindow/marketcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <client/item.h>
#include <client/staticdata.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QDateTime>
#include <QLocale>
#include <QStringList>

#include <algorithm>
#include <array>
#include <tuple>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Args = std::vector<std::any>;
using Proxy = TAppearanceTypeListModelSortFilterProxy;

/// The market's category order (CIP 0x100d5a820), copied from TMarketController.
constexpr std::array<int, 27> CategoryOrder{ 1, 2, 3, 4, 24, 5, 6, 7, 8, 9, 10, 25, 11, 12, 13, 26, 23, 14, 15, 16, 17, 18, 19, 27, 20, 21, 29 };
constexpr int TibiaCoinsCategory = 0x17;
constexpr int UnsortedCategory = 0x1c;
constexpr int GoldCategory = 0x1e;
/// CIP 0x100cad0a0: at most one inspect request per 500 ms, 100 ms at the soonest.
constexpr qint64 InspectInterval = 500;
constexpr qint64 InspectMinimumDelay = 100;
/// Crystal's sendItemInspection: 1 for the Cyclopedia.
constexpr uint8_t CyclopediaInspection = 1;

QString englishNumber(const qint64 value) { return QLocale(QLocale::English).toString(value); }

ThingTypePtr itemType(const uint16_t typeId)
{
    if (typeId == 0)
        return nullptr;
    return g_things.getThingType(typeId, ThingCategoryItem);
}

/// CIP 0x100935520: the market category; the coins are "Gold", the rest "Unsorted".
int itemCategory(const ThingTypePtr& type)
{
    switch (type->getId()) {
        case 3031:
        case 3035:
        case 3043: return GoldCategory;
        default: break;
    }
    return type->isMarketable() ? static_cast<int>(type->getMarketData().category) : UnsortedCategory;
}

/// The NPCs of one name and currency (CIP 0x100940650).
struct NpcOffer
{
    QString name;
    QStringList locations;
    uint32_t buyPrice = 0;
    uint32_t salePrice = 0;
    uint32_t currencyTypeId = 0;
    std::string currencyFlag;
};

std::vector<NpcOffer> npcOffers(const ThingTypePtr& type)
{
    std::vector<NpcOffer> offers;
    for (const NPCData& npc : type->getNpcSaleData()) {
        const QString name = QString::fromStdString(npc.name);
        auto it = std::ranges::find_if(offers, [&](const NpcOffer& offer) {
            return offer.name == name && offer.currencyTypeId == npc.currencyObjectTypeId
                   && offer.currencyFlag == npc.currencyQuestFlagDisplayName;
        });
        if (it == offers.end()) {
            offers.push_back({ name, {}, 0, 0, npc.currencyObjectTypeId, npc.currencyQuestFlagDisplayName });
            it = std::prev(offers.end());
        }
        const QString location = QString::fromStdString(npc.location);
        if (!it->locations.contains(location))
            it->locations << location;
        it->buyPrice = std::max(it->buyPrice, npc.buyPrice);
        if (npc.salePrice != 0 && (it->salePrice == 0 || npc.salePrice < it->salePrice))
            it->salePrice = npc.salePrice;
    }
    return offers;
}

QString currencyString(const NpcOffer& offer)
{
    if (offer.currencyTypeId != 0) {
        const ThingTypePtr currency = itemType(static_cast<uint16_t>(offer.currencyTypeId));
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
    // CIP 0x100cb1630: one table row per description, the first without top padding.
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

bool TItemInfoDialogController::Context::operator==(const Context& other) const
{
    return category == other.category && name == other.name && std::equal(std::begin(buttons), std::end(buttons), std::begin(other.buttons))
           && typeId == other.typeId;
}

TItemInfoDialogController::TItemInfoDialogController(QObject* parent)
    : cip::tibia::gamewindow::TItemInfoDialogController(parent)
    , m_storage(new cyclopedia::TItemInfoStorage(this))
    , m_model([](uint16_t, uint8_t) { return 0; })
    , m_proxy(&m_model)
{
    m_appearanceTypeListModel = QVariant::fromValue<QObject*>(&m_proxy);
    buildCategories();
    m_indexOfSelectedClassification = -1;
    m_availableClassifications = { QString() };
    m_inspectTimer.setSingleShot(true);
    connect(&m_inspectTimer, &QTimer::timeout, this, &TItemInfoDialogController::sendInspect);

    connect(m_storage, &cyclopedia::TItemInfoStorage::pricesChanged, this, &TItemInfoDialogController::lootValueChanged);
    connect(m_storage, &cyclopedia::TItemInfoStorage::trackingChanged, this, &TItemInfoDialogController::trackingChanged);
    connect(m_storage, &cyclopedia::TItemInfoStorage::lootListChanged, this, &TItemInfoDialogController::lootBlackWhitelistChanged);
    connect(m_storage, &cyclopedia::TItemInfoStorage::premiumChanged, this, &TItemInfoDialogController::premiumTimeChanged);
    connect(m_storage, &cyclopedia::TItemInfoStorage::characterChanged, this, [this] {
        Q_EMIT primaryLootValueSourceChanged();
        Q_EMIT premiumTimeChanged();
    });
    connect(m_storage, &cyclopedia::TItemInfoStorage::classificationsChanged, this, [this] {
        if (m_enableTierAndClassificationSelection)
            updateAvailableClassifications();
    });
    // CIP connects the option storage to the loot value (its background).
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String("colorizeLootStyle"))
            Q_EMIT lootValueChanged();
    });

    m_hooks.emplace_back("g_game", "onParseItemDetail", [this](LuaObject*, const Args& args) {
        const auto* data = args.empty() ? nullptr : std::any_cast<ItemInspectionData>(&args[0]);
        if (!data || data->inspectionType != CyclopediaInspection || !m_shown || m_typeId == 0)
            return std::any{};
        setInspectData(new TItemInspectData(QString::fromStdString(data->name), m_typeId, data->descriptions, this));
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        m_entriesBuilt = false;
        resetInspect();
        return std::any{};
    });

    resetInspect();
}

TItemInfoDialogController::~TItemInfoDialogController() = default;

void TItemInfoDialogController::buildCategories()
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
    for (const int identifier : identifiers)
        m_categories << new TQmlObjectCategory(identifier, this);
}

void TItemInfoDialogController::rebuildEntries()
{
    std::vector<TAppearanceTypeEntry> entries;
    for (const ThingTypePtr& type : g_things.getThingTypes(ThingCategoryItem)) {
        if (!type || type->getId() == 0 || type->getCyclopediaType() != type->getId())
            continue;
        TAppearanceTypeEntry entry;
        entry.typeId = type->getId();
        entry.name = QString::fromStdString(type->getName());
        entry.marketCategory = itemCategory(type);
        entry.classification = type->getClassification();
        if (type->isMarketable()) {
            const MarketData& market = type->getMarketData();
            entry.requiredLevel = market.requiredLevel;
            entry.professions = market.restrictVocation;
        }
        entry.hasClothSlot = type->isCloth();
        entry.clothSlot = type->getClothSlot();
        entries.push_back(std::move(entry));
    }
    m_model.reset(std::move(entries));
    m_proxy.invalidate();
    m_entriesBuilt = true;
    setEnableTier(false);
}

bool TItemInfoDialogController::categorySupportsTiers(const int category) const
{
    for (int row = 0; row < m_model.size(); ++row) {
        const TAppearanceTypeEntry& entry = m_model.entry(row);
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
    const std::vector<int>& classes = m_storage->classifications();
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
        for (const int classification : m_storage->classifications())
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

void TItemInfoDialogController::showDialogTab()
{
    if (!m_entriesBuilt)
        rebuildEntries();
    m_shown = true;
    Q_EMIT primaryLootValueSourceChanged();
    Q_EMIT lootValueChanged();
    Q_EMIT premiumTimeChanged();
    setEnableTier(false);
    setClassificationIndex(-1);
}

void TItemInfoDialogController::hideDialogTab()
{
    Q_EMIT disconnectFromControllerRequest();
    m_shown = false;
    m_proxy.setCategoryFilter(-1, false, false, false, false, {}, false);
    m_category = -1;
    m_name.clear();
    setButtons(false, false, false, false);
    m_typeId = 0;
    Q_EMIT itemSelectedChanged();
    resetInspect();
    m_inspectTimer.stop();
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
    const auto typeId = static_cast<uint16_t>(std::clamp(AppearanceId, 0, 0xffff));
    resetInspect();
    m_inspectTimer.stop();
    if (typeId != m_typeId) {
        m_typeId = typeId;
        Q_EMIT lootValueChanged();
        Q_EMIT trackingChanged();
        Q_EMIT itemSelectedChanged();
        Q_EMIT lootBlackWhitelistChanged();
    }
    if (m_typeId != 0 && g_game.isOnline()) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        m_inspectTimer.start(static_cast<int>(std::max(InspectMinimumDelay, m_lastInspect + InspectInterval - now)));
    }
    Q_EMIT primaryLootValueSourceChanged();
    updateNpcData();
}

void TItemInfoDialogController::sendInspect()
{
    if (m_typeId == 0 || !g_game.isOnline())
        return;
    m_lastInspect = QDateTime::currentMSecsSinceEpoch();
    g_game.inspectionObject(Otc::INSPECT_CYCLOPEDIA, m_typeId, 1);
    if (const ItemPtr item = Item::create(m_typeId))
        g_game.requestItemInfo(item, 0);
}

void TItemInfoDialogController::updateNpcData()
{
    m_npcBuyData.clear();
    m_npcSellData.clear();
    if (const ThingTypePtr type = itemType(m_typeId)) {
        std::vector<NpcOffer> offers = npcOffers(type);
        const auto key = [](const NpcOffer& offer) { return std::tie(offer.buyPrice, offer.locations, offer.name); };
        std::ranges::sort(offers, [](const NpcOffer& a, const NpcOffer& b) {
            return std::tie(b.buyPrice, a.locations, a.name) < std::tie(a.buyPrice, b.locations, b.name);
        });
        for (const NpcOffer& offer : offers) {
            if (offer.buyPrice != 0)
                m_npcBuyData << npcLine(offer, offer.buyPrice);
        }
        // CIP 0x100caf2a0 re-sorts the sellers by the price they pay, not the one they ask.
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
    const ThingTypePtr type = itemType(m_typeId);
    return type && type->getProficiencyId() != 0;
}

bool TItemInfoDialogController::isPremium() const
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player && player->isPremium();
}

int TItemInfoDialogController::primaryLootValueSource() const { return m_typeId ? m_storage->lootValueSource(m_typeId) : 0; }

QString TItemInfoDialogController::userDefinedValueString() const
{
    return m_typeId ? QString::number(m_storage->customPrice(m_typeId)) : QStringLiteral("0");
}

QString TItemInfoDialogController::marketValueString() const
{
    return m_typeId ? englishNumber(m_storage->marketPrice(m_typeId)) : QStringLiteral("0");
}

QString TItemInfoDialogController::lootValueString() const
{
    return m_typeId ? englishNumber(m_storage->lootPrice(m_typeId)) : QStringLiteral("0");
}

QString TItemInfoDialogController::lootValueBackgroundUrlSource() const
{
    if (m_typeId == 0 || !m_storage->hasCharacter())
        return {};
    if (app::ClientOptions::load().options().value(QLatin1String("colorizeLootStyle")).toInt() == 0)
        return {};
    const int level = m_storage->lootValueLevel(m_typeId);
    if (level == 0)
        return {};
    return QStringLiteral("image://%1/%2").arg(QStringLiteral("containerslot-glowingborders")).arg(level - 1);
}

bool TItemInfoDialogController::isTracked() const { return m_typeId && m_storage->isTracked(m_typeId); }

bool TItemInfoDialogController::canTrackAdditionalItems() const { return m_storage->canTrackMore(); }

bool TItemInfoDialogController::isLootBlackWhitelisted() const { return m_typeId && m_storage->isLootListed(m_typeId); }

bool TItemInfoDialogController::canBlackWhitelistAdditionalItems() const { return !m_storage->isLootListFull(); }

int TItemInfoDialogController::blackWhiteListType() const { return m_storage->lootListType(); }

void TItemInfoDialogController::requestPrimaryLootValueSelect(const int PrimaryLootValueSource)
{
    if (m_typeId == 0 || !m_storage->hasCharacter())
        return;
    m_storage->setLootValueSource(m_typeId, PrimaryLootValueSource);
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
    m_storage->setCustomPrice(m_typeId, value);
    Q_EMIT lootValueChanged();
}

void TItemInfoDialogController::requestOpenStoreWithPremiumTime()
{
    auto action = input::mutableClone(*input::makeAction(A::OPEN_STORE));
    action->parameter = 1;
    input::publish(action);
}

void TItemInfoDialogController::requestItemTracking(const bool TrackItem)
{
    if (m_typeId != 0)
        m_storage->setTracked(m_typeId, TrackItem);
    Q_EMIT trackingChanged();
}

void TItemInfoDialogController::requestItemBlackWhitelisting(const bool BlackWhitelistItem)
{
    if (m_typeId != 0)
        m_storage->setLootListed(m_typeId, BlackWhitelistItem);
    Q_EMIT lootBlackWhitelistChanged();
}

void TItemInfoDialogController::onWeaponProficiencyDialogButtonClicked()
{
    if (m_typeId == 0)
        return;
    auto action = input::mutableClone(*input::makeAction(A::OPEN_WEAPON_PROFICIENCY_DIALOG));
    action->instance = m_typeId;
    input::publish(action);
}

void TItemInfoDialogController::showType(const uint16_t typeId)
{
    m_category = -1;
    m_name.clear();
    m_proxy.setTypeFilter(typeId);
    requestItemSelect(typeId);
    Q_EMIT itemListSelectionRequest(0);
}

TItemInfoDialogController::Context TItemInfoDialogController::captureContext() const
{
    Context context;
    context.category = m_category;
    context.name = m_name;
    std::copy(std::begin(m_buttons), std::end(m_buttons), std::begin(context.buttons));
    context.typeId = m_typeId;
    return context;
}

void TItemInfoDialogController::restoreContext(const Context& context)
{
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
    for (int row = 0; row < m_proxy.rowCount(); ++row) {
        if (m_proxy.typeIdAt(row) == context.typeId) {
            Q_EMIT itemListSelectionRequest(row);
            return;
        }
    }
}

}
