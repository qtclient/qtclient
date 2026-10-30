#include "taskboardshopcontroller.h"

#include <client/const.h>
#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <client/outfit.h>

#include <QLocale>

#include <algorithm>
#include <climits>
#include <cstdlib>

namespace tibia::gamewindow {

namespace {

using ShopMap = std::map<std::string, std::string>;

QString mapStr(const ShopMap& map, const char* key)
{
    const auto it = map.find(key);
    return it == map.end() ? QString() : QString::fromStdString(it->second);
}

uint32_t mapU32(const ShopMap& map, const char* key, const uint32_t fallback = 0)
{
    const auto it = map.find(key);
    if (it == map.end() || it->second.empty())
        return fallback;
    return static_cast<uint32_t>(std::strtoul(it->second.c_str(), nullptr, 10));
}

QColor outfitColor(const int index)
{
    const Color c = Outfit::getColor(index);
    return QColor(c.r(), c.g(), c.b());
}

uint64_t huntingPoints()
{
    const LocalPlayerPtr player = g_game.getLocalPlayer();
    return player ? player->getResourceBalance(Otc::RESOURCE_TASK_HUNTING) : 0;
}

int toRewardType(const uint8_t offerType)
{
    using R = cip::tibia::taskboard::TTaskboardRewardListModel::EShopRewardType;
    switch (offerType) {
    case Otc::TASK_BOARD_SHOP_OFFER_MOUNT:
        return R::Mount;
    case Otc::TASK_BOARD_SHOP_OFFER_OUTFIT:
        return R::Outfit;
    case Otc::TASK_BOARD_SHOP_OFFER_ITEM_DOUBLE:
        return R::Bed;
    case Otc::TASK_BOARD_SHOP_OFFER_BONUS_PROMOTION:
        return R::PromotionPoint;
    default:
        return R::Object;
    }
}

QString formatCost(const uint32_t price)
{
    return QLocale::system().toString(static_cast<qulonglong>(price));
}

}

TTaskboardShopController::TTaskboardShopController(QObject* parent)
    : cip::tibia::gamewindow::TTaskboardShopController(parent)
    , m_rewardsModel(new taskboard::TTaskboardRewardListModel(this))
{
    m_rewardList = m_rewardsModel;
    syncPlayerColors();
    refreshHuntingTaskPoints();
}

void TTaskboardShopController::showDialogTab()
{
    syncPlayerColors();
    refreshHuntingTaskPoints();
    g_game.taskHuntingShopRequest();
}

void TTaskboardShopController::hideDialogTab() {}

void TTaskboardShopController::updateRewardList()
{
    Q_EMIT rewardListChanged();
}

void TTaskboardShopController::requestBuyOffer(const int Index)
{
    const taskboard::ShopRewardRow* row = m_rewardsModel->rowAt(Index);
    if (!row || row->alreadyOwned || !row->canBuy)
        return;
    if (huntingPoints() < static_cast<uint64_t>(row->cost))
        return;
    g_game.taskHuntingShopPurchase(static_cast<uint8_t>(std::clamp(row->offerIndex, 0, 255)));
}

void TTaskboardShopController::refreshHuntingTaskPoints()
{
    const int points = static_cast<int>(std::min<uint64_t>(huntingPoints(), INT_MAX));
    if (points == m_huntingTaskPoints)
        return;
    m_huntingTaskPoints = points;
    Q_EMIT huntingTaskPointsChanged();
}

void TTaskboardShopController::syncPlayerColors()
{
    const LocalPlayerPtr player = g_game.getLocalPlayer();
    if (!player)
        return;
    const Outfit& outfit = player->getOutfit();
    m_headColor = outfitColor(outfit.getHead());
    m_torsoColor = outfitColor(outfit.getBody());
    m_legsColor = outfitColor(outfit.getLegs());
    m_detailColor = outfitColor(outfit.getFeet());
}

void TTaskboardShopController::applyShopData(const std::vector<ShopMap>& offers)
{
    refreshHuntingTaskPoints();
    syncPlayerColors();

    QVector<taskboard::ShopRewardRow> rows;
    rows.reserve(static_cast<int>(offers.size()));

    for (size_t i = 0; i < offers.size(); ++i) {
        const ShopMap& raw = offers[i];
        taskboard::ShopRewardRow row;
        row.offerIndex = static_cast<int>(mapU32(raw, "id", static_cast<uint32_t>(i)));
        const auto offerType = static_cast<uint8_t>(mapU32(raw, "offerType"));
        row.rewardType = toRewardType(offerType);
        row.title = mapStr(raw, "title");
        row.description = mapStr(raw, "description");
        row.cost = static_cast<int>(mapU32(raw, "price"));
        row.costString = formatCost(static_cast<uint32_t>(row.cost));
        row.alreadyOwned = mapU32(raw, "bought") != 0;
        row.canBuy = !row.alreadyOwned;
        row.noBuyReasonString.clear();

        const uint32_t lookType = mapU32(raw, "lookType");
        const uint32_t itemId = mapU32(raw, "itemId");
        const uint8_t addons = static_cast<uint8_t>(mapU32(raw, "lookAddons"));
        row.addOn1 = (addons & 1) != 0;
        row.addOn2 = (addons & 2) != 0;
        row.optionalId = static_cast<int>(mapU32(raw, "optionalItemId"));

        if (offerType == Otc::TASK_BOARD_SHOP_OFFER_BONUS_PROMOTION) {
            const uint32_t current = mapU32(raw, "currentPurchases");
            row.title = QStringLiteral("Bonus Promotion Point");
            row.description = QStringLiteral(
                                  "Earn up to 50 Promotion Points to spend in your Wheel of Destiny.\n"
                                  "Already purchased %1 / 50.")
                                  .arg(current);
            row.id = 0;
        } else if (offerType == Otc::TASK_BOARD_SHOP_OFFER_OUTFIT || offerType == Otc::TASK_BOARD_SHOP_OFFER_MOUNT) {
            row.id = static_cast<int>(lookType);
        } else {
            row.id = static_cast<int>(itemId);
        }

        rows.push_back(std::move(row));
    }

    m_rewardsModel->setRows(std::move(rows));
    Q_EMIT rewardListChanged();
}

}
