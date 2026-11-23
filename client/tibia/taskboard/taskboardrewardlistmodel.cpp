#include "taskboard/taskboardrewardlistmodel.h"

#include "common/plural.h"
#include "cyclopedia/cyclopediaciptypes.h"

#include <QLocale>

namespace tibia::taskboard {

QHash<int, QByteArray> TTaskboardRewardListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TitleRole] = "title";
    roles[DescriptionRole] = "description";
    roles[CostRole] = "cost";
    roles[CostStringRole] = "costString";
    roles[RewardTypeRole] = "rewardType";
    roles[IdRole] = "id";
    roles[OptionalIdRole] = "optionalId";
    roles[AddOn1Role] = "addOn1";
    roles[AddOn2Role] = "addOn2";
    roles[CanBuyRole] = "canBuy";
    roles[NoBuyReasonStringRole] = "noBuyReasonString";
    roles[AlreadyOwnedRole] = "alreadyOwned";
    roles[AppearanceIdRole] = "appearanceId";
    return roles;
}

QVariant TTaskboardRewardListModel::itemData(const TShopReward& item, const int role) const
{
    switch (role) {
    case TitleRole:
        // The name without its article, every word capitalised.
        return cyclopedia::capitalizeWords(
            QString::fromUtf8(common::plural(item.name.toStdString(), -1)));
    case DescriptionRole:
        return item.description;
    case CostRole:
        return item.price;
    case CostStringRole:
        return QLocale(QLocale::English).toString(static_cast<qulonglong>(item.price));
    case RewardTypeRole:
        switch (item.type) {
        case ::tibia::taskboard::EShopRewardType::Object:
            return 0;
        case ::tibia::taskboard::EShopRewardType::Mount:
            return 1;
        case ::tibia::taskboard::EShopRewardType::Outfit:
            return 2;
        case ::tibia::taskboard::EShopRewardType::TwoObjects:
            return 3;
        case ::tibia::taskboard::EShopRewardType::PromotionPoint:
            return 4;
        }
        // An unknown type falls through to the id, like CipSoft's switch.
        return item.appearanceId;
    case IdRole:
    case AppearanceIdRole:
        return item.appearanceId;
    case OptionalIdRole:
        return item.secondObjectTypeId;
    case AddOn1Role:
        return item.firstAddon;
    case AddOn2Role:
        return item.secondAddon;
    case CanBuyRole:
        return item.status == EShopRewardStatus::Available;
    case NoBuyReasonStringRole:
        switch (item.status) {
        case EShopRewardStatus::Available:
            return QString::fromUtf8("");
        case EShopRewardStatus::NotAvailable:
            return qtTrId("taskboard_shop_tooltip_unknown_availability");
        case EShopRewardStatus::NotEnoughPoints:
            return qtTrId("taskboard_shop_tooltip_cannot_afford");
        case EShopRewardStatus::RequiresBaseOutfit:
            return qtTrId("taskboard_shop_tooltip_requires_base_outfit");
        case EShopRewardStatus::Bought:
            return qtTrId("taskboard_shop_tooltip_already_owned");
        }
        // An unknown status answers like canBuy.
        return item.status == EShopRewardStatus::Available;
    case AlreadyOwnedRole:
        return item.status == EShopRewardStatus::Bought;
    default:
        return {};
    }
}

}
