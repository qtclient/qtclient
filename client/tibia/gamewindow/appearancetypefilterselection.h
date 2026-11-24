#pragma once

#include <QString>
#include <QStringList>

#include <cstdint>

class QAbstractItemModel;

namespace tibia {
namespace game {
class TGameSessionBase;
}
namespace gamewindow {

struct TAppearanceTypeFilter;
class TAppearanceTypeListModelSortFilterProxy;

/// One drop-down selection of TAppearanceTypeFilter (CIP: the 0x20-byte
/// mode / value / label triple at filter +0x08, +0x28 and +0x48). The mode
/// is 0 off, 1 none only, 2 any, 3 `value` only.
struct TAppearanceTypeFilterSelection
{
    int mode = 0;
    int value = 0;
    QString name;
};

// The classification and tier drop-downs shared by the market, the depot
// search widget and the item info dialog (CIP: free functions of the item
// filter header, emitted once for the three controllers).

/// The classification drop-down's entries: one empty entry when the
/// selection is disabled, else "all", "none" and one entry per exaltation
/// classification of the session.
QStringList classificationFilterEntries(bool enabled, const game::TGameSessionBase* session);
/// The tier drop-down's entries for a classification selection: one empty
/// entry when disabled or without a session; the session's tiers of every
/// classification (mode 2) or of the selected one (mode 3); else none.
QStringList tierFilterEntries(bool enabled, const game::TGameSessionBase* session,
                              const TAppearanceTypeFilterSelection& classification);
/// The classification selection of drop-down row `index`: row 0 any, row 1
/// none, row 2.. the session's classifications.
TAppearanceTypeFilterSelection classificationFilterSelection(bool enabled, const game::TGameSessionBase* session, int index);
/// The tier selection of drop-down row `index` for a classification
/// selection of mode 2 or 3; off otherwise or when `index` is out of range.
TAppearanceTypeFilterSelection tierFilterSelection(bool enabled, const game::TGameSessionBase* session,
                                                   const TAppearanceTypeFilterSelection& classification, int index);
/// The filter of the four buttons with the classification and tier
/// selections; the imbuement selection is off.
TAppearanceTypeFilter appearanceTypeFilter(bool filterByLevel, bool filterByVocation, bool onlyOneHandWeapons,
                                           bool onlyTwoHandWeapons, TAppearanceTypeFilterSelection classification,
                                           TAppearanceTypeFilterSelection tier);
/// Whether a type of the market category (0xff all, 0x1d every hand weapon
/// category) in the proxy's model has a classification.
bool categoryHasClassifiedTypes(const TAppearanceTypeListModelSortFilterProxy& proxy, int category);
/// The display type id of row `row` of `model`.
uint16_t displayTypeIdAt(const QAbstractItemModel& model, int row);

}
}
