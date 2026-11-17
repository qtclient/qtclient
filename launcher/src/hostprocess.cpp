#include "hostprocess.h"

#include <QDebug>
#include <QFileInfo>

#ifdef Q_OS_WASM
#include <emscripten.h>
#else
#include <QProcess>
#include <QProcessEnvironment>
#endif

namespace launcher::host {

#ifdef Q_OS_WASM

namespace {

// /play's handoff (qtc-aac tibia-browser.ts PLAY_HANDOFF_KEY): "tibia.play"
// boots the client engine next, without it the launcher boots. Canvases and
// the Module are dropped before the reload so the next engine has the memory.
EM_JS(void, qtclientReloadInto, (int client), {
    try {
        if (client)
            sessionStorage.setItem("tibia.play", "1");
        sessionStorage.setItem("tibia.play.transition", "1");
    } catch (e) {}
    try {
        document.querySelectorAll("canvas").forEach(function(canvas) {
            canvas.width = 0;
            canvas.height = 0;
            canvas.remove();
        });
    } catch (e) {}
    try { window.Module = undefined; } catch (e) {}
    setTimeout(function() { location.reload(); }, 50);
});

} // namespace

bool startDetached(const QString& program, const QStringList& arguments, const QString& workingDirectory,
                   const QHash<QString, QString>& environment)
{
    Q_UNUSED(arguments);
    Q_UNUSED(workingDirectory);
    Q_UNUSED(environment);
    const QString name = QFileInfo(program).fileName();
    const bool client = name == QStringLiteral("tibia.js");
    if (!client && name != QStringLiteral("launcher.js")) {
        qWarning().noquote() << "web: cannot start" << program;
        return false;
    }
    qInfo().noquote() << "web: reloading into" << program;
    qtclientReloadInto(client ? 1 : 0);
    return true;
}

#else

bool startDetached(const QString& program, const QStringList& arguments, const QString& workingDirectory,
                   const QHash<QString, QString>& environment)
{
    QProcessEnvironment processEnvironment = QProcessEnvironment::systemEnvironment();
    for (auto it = environment.cbegin(); it != environment.cend(); ++it)
        processEnvironment.insert(it.key(), it.value());
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setWorkingDirectory(workingDirectory);
    process.setProcessEnvironment(processEnvironment);
    return process.startDetached();
}

#endif

} // namespace launcher::host
