#pragma once

#include "game/playerstates.h"

#include <QObject>
#include <QString>

#include <vector>

namespace tibia {
namespace game {

/// Keeps a TPlayerStateDisplayInfoModel in step with the local player (CIP:
/// TPlayerInventoryAndStatusController and its resting-area
/// setter): states, Bakragore's taint, skull, guild war emblem,
/// hunger and the resting area.
class TPlayerStatesTracker final : public QObject
{
    Q_OBJECT
public:
    TPlayerStatesTracker(config::SpecialConditionsFilter filter, QObject* parent = nullptr);
    ~TPlayerStatesTracker() override;

    TPlayerStateDisplayInfoModel* model() { return &m_model; }
    const config::SpecialConditionsOptions& options() const { return m_options; }

    /// Rereads clientoptions.json's specialConditionsOptions.
    void reloadOptions();
    void refresh();
    void updateRestingArea();

Q_SIGNALS:
    void restingAreaChanged(bool inRestingArea, bool hasBonus, const QString& bonusText);
    /// The options dialog stored a new specialConditionsOptions (now reloaded).
    void optionsChanged();

private:
    /// Host: the tracker follows the current game session's signals itself.
    void connectToCurrentGameSession();
    /// The own creature's skull and guild flag signals, once it is known.
    void connectToPlayerCreature();

    config::SpecialConditionsFilter m_filter;
    config::SpecialConditionsOptions m_options;
    TPlayerStateDisplayInfoModel m_model;
    bool m_inRestingArea = false;
    bool m_restingAreaBonus = false;
    bool m_lastInRestingArea = false;
    uint m_rewardStreak = 0;
    QString m_restingAreaText;
    int m_sessionListener = 0;
    std::vector<QMetaObject::Connection> m_sessionConnections;
    QMetaObject::Connection m_playerKillerMarkConnection;
    QMetaObject::Connection m_playerGuildFlagConnection;
};

}
}
