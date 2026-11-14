#pragma once

#include <QLoggingCategory>
#include <QString>

#include <client/position.h>

class QElapsedTimer;

namespace tibia::creatures {

Q_DECLARE_LOGGING_CATEGORY(lcWalk)
Q_DECLARE_LOGGING_CATEGORY(lcWalkGoPath)

/// Enable with `QTCLIENT_WALK_DEBUG=1` (or QT_LOGGING_RULES).
/// `QTCLIENT_WALK_DEBUG=verbose` also enables worldmap + per-frame next-skip.
/// Covers body tween, foot phase/grace, GoPath/auto-walk, and black-world frames.
void enableWalkDebugFromEnv();
bool walkDebugVerbose();

inline bool walkDebugEnabled()
{
    return lcWalk().isDebugEnabled() || lcWalkGoPath().isDebugEnabled();
}

inline QString walkPos(const Position& pos)
{
    return QString::fromStdString(pos.toString());
}

/// Rate-limited helper for noisy per-frame / per-tick snapshots.
bool walkDebugShouldLog(QElapsedTimer& timer, int intervalMs = 200);

} // namespace tibia::creatures
