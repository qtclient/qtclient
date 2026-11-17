#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace launcher::host {

/// Host: CipSoft's launcher starts programs with QProcess::startDetached (the
/// client, and itself after a self-update). On the web the page is the
/// process: the program is handed to /play, which reloads into it (the client
/// package's engine, or the launcher again). `environment` is added to the
/// current one on desktop; /play sets the engine's own on the web.
bool startDetached(const QString& program, const QStringList& arguments, const QString& workingDirectory,
                   const QHash<QString, QString>& environment = {});

} // namespace launcher::host
