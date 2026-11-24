#pragma once

#include <QByteArray>
#include <QString>

namespace tibia {
namespace game {

/// True when `bytes` begins with the magic "dmp" (a CIP session dump).
bool isSessiondumpDmpd(const QByteArray& bytes);
bool isSessiondumpDmpdFile(const QString& path);

/// Resolve tutorial fixture: `QTCLIENT_TUTORIAL_SESSIONDUMP`, then
/// `testdata/sessiondumps/tutorial-sessiondump.dmp`, then
/// `tmp/re/tibia/*/tutorial/tutorial-sessiondump.dmp`.
QString resolveTutorialSessiondumpPath();

}
}
