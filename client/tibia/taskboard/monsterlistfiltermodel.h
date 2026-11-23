#pragma once

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QVariant>

#include <cstdint>
#include <vector>

namespace tibia::taskboard {

/// The monster races a bounty task's preferred monster (or a soul seal) can
/// be picked from: one row a race id.
class TMonsterListFilterModel final : public cip::tibia::taskboard::TMonsterListFilterModel
{
    Q_OBJECT
public:
    enum Roles {
        RaceIdRole = Qt::UserRole + 2,
        NameRole,
        SoulPitFightPriceRole,
        AnimusMasteryCompletedRole,
    };

    explicit TMonsterListFilterModel(std::vector<uint16_t> raceIds, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    /// The race id, its capitalised name, the soul pit fight price of its
    /// difficulty and whether its animus mastery is completed.
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// The race id of a row; 0 past the last row.
    uint16_t raceIdAt(int row) const;

    /// Host: qtclient's bounty data brings the race list after the model is
    /// built (CipSoft passes it to the constructor).
    void setRaceIds(std::vector<uint16_t> raceIds);

    /// The races whose animus mastery is completed (no reset: the owner
    /// invalidates its proxy).
    void setCompletedAnimusMasteryRaceIds(std::vector<uint16_t> raceIds) { m_completedAnimusMasteryRaceIds = std::move(raceIds); }

private:
    std::vector<uint16_t> m_raceIds;
    std::vector<uint16_t> m_completedAnimusMasteryRaceIds;
};

/// Soul pit fight price of a bestiary difficulty: 10 a step from 10 (the
/// harmless races) up; difficulties above 5 cost nothing.
int soulPitFightPrice(unsigned difficulty);

}
