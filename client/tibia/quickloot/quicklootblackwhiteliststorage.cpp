#include "quicklootblackwhiteliststorage.h"

#include <QJsonArray>

#include <stdexcept>

namespace tibia::quickloot {

namespace {

const QString ConfigurationName = QStringLiteral("lootBlackWhitelist");
const QString ListTypeKey = QStringLiteral("listType");
const QString BlacklistTypesKey = QStringLiteral("blacklistTypes");
const QString WhitelistTypesKey = QStringLiteral("whitelistTypes");

QJsonArray toArray(const std::set<uint16_t>& list)
{
    QJsonArray array;
    for (const uint16_t objectType : list)
        array.append(static_cast<int>(objectType));
    return array;
}

void readList(const QJsonObject& json, const QString& key, std::set<uint16_t>& list)
{
    if (!json.contains(key) || !json.value(key).isArray())
        return;
    for (const QJsonValue& value : json.value(key).toArray()) {
        if (value.isDouble() && list.size() < TQuickLootBlackWhitelistStorage::MaxListSize)
            list.insert(static_cast<uint16_t>(value.toInt()));
    }
}

}

TQuickLootBlackWhitelistStorage::TQuickLootBlackWhitelistStorage(QObject* parent)
    : cip::tibia::quickloot::TQuickLootBlackWhitelistStorage(parent)
{}

QString TQuickLootBlackWhitelistStorage::configurationName() const { return ConfigurationName; }

QJsonObject TQuickLootBlackWhitelistStorage::configurationToJson() const
{
    QJsonObject json;
    switch (m_listType) {
        case Whitelist: json.insert(ListTypeKey, QStringLiteral("whitelist")); break;
        case Blacklist: json.insert(ListTypeKey, QStringLiteral("blacklist")); break;
        default: throw std::runtime_error("Unhandled enum value");
    }
    json.insert(BlacklistTypesKey, toArray(m_blacklist));
    json.insert(WhitelistTypesKey, toArray(m_whitelist));
    return json;
}

void TQuickLootBlackWhitelistStorage::configurationFromJson(const QJsonObject& json)
{
    if (json.contains(ListTypeKey)) {
        const QString type = json.value(ListTypeKey).toString();
        if (type == QLatin1String("blacklist"))
            m_listType = Blacklist;
        else
            m_listType = type == QLatin1String("whitelist") ? Whitelist : Blacklist;
    }
    activeList().clear();
    Q_EMIT configurationChangedFromUserInput();
    readList(json, BlacklistTypesKey, m_blacklist);
    readList(json, WhitelistTypesKey, m_whitelist);
}

void TQuickLootBlackWhitelistStorage::resetConfiguration() { clear(); }

void TQuickLootBlackWhitelistStorage::clear()
{
    m_listType = Blacklist;
    m_blacklist.clear();
    m_whitelist.clear();
}

const std::set<uint16_t>& TQuickLootBlackWhitelistStorage::list(const int listType) const
{
    return listType == Whitelist ? m_whitelist : m_blacklist;
}

bool TQuickLootBlackWhitelistStorage::isListed(const uint16_t objectType) const { return list(m_listType).contains(objectType); }

bool TQuickLootBlackWhitelistStorage::isListFull() const { return list(m_listType).size() >= MaxListSize; }

bool TQuickLootBlackWhitelistStorage::addItem(const uint16_t objectType)
{
    std::set<uint16_t>& active = activeList();
    if (active.size() >= MaxListSize || !active.insert(objectType).second)
        return false;
    Q_EMIT configurationChangedFromUserInput();
    return true;
}

void TQuickLootBlackWhitelistStorage::removeItem(const uint16_t objectType)
{
    if (activeList().erase(objectType) == 0)
        return;
    Q_EMIT configurationChangedFromUserInput();
}

void TQuickLootBlackWhitelistStorage::setListType(const int listType)
{
    const int next = listType == Whitelist ? Whitelist : Blacklist;
    if (m_listType == next)
        return;
    m_listType = next;
    Q_EMIT configurationChangedFromUserInput();
}

void TQuickLootBlackWhitelistStorage::replaceLists(const int listType, std::set<uint16_t> blacklist, std::set<uint16_t> whitelist)
{
    const auto trim = [](std::set<uint16_t>& list) {
        while (list.size() > MaxListSize)
            list.erase(std::prev(list.end()));
    };
    trim(blacklist);
    trim(whitelist);
    m_listType = listType == Whitelist ? Whitelist : Blacklist;
    m_blacklist = std::move(blacklist);
    m_whitelist = std::move(whitelist);
    Q_EMIT configurationChangedFromUserInput();
}

std::set<uint16_t>& TQuickLootBlackWhitelistStorage::activeList() { return m_listType == Whitelist ? m_whitelist : m_blacklist; }

}
