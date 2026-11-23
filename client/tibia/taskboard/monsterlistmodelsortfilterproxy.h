#pragma once

#include "taskboard/monsterlistfiltermodel.h"

#include <cip_gen.h>

#include <QSortFilterProxyModel>
#include <QString>

#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

namespace tibia::taskboard {

/// Every race passes the difficulty filter.
struct ShowAll
{
    friend bool operator==(ShowAll, ShowAll) { return true; }
    friend bool operator!=(ShowAll, ShowAll) { return false; }
};

/// The races of one bestiary difficulty, or all.
using TMonsterDifficultyFilter = std::variant<ShowAll, int>;

/// The monster race picker's list: owns the race model, filters it by
/// difficulty, name and the races to leave out, and sorts the races whose
/// animus mastery is not completed first, each part by name.
class TMonsterListModelSortFilterProxy final : public cip::tibia::taskboard::TMonsterListModelSortFilterProxy
{
    Q_OBJECT
public:
    explicit TMonsterListModelSortFilterProxy(std::vector<uint16_t> raceIds, QObject* parent = nullptr);
    ~TMonsterListModelSortFilterProxy() override;

    Q_INVOKABLE void setNameFilter(const QString& NameFilter) override;
    Q_INVOKABLE QVariant getRaceIDAtIndex(int RowIndex) override;

    /// The races the list leaves out (the ones already preferred).
    void setExcludedRaceIds(const std::vector<uint32_t>& raceIds);

    /// The races of one difficulty, or all; a changed filter refilters.
    void setDifficultyFilter(const TMonsterDifficultyFilter& DifficultyFilter);

    /// Host: the owned race model (qtclient fills it with the bounty data).
    TMonsterListFilterModel* monsterListFilterModel() const { return m_monsterListFilterModel.get(); }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    std::unique_ptr<TMonsterListFilterModel> m_monsterListFilterModel;
    TMonsterDifficultyFilter m_difficultyFilter;
    QString m_nameFilter;
    std::vector<uint32_t> m_excludedRaceIds;
};

}
