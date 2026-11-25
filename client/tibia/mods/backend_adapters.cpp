// Host API v9: the backends table (abi/backends.h). The host's MissionBackend /
// AttentionBackend / InstanceBackend over a pack's C vtable, bound by the host itself to
// CipSoft's quest tracker and quest log, the button bar and the Cyclopedia (map) dialog.
#include "mods/abi_internal.h"
#include "mods/attention_backend.h"
#include "mods/instance_backend.h"
#include "mods/mission_backend.h"
#include "mods/registry.h"

#include "gamewindow/buttonbarcontroller.h"
#include "gamewindow/cyclopediadialogcontroller.h"
#include "gamewindow/questlogcontroller.h"
#include "gamewindow/questtrackerwidgetcontroller.h"
#include "gamewindow/sidebarpanelsmanager.h"
#include "gamewindow/sidebarwidgetsmanager.h"

#include <QJsonArray>
#include <QJsonObject>

#include <cstring>
#include <memory>
#include <vector>

namespace tibia::mods {
namespace {

template <class T>
T copyVtable(const T* table)
{
    T copy{};
    if (table)
        std::memcpy(&copy, table, std::min(table->struct_size ? table->struct_size : sizeof(T), sizeof(T)));
    return copy;
}

/// Calls a sized-out JSON function, growing the buffer once when the text was cut.
template <class Fn>
QJsonObject readJsonObject(Fn&& call)
{
    std::vector<char> buf(4096);
    int n = call(buf.data(), buf.size());
    if (n >= static_cast<int>(buf.size())) {
        buf.resize(static_cast<size_t>(n) + 1);
        n = call(buf.data(), buf.size());
    }
    if (n < 0)
        return {};
    QJsonValue value;
    buf.back() = '\0';
    if (!parseJsonValue(buf.data(), value) || !value.isObject())
        return {};
    return value.toObject();
}

class TAbiMissionBackend final : public MissionBackend
{
public:
    explicit TAbiMissionBackend(const QtClientMissionBackendV1& v)
        : m_v(v)
    {
    }

    QHash<int, QVariantMap> trackerExtensions(const QList<TrackedMission>& missions) const override
    {
        QHash<int, QVariantMap> out;
        if (!m_v.tracker_extensions_json)
            return out;
        QJsonArray list;
        for (const TrackedMission& m : missions)
            list.append(QJsonObject{ { "questFlagId", m.questFlagId }, { "questLineId", m.questLineId }, { "name", m.name },
                                     { "completed", m.completed } });
        const QByteArray missionsJson = compactJson(list);
        const QJsonObject rows = readJsonObject([&](char* buf, size_t len) {
            return m_v.tracker_extensions_json(m_v.user, missionsJson.constData(), buf, len);
        });
        for (auto it = rows.begin(); it != rows.end(); ++it) {
            bool ok = false;
            const int flag = it.key().toInt(&ok);
            if (ok && it.value().isObject())
                out.insert(flag, it.value().toObject().toVariantMap());
        }
        return out;
    }

    QVariantMap questLogExtension(int questLineId, int questFlagId, const QString& missionName) const override
    {
        if (!m_v.quest_log_extension_json)
            return {};
        const QByteArray mission = missionName.toUtf8();
        return readJsonObject([&](char* buf, size_t len) {
                   return m_v.quest_log_extension_json(m_v.user, questLineId, questFlagId, mission.constData(), buf, len);
               })
            .toVariantMap();
    }

    void questLineSelected(int questLineId) override
    {
        if (m_v.quest_line_selected)
            m_v.quest_line_selected(m_v.user, questLineId);
    }

    void activate(int questLineId, int questFlagId, const QString& missionName, const QString& choice) override
    {
        if (!m_v.activate)
            return;
        const QByteArray mission = missionName.toUtf8();
        const QByteArray picked = choice.toUtf8();
        m_v.activate(m_v.user, questLineId, questFlagId, mission.constData(), picked.constData());
    }

private:
    QtClientMissionBackendV1 m_v;
};

class TAbiAttentionBackend final : public AttentionBackend
{
public:
    explicit TAbiAttentionBackend(const QtClientAttentionBackendV1& v)
        : m_v(v)
    {
    }
    bool rewardWall() const override { return m_v.reward_wall && m_v.reward_wall(m_v.user) != 0; }
    bool proficiency() const override { return m_v.proficiency && m_v.proficiency(m_v.user) != 0; }

private:
    QtClientAttentionBackendV1 m_v;
};

class TAbiInstanceBackend final : public InstanceBackend
{
public:
    explicit TAbiInstanceBackend(const QtClientInstanceBackendV1& v)
        : m_v(v)
    {
    }
    int subareaId() const override { return m_v.subarea_id ? m_v.subarea_id(m_v.user) : 0; }

private:
    QtClientInstanceBackendV1 m_v;
};

std::unique_ptr<TAbiMissionBackend> g_mission;
std::unique_ptr<TAbiAttentionBackend> g_attention;
std::unique_ptr<TAbiInstanceBackend> g_instance;

void bindMission()
{
    const HostObjects* host = hostObjects();
    if (!host)
        return;
    if (host->sidebarWidgets && host->sidebarWidgets->questTrackerWidget())
        host->sidebarWidgets->questTrackerWidget()->setMissionBackend(g_mission.get());
    if (host->questLog)
        host->questLog->setMissionBackend(g_mission.get());
}

void bindAttention()
{
    const HostObjects* host = hostObjects();
    if (host && host->sidebarPanels && host->sidebarPanels->buttonBar())
        host->sidebarPanels->buttonBar()->setAttentionStorage(g_attention.get());
}

void bindInstance()
{
    const HostObjects* host = hostObjects();
    if (host && host->cyclopediaDialog)
        host->cyclopediaDialog->setInstanceStorage(g_instance.get());
}

void beSetMission(const QtClientMissionBackendV1* backend)
{
    // Unbind before the old adapter goes.
    auto next = backend ? std::make_unique<TAbiMissionBackend>(copyVtable(backend)) : nullptr;
    std::swap(g_mission, next);
    setMissionBackend(g_mission.get());
    bindMission();
}

void beMissionChanged()
{
    if (g_mission)
        Q_EMIT g_mission->changed();
}

void beSetAttention(const QtClientAttentionBackendV1* backend)
{
    auto next = backend ? std::make_unique<TAbiAttentionBackend>(copyVtable(backend)) : nullptr;
    std::swap(g_attention, next);
    setAttentionBackend(g_attention.get());
    bindAttention();
}

void beAttentionChanged(const char* signal_id_utf8, int active)
{
    if (g_attention && signal_id_utf8)
        Q_EMIT g_attention->signalChanged(QString::fromUtf8(signal_id_utf8), active != 0);
}

void beSetInstance(const QtClientInstanceBackendV1* backend)
{
    auto next = backend ? std::make_unique<TAbiInstanceBackend>(copyVtable(backend)) : nullptr;
    std::swap(g_instance, next);
    setInstanceBackend(g_instance.get());
    bindInstance();
}

const QtClientBackendApiV1 g_backendApi = {
    .struct_size = sizeof(QtClientBackendApiV1),
    .set_mission_backend = &beSetMission,
    .mission_backend_changed = &beMissionChanged,
    .set_attention_backend = &beSetAttention,
    .attention_changed = &beAttentionChanged,
    .set_instance_backend = &beSetInstance,
};

} // namespace

void bindBackendsToHost()
{
    bindMission();
    bindAttention();
    bindInstance();
}

const QtClientBackendApiV1* backendApiV1()
{
    return &g_backendApi;
}

} // namespace tibia::mods
