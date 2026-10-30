#pragma once

#include "client/gamecore.h"

#include <QObject>
#include <QVector>

#include <cstdint>
#include <vector>

namespace tibia::gamewindow {

/// CipSoft subscribers for CIP g_game publishes that have no dedicated CipSoft
/// controller yet (gem atelier, player helpers, virtues yellow-border spell ids).
/// Owns hooks so orphan-publish stays green; CipSoft QML can bind the Qt signals
/// when those surfaces are fleshed.
class TCipSoftUiEventBridge final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int playerHelpers READ playerHelpers NOTIFY playerHelpersChanged FINAL)
    Q_PROPERTY(quint32 lastRevealedGemId READ lastRevealedGemId NOTIFY gemRevealed FINAL)
public:
    explicit TCipSoftUiEventBridge(QObject* parent = nullptr);

    int playerHelpers() const { return m_playerHelpers; }
    quint32 lastRevealedGemId() const { return m_lastRevealedGemId; }
    QVector<int> virtuesYellowBorderSpellIds() const { return m_virtuesYellowBorder; }

Q_SIGNALS:
    void playerHelpersChanged();
    void gemRevealed(quint32 gemId);
    void virtuesYellowBorderChanged();

private:
    std::vector<client::gamecore::Hook> m_hooks;
    int m_playerHelpers = 0;
    quint32 m_lastRevealedGemId = 0;
    QVector<int> m_virtuesYellowBorder;
};

}
