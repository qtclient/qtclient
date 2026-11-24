#pragma once

#include "gamewindow/passiveabilitylistdatamodel.h"

#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <cstdint>
#include <memory>

namespace tibia {
namespace game {
class IGameSession;
class TPassiveAbilityInfoStorage;
class TPlayerData;
}
namespace gamewindow {

/// CIP gamewindow::TPassiveAbilityListDataModelSortFilterProxy: the passive
/// ability list of the assign dialog, sorted by name. It owns its source model
/// and follows the session's player for the vocation filter and the
/// usableByCurrentVocation role.
class TPassiveAbilityListDataModelSortFilterProxy final : public cip::tibia::gamewindow::TPassiveAbilityListDataModelSortFilterProxy
{
    Q_OBJECT
public:
    explicit TPassiveAbilityListDataModelSortFilterProxy(const game::TPassiveAbilityInfoStorage* PassiveAbilityInfoStorage);
    ~TPassiveAbilityListDataModelSortFilterProxy() override;

    /// Follows the session's player data (none without a session).
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// The id of the ability shown in `RowIndex`.
    uint8_t passiveAbilityAt(int RowIndex) const;

    /// The source model's data; usableByCurrentVocation is whether the ability
    /// lists the player's vocation.
    QVariant data(const QModelIndex& index, int role) const override;

public Q_SLOTS:
    /// The role's value in the row, as a string.
    QVariant getDataWithRoleName(int RowIndex, const QByteArray& RoleName) override;
    /// Takes the player's vocation and level; refilters when either changed.
    void onPlayerDataChanged() override;

protected:
    /// With the vocation filter on, only abilities the player's vocation can use.
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    /// By the lower-cased names.
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    std::shared_ptr<TPassiveAbilityListDataModel> m_model;
    shared::TQtConnectionManager m_gameSessionConnections;
    /// Never switched on: the filter has no setter that runs.
    bool m_showOnlyCurrentVocation = false;
    uint8_t m_playerVocation = 0;
    uint16_t m_playerLevel = 0;
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::shared_ptr<game::TPlayerData> m_playerData;
};

}
}
