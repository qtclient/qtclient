#include "playerstates.h"

#include <algorithm>

namespace tibia::game {

namespace {

using config::SpecialCondition;
using config::SpecialConditionsFilter;
using config::SpecialConditionsOptions;

QString image(const char* provider, const int index) { return QStringLiteral("image://%1/%2").arg(QLatin1StringView(provider)).arg(index); }

/// CIP FUN_10101dd50.
QString rottenBloodTooltip(const int state)
{
    const bool finalTaint = state >= 5;
    const int level = finalTaint ? state - 4 : state;
    QStringList lines{ qtTrId("playerstatus_rotten_blood_intro") };
    if (finalTaint)
        lines << qtTrId("playerstatus_rotten_blood_intro_final_taint");
    if (level >= 1)
        lines << qtTrId("playerstatus_rotten_blood_monster_swap").arg(finalTaint ? 9 : 6);
    if (level >= 2)
        lines << qtTrId("playerstatus_rotten_blood_extra_monster").arg(finalTaint ? 9.375 : 6.25, 0, 'g');
    if (level >= 3)
        lines << qtTrId("playerstatus_rotten_blood_extra_spells");
    if (level >= 4)
        lines << qtTrId("playerstatus_rotten_blood_more_damage").arg(finalTaint ? 21 : 14);
    return lines.join(QStringLiteral("<br>\n"));
}

/// CIP FUN_10101beb0.
QString stateTooltip(const int bit)
{
    switch (bit) {
        case 0: return qtTrId("playerstatus_poisoned");
        case 1: return qtTrId("playerstatus_burning");
        case 2: return qtTrId("playerstatus_electrified");
        case 3: return qtTrId("playerstatus_drunk");
        case 4:
        case 26: return qtTrId("playerstatus_manashield");
        case 5: return qtTrId("playerstatus_paralysed");
        case 6: return qtTrId("playerstatus_hasted");
        case 7: return qtTrId("playerstatus_fighting");
        case 8: return qtTrId("playerstatus_drowning");
        case 9: return qtTrId("playerstatus_freezing");
        case 10: return qtTrId("playerstatus_dazzled");
        case 11: return qtTrId("playerstatus_cursed");
        case 12: return qtTrId("playerstatus_strengthened");
        case 13: return qtTrId("playerstatus_pzblock");
        case 14: return qtTrId("playerstatus_pz");
        case 15: return qtTrId("playerstatus_bleeding");
        case 16: return qtTrId("playerstatus_hex_lesser").arg(qtTrId("playerstatus_hex_lesser_effect"));
        case 17:
            return qtTrId("playerstatus_hex_intense").arg(qtTrId("playerstatus_hex_lesser_effect"), qtTrId("playerstatus_hex_intense_effect"));
        case 18:
            return qtTrId("playerstatus_hex_greater")
                .arg(qtTrId("playerstatus_hex_lesser_effect"), qtTrId("playerstatus_hex_intense_effect"),
                     qtTrId("playerstatus_hex_greater_effect"));
        case 19: return qtTrId("playerstatus_rooted");
        case 20: return qtTrId("playerstatus_feared");
        case 27: return qtTrId("playerstatus_agony");
        case 28: return qtTrId("playerstatus_powerless");
        case 29: return qtTrId("playerstatus_monkvirtue");
        default: break;
    }
    if (bit < 21 || bit > 25)
        return {};
    // Goshnar's taints: each level adds one penalty.
    const QString teleport = qtTrId("playerstatus_hrmonsterteleport_effect").arg(10);
    const QString spawn = qtTrId("playerstatus_hrspawnoffurthermonsters_effect").arg(0.5, 0, 'g');
    const QString damage = qtTrId("playerstatus_hrdamageincrease_effect").arg(15);
    const QString resurrection = qtTrId("playerstatus_hrmonsterresurrection_effect").arg(10);
    const QString loss = qtTrId("playerstatus_hrhpandmanaloss_effect").arg(10).arg(10);
    switch (bit) {
        case 21: return qtTrId("playerstatus_hrmonsterteleport").arg(teleport);
        case 22: return qtTrId("playerstatus_hrspawnoffurthermonsters").arg(teleport, spawn);
        case 23: return qtTrId("playerstatus_hrdamageincrease").arg(teleport, spawn, damage);
        case 24: return qtTrId("playerstatus_hrmonsterresurrection").arg(teleport, spawn, damage, resurrection);
        default: return qtTrId("playerstatus_hrhpandmanaloss").arg(teleport, spawn, damage, resurrection, loss);
    }
}

/// CIP FUN_10101f0e0.
QString playerKillerTooltip(const int flag)
{
    switch (flag) {
        case 1: return qtTrId("playerstatus_pkflag_attacker");
        case 2: return qtTrId("playerstatus_pkflag_partymode");
        case 3: return qtTrId("playerstatus_pkflag_aggressor");
        case 4: return qtTrId("playerstatus_pkflag_playerkiller");
        case 5: return qtTrId("playerstatus_pkflag_excessive_playerkiller");
        case 6: return qtTrId("playerstatus_pkflag_revenge");
        default: return {};
    }
}

}

QList<PlayerStateDisplayInfo> buildPlayerStates(const PlayerStatesInput& input, const SpecialConditionsOptions& options,
                                                const SpecialConditionsFilter filter)
{
    QList<PlayerStateDisplayInfo> entries;
    const auto add = [&](const SpecialCondition& key, QString imageSource, QString tooltip) {
        if (!options.visible(key, filter))
            return;
        entries.append({ std::move(imageSource), std::move(tooltip), options.sortOrder(key) });
    };

    if (input.rottenBlood != 0)
        add({ SpecialCondition::Generic, SpecialCondition::RottenBlood }, image("playerstate-flags-rottenblood", input.rottenBlood - 1),
            rottenBloodTooltip(input.rottenBlood));

    if (input.inRestingArea)
        add({ SpecialCondition::Generic, SpecialCondition::RestingArea }, image("playerstate-flags-client", input.restingAreaBonus ? 1 : 0),
            input.restingAreaText);

    const quint32 states = input.states & 0x3fffffffu;
    for (int bit = 0; bit < 30; ++bit) {
        if ((states & (1u << bit)) == 0)
            continue;
        // A protection zone block implies the logout block; only the first shows.
        if (bit == 7 && (states & (1u << 13)) != 0)
            continue;
        if (const auto key = SpecialConditionsOptions::forStateBit(bit))
            add(*key, image("playerstate-flags", bit), stateTooltip(bit));
    }

    if (input.playerKillerFlag != 0) {
        const SpecialCondition key{ SpecialCondition::PlayerKillerFlag, input.playerKillerFlag };
        if (SpecialConditionsOptions::isKnown(key))
            add(key, image("playerstate-playerkiller-flags", input.playerKillerFlag - 1), playerKillerTooltip(input.playerKillerFlag));
    }

    if (input.guildWar)
        add({ SpecialCondition::Generic, SpecialCondition::GuildWar }, image("playerstate-guildwar-flag", 0), qtTrId("playerstatus_guildwar"));

    if (input.hungry)
        add({ SpecialCondition::PlayerStateClient, SpecialCondition::Hungry }, image("playerstate-flags-client", 2), qtTrId("playerstatus_hungry"));

    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.sortOrder < b.sortOrder; });
    return entries;
}

