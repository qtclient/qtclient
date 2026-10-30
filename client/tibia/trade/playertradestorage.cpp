#include "playertradestorage.h"

#include <tibia/world/game.h>
#include <client/item.h>

namespace tibia::trade {

namespace {

using Args = std::vector<std::any>;

QString nameArg(const Args& args)
{
    return client::gamecore::argString(args, 0);
}

std::vector<ItemPtr> itemsArg(const Args& args)
{
    if (args.size() < 2)
        return {};
    if (const auto* items = std::any_cast<std::vector<ItemPtr>>(&args[1]))
        return *items;
    return {};
}

}

TPlayerTradeStorage::TPlayerTradeStorage(QObject* parent)
    : cip::tibia::trade::TPlayerTradeStorage(parent)
{
    // CIP 0x10103f880 / 0x101040430.
    m_hooks.emplace_back("g_game", "onOwnTrade", [this](LuaObject*, const Args& args) {
        m_ownName = nameArg(args);
        m_ownOffer = itemsArg(args);
        const bool opened = !m_open;
        m_open = true;
        Q_EMIT ownOfferChanged();
        Q_EMIT counterOfferChanged();
        if (opened)
            Q_EMIT requestOpenPlayerTradeWidget();
        return std::any{};
    });
    // CIP 0x10103fb40 / 0x1010404b0.
    m_hooks.emplace_back("g_game", "onCounterTrade", [this](LuaObject*, const Args& args) {
        m_counterName = nameArg(args);
        m_counterOffer = itemsArg(args);
        const bool opened = !m_open;
        m_open = true;
        Q_EMIT counterOfferChanged();
        if (opened)
            Q_EMIT requestOpenPlayerTradeWidget();
        return std::any{};
    });
    // CIP 0x10103fdf0.
    m_hooks.emplace_back("g_game", "onCloseTrade", [this](LuaObject*, const Args&) {
        close();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        close();
        return std::any{};
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

void TPlayerTradeStorage::accept() const
{
    g_game.acceptTrade();
}

void TPlayerTradeStorage::reject()
{
    g_game.rejectTrade();
    close();
}

void TPlayerTradeStorage::look(const bool counterOffer, const int index) const
{
    g_game.inspectTrade(counterOffer, static_cast<uint8_t>(index));
}

}
