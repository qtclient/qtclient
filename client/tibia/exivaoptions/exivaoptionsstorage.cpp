#include "exivaoptionsstorage.h"

namespace tibia::exivaoptions {

TExivaOptionsStorage::TExivaOptionsStorage(QObject* parent)
    : QObject(parent)
{
}

TExivaOptionsStorage::~TExivaOptionsStorage() = default;

void TExivaOptionsStorage::setAllowEveryone(const bool allow)
{
    if (m_options.allowAll == allow)
        return;
    m_options.allowAll = allow;
    Q_EMIT allowEveryoneChanged();
}

void TExivaOptionsStorage::setAllowedGroups(const bool ownGuild, const bool ownParty, const bool vipList, const bool characterWhitelist,
                                            const bool guildWhitelist)
{
    if (m_options.allowOwnGuild == ownGuild && m_options.allowOwnParty == ownParty && m_options.allowVipList == vipList
        && m_options.allowPlayerWhitelist == characterWhitelist && m_options.allowGuildWhitelist == guildWhitelist)
        return;
    m_options.allowOwnGuild = ownGuild;
    m_options.allowOwnParty = ownParty;
    m_options.allowVipList = vipList;
    m_options.allowPlayerWhitelist = characterWhitelist;
    m_options.allowGuildWhitelist = guildWhitelist;
    Q_EMIT allowedGroupsChanged();
}

void TExivaOptionsStorage::addToCharacterWhitelist(const Names& names)
{
    const size_t before = m_characterWhitelist.size();
    m_characterWhitelist.insert(names.begin(), names.end());
    if (before != m_characterWhitelist.size())
        Q_EMIT characterWhitelistChanged();
}

void TExivaOptionsStorage::removeFromCharacterWhitelist(const Names& names)
{
    if (names.empty())
        return;
    const size_t before = m_characterWhitelist.size();
    for (const QString& name : names)
        m_characterWhitelist.erase(name);
    if (before != m_characterWhitelist.size())
        Q_EMIT characterWhitelistChanged();
}

void TExivaOptionsStorage::addToGuildWhitelist(const Names& names)
{
    const size_t before = m_guildWhitelist.size();
    m_guildWhitelist.insert(names.begin(), names.end());
    if (before != m_guildWhitelist.size())
        Q_EMIT guildWhitelistChanged();
}

void TExivaOptionsStorage::removeFromGuildWhitelist(const Names& names)
{
    if (names.empty())
        return;
    const size_t before = m_guildWhitelist.size();
    for (const QString& name : names)
        m_guildWhitelist.erase(name);
    if (before != m_guildWhitelist.size())
        Q_EMIT guildWhitelistChanged();
}

}
