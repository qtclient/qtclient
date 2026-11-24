#pragma once

#include <QDir>
#include <QString>
#include <QStringList>

#include <functional>
#include <string>
#include <tibia/game/tgamenotify.h>

class GameObject;

namespace tibia {
namespace client {

/// CIP's engine (gamecore: protocol, game model, assets, net) hosted on the
/// Qt GUI thread, the way CIP runs its logic: no qtclient window, graphics or
/// Lua. The app polls it; engine hooks arrive through TGameNotify / SoftHooks.
namespace gamecore {

/// Mounts `workDir` (monorepo: dir holding data/assets/; package: CipSoft
/// Resources next to the binary) and starts the engine.
bool init(const QString& argv0, const QString& workDir);
void terminate();
/// Runs on the GUI thread when SIGINT or SIGTERM arrives. Show the quit-game
/// prompt; do not quit the process from here.
void setForceExitHandler(std::function<void()> handler);

/// Runs network I/O and the engine dispatchers once.
void poll();

/// The engine time, in microseconds (the scale advanceTo takes).
qint64 nowMicros();
/// Moves the engine clock to `micros` and runs the events due by then: walk
/// steps, outfit and effect animations are then evaluated at that time.
void advanceTo(qint64 micros);
/// Back to the real time after advanceTo.
void resumeRealTime();

/// Sets the client and protocol version, the qtclient features of that version
/// (modules/game_features) and the server RSA key.
void setClientVersion(int version);

/// CIP: the package's Resources directory: the application directory's
/// parent, its `Resources` when there is one (QDir::cd leaves the directory
/// unchanged without one, so the same rule gives `Contents/Resources` on
/// macOS and the package root on Linux, Windows and /play). See
/// docs/reverse-engineering/cip-package-layout.md.
QDir resourcesDirectory();

/// Absolute CipSoft Resources directory (qtclient AppData / bundle / monorepo data/).
/// Empty when unresolved. Used for storeimages disk cache, etc.
QString cipResourcesDir();

/// CIP assets directory (contains catalog-content.json).
///
/// CipSoft's rule on every platform: Resources = applicationDirPath/.. plus
/// `Resources` when it exists (macOS `Contents/Resources`, Linux/Windows//play
/// the package root next to `bin/`), or the monorepo `workDir/data` when the
/// build runs outside a package. Registers it as the search root and returns
/// the short name `assets/`.
std::string assetsRoot(int version);

/// CIP sounds directory — sibling `sounds/` under the same Resources search root.
std::string soundsRoot(int version);

/// Pack / qtclient subscribe + arg helpers live on allowlisted `tibia/game/tgamenotify.h`.
using Hook = tibia::game::Hook;
using Args = tibia::game::Args;
using tibia::game::argString;
using tibia::game::argInt;
using tibia::game::argBool;

}

}
}
