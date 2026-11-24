#pragma once

#include <QObject>
#include <QString>

#include <any>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tibia {
namespace game {

/// CIP session notify: typed Qt signals for high-traffic fields, plus the
/// string-field bus the pack ABI subscribes to (`notify_api.h`).
class TGameNotify final : public QObject
{
    Q_OBJECT

public:
    using Args = std::vector<std::any>;
    /// A field handler. Returning a non-empty any is the last-wins result for
    /// `notifyR`.
    using FieldHandler = std::function<std::any(const Args& args)>;

    static TGameNotify* instance();

    /// Subscribes `handler` to `field`; the id unsubscribes it.
    int subscribe(std::string_view field, FieldHandler handler);
    void unsubscribe(int id);

    bool wants(std::string_view field) const;
    std::any publish(std::string_view field, const Args& args);

    void emitGameStarted();
    void emitGameEnded();
    void emitCharacterSwitched(const QString& name);
    void emitConnectionFailing(bool failing);
    void emitTextMessage(int mode, const QString& text);
    void emitResourcesBalanceChanged(qint64 value, qint64 oldBalance, int type);
    void emitServerError(int code, const QString& message);

Q_SIGNALS:
    void gameStarted();
    void gameEnded();
    void characterSwitched(const QString& name);
    void connectionFailing(bool failing);
    void textMessage(int mode, const QString& text);
    void resourcesBalanceChanged(qint64 value, qint64 oldBalance, int type);
    void serverError(int code, const QString& message);

private:
    explicit TGameNotify(QObject* parent = nullptr);

    struct Entry {
        int id = 0;
        FieldHandler handler;
    };

    int m_nextId = 1;
    std::unordered_map<std::string, std::vector<Entry>> m_handlers;
};

using Args = TGameNotify::Args;

/// RAII field subscription.
class SoftHook
{
public:
    SoftHook() = default;
    SoftHook(std::string_view field, TGameNotify::FieldHandler handler);
    SoftHook(SoftHook&& other) noexcept;
    SoftHook& operator=(SoftHook&& other) noexcept;
    SoftHook(const SoftHook&) = delete;
    SoftHook& operator=(const SoftHook&) = delete;
    ~SoftHook();

private:
    int m_id = 0;
};

/// SoftHook arguments as the engine passed them.
QString argString(const Args& args, size_t index);
qint64 argInt(const Args& args, size_t index);
bool argBool(const Args& args, size_t index);

}
}
