// Host API v9: the options table (abi/options_api.h) over the client options
// (app/clientoptions.h, CipSoft TClientOptions). Split out of host_api_v1.cpp.
#include "mods/abi_internal.h"

#include "app/clientoptions.h"
#include "config/clientoptions.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLatin1String>
#include <QObject>
#include <QString>

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace tibia::mods {
namespace {

// --- Options nested API ---

char* dupCString(const QByteArray& bytes)
{
    char* p = static_cast<char*>(std::malloc(static_cast<size_t>(bytes.size()) + 1));
    if (!p)
        return nullptr;
    std::memcpy(p, bytes.constData(), static_cast<size_t>(bytes.size()));
    p[bytes.size()] = '\0';
    return p;
}

int optLoadJson(char** out_json)
{
    if (!out_json)
        return -1;
    const QJsonObject opts = tibia::app::ClientOptions::load().options();
    const QByteArray bytes = QJsonDocument(opts).toJson(QJsonDocument::Compact);
    *out_json = dupCString(bytes);
    return *out_json ? 0 : -1;
}

void optNotify(const QString& name);

/// ClientOptions::store; a host option (no CIP option, no signal of its own)
/// is announced to the pack's subscribers here.
void optStoreValue(const QString& name, const QJsonValue& value)
{
    tibia::config::EClientOption id;
    if (tibia::config::TClientOptions::optionId(name, id)) {
        tibia::app::ClientOptions::store(name, value);
        return;
    }
    const QJsonValue before = tibia::app::ClientOptions::load().options().value(name);
    tibia::app::ClientOptions::store(name, value);
    if (tibia::app::ClientOptions::load().options().value(name) != before)
        optNotify(name);
}

int optStore(const char* name, const char* value_json)
{
    if (!name || !value_json)
        return -1;
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(value_json));
    if (doc.isNull() && QByteArray(value_json).trimmed() != QByteArray("null")) {
        // Allow raw JSON strings that are values: try wrap parse of single value via QJsonValue
        QJsonParseError err{};
        const QJsonDocument asArr = QJsonDocument::fromJson(QByteArray("[") + value_json + "]", &err);
        if (err.error != QJsonParseError::NoError || !asArr.isArray() || asArr.array().isEmpty())
            return -1;
        optStoreValue(QString::fromUtf8(name), asArr.array().at(0));
        return 0;
    }
    if (doc.isObject())
        optStoreValue(QString::fromUtf8(name), doc.object());
    else if (doc.isArray())
        optStoreValue(QString::fromUtf8(name), doc.array());
    else
        optStoreValue(QString::fromUtf8(name), QJsonValue());
    return 0;
}

void optFree(char* p) { std::free(p); }

struct OptChangedSlot {
    QtClientOptionChangedFn fn = nullptr;
    void* user = nullptr;
};

std::mutex g_optMu;
std::unordered_map<int, OptChangedSlot> g_optSlots;
int g_optNextId = 1;

/// Calls every subscriber with `name` (a CIP option, a section or a host option).
void optNotify(const QString& name)
{
    std::vector<OptChangedSlot> targets;
    {
        std::lock_guard lock(g_optMu);
        targets.reserve(g_optSlots.size());
        for (const auto& [id, slot] : g_optSlots)
            targets.push_back(slot);
    }
    const QByteArray utf8 = name.toUtf8();
    for (const OptChangedSlot& slot : targets)
        slot.fn(slot.user, utf8.constData());
}

/// The client options' change signals, by name, to the pack's subscribers
/// (connected once, on the first subscription).
void optForwardChanges()
{
    static bool forwarding = false;
    if (forwarding)
        return;
    forwarding = true;
    using Options = tibia::config::TClientOptions;
    Options* options = Options::instance();
    QObject::connect(options, &Options::optionChanged, options,
                     [](const tibia::config::EClientOption id) { optNotify(Options::optionName(id)); });
    const auto section = [options](auto signal, const char* name) {
        QObject::connect(options, signal, options, [name] { optNotify(QLatin1String(name)); });
    };
    section(&Options::clientWindowOptionsChanged, "clientWindowOptions");
    section(&Options::npcDialogOptionsChanged, "npcDialogOptions");
    section(&Options::buttonBarOptionsChanged, "controlButtonsOptions");
    section(&Options::hotkeyOptionsChanged, "hotkeyOptions");
    section(&Options::chatOptionsChanged, "chatOptions");
    section(&Options::whiteBlacklistOptionsChanged, "whiteBlacklistOptions");
    section(&Options::surveyInvitationOptionsChanged, "survey");
    section(&Options::newsOptionsChanged, "news");
    section(&Options::characterSelectionControllerOptionsChanged, "pinnedCharacters");
    section(&Options::specialConditionsOptionsChanged, "specialConditionsOptions");
}

int optSubscribeChanged(QtClientOptionChangedFn fn, void* user)
{
    if (!fn)
        return 0;
    optForwardChanges();
    std::lock_guard lock(g_optMu);
    const int id = g_optNextId++;
    g_optSlots.emplace(id, OptChangedSlot{ fn, user });
    return id;
}

void optUnsubscribeChanged(int id)
{
    std::lock_guard lock(g_optMu);
    g_optSlots.erase(id);
}

const QtClientOptionsApiV1 g_optionsApi = {
    .load_options_json = &optLoadJson,
    .store = &optStore,
    .free_string = &optFree,
    .subscribe_changed = &optSubscribeChanged,
    .unsubscribe_changed = &optUnsubscribeChanged,
};

} // namespace

const QtClientOptionsApiV1* optionsApiV1()
{
    return &g_optionsApi;
}

} // namespace tibia::mods
