#include "vipstorage.h"

#include "input/gameactionhandlers.h"

#include <tibia/world/creature.h>
#include <tibia/world/game.h>
#include <tibia/world/map.h>

#include <tuple>
#include <utility>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Args = std::vector<std::any>;
using GroupTuples = std::vector<std::tuple<uint8_t, std::string, bool>>;

constexpr A HandledActions[] = { A::ADD_NEW_VIP,           A::REMOVE_VIP,         A::ADD_NEW_VIP_GROUP,
                                 A::REMOVE_VIP_GROUP,      A::REQUEST_VIP_GROUP_UPDATE, A::REQUST_VIP_DATA_UPDATE };

QString qstr(const std::string& text) { return QString::fromStdString(text); }

}

TVipStorage::TVipStorage(QObject* parent)
    : QObject(parent)
{
    m_vipsChangedTimer.setSingleShot(true);
    m_vipsChangedTimer.setInterval(0);
    connect(&m_vipsChangedTimer, &QTimer::timeout, this, &TVipStorage::vipsChanged);

    m_hooks.emplace_back("g_game", "onAddVip", [this](LuaObject*, const Args& args) {
        m_states[static_cast<uint32_t>(client::gamecore::argInt(args, 0))] = static_cast<int>(client::gamecore::argInt(args, 2));
        m_vipsChangedTimer.start();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onVipStateChange", [this](LuaObject*, const Args& args) {
        const auto id = static_cast<uint32_t>(client::gamecore::argInt(args, 0));
        const auto state = static_cast<int>(client::gamecore::argInt(args, 1));
        m_vipsChangedTimer.start();
        const auto it = m_states.find(id);
        if (it == m_states.end() || it->second == state)
            return std::any{};
        const int previous = std::exchange(it->second, state);
        Q_EMIT vipStateChanged(id, previous, state);
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onVipGroupChange", [this](LuaObject*, const Args& args) {
        m_groups.clear();
        if (const auto* groups = args.empty() ? nullptr : std::any_cast<GroupTuples>(&args[0])) {
            for (const auto& [id, name, editable] : *groups)
                m_groups.push_back({ id, qstr(name), editable });
        }
        m_groupsLeft = static_cast<int>(client::gamecore::argInt(args, 1));
        Q_EMIT groupsChanged();
        m_vipsChangedTimer.start();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        m_states.clear();
        m_groups.clear();
        m_groupsLeft = 0;
        Q_EMIT groupsChanged();
        m_vipsChangedTimer.start();
        return std::any{};
    });

    input::setActionHandler(A::ADD_NEW_VIP, [this](const input::TGameAction& action) {
        QString name = action.text;
        if (action.creatureId) {
            if (const CreaturePtr creature = g_map.getCreatureById(action.creatureId))
                name = qstr(creature->getName());
        }
        addVip(name);
        return true;
    });
    input::setActionHandler(A::REMOVE_VIP, [this](const input::TGameAction& action) {
        uint32_t id = static_cast<uint32_t>(action.instance);
        if (!id) {
            if (const std::optional<Vip> vip = vipByName(action.text))
                id = vip->id;
        }
        removeVip(id);
        return true;
    });
    input::setActionHandler(A::ADD_NEW_VIP_GROUP, [this](const input::TGameAction& action) {
        addGroup(action.text);
        return true;
    });
    input::setActionHandler(A::REMOVE_VIP_GROUP, [this](const input::TGameAction& action) {
        removeGroup(static_cast<uint8_t>(action.parameter));
        return true;
    });
    input::setActionHandler(A::REQUEST_VIP_GROUP_UPDATE, [this](const input::TGameAction& action) {
        editGroup(static_cast<uint8_t>(action.parameter), action.text);
        return true;
    });
    input::setActionHandler(A::REQUST_VIP_DATA_UPDATE, [this](const input::TGameAction& action) {
        // CIP TVipGameActionHandler::handleRequestVipDataUpdateGameAction
        // (0x100a4be30): EditBuddy from instance / text / parameter / enable /
        // vipGroups.
        editVip(static_cast<uint32_t>(action.instance), action.text, action.parameter, action.enable, action.vipGroups);
        return true;
    });
}

TVipStorage::~TVipStorage()
{
    for (const A type : HandledActions)
        input::setActionHandler(type, nullptr);
}

std::vector<TVipStorage::Vip> TVipStorage::vips() const
{
    std::vector<Vip> result;
    for (const auto& [id, vip] : g_game.getVips()) {
        const auto& [name, state, description, iconId, notify, groups] = vip;
        result.push_back({ static_cast<uint32_t>(id), qstr(name), static_cast<int>(state), qstr(description), iconId, notify, groups });
    }
    return result;
}

std::optional<TVipStorage::Vip> TVipStorage::vipByName(const QString& name) const
{
    for (Vip& vip : vips()) {
        if (vip.name.compare(name, Qt::CaseInsensitive) == 0)
            return std::move(vip);
    }
    return std::nullopt;
}

std::optional<TVipStorage::Group> TVipStorage::groupByName(const QString& name) const
{
    for (const Group& group : m_groups) {
        if (group.name == name)
            return group;
    }
    return std::nullopt;
}

void TVipStorage::addVip(const QString& name)
{
    if (!name.trimmed().isEmpty())
        g_game.addVip(name.trimmed().toStdString());
}

void TVipStorage::removeVip(const uint32_t id)
{
    if (!id)
        return;
    g_game.removeVip(id);
    m_vipsChangedTimer.start();
}

void TVipStorage::editVip(const uint32_t id, const QString& description, const int iconId, const bool notifyLogin,
                          const std::vector<uint8_t>& groups)
{
    g_game.editVip(id, description.toStdString(), static_cast<uint32_t>(iconId), notifyLogin, groups);
    m_vipsChangedTimer.start();
}

void TVipStorage::addGroup(const QString& name)
{
    if (!name.trimmed().isEmpty())
        g_game.editVipGroups(Otc::VIP_GROUP_ADD, 0, name.trimmed().toStdString());
}

void TVipStorage::editGroup(const uint8_t id, const QString& name)
{
    if (!name.trimmed().isEmpty())
        g_game.editVipGroups(Otc::VIP_GROUP_EDIT, id, name.trimmed().toStdString());
}

void TVipStorage::removeGroup(const uint8_t id) { g_game.editVipGroups(Otc::VIP_GROUP_REMOVE, id, {}); }

}
