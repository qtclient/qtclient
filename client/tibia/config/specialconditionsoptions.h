#pragma once

#include <QJsonObject>
#include <QList>
#include <QSet>

#include <compare>
#include <optional>

namespace tibia::config {

/// One entry of clientoptions.json's specialConditionsOptions: CIP's
/// std::variant<EPlayerStateFlag, EPlayerStateClient, ERottenBloodState,
/// EPlayerKillerFlag, ESpecialConditionGenericDataType>, stored as
/// [variant index, value].
struct SpecialCondition
{
    enum Type { PlayerStateFlag = 0, PlayerStateClient = 1, RottenBloodState = 2, PlayerKillerFlag = 3, Generic = 4 };
    /// ESpecialConditionGenericDataType.
    enum GenericType { GuildWar = 0, Goshnar = 1, RottenBlood = 2, RestingArea = 3, ManaShield = 4 };
    /// EPlayerStateClient.
    enum ClientState { Hungry = 2 };

    int type = 0;
    int value = 0;

    auto operator<=>(const SpecialCondition&) const = default;
};

inline size_t qHash(const SpecialCondition& c, size_t seed = 0) { return ::qHash((c.type << 16) | c.value, seed); }

/// Where a special condition shows (CIP's filter argument of FUN_10101f1f0).
enum class SpecialConditionsFilter { HUD = 1, Bar = 2 };

/// clientoptions.json's specialConditionsOptions (CIP: load FUN_1009ea0b0,
/// save FUN_1009e9b20).
struct SpecialConditionsOptions
{
    bool showInHUD = true;
    bool showInBar = true;
    QSet<SpecialCondition> visibleInHUD;
    QSet<SpecialCondition> visibleInBar;
    QList<SpecialCondition> specialConditionsOrder;

    static SpecialConditionsOptions load();
    static SpecialConditionsOptions fromJson(const QJsonObject& json);
    /// CIP's default file (:/conf/clientoptions.json), for "reset to default".
    static SpecialConditionsOptions defaults();
    /// The section as CIP stores it ([type, value] pairs, sets sorted).
    QJsonObject toJson() const;

    bool visible(const SpecialCondition& condition, SpecialConditionsFilter filter) const;
    /// The condition's place in specialConditionsOrder, 9999 when absent.
    int sortOrder(const SpecialCondition& condition) const;

    /// Every condition the client knows, in the default order (DAT_10279f3a0).
    static const QList<SpecialCondition>& known();
    static bool isKnown(const SpecialCondition& condition);
    /// The condition shown for player state bit `bit` (FUN_10101f1f0): Goshnar's
    /// taints share one entry, as do the two mana shield bits.
    static std::optional<SpecialCondition> forStateBit(int bit);
};

}
