#pragma once

// CIP-named teamfinder surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QDateTime>
#include <QObject>

namespace tibia {
namespace teamfinder {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TTeamFinderProtocolMessageHandler final : public cip::tibia::teamfinder::TTeamFinderProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::teamfinder::TTeamFinderProtocolMessageHandler::TTeamFinderProtocolMessageHandler;
};

class TTeamFinderStorage final : public cip::tibia::teamfinder::TTeamFinderStorage
{
    Q_OBJECT
public:
    using cip::tibia::teamfinder::TTeamFinderStorage::TTeamFinderStorage;
};

/// CIP: TTeamStartTimeManager (Join/Assemble start-time controls).
class TTeamStartTimeManager final : public cip::tibia::teamfinder::TTeamStartTimeManager
{
    Q_OBJECT
public:
    explicit TTeamStartTimeManager(QObject* parent = nullptr);

    QString localDateString() const override;
    QString localTimeString() const override;
    QString serverTimeString() const override;
    int hour() const override;
    int minute() const override;
    bool startImmediately() const override;
    void setStartImmediately(bool v) override;

    Q_INVOKABLE void setTime(int Hour, int Minute) override;

    /// Unix timestamp for assemble/join payloads (0 when startImmediately).
    uint32_t unixTime() const;

private:
    void refreshStrings();

    QDateTime m_when;
};

/// CIP: TTeamSettings (Assemble Team settings pane) — qtclient twin only for
/// startTimeManager wiring + sensible CIP defaults.
class TTeamSettings final : public cip::tibia::teamfinder::TTeamSettings
{
    Q_OBJECT

public:
    explicit TTeamSettings(QObject* parent = nullptr);

    QString minLevel() const override;
    void setMinLevel(const QString& v) override;
    QString maxLevel() const override;
    void setMaxLevel(const QString& v) override;
    QString teamSize() const override;
    void setTeamSize(const QString& v) override;
    QString freeSlots() const override;
    void setFreeSlots(const QString& v) override;
    bool druidNeeded() const override;
    void setDruidNeeded(bool v) override;
    bool knightNeeded() const override;
    void setKnightNeeded(bool v) override;
    bool paladinNeeded() const override;
    void setPaladinNeeded(bool v) override;
    bool sorcererNeeded() const override;
    void setSorcererNeeded(bool v) override;
    bool monkNeeded() const override;
    void setMonkNeeded(bool v) override;
    bool includeParty() const override;
    void setIncludeParty(bool v) override;
    QObject* startTimeManager() const override;

    TTeamStartTimeManager* startTime() const { return m_time; }

Q_SIGNALS:
    /// CIP NOTIFY for settings properties — tip redeclare; base setters emit.

private:
    TTeamStartTimeManager* m_time = nullptr;
};

}
}
