#include "game/passiveabilityinfostorage.h"

#include <QtGlobal>

#include <utility>

namespace tibia::game {

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

TPassiveAbilityInfoStorage::TIterator& TPassiveAbilityInfoStorage::TIterator::operator++()
{
    ++m_node;
    m_value.reset();
    return *this;
}

TPassiveAbilityInfoStorage::TPassiveAbilityInfoStorage(QObject* parent)
    : cip::tibia::game::TPassiveAbilityInfoStorage(parent)
{
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
