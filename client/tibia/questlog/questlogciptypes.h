#pragma once

#include "config/icharacterconfiguration.h"

// CIP-named questlog surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <protocol.pb.h>

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace tibia::protobuf::protocol {
class GameserverMessageQuestLog;
class GameserverMessageQuestLine;
class GameserverMessageTrackQuestFlags;
}

namespace tibia::shared {
class IClockService;
}

namespace tibia {
namespace questlog {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


using TQuestLineID = int;
using TQuestFlagID = quint16;

/// Whether a quest flag name carries the server's "(completed)" marker (CIP).
bool hasCompletedMarker(const QString& name);
/// The name without the "(completed)" marker (CIP).
QString stripCompletedMarker(const QString& name);

/// One mission of a quest line (CIP: 0x40-byte part). The completed state comes
/// from the name's marker, which is stripped.
struct TQuestLinePart
{
    TQuestLinePart() = default;
    TQuestLinePart(TQuestFlagID partId, const QString& partName, const QString& partDescription);
    /// CIP: sets name and description, then derives `completed` from the name.
    void setData(const QString& partName, const QString& partDescription);

    TQuestFlagID id = 0;
    QString name;
    QString description;
    bool completed = false;
};

/// One quest line of the quest log (CIP: 0x40-byte map value).
struct TQuestLine
{
    TQuestLineID id = 0;
    QString name;
    bool completed = false;
    std::vector<TQuestLinePart> parts;
};

/// What the server says about a tracked quest flag (CIP: 0x48-byte data).
struct TQuestFlagData
{
    TQuestLineID questLineId = -1;
    QString questLineName;
    QString questFlagName;
    QString description;

    bool operator==(const TQuestFlagData&) const = default;
};

/// One tracked quest flag (CIP: 0x68-byte entry).
struct TQuestFlag
{
    TQuestFlag() = default;
    /// CIP: a flag read from the character configuration, without data yet.
    TQuestFlag(TQuestFlagID flagId, bool isPinned);
    TQuestFlag(TQuestFlagID flagId, const TQuestFlagData& flagData, qint64 timestamp, bool isPinned);

    TQuestFlagID id = 0;
    TQuestFlagData data;
    qint64 lastUpdateTimestamp = 0;
    bool pinned = false;
};

/// The character's quest log and quest tracker state (CIP: questlog::TQuestLogStorage,
/// character configuration "questtracking").
class TQuestLogStorage final : public cip::tibia::questlog::TQuestLogStorage, public config::ICharacterConfiguration
{
    Q_OBJECT
public:
    explicit TQuestLogStorage(QObject* parent = nullptr);
    ~TQuestLogStorage() override;

    QString configurationName() const override;
    QJsonObject configurationToJson() const override;
    void configurationFromJson(const QJsonObject& json) override;
    void resetConfiguration() override;

    void clearQuestLines();
    const std::map<TQuestLineID, TQuestLine>& questLines() const { return m_questLines; }
    void setQuestLines(const std::vector<TQuestLine>& questLines);
    std::size_t completedQuestLinesCount() const;
    std::size_t hiddenQuestLinesCount() const { return m_hiddenQuestLines.size(); }
    TQuestLine questLine(TQuestLineID questLineId) const;
    std::optional<TQuestLineID> questLineIdOfQuestFlag(TQuestFlagID questFlagId) const;
    void setQuestLineParts(TQuestLineID questLineId, const std::vector<TQuestLinePart>& parts);

    void setCanTrackAdditionalQuestFlags(bool canTrack) { m_canTrackAdditionalQuestFlags = canTrack; }
    bool canTrackAdditionalQuestFlags() const { return m_canTrackAdditionalQuestFlags; }
    void setTrackedQuestFlags(const std::map<TQuestFlagID, TQuestFlagData>& questFlags, qint64 timestamp, bool fullList);
    void setQuestFlagPinned(TQuestFlagID questFlagId, bool pinned);
    void updateQuestLine(TQuestLineID questLineId, const QString& name);
    void updateQuestLinePart(TQuestLineID questLineId, TQuestFlagID partId, const QString& name, const QString& description);
    std::vector<TQuestFlag> trackedQuestFlags() const { return m_trackedQuestFlags; }
    std::vector<TQuestFlagID> completedTrackedQuestFlagIds() const;

    void setAutoTrackNewQuests(bool value) { m_autoTrackNewQuests = value; }
    bool autoTrackNewQuests() const { return m_autoTrackNewQuests; }
    void setAutoUntrackCompletedQuests(bool value) { m_autoUntrackCompletedQuests = value; }
    bool autoUntrackCompletedQuests() const { return m_autoUntrackCompletedQuests; }
    void setShowCompleted(bool show);
    bool showCompleted() const { return m_showCompleted; }
    void setShowHidden(bool show);
    bool showHidden() const { return m_showHidden; }

    std::set<TQuestLineID> hiddenQuestLines() const { return m_hiddenQuestLines; }
    void setHidden(TQuestLineID questLineId, bool hidden);
    std::set<TQuestLineID> pinnedQuestLines() const { return m_pinnedQuestLines; }
    void setPinned(TQuestLineID questLineId, bool pinned);

Q_SIGNALS:
    /// CIP signal the generated base leaves out (its TQuestLineID argument).
    void questLinePartsChanged(int QuestLineId);

private:
    void dropUnknownHiddenQuestLines();
    void dropUnknownPinnedQuestLines();

    std::map<TQuestLineID, TQuestLine> m_questLines;
    std::vector<TQuestFlag> m_trackedQuestFlags;
    bool m_canTrackAdditionalQuestFlags = true;
    bool m_autoTrackNewQuests = true;
    bool m_autoUntrackCompletedQuests = true;
    bool m_showCompleted = true;
    bool m_showHidden = false;
    std::set<TQuestLineID> m_hiddenQuestLines;
    std::set<TQuestLineID> m_pinnedQuestLines;
};

/// The session's handler of the quest log messages (CIP:
/// questlog::TQuestLogProtocolMessageHandler). It hands what the server sent to
/// the session's quest log storage; tracked flags are stamped with the
/// session clock's time.
class TQuestLogProtocolMessageHandler final : public cip::tibia::questlog::TQuestLogProtocolMessageHandler
{
    Q_OBJECT
public:
    TQuestLogProtocolMessageHandler(std::shared_ptr<TQuestLogStorage> questLogStorage,
                                    std::shared_ptr<shared::IClockService> clockService);

    /// The quest lines of the quest log.
    void handleQuestLogMessage(const ::tibia::protobuf::protocol::GameserverMessageQuestLog& message);
    /// The parts (missions) of one quest line.
    void handleQuestLineMessage(const ::tibia::protobuf::protocol::GameserverMessageQuestLine& message);
    /// The tracked quest flags; a full list also says whether more flags can be tracked.
    void handleTrackedQuestFlagsMessage(const ::tibia::protobuf::protocol::GameserverMessageTrackQuestFlags& message);

Q_SIGNALS:
    void sendGetQuestLog(const ::tibia::protobuf::protocol::GameclientMessageGetQuestLog& GetQuestLogMessage);
    void sendGetQuestLine(const ::tibia::protobuf::protocol::GameclientMessageGetQuestLine& GetQuestLineMessage);

private:
    std::shared_ptr<shared::IClockService> m_clockService;
    std::shared_ptr<TQuestLogStorage> m_questLogStorage;
};

}
}
