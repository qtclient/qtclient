#include "appearancetypefilterselection.h"

#include "exaltation/exaltationstorage.h"
#include "game/gamesessionbase.h"
#include "gamewindow/appearancetypelistmodel.h"

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>

#include <utility>
#include <vector>

namespace tibia::gamewindow {

namespace {

exaltation::TExaltationStorage* exaltationStorageOf(const game::TGameSessionBase* session)
{
    return session ? session->exaltationStorage().get() : nullptr;
}

}

QStringList classificationFilterEntries(const bool enabled, const game::TGameSessionBase* session)
{
    QStringList entries;
    if (!enabled) {
        entries << QString();
        return entries;
    }
    entries << qtTrId("market_filter_class_all");
    entries << qtTrId("market_filter_class_none");
    if (exaltation::TExaltationStorage* exaltation = exaltationStorageOf(session)) {
        for (const uint8_t classification : exaltation->classifications())
            entries << qtTrId("market_filter_class").arg(static_cast<uint>(classification));
    }
    return entries;
}

QStringList tierFilterEntries(const bool enabled, const game::TGameSessionBase* session,
                              const TAppearanceTypeFilterSelection& classification)
{
    QStringList entries;
    exaltation::TExaltationStorage* exaltation = exaltationStorageOf(session);
    if (!enabled || !exaltation) {
        entries << QString();
        return entries;
    }
    std::vector<uint8_t> tiers;
    if (classification.mode == 2)
        tiers = exaltation->allTiers();
    else if (classification.mode == 3)
        tiers = exaltation->tiers(static_cast<uint8_t>(classification.value));
    else
        return entries;
    for (const uint8_t tier : tiers)
        entries << qtTrId("market_filter_tier").arg(static_cast<uint>(tier));
    return entries;
}

TAppearanceTypeFilterSelection classificationFilterSelection(const bool enabled, const game::TGameSessionBase* session,
                                                             const int index)
{
    TAppearanceTypeFilterSelection selection;
    if (!enabled)
        return selection;
    if (index == 1)
        selection.mode = 1;
    if (index == 0)
        selection.mode = 2;
    exaltation::TExaltationStorage* exaltation = exaltationStorageOf(session);
    if (!exaltation)
        return selection;
    const std::vector<uint8_t> classifications = exaltation->classifications();
    if (index > 1 && index - 2 < static_cast<int>(classifications.size())) {
        selection.value = classifications[static_cast<size_t>(index - 2)];
        selection.mode = 3;
    }
    return selection;
}

TAppearanceTypeFilterSelection tierFilterSelection(const bool enabled, const game::TGameSessionBase* session,
                                                   const TAppearanceTypeFilterSelection& classification, const int index)
{
    TAppearanceTypeFilterSelection selection;
    if (!enabled)
        return selection;
    exaltation::TExaltationStorage* exaltation = exaltationStorageOf(session);
    if ((classification.mode != 2 && classification.mode != 3) || !exaltation)
        return selection;
    const std::vector<uint8_t> tiers = classification.mode == 3
                                           ? exaltation->tiers(static_cast<uint8_t>(classification.value))
                                           : exaltation->allTiers();
    if (index >= 0 && index < static_cast<int>(tiers.size())) {
        selection.value = tiers[static_cast<size_t>(index)];
        selection.mode = 3;
    }
    return selection;
}

TAppearanceTypeFilter appearanceTypeFilter(const bool filterByLevel, const bool filterByVocation, const bool onlyOneHandWeapons,
                                           const bool onlyTwoHandWeapons, TAppearanceTypeFilterSelection classification,
                                           TAppearanceTypeFilterSelection tier)
{
    TAppearanceTypeFilter filter;
    filter.filterByLevel = filterByLevel;
    filter.filterByVocation = filterByVocation;
    filter.onlyOneHandWeapons = onlyOneHandWeapons;
    filter.onlyTwoHandWeapons = onlyTwoHandWeapons;
    filter.classificationMode = classification.mode;
    filter.classification = classification.value;
    filter.classificationName = std::move(classification.name);
    filter.tierMode = tier.mode;
    filter.tier = tier.value;
    filter.tierName = std::move(tier.name);
    filter.imbuementMode = 0;
    filter.imbuementValue = 0;
    filter.imbuement = QString();
    return filter;
}

bool categoryHasClassifiedTypes(const TAppearanceTypeListModelSortFilterProxy& proxy, const int category)
{
    using Proxy = TAppearanceTypeListModelSortFilterProxy;
    const TAppearanceTypeListModel& model = proxy.model();
    const int rows = static_cast<int>(model.size());
    for (int row = 0; row < rows; ++row) {
        const QModelIndex index = model.index(row, 0);
        const int marketCategory = index.data(TAppearanceTypeListModel::MarketCategory).toInt();
        if (marketCategory == category || category == Proxy::AllCategories
            || (category == Proxy::AllWeaponsCategory && Proxy::isHandWeaponCategory(marketCategory))) {
            if (index.data(TAppearanceTypeListModel::Classification).toInt() != 0)
                return true;
        }
    }
    return false;
}

uint16_t displayTypeIdAt(const QAbstractItemModel& model, const int row)
{
    return static_cast<uint16_t>(model.index(row, 0).data(TAppearanceTypeListModel::DisplayTypeID).toInt());
}

}
