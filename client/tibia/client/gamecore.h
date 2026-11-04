#pragma once

#include <QString>
#include <QStringList>

#include <functional>
#include <string>
#include <tibia/game/tgamenotify.h>

class GameObject;

namespace tibia::client {

/// CIP's engine (gamecore: protocol, game model, assets, net) hosted on the
/// Qt GUI thread, the way CIP runs its logic: no qtclient window, graphics or
/// Lua. The app polls it; engine hooks arrive through GameEvents.
namespace gamecore {

/// Mounts `workDir` (the dir holding data/setup.otml) and starts the engine.
bool init(const QString& argv0, const QString& workDir);
void terminate();
bool isRunning();
/// True only if a caller still marks a forced exit. SIGINT and SIGTERM do not:
/// they show the quit-game prompt instead of closing the window.
bool isForceExit();
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

/// Absolute CipSoft Resources directory (tip AppData / bundle / monorepo data/).
/// Empty when unresolved. Used for storeimages disk cache, etc.
QString cipResourcesDir();

/// CIP assets directory (contains catalog-content.json).
///
/// Single layout for desktop + wasm: resolve Cip Resources
/// (`applicationDirPath()/../Resources`, AppData tip
/// `packages/<TIBIA_CLIENT_PACKAGE_DIR>/Contents/Resources`, or monorepo
/// `workDir/data`), register it as the search root, return short name `assets/`.
std::string assetsRoot(int version);

/// CIP sounds directory — sibling `sounds/` under the same Resources search root.
std::string soundsRoot(int version);

/// Loads appearances, static data, and proficiencies from assetsRoot(), and the
/// sound bank from soundsRoot().
bool loadAssets(int version, QString* error);

/// Pack / tip subscribe + arg helpers live on allowlisted `tibia/game/tgamenotify.h`.
using Hook = tibia::game::Hook;
using Args = tibia::game::Args;
using tibia::game::argString;
using tibia::game::argInt;
using tibia::game::argBool;

}

}
