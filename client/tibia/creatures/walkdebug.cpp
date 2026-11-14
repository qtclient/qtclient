#include "creatures/walkdebug.h"

#include <QElapsedTimer>
#include <QtGlobal>

namespace tibia::creatures {

Q_LOGGING_CATEGORY(lcWalk, "tibia.walk", QtWarningMsg)
Q_LOGGING_CATEGORY(lcWalkGoPath, "tibia.walk.gopath", QtWarningMsg)

namespace {
bool g_walkDebugVerbose = false;
}

void enableWalkDebugFromEnv()
{
    if (qEnvironmentVariableIsEmpty("QTCLIENT_WALK_DEBUG"))
        return;
    // Keep signal high: suppress per-frame next-skip / worldmap spam by default.
    // QTCLIENT_WALK_DEBUG=verbose enables the noisy categories too.
    g_walkDebugVerbose = qEnvironmentVariable("QTCLIENT_WALK_DEBUG") == QLatin1String("verbose");
    if (g_walkDebugVerbose) {
        QLoggingCategory::setFilterRules(QStringLiteral(
            "tibia.walk.debug=true\n"
            "tibia.walk.gopath.debug=true\n"
            "tibia.worldmap.debug=true"));
    } else {
        QLoggingCategory::setFilterRules(QStringLiteral(
            "tibia.walk.debug=true\n"
            "tibia.walk.gopath.debug=true\n"
            "tibia.worldmap.debug=false"));
    }
}

bool walkDebugVerbose()
{
    return g_walkDebugVerbose;
}

bool walkDebugShouldLog(QElapsedTimer& timer, const int intervalMs)
{
    if (!timer.isValid() || timer.elapsed() >= intervalMs) {
        timer.start();
        return true;
    }
    return false;
}

} // namespace tibia::creatures
