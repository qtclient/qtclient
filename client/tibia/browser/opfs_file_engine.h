#pragma once

#include <QByteArray>
#include <QString>

#if defined(__EMSCRIPTEN__)

/// Register QAbstractFileEngineHandler for `/home/web_user`, `/mods`, and
/// `/data` when Module.tipFs is live. Product code keeps using QFile; I/O goes
/// to the JS OPFS Worker. Returns true when the handler was installed.
bool tibiaRegisterTipFsFileEngine();

/// One-shot tip-fs whole-file read (JS assemble → single HEAP copy). Use for
/// multi-MB assets; chunked QFile::readAll across JSPI suspends can corrupt.
QByteArray tibiaTipFsReadFile(const QString& path);

#endif
