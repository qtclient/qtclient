#include "game/passiveabilityinfostorage.h"

#include "gamewindow/gamewindowciptypes.h"

#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>

#include <QtGlobal>

#include <any>
#include <utility>

namespace tibia::game {

using client::gamecore::Args;
using client::gamecore::argBool;
using client::gamecore::argInt;

TPassiveAbilityInfoStorage* TPassiveAbilityInfoStorage::s_instance = nullptr;

TPassiveAbilityInfoStorage::TIterator::TIterator(std::map<uint8_t, TPassiveAbilityInfo>::const_iterator node)
    : m_node(node)
{
}

const TPassiveAbilityInfo& TPassiveAbilityInfoStorage::TIterator::operator*()
{
    if (!m_value)
        m_value = m_node->second;
    return *m_value;
}

bool TPassiveAbilityInfoStorage::TIterator::operator==(const TIterator& other) const
{
    return m_node == other.m_node;
}

TPassiveAbilityInfoStorage::TIterator TPassiveAbilityInfoStorage::TIterator::operator++(int)
{
    TIterator previous = *this;
    ++m_node;
    m_value.reset();
    return previous;
}

TPassiveAbilityInfoStorage::TPassiveAbilityInfoStorage(QObject* parent)
    : cip::tibia::game::TPassiveAbilityInfoStorage(parent)
    , m_model(new gamewindow::TPassiveAbilityListDataModel(this))
{
    s_instance = this;
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPassiveData", [this](GameObject*, const Args& args) {
        // onPassiveData(abilityId, remainingMs, totalMs, decaying) — cooldown
        // also lands in TCooldownStorage for action-bar overlays.
        const auto abilityId = static_cast<uint8_t>(argInt(args, 0));
        m_currentCooldown = static_cast<uint32_t>(argInt(args, 1));
        m_maxCooldown = static_cast<uint32_t>(argInt(args, 2));
        m_canDecay = argBool(args, 3);
        std::vector<gamewindow::TPassiveAbilityListDataModel::Entry> entries;
        if (const std::optional<TPassiveAbilityInfo> ability = findInfo(abilityId)) {
            gamewindow::TPassiveAbilityListDataModel::Entry entry;
            entry.id = ability->id;
            entry.name = ability->name;
            entry.iconUrl = QStringLiteral("image://passiveability-icons-32x32/%1").arg(ability->iconId);
            entry.usableByCurrentVocation = true;
            entries.push_back(std::move(entry));
        }
        m_model->setEntries(std::move(entries));
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        m_currentCooldown = 0;
        m_maxCooldown = 0;
        m_canDecay = false;
        m_model->setEntries({});
    });
}

TPassiveAbilityInfoStorage* TPassiveAbilityInfoStorage::instance()
{
    return s_instance;
}

const std::map<uint8_t, TPassiveAbilityInfo>& TPassiveAbilityInfoStorage::infos()
{
    static const std::map<uint8_t, TPassiveAbilityInfo> table{
        { 1, TPassiveAbilityInfo{ 1, qtTrId("passiveability_name_giftoflife"), 0, { 1, 2, 3, 4, 5 }, false } },
    };
    return table;
}

TPassiveAbilityInfo TPassiveAbilityInfoStorage::info(const uint8_t id) const
{
    return infos().at(id);
}

std::optional<TPassiveAbilityInfo> TPassiveAbilityInfoStorage::findInfo(const uint8_t id) const
{
    const auto& table = infos();
    const auto it = table.find(id);
    if (it == table.end())
        return std::nullopt;
    return it->second;
}

std::size_t TPassiveAbilityInfoStorage::count() const
{
    return infos().size();
}

TPassiveAbilityInfoStorage::TIterator TPassiveAbilityInfoStorage::begin() const
{
    return TIterator(infos().begin());
}

TPassiveAbilityInfoStorage::TIterator TPassiveAbilityInfoStorage::end() const
{
    return TIterator(infos().end());
}

QString TPassiveAbilityInfoStorage::name(const uint8_t id) const
{
    if (!isKnownPassiveAbility(id))
        return {};
    return info(id).name;
}

bool TPassiveAbilityInfoStorage::isKnownPassiveAbility(const uint8_t id)
{
    return infos().count(id) != 0;
}

}
