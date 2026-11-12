#include "specialconditionobject.h"

namespace tibia::config {

namespace {

using C = SpecialCondition;

QString image(const char* provider, const int index) { return QStringLiteral("image://%1/%2").arg(QLatin1StringView(provider)).arg(index); }

/// EPlayerStateFlag names, indexed by bit.
const char* const StateKeys[] = {
    "poisoned", "fire", "energy", "drunk", "manashield_group", "gostrengthslow", "gostrengthboost", "logoutblock",
    "drowning", "ice", "holy", "death", "strengthened", "protectionzoneblock", "inprotectionzone", "bleeding",
    "hexhealing", "hexdamage", "hexhitpoints", "rooted", "feared", "highrisk_group", "highrisk_group", "highrisk_group",
    "highrisk_group", "highrisk_group", "manashield_group", "agony", "powerless", "monkvirtue",
};

/// EPlayerStateFlag tooltips, indexed by bit.
const char* const StateTooltipKeys[] = {
    "poisoned", "burning", "electrified", "drunk", "manashield", "paralysed", "hasted", "logoutblock",
    "drowning", "freezing", "dazzled", "cursed", "strengthened", "pzblock", "pz", "bleeding",
    "hexhealing", "hexdamage", "hexhitpoints", "rooted", "feared", "highrisk_group", "highrisk_group", "highrisk_group",
    "highrisk_group", "highrisk_group", "manashield", "agony", "powerless", "monkvirtue",
};

/// EPlayerKillerFlag 1..6.
const char* const PlayerKillerKeys[] = { "attacker", "partymode", "aggressor", "playerkiller", "excplayerkiller", "revenge" };

/// ESpecialConditionGenericDataType names / tooltips.
const char* const GenericKeys[] = { "isinguildwar", "highrisk_group", "rottenblood_group", "client_restingarea_group", "manashield_group" };
const char* const GenericTooltipKeys[] = { "isinguildwar", "highrisk_group", "rottenblood_group", "restingarea_group", "manashield" };

QString trId(const QString& id) { return qtTrId(id.toLatin1().constData()); }

QString status(const char* key, const char* suffix) { return trId(QStringLiteral("playerstatus_%1_%2").arg(QLatin1StringView(key), QLatin1StringView(suffix))); }

template <size_t N>
const char* at(const char* const (&keys)[N], const int index)
{
    return index >= 0 && index < static_cast<int>(N) ? keys[index] : nullptr;
}

}

TSpecialConditionObject::TSpecialConditionObject(const SpecialCondition& condition, const int listIndex, const bool showInHUD,
                                                 const bool showInBar, QObject* parent)
    : cip::tibia::config::TSpecialConditionObject(parent)
{
    m_specialConditionInfo = name(condition);
    m_showInHUD = showInHUD;
    m_showInBar = showInBar;
    m_conditionListEntryID = condition.type * 1000 + condition.value;
    m_conditionListIndex = listIndex;
    m_iconSource = icon(condition);
    m_iconTooltip = tooltip(condition);
}

QString TSpecialConditionObject::name(const SpecialCondition& condition)
{
    switch (condition.type) {
        case C::PlayerStateFlag:
            if (const char* key = at(StateKeys, condition.value))
                return status(key, "name");
            return QStringLiteral("Unknown");
        case C::PlayerStateClient:
            return condition.value == C::Hungry ? qtTrId("playerstatus_client_hungry_name") : qtTrId("playerstatus_client_restingarea_group_name");
        case C::RottenBloodState:
            return qtTrId("playerstatus_rottenblood_group_name");
        case C::PlayerKillerFlag:
            if (const char* key = at(PlayerKillerKeys, condition.value - 1))
                return trId(QStringLiteral("playerstatus_pkflag_%1_name").arg(QLatin1StringView(key)));
            return {};
        case C::Generic:
            if (const char* key = at(GenericKeys, condition.value))
                return status(key, "name");
            return {};
    }
    return {};
}

QString TSpecialConditionObject::tooltip(const SpecialCondition& condition)
{
    switch (condition.type) {
        case C::PlayerStateFlag:
            if (const char* key = at(StateTooltipKeys, condition.value))
                return status(key, "description_tooltip");
            return {};
        case C::PlayerStateClient:
            if (condition.value == C::Hungry)
                return qtTrId("playerstatus_client_hungry_description_tooltip");
            return condition.value < C::Hungry ? qtTrId("playerstatus_restingarea_group_description_tooltip") : QString();
        case C::RottenBloodState:
            return qtTrId("playerstatus_rottenblood_group_description_tooltip");
        case C::PlayerKillerFlag:
            if (const char* key = at(PlayerKillerKeys, condition.value - 1))
                return trId(QStringLiteral("playerstatus_pkflag_%1_description_tooltip").arg(QLatin1StringView(key)));
            return {};
        case C::Generic:
            if (const char* key = at(GenericTooltipKeys, condition.value))
                return status(key, "description_tooltip");
            return {};
    }
    return {};
}

QString TSpecialConditionObject::icon(const SpecialCondition& condition)
{
    switch (condition.type) {
        case C::PlayerStateFlag:
            if (condition.value >= 21 && condition.value <= 25)
                return image("playerstate-flags", 25);
            if (condition.value == 4 || condition.value == 26)
                return image("playerstate-flags", 26);
            return image("playerstate-flags", condition.value);
        case C::PlayerStateClient:
            return image("playerstate-flags-client", condition.value);
        case C::RottenBloodState:
            return image("playerstate-flags-rottenblood", 8);
        case C::PlayerKillerFlag:
            return image("playerstate-playerkiller-flags", condition.value - 1);
        case C::Generic:
            switch (condition.value) {
                case C::GuildWar: return image("playerstate-guildwar-flag", 0);
                case C::Goshnar: return image("playerstate-flags", 25);
                case C::RottenBlood: return image("playerstate-flags-rottenblood", 8);
                case C::RestingArea: return image("playerstate-flags-client", 0);
                case C::ManaShield: return image("playerstate-flags", 26);
            }
            return {};
    }
    return {};
}

}
