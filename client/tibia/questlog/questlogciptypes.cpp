#include "questlog/questlogciptypes.h"

#include "config/sidebaroptions.h"
#include "protocol/protocolciptypes.h"
#include "iclockservice.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>


#include <QDateTime>
#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>

#include <protocol.pb.h>

namespace tibia::questlog {

namespace {

const QString CompletedMarker = QStringLiteral("(completed)");

const QString TrackedQuestsKey = QStringLiteral("trackedQuests");
const QString IdKey = QStringLiteral("id");
const QString IsPinnedKey = QStringLiteral("isPinned");
const QString AutoTrackNewQuestsKey = QStringLiteral("autoTrackNewQuests");
const QString AutoUntrackCompletedQuestsKey = QStringLiteral("autoUntrackCompletedQuests");
const QString ShowCompletedKey = QStringLiteral("showCompletedInQuestLog");
const QString ShowHiddenKey = QStringLiteral("showHiddenInQuestLog");
const QString HiddenQuestLinesKey = QStringLiteral("hiddenQuestLines");
const QString PinnedQuestLinesKey = QStringLiteral("pinnedQuestLines");
const QString OptionsKey = QStringLiteral("options");

QJsonArray toJsonArray(const std::set<TQuestLineID>& ids)
{
    QJsonArray array;
    for (const TQuestLineID id : ids)
        array.append(id);
    return array;
}

/// The set reduced to the ids that are quest lines.
std::set<TQuestLineID> knownQuestLines(const std::set<TQuestLineID>& ids, const std::map<TQuestLineID, TQuestLine>& questLines)
{
    std::set<TQuestLineID> known;
    for (const TQuestLineID id : ids) {
        if (questLines.contains(id))
            known.insert(id);
    }
    return known;
}

} // namespace

/// CIP: the server marks a finished mission by "(completed)" in its name.
bool hasCompletedMarker(const QString& name)
{
    return name.contains(CompletedMarker);
}

QString stripCompletedMarker(const QString& name)
{
    return QString(name).replace(CompletedMarker, QString()).trimmed();
}

TQuestLinePart::TQuestLinePart(const TQuestFlagID partId, const QString& partName, const QString& partDescription)
    : id(partId)
{
    setData(partName, partDescription);
}

void TQuestLinePart::setData(const QString& partName, const QString& partDescription)
{
    name = partName;
    description = partDescription;
    completed = hasCompletedMarker(name);
    name = stripCompletedMarker(name);
}

TQuestFlag::TQuestFlag(const TQuestFlagID flagId, const bool isPinned)
    : id(flagId)
    , pinned(isPinned)
{
}

TQuestFlag::TQuestFlag(const TQuestFlagID flagId, const TQuestFlagData& flagData, const qint64 timestamp, const bool isPinned)
    : id(flagId)
    , data(flagData)
    , lastUpdateTimestamp(timestamp)
    , pinned(isPinned)
{
}

TQuestLogStorage::TQuestLogStorage(QObject* parent)
    : cip::tibia::questlog::TQuestLogStorage(parent)
{
}

TQuestLogStorage::~TQuestLogStorage() = default;

QString TQuestLogStorage::configurationName() const
{
    return QStringLiteral("questtracking");
}

QJsonObject TQuestLogStorage::configurationToJson() const
{
    QJsonArray trackedQuests;
    for (const TQuestFlag& flag : m_trackedQuestFlags)
        trackedQuests.append(QJsonObject{ { IdKey, int(flag.id) }, { IsPinnedKey, flag.pinned } });
    QJsonObject options;
    options.insert(AutoTrackNewQuestsKey, m_autoTrackNewQuests);
    options.insert(AutoUntrackCompletedQuestsKey, m_autoUntrackCompletedQuests);
    options.insert(ShowCompletedKey, m_showCompleted);
    options.insert(ShowHiddenKey, m_showHidden);
    QJsonObject json;
    json.insert(TrackedQuestsKey, trackedQuests);
    json.insert(OptionsKey, options);
    json.insert(HiddenQuestLinesKey, toJsonArray(m_hiddenQuestLines));
    json.insert(PinnedQuestLinesKey, toJsonArray(m_pinnedQuestLines));
    return json;
}

/// CIP: tracked flags are plain ids (older files) or {id, isPinned}; the server
/// fills their data in later.
void TQuestLogStorage::configurationFromJson(const QJsonObject& json)
{
    m_trackedQuestFlags.clear();
    if (json.contains(TrackedQuestsKey) && json[TrackedQuestsKey].type() == QJsonValue::Array) {
        for (const QJsonValue value : json[TrackedQuestsKey].toArray()) {
            if (value.type() == QJsonValue::Double) {
                m_trackedQuestFlags.emplace_back(TQuestFlagID(value.toInt()), false);
            } else if (value.type() == QJsonValue::Object) {
                const QJsonObject object = value.toObject();
                const int id = std::clamp(object.value(IdKey).toInt(0), 0, 0x7fffffff);
                m_trackedQuestFlags.emplace_back(TQuestFlagID(id), object.value(IsPinnedKey).toBool(false));
            }
        }
    }
    if (json.contains(OptionsKey) && json[OptionsKey].type() == QJsonValue::Object) {
        const QJsonObject options = json[OptionsKey].toObject();
        m_autoTrackNewQuests = options.value(AutoTrackNewQuestsKey).toBool(true);
        m_autoUntrackCompletedQuests = options.value(AutoUntrackCompletedQuestsKey).toBool(true);
        m_showCompleted = options.value(ShowCompletedKey).toBool(true);
        m_showHidden = options.value(ShowHiddenKey).toBool(false);
    }
    if (json.contains(HiddenQuestLinesKey) && json[HiddenQuestLinesKey].type() == QJsonValue::Array)
        m_hiddenQuestLines = config::intSet(json[HiddenQuestLinesKey].toArray());
    if (json.contains(PinnedQuestLinesKey) && json[PinnedQuestLinesKey].type() == QJsonValue::Array)
        m_pinnedQuestLines = config::intSet(json[PinnedQuestLinesKey].toArray());
}

void TQuestLogStorage::resetConfiguration()
{
    m_trackedQuestFlags.clear();
    m_autoTrackNewQuests = true;
    m_autoUntrackCompletedQuests = true;
    m_showCompleted = true;
    m_showHidden = false;
    m_hiddenQuestLines.clear();
    m_pinnedQuestLines.clear();
}

void TQuestLogStorage::clearQuestLines()
{
    m_questLines.clear();
    Q_EMIT questLinesChanged();
}

/// CIP: the new list replaces the old; a quest line keeps the parts it had.
/// Hidden and pinned lines the list no longer has are dropped.
void TQuestLogStorage::setQuestLines(const std::vector<TQuestLine>& questLines)
{
    std::map<TQuestLineID, TQuestLine> oldQuestLines;
    for (const auto& [id, line] : m_questLines)
        oldQuestLines.emplace(id, line);
    m_questLines.clear();
    for (const TQuestLine& line : questLines) {
        m_questLines.emplace(line.id, line);
        if (oldQuestLines.contains(line.id))
            m_questLines.at(line.id).parts = oldQuestLines.at(line.id).parts;
    }
    Q_EMIT questLinesChanged();
    dropUnknownHiddenQuestLines();
    dropUnknownPinnedQuestLines();
}

void TQuestLogStorage::dropUnknownHiddenQuestLines()
{
    std::set<TQuestLineID> known = knownQuestLines(m_hiddenQuestLines, m_questLines);
    if (known == m_hiddenQuestLines)
        return;
    m_hiddenQuestLines = std::move(known);
    Q_EMIT hiddenQuesLinesChanged();
}

void TQuestLogStorage::dropUnknownPinnedQuestLines()
{
    std::set<TQuestLineID> known = knownQuestLines(m_pinnedQuestLines, m_questLines);
    if (known == m_pinnedQuestLines)
        return;
    m_pinnedQuestLines = std::move(known);
    Q_EMIT pinnedQuesLinesChanged();
}

std::size_t TQuestLogStorage::completedQuestLinesCount() const
{
    return std::ranges::count_if(m_questLines, [](const auto& entry) { return entry.second.completed; });
}

TQuestLine TQuestLogStorage::questLine(const TQuestLineID questLineId) const
{
    const auto it = m_questLines.find(questLineId);
    return it != m_questLines.end() ? it->second : TQuestLine{};
}

std::optional<TQuestLineID> TQuestLogStorage::questLineIdOfQuestFlag(const TQuestFlagID questFlagId) const
{
    for (const auto& [id, line] : m_questLines) {
        if (std::ranges::any_of(line.parts, [questFlagId](const TQuestLinePart& part) { return part.id == questFlagId; }))
            return id;
    }
    return std::nullopt;
}

void TQuestLogStorage::setQuestLineParts(const TQuestLineID questLineId, const std::vector<TQuestLinePart>& parts)
{
    const auto it = m_questLines.find(questLineId);
    if (it == m_questLines.end())
        return;
    it->second.parts = parts;
    Q_EMIT questLinePartsChanged(it->second.id);
}

/// CIP: the server's tracked flags. A full list drops the flags it does not
/// have. New flags get `timestamp`; a changed flag gets it unless it had no data
/// yet (read from the configuration). Each flag also names its quest line and part.
void TQuestLogStorage::setTrackedQuestFlags(const std::map<TQuestFlagID, TQuestFlagData>& questFlags, const qint64 timestamp,
                                            const bool fullList)
{
    bool removed = false;
    if (fullList) {
        const std::size_t oldSize = m_trackedQuestFlags.size();
        std::erase_if(m_trackedQuestFlags, [&questFlags](const TQuestFlag& flag) { return !questFlags.contains(flag.id); });
        removed = m_trackedQuestFlags.size() != oldSize;
    }
    bool added = false;
    for (const auto& [id, data] : questFlags) {
        const auto flag = std::ranges::find(m_trackedQuestFlags, id, &TQuestFlag::id);
        if (flag == m_trackedQuestFlags.end()) {
            m_trackedQuestFlags.emplace_back(id, data, timestamp, false);
            added = true;
        } else if (!(flag->data == data)) {
            const TQuestFlagData oldData = flag->data;
            flag->data = data;
            if (!(oldData == TQuestFlagData{}))
                flag->lastUpdateTimestamp = timestamp;
        }
        updateQuestLine(data.questLineId, data.questLineName);
        updateQuestLinePart(data.questLineId, id, data.questFlagName, data.description);
    }
    Q_EMIT trackedQuestFlagsChanged(added, removed);
    Q_EMIT configurationChangedFromUserInput();
}

void TQuestLogStorage::setQuestFlagPinned(const TQuestFlagID questFlagId, const bool pinned)
{
    const auto flag = std::ranges::find(m_trackedQuestFlags, questFlagId, &TQuestFlag::id);
    if (flag == m_trackedQuestFlags.end())
        return;
    flag->pinned = pinned;
    Q_EMIT trackedQuestFlagsChanged(false, false);
    Q_EMIT configurationChangedFromUserInput();
}

/// CIP: a renamed quest line loses its parts; a new one starts not completed.
void TQuestLogStorage::updateQuestLine(const TQuestLineID questLineId, const QString& name)
{
    const auto it = m_questLines.find(questLineId);
    if (it != m_questLines.end()) {
        if (it->second.name == name)
            return;
        it->second = TQuestLine{ .id = questLineId, .name = name, .completed = it->second.completed, .parts = {} };
    } else {
        m_questLines.emplace(questLineId, TQuestLine{ .id = questLineId, .name = name, .completed = false, .parts = {} });
    }
    Q_EMIT questLinesChanged();
}

void TQuestLogStorage::updateQuestLinePart(const TQuestLineID questLineId, const TQuestFlagID partId, const QString& name,
                                           const QString& description)
{
    const auto it = m_questLines.find(questLineId);
    if (it == m_questLines.end())
        return;
    std::vector<TQuestLinePart> parts = it->second.parts;
    const auto part = std::ranges::find(parts, partId, &TQuestLinePart::id);
    if (part == parts.end())
        parts.emplace_back(partId, name, description);
    else
        part->setData(name, description);
    it->second.parts = std::move(parts);
    Q_EMIT questLinePartsChanged(it->second.id);
}

/// CIP: the tracked flags whose name carries "(completed)".
std::vector<TQuestFlagID> TQuestLogStorage::completedTrackedQuestFlagIds() const
{
    std::vector<TQuestFlagID> ids;
    for (const TQuestFlag& flag : m_trackedQuestFlags) {
        if (hasCompletedMarker(flag.data.questFlagName))
            ids.push_back(flag.id);
    }
    return ids;
}

void TQuestLogStorage::setShowCompleted(const bool show)
{
    if (m_showCompleted == show)
        return;
    m_showCompleted = show;
    Q_EMIT filterOptionChanged();
    Q_EMIT configurationChangedFromUserInput();
}

void TQuestLogStorage::setShowHidden(const bool show)
{
    if (m_showHidden == show)
        return;
    m_showHidden = show;
    Q_EMIT filterOptionChanged();
    Q_EMIT configurationChangedFromUserInput();
}

void TQuestLogStorage::setHidden(const TQuestLineID questLineId, const bool hidden)
{
    if (hidden == m_hiddenQuestLines.contains(questLineId))
        return;
    if (hidden)
        m_hiddenQuestLines.insert(questLineId);
    else
        m_hiddenQuestLines.erase(questLineId);
    Q_EMIT hiddenQuesLinesChanged();
    Q_EMIT configurationChangedFromUserInput();
}

void TQuestLogStorage::setPinned(const TQuestLineID questLineId, const bool pinned)
{
    if (pinned == m_pinnedQuestLines.contains(questLineId))
        return;
    if (pinned)
        m_pinnedQuestLines.insert(questLineId);
    else
        m_pinnedQuestLines.erase(questLineId);
    Q_EMIT pinnedQuesLinesChanged();
    Q_EMIT configurationChangedFromUserInput();
}

TQuestLogProtocolMessageHandler::TQuestLogProtocolMessageHandler(std::shared_ptr<TQuestLogStorage> questLogStorage,
                                                                 std::shared_ptr<shared::IClockService> clockService)
    : cip::tibia::questlog::TQuestLogProtocolMessageHandler(nullptr)
    , m_clockService(std::move(clockService))
    , m_questLogStorage(std::move(questLogStorage))
{
}

void TQuestLogProtocolMessageHandler::handleQuestLogMessage(const ::tibia::protobuf::protocol::GameserverMessageQuestLog& message)
{
    std::vector<TQuestLine> questLines;
    questLines.reserve(static_cast<std::size_t>(message.quests_size()));
    for (const auto& quest : message.quests())
        questLines.push_back({ .id = static_cast<TQuestLineID>(quest.quest_id()),
                               .name = QString::fromStdString(quest.name()),
                               .completed = quest.completed(),
                               .parts = {} });
    m_questLogStorage->setQuestLines(questLines);
}

void TQuestLogProtocolMessageHandler::handleQuestLineMessage(const ::tibia::protobuf::protocol::GameserverMessageQuestLine& message)
{
    std::vector<TQuestLinePart> parts;
    parts.reserve(static_cast<std::size_t>(message.missions_size()));
    for (const auto& mission : message.missions())
        parts.emplace_back(static_cast<TQuestFlagID>(mission.mission_id()), QString::fromStdString(mission.name()),
                           QString::fromStdString(mission.description()));
    m_questLogStorage->setQuestLineParts(static_cast<TQuestLineID>(message.quest_id()), parts);
}

void TQuestLogProtocolMessageHandler::handleTrackedQuestFlagsMessage(
    const ::tibia::protobuf::protocol::GameserverMessageTrackQuestFlags& message)
{
    const qint64 timestamp = m_clockService->nowMillis();
    // Only a full list says whether more flags can be tracked.
    if (message.full_list())
        m_questLogStorage->setCanTrackAdditionalQuestFlags(static_cast<uint8_t>(message.remaining()) != 0);
    std::map<TQuestFlagID, TQuestFlagData> questFlags;
    for (const auto& mission : message.missions())
        questFlags[static_cast<TQuestFlagID>(mission.mission_id())] = { .questLineId = static_cast<TQuestLineID>(mission.quest_id()),
                                                                         .questLineName = QString::fromStdString(mission.quest_name()),
                                                                         .questFlagName = QString::fromStdString(mission.mission_name()),
                                                                         .description = QString::fromStdString(mission.description()) };
    m_questLogStorage->setTrackedQuestFlags(questFlags, timestamp, message.full_list());
}

} // namespace tibia::questlog
