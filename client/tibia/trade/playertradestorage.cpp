#include "playertradestorage.h"

#include <tibia/game/gamesession.h>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::trade {

namespace {

using Args = std::vector<std::any>;

QString nameArg(const Args& args)
{
    return client::gamecore::argString(args, 0);
}

std::vector<container::TInventorySlot> slotsArg(const Args& args)
{
    if (args.size() < 2)
        return {};
    if (const auto* slots = std::any_cast<std::vector<container::TInventorySlot>>(&args[1]))
        return *slots;
    return {};
}

}

TPlayerTradeStorage::TPlayerTradeStorage(QObject* parent)
    : cip::tibia::trade::TPlayerTradeStorage(parent)
{
    // CIP.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onOwnTrade", [this](GameObject*, const Args& args) {
        m_ownName = nameArg(args);
        m_ownOffer = slotsArg(args);
        const bool opened = !m_open;
        m_open = true;
        Q_EMIT ownOfferChanged();
        Q_EMIT counterOfferChanged();
        if (opened)
            Q_EMIT requestOpenPlayerTradeWidget();
        return std::any{};
    });
    // CIP.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCounterTrade", [this](GameObject*, const Args& args) {
        m_counterName = nameArg(args);
        m_counterOffer = slotsArg(args);
        const bool opened = !m_open;
        m_open = true;
        Q_EMIT counterOfferChanged();
        if (opened)
            Q_EMIT requestOpenPlayerTradeWidget();
        return std::any{};
    });
    // CIP .
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCloseTrade", [this](GameObject*, const Args&) {
        close();
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        close();
    });
}

void TPlayerTradeStorage::reset()
{
    m_ownName.clear();
    m_ownOffer.clear();
    m_counterName.clear();
    m_counterOffer.clear();
}

void TPlayerTradeStorage::close()
{
    if (!m_open)
        return;
    m_open = false;
    reset();
    Q_EMIT requestClosePlayerTradeWidget();
    Q_EMIT ownOfferChanged();
    Q_EMIT counterOfferChanged();
}

// b1/d47a820371909f92
void TPlayerTradeStorage::accept() const
{
    tibia::game::acceptTrade();
}

void TPlayerTradeStorage::reject()
{
    tibia::game::rejectTrade();
    close();
}

void TPlayerTradeStorage::look(const bool counterOffer, const int index) const
{
    tibia::game::inspectTrade(counterOffer, static_cast<uint8_t>(index));
}

}
