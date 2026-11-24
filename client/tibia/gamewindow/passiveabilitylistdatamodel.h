#pragma once

#include <cip_gen.h>

#include <QHash>

namespace tibia {
namespace game {
class TPassiveAbilityInfoStorage;
}
namespace gamewindow {

/// CIP gamewindow::TPassiveAbilityListDataModel: the passive abilities of the
/// info storage, one row each in the storage's order.
class TPassiveAbilityListDataModel final : public cip::tibia::gamewindow::TPassiveAbilityListDataModel
{
    Q_OBJECT
public:
    enum ERole {
        EnumIdRole = Qt::UserRole + 1,
        NameRole,
        IconIndexRole,
        IconUrlRole,
        /// Named for QML; the model itself has no value for it (the proxy
        /// answers it from the player's vocation).
        UsableByCurrentVocationRole,
    };

    explicit TPassiveAbilityListDataModel(const game::TPassiveAbilityInfoStorage* PassiveAbilityInfoStorage);

    const game::TPassiveAbilityInfoStorage* passiveAbilityInfoStorage() const { return m_passiveAbilityInfoStorage; }

    /// How many abilities the storage has.
    int rowCount(const QModelIndex& parent = {}) const override;
    /// The row's ability: its id, name, icon index or icon url.
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    const game::TPassiveAbilityInfoStorage* m_passiveAbilityInfoStorage;
};

}
}
