#pragma once

#if defined(__EMSCRIPTEN__)

/// Mounts the origin OPFS at /home/web_user (and links /mods to
/// /home/web_user/mods) through the portal's qtclient-fs Worker
/// (Module.qtclientFs). Call first thing in main(), before anything touches
/// files. Returns false (after logging why) when the portal did not provide
/// the Worker or the mount failed.
bool qtclientMountOpfs();

#endif
