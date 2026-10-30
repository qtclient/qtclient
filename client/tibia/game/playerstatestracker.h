#pragma once

#include "client/gamecore.h"
#include "game/playerstates.h"

#include <QObject>

namespace tibia::game {

/// Keeps a TPlayerStateDisplayInfoModel in step with the local player (CIP:
/// TPlayerInventoryAndStatusController 0x100b779f0 and its resting-area
/// setter 0x100b780e0): states, Bakragore's taint, skull, guild war emblem,
/// hunger and the resting area.
class TPlayerStatesTracker final : public QObject
{
    Q_OBJECT
public:
    TPlayerStatesTracker(config::SpecialConditionsFilter filter, QObject* parent = nullptr);

    TPlayerStateDisplayInfoModel* model() { return &m_model; }
    const config::SpecialConditionsOptions& options() const { return m_options; }

    /// Rereads clientoptions.json's specialConditionsOptions.
    void reloadOptions();
    void refresh();

Q_SIGNALS:
    void restingAreaChanged(bool inRestingArea, bool hasBonus, const QString& bonusText);
    /// The options dialog stored a new specialConditionsOptions (now reloaded).
    void optionsChanged();

private:
    config::SpecialConditionsFilter m_filter;
    config::SpecialConditionsOptions m_options;
    TPlayerStateDisplayInfoModel m_model;
    bool m_inRestingArea = false;
    bool m_restingAreaBonus = false;
    QString m_restingAreaText;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
