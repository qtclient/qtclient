#include "stage.h"

#include "package.h"
#include "window.h"

#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleFactory>
#include <QUrl>

namespace tibia::launcher {

LauncherWindow* createEmbeddedLauncher(QApplication& app, const QString& cdnBase)
{
    if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion")))
        app.setStyle(fusion);

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);

    auto* client = new PackageClient(&app);
    client->setCdnBase(cdnBase);
    const QString tip = qEnvironmentVariable("QTCLIENT_TIP_URL");
    if (!tip.isEmpty())
        client->setTipUrl(QUrl(tip));
    client->setDataDir(dataDir);

    auto* window = new LauncherWindow(client);
    // Parent to the app so the window outlives this function without a nested
    // event loop. Play/Quit handlers are wired by main.
    window->setAttribute(Qt::WA_DeleteOnClose, true);
    window->show();
    return window;
}

} // namespace tibia::launcher
