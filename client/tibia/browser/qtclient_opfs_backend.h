#pragma once

#if defined(__EMSCRIPTEN__)

/// Mounts the origin OPFS at /home/web_user (and links /mods to
/// /home/web_user/mods), served by the engine's qtclient-fs Worker
/// (browser/qtclient-fs.js). Call first thing in main(), before anything
/// touches files. Returns false (after logging why) when the mount failed.
bool qtclientMountOpfs();

#endif
