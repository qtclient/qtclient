#pragma once

#include <QByteArray>
#include <QString>

#if defined(__EMSCRIPTEN__)

/// Register QAbstractFileEngineHandler for `/home/web_user`, `/mods`, and
/// `/data` when Module.qtclientFs is live. Product code keeps using QFile; I/O goes
/// to the JS OPFS Worker. Returns true when the handler was installed.
bool tibiaRegisterQtclientFsFileEngine();

/// One-shot qtclient-fs whole-file read (JS assemble → single HEAP copy). Use for
/// multi-MB assets; chunked QFile::readAll across JSPI suspends can corrupt.
QByteArray tibiaQtclientFsReadFile(const QString& path);

#endif
