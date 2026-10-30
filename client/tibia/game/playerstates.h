#pragma once

#include "config/specialconditionsoptions.h"

#include <cip_gen.h>

#include <QList>
#include <QString>

namespace tibia::game {

/// What the player-state icons are built from (CIP: TPlayerInventoryAndStatusController
/// 0x100b779f0 gathers it from TPlayerData and the player's creature).
struct PlayerStatesInput
{
    /// EPlayerStateFlag bits 0..29.
    quint32 states = 0;
    /// Bakragore's taint, 0 none.
    quint8 rottenBlood = 0;
    bool inRestingArea = false;
    bool restingAreaBonus = false;
    QString restingAreaText;
    /// EPlayerKillerFlag (the creature's skull), 0 none.
    quint8 playerKillerFlag = 0;
    bool guildWar = false;
    bool hungry = false;
};

/// One row of TPlayerStateDisplayInfoModel.
struct PlayerStateDisplayInfo
{
    QString imageSource;
    QString tooltip;
    int sortOrder = 0;

    bool operator==(const PlayerStateDisplayInfo&) const = default;
};

/// The icons for `input`, sorted by the options' order (CIP: FUN_10101f1f0).
QList<PlayerStateDisplayInfo> buildPlayerStates(const PlayerStatesInput& input, const config::SpecialConditionsOptions& options,
                                                config::SpecialConditionsFilter filter);

/// TPlayerStateDisplayInfoModel: roles imageSource, tooltip and sortOrder.
class TPlayerStateDisplayInfoModel final : public cip::tibia::shared::TPlayerStateDisplayInfoModel
{
    Q_OBJECT
public:
    enum Role { ImageSourceRole = Qt::UserRole + 1, TooltipRole, SortOrderRole };

    explicit TPlayerStateDisplayInfoModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Resizes the rows to fit, then refreshes them all (CIP: vtable slot 49).
    void setEntries(const QList<PlayerStateDisplayInfo>& entries);

private:
    QList<PlayerStateDisplayInfo> m_entries;
};

}
