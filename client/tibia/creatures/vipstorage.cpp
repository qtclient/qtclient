#include "vipstorage.h"

#include "chat/chatchannel.h"
#include "creatures/creaturesciptypes.h"
#include "input/gameactionhandlers.h"

#include <tibia/game/tgamenotify.h>

#include <QColor>
#include <QCoreApplication>

namespace tibia::creatures {

namespace {

using A = input::EGameAction;

/// The status line colour of the VIP login notice.
const QColor LoginNoticeColor(0xf0, 0xf0, 0xf0);
/// The console colour (message mode) of the VIP login notice.
constexpr int LoginNoticeMessageMode = 30;
/// Icons above this are stored as 0.
constexpr uint8_t MaxIconId = 10;

bool isLoggedOut(const int state) { return state == TVipStorage::LoggedOut || state == TVipStorage::Offline; }

bool isLoggedIn(const int state) { return state == TVipStorage::LoggedIn || state == TVipStorage::Online; }

}

TVipStorage::TVipStorage(QObject* parent)
    : cip::tibia::creatures::TVipStorage(parent)
{
    // CIP's session connects the storage's game actions to the input handlers.
    connect(this, &TVipStorage::publishGameAction, this, [](const input::IGameActionPtr& action) { input::publish(action); });
    // CIP makes a new storage per game session; the shared one is emptied instead.
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, &TVipStorage::clear);
}

TVipStorage* TVipStorage::instance()
{
    static TVipStorage* storage = [] {
        auto* created = new TVipStorage(QCoreApplication::instance());
        new TVipGameActionHandler(created, created);
        return created;
    }();
    return storage;
}

std::map<uint32_t, TVipStorage::Vip>& TVipStorage::entries() { return instance()->m_vips; }

void TVipStorage::clear()
{
    m_vips.clear();
    m_groups.clear();
    m_groupsLeft = 0;
    Q_EMIT vipGroupDataChanged();
}

size_t TVipStorage::vipCount() const { return m_vips.size(); }

std::optional<TVipStorage::Vip> TVipStorage::vipById(const uint32_t id) const
{
    const auto it = m_vips.find(id);
    if (it == m_vips.end())
        return std::nullopt;
    return it->second;
}

std::optional<TVipStorage::Vip> TVipStorage::vipByName(const QString& name) const
{
    for (const auto& [id, vip] : m_vips) {
        if (vip.name == name)
            return vip;
    }
    return std::nullopt;
}

std::vector<TVipStorage::Vip> TVipStorage::vips() const
{
    std::vector<Vip> result;
    for (const auto& [id, vip] : m_vips)
        result.push_back(vip);
    return result;
}

void TVipStorage::addOrUpdateVip(const Vip& vip)
{
    m_vips[vip.id] = vip;
    Q_EMIT vipDataChangedOrAdded(vip);
}

void TVipStorage::addVip(const uint32_t id, const QString& name, const QString& description, const uint8_t iconId,
                         const bool notifyLogin, const int state, const std::vector<uint8_t>& groups)
{
    addOrUpdateVip({ id, name, state, description, iconId <= MaxIconId ? iconId : 0, notifyLogin, groups });
}

void TVipStorage::updateVipState(const uint32_t id, const int state)
{
    const auto it = m_vips.find(id);
    if (it == m_vips.end())
        return;
    if (state == LoggedIn || state == LoggedOut)
        Q_EMIT loginOrLogoutStateUpdate(state);
    Vip& vip = it->second;
    if (vip.notifyLogin && isLoggedOut(vip.state) && isLoggedIn(state)) {
        const QString text = qtTrId("speech_volume_vip_has_logged_in").arg(vip.name);
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionSetStatusLine>(text));
        auto message = std::make_shared<input::TGameActionAddTextMessage>(A::ADD_SYSTEM_CHAT_MESSAGE, text, chat::consoleColor(LoginNoticeMessageMode));
        Q_EMIT publishGameAction(message);
    }
    vip.state = state;
    Q_EMIT vipStateChanged(id, state);
}

void TVipStorage::removeVip(const uint32_t id)
{
    if (m_vips.erase(id) == 0)
        return;
    Q_EMIT vipDataRemoved(id);
}

void TVipStorage::setGroups(const std::vector<Group>& groups, const int groupsLeft)
{
    m_groupsLeft = groupsLeft;
    m_groups.clear();
    for (const Group& group : groups)
        m_groups[group.id] = group;
    Q_EMIT vipGroupDataChanged();
}

std::vector<TVipStorage::Group> TVipStorage::groups() const
{
    std::vector<Group> result;
    for (const auto& [id, group] : m_groups)
        result.push_back(group);
    return result;
}

std::map<uint8_t, TVipStorage::Group> TVipStorage::groupMap() const { return m_groups; }

std::optional<TVipStorage::Group> TVipStorage::groupByName(const QString& name) const
{
    for (const auto& [id, group] : m_groups) {
        if (group.name == name)
            return group;
    }
    return std::nullopt;
}

int TVipStorage::groupsLeft() const { return m_groupsLeft; }

}
