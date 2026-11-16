#pragma once

#include "shared/cip_typed.h"

/// CIP anonymous-namespace QAbstractItemModel / DTO names from the pin units.
/// Live models often live under feature controllers; these shells keep CIP names.

namespace tibia {
namespace gamewindow {

class TAccountBadgesSortProxyModel final : public ::tibia::CipTyped { public: TAccountBadgesSortProxyModel() = default; };
class TBossTrackerModel final : public ::tibia::CipTyped { public: TBossTrackerModel() = default; };
class TBossTrackerSortProxyModel final : public ::tibia::CipTyped { public: TBossTrackerSortProxyModel() = default; };
class TBossesModel final : public ::tibia::CipTyped { public: TBossesModel() = default; };
class TDuePaymentModel final : public ::tibia::CipTyped { public: TDuePaymentModel() = default; };
class TFriendListSortProxyModel final : public ::tibia::CipTyped { public: TFriendListSortProxyModel() = default; };
class TMyException final : public ::tibia::CipTyped { public: TMyException() = default; };
class TPerkInfoModel final : public ::tibia::CipTyped { public: TPerkInfoModel() = default; };
class TPreyBestiarySelectionSortProxyModel final : public ::tibia::CipTyped { public: TPreyBestiarySelectionSortProxyModel() = default; };
class TQuestTrackerModel final : public ::tibia::CipTyped { public: TQuestTrackerModel() = default; };
class TQuestTrackerSortProxyModel final : public ::tibia::CipTyped { public: TQuestTrackerSortProxyModel() = default; };
class TSortedDamageSourceModel final : public ::tibia::CipTyped { public: TSortedDamageSourceModel() = default; };
class TSortedDamageTypedModel final : public ::tibia::CipTyped { public: TSortedDamageTypedModel() = default; };
class TSortedTypedDamageModel final : public ::tibia::CipTyped { public: TSortedTypedDamageModel() = default; };
class TTypedDamageModel final : public ::tibia::CipTyped { public: TTypedDamageModel() = default; };
class TVisibilitySettingsModel final : public ::tibia::CipTyped { public: TVisibilitySettingsModel() = default; };
class TWorldTypeModel final : public ::tibia::CipTyped { public: TWorldTypeModel() = default; };

}
}
