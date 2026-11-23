#pragma once

#include <memory>

#include <cip_gen.h>

#include <QObject>
#include <QString>

#include <cstdint>
#include <vector>

namespace tibia {
namespace taskboard {

/// The hunting task shop offer kinds (the server's 0..4).
enum class EShopRewardType : int
{
    Object = 0,
    Mount = 1,
    Outfit = 2,
    TwoObjects = 3,
    PromotionPoint = 4,
};

/// The offer state (the server's 0..4; 4 is bought).
enum class EShopRewardStatus : int
{
    Available = 0,
    NotAvailable = 1,
    NotEnoughPoints = 2,
    RequiresBaseOutfit = 3,
    Bought = 4,
};

struct TShopReward
{
    QString name;
    QString description;
    uint32_t price = 0;
    EShopRewardType type = EShopRewardType::Object;
    uint32_t appearanceId = 0;
    uint32_t secondObjectTypeId = 0;
    uint16_t purchaseCount = 0;
    bool firstAddon = false;
    bool secondAddon = false;
    EShopRewardStatus status = EShopRewardStatus::Available;
};

/// The hunting task shop offers.
class TTaskboardShopStorage final : public cip::tibia::taskboard::TTaskboardShopStorage
{
    Q_OBJECT
public:
    explicit TTaskboardShopStorage(QObject* parent = nullptr);

    void setShopRewardData(const std::vector<TShopReward>& data);
    const std::vector<TShopReward>& shopRewardData() const { return m_data; }

private:
    std::vector<TShopReward> m_data;
};

}
}
