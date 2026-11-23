#pragma once

#include "shared/simplevectorabstractlistmodel.h"
#include "taskboard/taskboardshopstorage.h"

#include <cip_gen.h>

#include <QHash>
#include <QVariant>

namespace tibia {
namespace taskboard {

/// The hunting task shop offers (TaskboardRewardShopDialogTab.qml): one row
/// per offer of the shop storage.
class TTaskboardRewardListModel final
    : public shared::TSimpleVectorAbstractListModel<TShopReward, cip::tibia::taskboard::TTaskboardRewardListModel>
{
    Q_OBJECT
public:
    enum Role
    {
        TitleRole = 0x102,
        DescriptionRole,
        CostRole,
        CostStringRole,
        RewardTypeRole,
        IdRole,
        OptionalIdRole,
        AddOn1Role,
        AddOn2Role,
        CanBuyRole,
        NoBuyReasonStringRole,
        AlreadyOwnedRole,
        /// Host (pack ABI): the pack's TaskboardRewardShopDialogTab.qml binds
        /// `appearanceId` as a required property; same value as `id`.
        AppearanceIdRole,
    };

    using TSimpleVectorAbstractListModel::TSimpleVectorAbstractListModel;

    QHash<int, QByteArray> roleNames() const override;

protected:
    QVariant itemData(const TShopReward& item, int role) const override;
};

}
}
