#include "imbuingstorage.h"

#include <tibia/game/gamesession.h>
#include <client/item.h>

#include <algorithm>
#include <tuple>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::imbuing {

namespace {

using Args = std::vector<std::any>;
using ActiveSlots = std::unordered_map<int, std::tuple<Imbuement, uint32_t, uint32_t>>;

/// CIP : the dialog types (mask 0xc0f) shown as an imbuing result.
bool isResultMessageType(const int type)
{
    return type >= 0 && type < 12 && (0xc0f >> type & 1);
}

}

TImbuingStorage::TImbuingStorage(QObject* parent)
    : cip::tibia::imbuing::TImbuingStorage(parent)
{
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onOpenImbuementWindow", [this](GameObject*, const Args&) {
        refresh(MODE_NONE, 0, 0, {}, {}, nullptr);
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onImbuementItem", [this](GameObject*, const Args& args) {
        const auto* active = args.size() > 3 ? std::any_cast<ActiveSlots>(&args[3]) : nullptr;
        const auto* imbuements = args.size() > 4 ? std::any_cast<std::vector<Imbuement>>(&args[4]) : nullptr;
        const auto* owned = args.size() > 5 ? std::any_cast<std::vector<std::pair<uint16_t, uint16_t>>>(&args[5]) : nullptr;
        const int count = static_cast<int>(client::gamecore::argInt(args, 2));
        std::vector<TImbuementSlot> slots(static_cast<size_t>(std::max(count, 0)));
        for (int i = 0; i < count; ++i) {
            TImbuementSlot& slot = slots[i];
            slot.index = i;
            if (!active)
                continue;
            if (const auto it = active->find(i); it != active->end()) {
                slot.isEmpty = false;
                std::tie(slot.imbuement, slot.remainingDuration, slot.clearingGoldCost) = it->second;
            }
        }
        refresh(MODE_ITEM, static_cast<uint16_t>(client::gamecore::argInt(args, 0)), static_cast<uint8_t>(client::gamecore::argInt(args, 1)),
                std::move(slots), imbuements ? *imbuements : std::vector<Imbuement>{}, owned);
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onImbuementScroll", [this](GameObject*, const Args& args) {
        const auto* imbuements = args.empty() ? nullptr : std::any_cast<std::vector<Imbuement>>(&args[0]);
        const auto* owned = args.size() > 1 ? std::any_cast<std::vector<std::pair<uint16_t, uint16_t>>>(&args[1]) : nullptr;
        refresh(MODE_SCROLL, 0, 0, {}, imbuements ? *imbuements : std::vector<Imbuement>{}, owned);
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCloseImbuementWindow", [this](GameObject*, const Args&) {
        Q_EMIT requestCloseImbuingDialog();
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::death, this,
            [this](int, int) { Q_EMIT requestCloseImbuingDialog(); });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        Q_EMIT requestCloseImbuingDialog();
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::serverError, this,
            [this](int code, const QString& message) {
                if (isResultMessageType(code))
                    Q_EMIT requestShowResultDialog(message);
            });
}

uint32_t TImbuingStorage::ownedCount(const uint16_t typeId, bool* listed) const
{
    const auto it = m_owned.find(typeId);
    if (listed)
        *listed = it != m_owned.end;
    return it == m_owned.end() ? 0 : it->second;
}

void TImbuingStorage::addOwned(const uint16_t typeId, const uint32_t count)
{
    m_owned[typeId] += count;
    Q_EMIT imbuingDataChanged();
}

void TImbuingStorage::refresh(const EMode mode, const uint16_t typeId, const uint8_t tier, std::vector<TImbuementSlot> slots,
                              std::vector<Imbuement> imbuements, const std::vector<std::pair<uint16_t, uint16_t>>* owned)
{
    m_mode = mode;
    m_itemTypeId = typeId;
    m_itemTier = tier;
    m_slots = std::move(slots);
    m_imbuements = std::move(imbuements);
    m_owned.clear();
    if (owned) {
        for (const auto& [ownedTypeId, count] : *owned)
            m_owned[ownedTypeId] = count;
    }
    Q_EMIT imbuingDataChanged();
    Q_EMIT requestShowOrRefreshImbuingDialog();
}

}