TPlayerStateDisplayInfoModel::TPlayerStateDisplayInfoModel(QObject* parent)
    : cip::tibia::shared::TPlayerStateDisplayInfoModel(parent)
{}

int TPlayerStateDisplayInfoModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TPlayerStateDisplayInfoModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const PlayerStateDisplayInfo& entry = m_entries.at(index.row());
    switch (role) {
        case ImageSourceRole: return entry.imageSource;
        case TooltipRole: return entry.tooltip;
        case SortOrderRole: return entry.sortOrder;
        default: return {};
    }
}

QHash<int, QByteArray> TPlayerStateDisplayInfoModel::roleNames() const
{
    return { { ImageSourceRole, "imageSource" }, { TooltipRole, "tooltip" }, { SortOrderRole, "sortOrder" } };
}

void TPlayerStateDisplayInfoModel::setEntries(const QList<PlayerStateDisplayInfo>& entries)
{
    if (entries == m_entries)
        return;
    const int oldCount = static_cast<int>(m_entries.size());
    const int newCount = static_cast<int>(entries.size());
    if (newCount < oldCount) {
        beginRemoveRows({}, newCount, oldCount - 1);
        m_entries = entries;
        endRemoveRows();
    } else if (newCount > oldCount) {
        beginInsertRows({}, oldCount, newCount - 1);
        m_entries = entries;
        endInsertRows();
    } else {
        m_entries = entries;
    }
    if (newCount > 0)
        Q_EMIT dataChanged(index(0), index(newCount - 1));
}

}
