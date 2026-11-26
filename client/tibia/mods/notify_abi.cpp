// Host API v9: the notify table (abi/notify_api.h): CipSoft's field bus
// (game/tgamenotify.h), session / world time / Team Finder / container changes, and the
// arguments of a field call (arg_*; structured payloads as JSON, documented in
// include/qtclient/README.md). Split out of host_api_v1.cpp.
#include "mods/abi_internal.h"

#include "container/containerciptypes.h"
#include "game/currentgamesession.h"
#include "game/gamenotify.h"
#include "game/sessionhooks.h"
#include "game/tgamenotify.h"
#include "quickloot/quicklootblackwhiteliststorage.h"
#include "teamfinder/teamfinderciptypes.h"

#include <client/const.h>
#include <client/position.h>
#include <client/staticdata.h>
#include <teamfinder/teamfinder_types.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QMetaObject>
#include <QObject>
#include <QString>

#include <any>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tibia::mods {
namespace {

// --- payload JSON (arg_kind / arg_json) ---

QJsonValue jsonOf(const BestiaryCharmsData& d);
QJsonValue jsonOf(const BestiaryMonsterData& d);
QJsonValue jsonOf(const TeamFinderLeaderData& d);
QJsonValue jsonOf(const TeamFinderListEntry& d);
QJsonValue jsonOf(const BossCooldownData& d);
QJsonValue jsonOf(const CharmData& d);
QJsonValue jsonOf(const LootItem& d);
QJsonValue jsonOf(const TeamFinderMember& d);
QJsonValue jsonOf(const Position& p);
template <class T>
QJsonValue jsonOf(const std::vector<T>& list);
template <class... T>
QJsonValue jsonOf(const std::tuple<T...>& t);
template <class K, class V>
QJsonValue jsonOf(const std::map<K, V>& map);

QJsonValue jsonOf(const std::string& s) { return QString::fromStdString(s); }
QJsonValue jsonOf(const QString& s) { return s; }
QJsonValue jsonOf(const bool v) { return v; }
QJsonValue jsonOf(const double v) { return v; }
template <class T>
    requires std::is_integral_v<T>
QJsonValue jsonOf(const T v)
{
    return static_cast<qint64>(v);
}
QJsonValue jsonOf(const Position& p) { return QJsonObject{ { "x", p.x }, { "y", p.y }, { "z", p.z } }; }
QJsonValue jsonOf(const BossCooldownData& d)
{
    return QJsonObject{ { "bossRaceId", jsonOf(d.bossRaceId) }, { "cooldownTime", jsonOf(d.cooldownTime) } };
}
QJsonValue jsonOf(const CharmData& d)
{
    return QJsonObject{ { "id", jsonOf(d.id) },
                        { "name", jsonOf(d.name) },
                        { "description", jsonOf(d.description) },
                        { "unlockPrice", jsonOf(d.unlockPrice) },
                        { "unlocked", d.unlocked },
                        { "asignedStatus", d.asignedStatus },
                        { "raceId", jsonOf(d.raceId) },
                        { "removeRuneCost", jsonOf(d.removeRuneCost) },
                        { "availableCharmSlots", jsonOf(d.availableCharmSlots) },
                        { "tier", jsonOf(d.tier) } };
}
QJsonValue jsonOf(const LootItem& d)
{
    return QJsonObject{ { "itemId", jsonOf(d.itemId) }, { "diffculty", jsonOf(d.diffculty) },
                        { "specialEvent", jsonOf(d.specialEvent) }, { "name", jsonOf(d.name) }, { "amount", jsonOf(d.amount) } };
}
QJsonValue jsonOf(const TeamFinderMember& d)
{
    return QJsonObject{ { "guid", jsonOf(d.guid) }, { "name", jsonOf(d.name) }, { "level", jsonOf(d.level) },
                        { "vocation", jsonOf(d.vocation) }, { "status", jsonOf(d.status) } };
}
template <class T>
QJsonValue jsonOf(const std::vector<T>& list)
{
    QJsonArray a;
    for (const T& v : list)
        a.append(jsonOf(v));
    return a;
}
template <class K, class V>
QJsonValue jsonOf(const std::map<K, V>& map)
{
    QJsonObject o;
    for (const auto& [k, v] : map)
        o.insert(QString::number(k), jsonOf(v));
    return o;
}
template <class... T>
QJsonValue jsonOf(const std::tuple<T...>& t)
{
    QJsonArray a;
    std::apply([&a](const auto&... v) { (a.append(jsonOf(v)), ...); }, t);
    return a;
}
QJsonValue jsonOf(const BestiaryCharmsData& d)
{
    return QJsonObject{ { "resetAllCharmsCost", jsonOf(d.resetAllCharmsCost) },
                        { "availableCharmSlots", jsonOf(d.availableCharmSlots) },
                        { "points", jsonOf(d.points) },
                        { "charms", jsonOf(d.charms) },
                        { "finishedMonsters", jsonOf(d.finishedMonsters) } };
}
QJsonValue jsonOf(const BestiaryMonsterData& d)
{
    return QJsonObject{ { "id", jsonOf(d.id) },
                        { "bestClass", jsonOf(d.bestClass) },
                        { "currentLevel", jsonOf(d.currentLevel) },
                        { "AnimusMasteryBonus", jsonOf(d.AnimusMasteryBonus) },
                        { "AnimusMasteryPoints", jsonOf(d.AnimusMasteryPoints) },
                        { "killCounter", jsonOf(d.killCounter) },
                        { "thirdDifficulty", jsonOf(d.thirdDifficulty) },
                        { "secondUnlock", jsonOf(d.secondUnlock) },
                        { "lastProgressKillCount", jsonOf(d.lastProgressKillCount) },
                        { "difficulty", jsonOf(d.difficulty) },
                        { "ocorrence", jsonOf(d.ocorrence) },
                        { "loot", jsonOf(d.loot) },
                        { "charmValue", jsonOf(d.charmValue) },
                        { "attackMode", jsonOf(d.attackMode) },
                        { "maxHealth", jsonOf(d.maxHealth) },
                        { "experience", jsonOf(d.experience) },
                        { "speed", jsonOf(d.speed) },
                        { "armor", jsonOf(d.armor) },
                        { "mitigation", d.mitigation },
                        { "combat", jsonOf(d.combat) },
                        { "location", jsonOf(d.location) },
                        { "leaderKilled", d.leaderKilled },
                        { "castsSpells", d.castsSpells } };
}
QJsonValue jsonOf(const TeamFinderLeaderData& d)
{
    return QJsonObject{ { "reset", d.reset },
                        { "minLevel", jsonOf(d.minLevel) },
                        { "maxLevel", jsonOf(d.maxLevel) },
                        { "vocationIDs", jsonOf(d.vocationIDs) },
                        { "teamSlots", jsonOf(d.teamSlots) },
                        { "freeSlots", jsonOf(d.freeSlots) },
                        { "timestamp", jsonOf(d.timestamp) },
                        { "teamType", jsonOf(d.teamType) },
                        { "bossID", jsonOf(d.bossID) },
                        { "huntType", jsonOf(d.huntType) },
                        { "huntArea", jsonOf(d.huntArea) },
                        { "questID", jsonOf(d.questID) },
                        { "members", jsonOf(d.members) } };
}
QJsonValue jsonOf(const TeamFinderListEntry& d)
{
    return QJsonObject{ { "leaderGuid", jsonOf(d.leaderGuid) },
                        { "leaderName", jsonOf(d.leaderName) },
                        { "minLevel", jsonOf(d.minLevel) },
                        { "maxLevel", jsonOf(d.maxLevel) },
                        { "vocationIDs", jsonOf(d.vocationIDs) },
                        { "teamSlots", jsonOf(d.teamSlots) },
                        { "membersCount", jsonOf(d.membersCount) },
                        { "timestamp", jsonOf(d.timestamp) },
                        { "teamType", jsonOf(d.teamType) },
                        { "bossID", jsonOf(d.bossID) },
                        { "huntType", jsonOf(d.huntType) },
                        { "huntArea", jsonOf(d.huntArea) },
                        { "questID", jsonOf(d.questID) },
                        { "status", jsonOf(d.status) } };
}

/// The types the host's field calls carry, each with its QTCLIENT_NOTIFY_ARG_* kind.
template <class T>
bool tryJson(const std::any& a, std::optional<QJsonValue>& out)
{
    if (const T* v = std::any_cast<T>(&a)) {
        out = jsonOf(*v);
        return true;
    }
    return false;
}

using TrackerRows = std::vector<std::tuple<uint16_t, uint32_t, uint16_t, uint16_t, uint16_t, uint8_t>>;

std::optional<QJsonValue> notifyStructJson(const std::any& a)
{
    std::optional<QJsonValue> out;
    tryJson<Position>(a, out) || tryJson<std::vector<BossCooldownData>>(a, out) || tryJson<BestiaryMonsterData>(a, out)
        || tryJson<BestiaryCharmsData>(a, out) || tryJson<TeamFinderLeaderData>(a, out)
        || tryJson<std::vector<TeamFinderListEntry>>(a, out) || tryJson<TrackerRows>(a, out)
        || tryJson<std::vector<std::vector<int>>>(a, out) || tryJson<std::vector<int>>(a, out)
        || tryJson<std::vector<uint16_t>>(a, out) || tryJson<std::vector<uint32_t>>(a, out)
        || tryJson<std::vector<std::string>>(a, out);
    return out;
}

int notifyValueKind(const std::any& a)
{
    if (!a.has_value())
        return QTCLIENT_NOTIFY_ARG_OTHER;
    const std::type_info& t = a.type();
    if (t == typeid(bool))
        return QTCLIENT_NOTIFY_ARG_BOOL;
    if (t == typeid(int) || t == typeid(uint8_t) || t == typeid(uint16_t) || t == typeid(uint32_t) || t == typeid(int64_t)
        || t == typeid(uint64_t) || t == typeid(int8_t) || t == typeid(int16_t) || t == typeid(tibia::client::Direction))
        return QTCLIENT_NOTIFY_ARG_INT;
    if (t == typeid(double) || t == typeid(float))
        return QTCLIENT_NOTIFY_ARG_DOUBLE;
    if (t == typeid(std::string) || t == typeid(std::string_view) || t == typeid(const char*) || t == typeid(QString))
        return QTCLIENT_NOTIFY_ARG_STRING;
    return notifyStructJson(a) ? QTCLIENT_NOTIFY_ARG_JSON : QTCLIENT_NOTIFY_ARG_OTHER;
}

std::optional<QJsonValue> notifyValueJson(const std::any& a)
{
    std::optional<QJsonValue> out;
    if (tryJson<bool>(a, out) || tryJson<int>(a, out) || tryJson<uint8_t>(a, out) || tryJson<uint16_t>(a, out)
        || tryJson<uint32_t>(a, out) || tryJson<int64_t>(a, out) || tryJson<uint64_t>(a, out) || tryJson<int8_t>(a, out)
        || tryJson<int16_t>(a, out) || tryJson<std::string>(a, out) || tryJson<QString>(a, out))
        return out;
    if (const auto* d = std::any_cast<tibia::client::Direction>(&a))
        return QJsonValue(static_cast<int>(*d));
    if (const auto* f = std::any_cast<double>(&a))
        return QJsonValue(*f);
    if (const auto* f = std::any_cast<float>(&a))
        return QJsonValue(static_cast<double>(*f));
    if (const auto* v = std::any_cast<std::string_view>(&a))
        return QJsonValue(QString::fromUtf8(v->data(), static_cast<qsizetype>(v->size())));
    if (const auto* c = std::any_cast<const char*>(&a))
        return QJsonValue(QString::fromUtf8(*c));
    return notifyStructJson(a);
}

/// Listeners of notify->subscribe_team_finder_changed, bound to the current session's storage.
struct TTeamFinderWatch
{
    int nextId = 1;
    int sessionListener = 0;
    std::vector<std::pair<int, std::function<void()>>> listeners;
    std::vector<QMetaObject::Connection> connections;
};

TTeamFinderWatch& teamFinderWatch()
{
    static TTeamFinderWatch watch;
    return watch;
}

void teamFinderChanged()
{
    const auto listeners = teamFinderWatch().listeners;
    for (const auto& [id, fn] : listeners)
        fn();
}

void bindTeamFinderStorage()
{
    TTeamFinderWatch& watch = teamFinderWatch();
    for (const QMetaObject::Connection& c : watch.connections)
        QObject::disconnect(c);
    watch.connections.clear();
    tibia::teamfinder::TTeamFinderStorage* storage = tibia::game::currentTeamFinderStorage();
    if (!storage)
        return;
    using Storage = tibia::teamfinder::TTeamFinderStorage;
    watch.connections.push_back(QObject::connect(storage, &Storage::teamParametersChanged, storage, &teamFinderChanged));
    watch.connections.push_back(QObject::connect(storage, &Storage::membersChanged, storage, &teamFinderChanged));
    watch.connections.push_back(QObject::connect(storage, &Storage::teamsChanged, storage, &teamFinderChanged));
}

// --- Notify nested API ---

struct NotifySimpleSlot {
    QtClientNotifySimpleFn fn = nullptr;
    void* user = nullptr;
};

std::mutex g_notifyMu;
std::unordered_map<int, NotifySimpleSlot> g_notifySimple;

int notifySubscribeSimple(const char* field, QtClientNotifySimpleFn fn, void* user)
{
    if (!field || !fn)
        return 0;
    auto slot = std::make_shared<NotifySimpleSlot>(NotifySimpleSlot{fn, user});
    const int id = tibia::game::TGameNotify::instance()->subscribe(
        field, [slot](const tibia::game::Args&) -> std::any {
            if (slot->fn)
                slot->fn(slot->user);
            return {};
        });
    std::lock_guard lock(g_notifyMu);
    g_notifySimple[id] = *slot;
    return id;
}

int notifySubscribeField(const char* field, QtClientNotifyFieldFn fn, void* user)
{
    if (!field || !fn)
        return 0;
    return tibia::game::TGameNotify::instance()->subscribe(
        field, [fn, user](const tibia::game::Args& args) -> std::any {
            fn(user, reinterpret_cast<const QtClientNotifyArgs*>(&args));
            return {};
        });
}

void notifyUnsubscribe(int id)
{
    tibia::game::TGameNotify::instance()->unsubscribe(id);
    std::lock_guard lock(g_notifyMu);
    g_notifySimple.erase(id);
}

int notifySubWorldTime(void (*fn)(void* user, int hour, int minute), void* user)
{
    if (!fn)
        return 0;
    return tibia::world::subscribeWorldTime([fn, user](int hour, int minute) { fn(user, hour, minute); });
}

void notifyUnsubWorldTime(int id) { tibia::world::unsubscribeWorldTime(id); }

int notifySubSessionEnd(void (*fn)(void* user), void* user)
{
    if (!fn)
        return 0;
    return tibia::world::subscribeSessionEnd([fn, user]() { fn(user); });
}

void notifyUnsubSessionEnd(int id) { tibia::world::unsubscribeSessionEnd(id); }

int notifySubSessionChanged(void (*fn)(void* user), void* user)
{
    if (!fn)
        return 0;
    return tibia::game::subscribeCurrentGameSessionChanged([fn, user]() { fn(user); });
}

void notifyUnsubSessionChanged(int id) { tibia::game::unsubscribeCurrentGameSessionChanged(id); }

int notifySubTeamFinderChanged(void (*fn)(void* user), void* user)
{
    if (!fn)
        return 0;
    TTeamFinderWatch& watch = teamFinderWatch();
    if (watch.sessionListener == 0) {
        watch.sessionListener = tibia::game::subscribeCurrentGameSessionChanged([]() {
            bindTeamFinderStorage();
            teamFinderChanged();
        });
        bindTeamFinderStorage();
    }
    const int id = watch.nextId++;
    watch.listeners.emplace_back(id, [fn, user]() { fn(user); });
    return id;
}

void notifyUnsubTeamFinderChanged(int id)
{
    std::erase_if(teamFinderWatch().listeners, [id](const auto& entry) { return entry.first == id; });
}

const tibia::game::Args* argsOf(const QtClientNotifyArgs* args)
{
    return reinterpret_cast<const tibia::game::Args*>(args);
}

int notifyArgString(const QtClientNotifyArgs* args, size_t index, char* buf, size_t buf_len)
{
    if (!args || !buf || buf_len == 0)
        return -1;
    QString text = tibia::game::argString(*argsOf(args), index);
    if (text.isEmpty() && index < argsOf(args)->size()) {
        if (const auto* q = std::any_cast<QString>(&(*argsOf(args))[index]))
            text = *q;
    }
    return writeBytesWritten(text.toUtf8(), buf, buf_len);
}

int64_t notifyArgInt(const QtClientNotifyArgs* args, size_t index)
{
    if (!args)
        return 0;
    const tibia::game::Args& a = *argsOf(args);
    if (index < a.size()) {
        if (const auto* d = std::any_cast<tibia::client::Direction>(&a[index]))
            return static_cast<int64_t>(*d);
    }
    return tibia::game::argInt(a, index);
}

int notifyArgBool(const QtClientNotifyArgs* args, size_t index)
{
    if (!args)
        return 0;
    return tibia::game::argBool(*argsOf(args), index) ? 1 : 0;
}

size_t notifyArgCount(const QtClientNotifyArgs* args)
{
    return args ? argsOf(args)->size() : 0;
}

int notifyArgKind(const QtClientNotifyArgs* args, size_t index)
{
    if (!args || index >= argsOf(args)->size())
        return QTCLIENT_NOTIFY_ARG_NONE;
    return notifyValueKind((*argsOf(args))[index]);
}

int notifyArgJson(const QtClientNotifyArgs* args, size_t index, char* buf, size_t buf_len)
{
    if (!args || index >= argsOf(args)->size())
        return -1;
    const std::optional<QJsonValue> value = notifyValueJson((*argsOf(args))[index]);
    if (!value)
        return -1;
    return writeSizedOut(compactJson(*value), buf, buf_len);
}

// --- v9 tail: the player edited the quick-loot lists (CipSoft UI) ---

struct TQuickLootWatch
{
    Subscribers<std::function<void()>> listeners;
    int sessionListener = 0;
    QMetaObject::Connection connection;
};

TQuickLootWatch& quickLootWatch()
{
    static TQuickLootWatch watch;
    return watch;
}

void bindQuickLootStorage()
{
    TQuickLootWatch& watch = quickLootWatch();
    QObject::disconnect(watch.connection);
    tibia::quickloot::TQuickLootBlackWhitelistStorage* storage = tibia::game::currentQuickLootBlackWhitelistStorage();
    if (!storage)
        return;
    watch.connection = QObject::connect(
        storage, &cip::tibia::quickloot::TQuickLootBlackWhitelistStorage::configurationChangedFromUserInput, storage,
        [] { quickLootWatch().listeners.call(); });
}

int notifySubQuickLootUserChanged(void (*fn)(void* user), void* user)
{
    if (!fn)
        return 0;
    TQuickLootWatch& watch = quickLootWatch();
    if (watch.sessionListener == 0) {
        watch.sessionListener = tibia::game::subscribeCurrentGameSessionChanged([] { bindQuickLootStorage(); });
        bindQuickLootStorage();
    }
    return watch.listeners.add([fn, user] { fn(user); });
}

void notifyUnsubQuickLootUserChanged(int id)
{
    quickLootWatch().listeners.remove(id);
}

// --- v9: the current session's container changes ---

struct TContainerWatch
{
    Subscribers<std::function<void(int, uint8_t)>> listeners;
    int sessionListener = 0;
    std::vector<QMetaObject::Connection> connections;
};

TContainerWatch& containerWatch()
{
    static TContainerWatch watch;
    return watch;
}

void bindContainerStorage()
{
    TContainerWatch& watch = containerWatch();
    for (const QMetaObject::Connection& c : watch.connections)
        QObject::disconnect(c);
    watch.connections.clear();
    tibia::container::TContainerStorage* storage = tibia::game::currentContainerStorage();
    if (!storage)
        return;
    using Storage = tibia::container::TContainerStorage;
    watch.connections.push_back(QObject::connect(storage, &Storage::containerUpdated, storage,
                                                 [](const tibia::container::TContainerID id, const bool isNew) {
                                                     containerWatch().listeners.call(
                                                         isNew ? QTCLIENT_CONTAINER_OPENED : QTCLIENT_CONTAINER_UPDATED,
                                                         static_cast<uint8_t>(id));
                                                 }));
    watch.connections.push_back(QObject::connect(storage, &Storage::containerRemoved, storage,
                                                 [](const tibia::container::TContainerID id) {
                                                     containerWatch().listeners.call(QTCLIENT_CONTAINER_REMOVED,
                                                                                     static_cast<uint8_t>(id));
                                                 }));
}

int notifySubContainers(void (*fn)(void* user, int event, uint8_t container_id), void* user)
{
    if (!fn)
        return 0;
    TContainerWatch& watch = containerWatch();
    if (watch.sessionListener == 0) {
        watch.sessionListener = tibia::game::subscribeCurrentGameSessionChanged([] { bindContainerStorage(); });
        bindContainerStorage();
    }
    return watch.listeners.add([fn, user](const int event, const uint8_t id) { fn(user, event, id); });
}

void notifyUnsubContainers(int id)
{
    containerWatch().listeners.remove(id);
}

int notifyEmitIntString(const char* field, int64_t a0, const char* a1)
{
    if (!field || !a1)
        return -1;
    tibia::game::notify(field, static_cast<int>(a0), std::string(a1));
    return 0;
}

const QtClientNotifyApiV1 g_notifyApi = {
    .subscribe_simple = &notifySubscribeSimple,
    .subscribe_field = &notifySubscribeField,
    .unsubscribe = &notifyUnsubscribe,
    .subscribe_world_time = &notifySubWorldTime,
    .unsubscribe_world_time = &notifyUnsubWorldTime,
    .subscribe_session_end = &notifySubSessionEnd,
    .unsubscribe_session_end = &notifyUnsubSessionEnd,
    .arg_string = &notifyArgString,
    .arg_int = &notifyArgInt,
    .arg_bool = &notifyArgBool,
    .emit_int_string = &notifyEmitIntString,
    .subscribe_session_changed = &notifySubSessionChanged,
    .unsubscribe_session_changed = &notifyUnsubSessionChanged,
    .subscribe_team_finder_changed = &notifySubTeamFinderChanged,
    .unsubscribe_team_finder_changed = &notifyUnsubTeamFinderChanged,
    .arg_count = &notifyArgCount,
    .arg_kind = &notifyArgKind,
    .arg_json = &notifyArgJson,
    .subscribe_containers = &notifySubContainers,
    .unsubscribe_containers = &notifyUnsubContainers,
    .subscribe_quick_loot_user_changed = &notifySubQuickLootUserChanged,
    .unsubscribe_quick_loot_user_changed = &notifyUnsubQuickLootUserChanged,
};

} // namespace

const QtClientNotifyApiV1* notifyApiV1()
{
    return &g_notifyApi;
}

} // namespace tibia::mods
