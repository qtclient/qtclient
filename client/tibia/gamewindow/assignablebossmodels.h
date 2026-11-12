#pragma once

#include "cyclopedia/bosstiarystorage.h"

#include <cip_gen.h>

#include <vector>

namespace tibia {
namespace gamewindow {

/// The bosses a slot can be set to (CIP: TAssignableBossModel, data).
class TAssignableBossModel final : public cip::TAssignableBossModel
{
    Q_OBJECT
public:
    enum Role
    {
        BossRaceId = 0x101,
        Name,
        BossRaritySource,
        BossRarityTooltip,
    };

    TAssignableBossModel(const cyclopedia::TBosstiaryStorage& storage, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const override;

    void setBosses(const std::vector<cyclopedia::TAssignableBoss>& bosses);
    void refreshAll();

private:
    const cyclopedia::TBosstiaryStorage& m_storage;
    std::vector<cyclopedia::TAssignableBoss> m_bosses;
};

/// CIP: TAssignableBossSortProxyModel (filter , lessThan).
class TAssignableBossSortProxyModel final : public cip::TAssignableBossSortProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int raceIDRoleEnumValue READ raceIDRoleEnumValue CONSTANT)
public:
    TAssignableBossSortProxyModel(const cyclopedia::TBosstiaryStorage& storage, QObject* parent = nullptr);

    TAssignableBossModel* source() const { return m_source; }

    QString nameFilter() const override;
    void setNameFilter(const QString& v) override;
    int raceIDRoleEnumValue() const override;
    /// CIP .
    void resetFilter() override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    TAssignableBossModel* m_source;
};

}
}
