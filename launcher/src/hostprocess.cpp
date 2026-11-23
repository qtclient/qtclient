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
// holds the client program to start (argv[0]); without it the launcher boots.
// Canvases and the Module are dropped before the reload so the next engine has
// the memory.
EM_JS(void, qtclientReloadInto, (const char* clientProgram), {
    try {
        if (clientProgram)
            sessionStorage.setItem("tibia.play", UTF8ToString(clientProgram));
        sessionStorage.setItem("tibia.play.transition", "1");
    } catch (e) {}
    // Keep the branded cover up through canvas teardown and the document gap
    // before location.reload() — otherwise /play flashes white/empty.
    try {
        document.documentElement.style.background = "#0b0e12";
        if (document.body)
            document.body.style.background = "#0b0e12";
        var transition = document.getElementById("play-transition");
        if (transition) {
            transition.style.opacity = "1";
            transition.style.pointerEvents = "auto";
        }
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
        return false;
    }
    const QByteArray path = QFileInfo(program).absoluteFilePath().toUtf8();
    qtclientReloadInto(client ? path.constData() : nullptr);
    return true;
}

#else

bool startDetached(const QString& program, const QStringList& arguments, const QString& workingDirectory,
                   const QHash<QString, QString>& environment)
{
    QProcessEnvironment processEnvironment = QProcessEnvironment::systemEnvironment();
    for (auto it = environment.cbegin(); it != environment.cend(); ++it)
        processEnvironment.insert(it.key(), it.value());
    // Launcher may carry QTCLIENT_MODS_DIR for brand.json only (e.g. pack/launcher/).
    // The game client must load tip mods/<id> next to the binary (loginBase /
    // identity); an exclusive brand-only env would skip that and fall back to
    // CipSoft login hosts while still showing linked IdleOT chrome.
    const QString name = QFileInfo(program).fileName();
    if (name == QStringLiteral("client") || name == QStringLiteral("tibia")
        || name == QStringLiteral("tibia.js") || name.startsWith(QStringLiteral("client."))) {
        processEnvironment.remove(QStringLiteral("QTCLIENT_MODS_DIR"));
    }
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setWorkingDirectory(workingDirectory);
    process.setProcessEnvironment(processEnvironment);
    return process.startDetached();
}

#endif

} // namespace launcher::host
