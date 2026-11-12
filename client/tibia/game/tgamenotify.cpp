#include "tgamenotify.h"

#include <tibia/game/gamenotify.h>

#include <algorithm>
#include <utility>

namespace tibia::game {

TGameNotify::TGameNotify(QObject* parent)
    : QObject(parent)
{
}

TGameNotify* TGameNotify::instance()
{
    static TGameNotify hub;
    return &hub;
}

int TGameNotify::subscribe(std::string_view field, FieldHandler handler)
{
    const int id = m_nextId++;
    m_handlers[std::string(field)].push_back(Entry{ id, std::move(handler) });
    return id;
}

void TGameNotify::unsubscribe(const int id)
{
    for (auto it = m_handlers.begin(); it != m_handlers.end();) {
        auto& entries = it->second;
        entries.erase(std::remove_if(entries.begin(), entries.end(),
                                     [id](const Entry& e) { return e.id == id; }),
                      entries.end());
        it = entries.empty() ? m_handlers.erase(it) : std::next(it);
    }
}

bool TGameNotify::wants(const std::string_view field) const
{
    const auto it = m_handlers.find(std::string(field));
    return it != m_handlers.end() && !it->second.empty();
}

std::any TGameNotify::publish(const std::string_view field, const Args& args)
{
    const auto it = m_handlers.find(std::string(field));
    if (it == m_handlers.end())
        return {};
    std::any result;
    const auto entries = it->second;
    for (const auto& entry : entries) {
        if (std::any r = entry.handler(args); r.has_value())
            result = std::move(r);
    }
    return result;
}

void TGameNotify::emitGameStarted()
{
    Q_EMIT gameStarted();
}

void TGameNotify::emitGameEnded()
{
    Q_EMIT gameEnded();
}

void TGameNotify::emitCharacterSwitched(const QString& name)
{
    Q_EMIT characterSwitched(name);
}

void TGameNotify::emitConnectionFailing(bool failing)
{
    Q_EMIT connectionFailing(failing);
}

void TGameNotify::emitTextMessage(int mode, const QString& text)
{
    Q_EMIT textMessage(mode, text);
}

void TGameNotify::emitResourcesBalanceChanged(qint64 value, qint64 oldBalance, int type)
{
    Q_EMIT resourcesBalanceChanged(value, oldBalance, type);
}

void TGameNotify::emitDeath(int deathType, int penality)
{
    Q_EMIT death(deathType, penality);
}

void TGameNotify::emitServerError(int code, const QString& message)
{
    Q_EMIT serverError(code, message);
}

SoftHook::SoftHook(std::string_view field, TGameNotify::FieldHandler handler)
    : m_id(TGameNotify::instance()->subscribe(field, std::move(handler)))
{
}

SoftHook::SoftHook(SoftHook&& other) noexcept
    : m_id(other.m_id)
{
    other.m_id = 0;
}

SoftHook& SoftHook::operator=(SoftHook&& other) noexcept
{
    if (this != &other) {
        if (m_id)
            TGameNotify::instance()->unsubscribe(m_id);
        m_id = other.m_id;
        other.m_id = 0;
    }
    return *this;
}

SoftHook::~SoftHook()
{
    if (m_id)
        TGameNotify::instance()->unsubscribe(m_id);
}

Hook::Hook(const std::string_view scope, const std::string_view field, Handler handler)
{
    // Legacy GameEvents scopes ("cip" / "object") share SoftHooks; self is unused.
    (void)scope;
    m_id = TGameNotify::instance()->subscribe(
        field, [handler = std::move(handler)](const Args& args) {
            return handler(nullptr, args);
        });
}

Hook::Hook(Hook&& other) noexcept
    : m_id(std::exchange(other.m_id, 0))
{}

Hook& Hook::operator=(Hook&& other) noexcept
{
    if (this != &other) {
        if (m_id)
            TGameNotify::instance()->unsubscribe(m_id);
        m_id = std::exchange(other.m_id, 0);
    }
    return *this;
}

Hook::~Hook()
{
    if (m_id)
        TGameNotify::instance()->unsubscribe(m_id);
}

QString argString(const Args& args, const size_t index)
{
    if (index >= args.size())
        return {};
    const auto& a = args[index];
    if (const auto* s = std::any_cast<std::string>(&a))
        return QString::fromStdString(*s);
    if (const auto* v = std::any_cast<std::string_view>(&a))
        return QString::fromUtf8(v->data(), static_cast<qsizetype>(v->size()));
    if (const auto* c = std::any_cast<const char*>(&a))
        return QString::fromUtf8(*c);
    return {};
}

qint64 argInt(const Args& args, const size_t index)
{
    if (index >= args.size())
        return 0;
    const auto& a = args[index];
    if (const auto* v = std::any_cast<int>(&a)) return *v;
    if (const auto* v = std::any_cast<uint8_t>(&a)) return *v;
    if (const auto* v = std::any_cast<uint16_t>(&a)) return *v;
    if (const auto* v = std::any_cast<uint32_t>(&a)) return *v;
    if (const auto* v = std::any_cast<int64_t>(&a)) return *v;
    if (const auto* v = std::any_cast<uint64_t>(&a)) return static_cast<qint64>(*v);
    if (const auto* v = std::any_cast<int8_t>(&a)) return *v;
    if (const auto* v = std::any_cast<int16_t>(&a)) return *v;
    if (const auto* v = std::any_cast<bool>(&a)) return *v ? 1 : 0;
    return 0;
}

bool argBool(const Args& args, const size_t index)
{
    if (index < args.size())
        if (const auto* v = std::any_cast<bool>(&args[index]))
            return *v;
    return argInt(args, index) != 0;
}

} // namespace tibia::game
