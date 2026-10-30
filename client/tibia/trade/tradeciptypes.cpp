#include "trade/tradeciptypes.h"

#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

namespace tibia::trade {

namespace {

using Args = std::vector<std::any>;
using client::gamecore::argInt;
using client::gamecore::argString;

QString itemNameOf(const uint16_t typeId)
{
    const ThingTypePtr& type = g_things.getThingType(typeId, ThingCategoryItem);
    if (!type || type->getId() == 0)
        return QString::number(typeId);
    const MarketData& market = type->getMarketData();
    return QString::fromStdString(type->isMarketable() && !market.name.empty() ? market.name : type->getName());
}

QString categoryIconFor(const uint8_t category)
{
    switch (category) {
        case 1: return QStringLiteral("/images/icon-yes.png");
        case 2: return QStringLiteral("/images/icon-exclamationmark.png");
        default: return QStringLiteral("/images/icon-no.png");
    }
}

} // namespace

TCharacterTradeStorage* TCharacterTradeStorage::s_instance = nullptr;

TCharacterTradeStorage::TCharacterTradeStorage(QObject* parent)
    : cip::tibia::trade::TCharacterTradeStorage(parent)
{
    s_instance = this;

    m_hooks.emplace_back("g_game", "onCharacterTradeConditions", [this](LuaObject*, const Args& args) {
        m_characterName = argString(args, 0);
        m_characterLevel = static_cast<int>(argInt(args, 1));
        m_vocationClientId = static_cast<uint8_t>(argInt(args, 2));
        if (const auto* outfit = args.size() > 3 ? std::any_cast<Outfit>(&args[3]) : nullptr)
            m_outfit = *outfit;
        m_worldType = static_cast<uint8_t>(argInt(args, 4));
        m_startTimestamp = static_cast<uint32_t>(argInt(args, 5));
        m_endTimestamp = static_cast<uint32_t>(argInt(args, 6));
        m_minimumBid = static_cast<uint16_t>(argInt(args, 7));
        m_conditions.clear();
        if (const auto* list = args.size() > 8 ? std::any_cast<std::vector<std::tuple<std::string, uint8_t>>>(&args[8]) : nullptr) {
            m_conditions.reserve(list->size());
            for (const auto& [text, status] : *list) {
                TCharacterTradeConditionEntry entry;
                entry.notification = QString::fromStdString(text);
                entry.status = static_cast<int>(status);
                m_conditions.push_back(std::move(entry));
            }
        }
        Q_EMIT infoUpdated(Conditions);
        return std::any{};
    });

    m_hooks.emplace_back("g_game", "onCharacterTradeItems", [this](LuaObject*, const Args& args) {
        const int dataType = static_cast<int>(argInt(args, 0));
        std::vector<TCharacterTradeItemEntry> rows;
        if (const auto* list = args.size() > 1 ? std::any_cast<std::vector<std::tuple<uint16_t, uint8_t, uint32_t>>>(&args[1]) : nullptr) {
            rows.reserve(list->size());
            for (const auto& [itemId, tier, count] : *list) {
                TCharacterTradeItemEntry entry;
                entry.id = static_cast<int>(itemId);
                entry.upgradeTier = static_cast<int>(tier);
                entry.amount = static_cast<int>(count);
                entry.name = itemNameOf(itemId);
                rows.push_back(std::move(entry));
            }
        }
        if (dataType == StoreItems) {
            m_storeItems = std::move(rows);
            Q_EMIT infoUpdated(StoreItems);
        } else {
            m_items = std::move(rows);
            Q_EMIT infoUpdated(Items);
        }
        return std::any{};
    });

    m_hooks.emplace_back("g_game", "onCharacterTradeHighlights", [this](LuaObject*, const Args& args) {
        m_highlights.clear();
        using Group = std::tuple<uint8_t, std::string, std::vector<std::tuple<uint16_t, std::string>>>;
        if (const auto* groups = args.empty() ? nullptr : std::any_cast<std::vector<Group>>(&args[0])) {
            for (const auto& [category, groupName, entries] : *groups) {
                const QString categoryLabel = QString::fromStdString(groupName);
                const QString icon = categoryIconFor(category);
                for (const auto& [id, text] : entries) {
                    TCharacterTradeHighlightEntry entry;
                    entry.highlightId = static_cast<int>(id);
                    entry.highlight = QString::fromStdString(text);
                    entry.category = categoryLabel;
                    entry.categoryIcon = icon;
                    m_highlights.push_back(std::move(entry));
                }
            }
        }
        Q_EMIT infoUpdated(Highlights);
        return std::any{};
    });

    m_hooks.emplace_back("g_game", "onCharacterTradeSummary", [this](LuaObject*, const Args& args) {
        m_summaryFee = static_cast<uint16_t>(argInt(args, 0));
        m_confirmations.clear();
        if (const auto* problems = args.size() > 1 ? std::any_cast<std::vector<std::string>>(&args[1]) : nullptr) {
            m_confirmations.reserve(problems->size());
            for (const std::string& problem : *problems) {
                TCharacterTradeConditionEntry entry;
                entry.notification = QString::fromStdString(problem);
                entry.status = 1; // No / error
                m_confirmations.push_back(std::move(entry));
            }
        }
        Q_EMIT infoUpdated(Summary);
        return std::any{};
    });

    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        m_characterName.clear();
        m_characterLevel = 0;
        m_vocationClientId = 0;
        m_outfit = {};
        m_worldType = 0;
        m_startTimestamp = 0;
        m_endTimestamp = 0;
        m_minimumBid = 0;
        m_conditions.clear();
        m_items.clear();
        m_storeItems.clear();
        m_highlights.clear();
        m_summaryFee = 0;
        m_confirmations.clear();
        return std::any{};
    });
}

TCharacterTradeStorage* TCharacterTradeStorage::instance()
{
    return s_instance;
}

bool TCharacterTradeStorage::hasUnfulfilledConditions() const
{
    for (const TCharacterTradeConditionEntry& entry : m_conditions) {
        if (entry.status != 0)
            return true;
    }
    return false;
}

bool TCharacterTradeStorage::hasConfirmationError() const
{
    return !m_confirmations.empty();
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    new TCharacterTradeStorage(parent);
}

} // namespace tibia::trade
