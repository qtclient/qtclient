#pragma once

#include "randomhints.h"
#include "packageclient.h"

#include <QElapsedTimer>
#include <QMainWindow>
#include <QPointer>
#include <QVector>

#include <functional>
#include <memory>

class QStackedLayout;

class QCheckBox;
class QGraphicsProxyWidget;
class QGraphicsView;
class QGroupBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QResizeEvent;
class QWidget;
#ifndef __EMSCRIPTEN__
class QProcess;
#endif

namespace tibia::shared {
class QTextBrowserWithNetwork;
class TDownloadPool;
}

class LauncherWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LauncherWindow(PackageClient* client, QWidget* parent = nullptr);

    void start();
    /**
     * Optional Play hook (tests). Production wasm Play remounts into a game-only
     * Module (sessionStorage qtclient.play + location.reload). Desktop spawn is unchanged.
     */
    void setInProcessPlayHandler(std::function<void()> handler) { m_inProcessPlay = std::move(handler); }

private:
    enum class LogoState { Classic, Updating, Launching };

    void applyStyle();
    void paintBackground();
    void acceptEula();
    void setActionSkin(const QString& color, const QString& icon);
    void setLogoState(LogoState state);
    void applyLogoLink();
#ifdef __EMSCRIPTEN__
    void resizeEvent(QResizeEvent* event) override;
    void layoutStage();
#endif
    void applyHint(const QVector<tibia::shared::Hint>& hints);
    void loadFeed();
    void refresh();
    void setBusy(bool busy, const QString& label);
    void onProgress(qint64 bytes, qint64 total, int fileIndex, int fileCount);
    void onAction();
    void onDeleteOrphans();
    void installIfNeeded(bool thenPlay);
#ifdef __EMSCRIPTEN__
    void startAsync();
    void installIfNeededAsync(bool thenPlay);
    /** After the engine package: CIP assets (assets.json), as installIfNeeded does. */
    void installAssetsAsync(bool thenPlay, const QString& version);
    /** After assets: optional tip mods + minimap channels, then finish. */
    void installModsThenFinishAsync(bool thenPlay, const QString& version);
    void installMinimapThenFinishAsync(bool thenPlay, const QString& version);
    void finishInstallAsync(bool thenPlay, const QString& version);
#endif
    void play();
    /// Local DEV: `QTCLIENT_DEV_JSON` desktop.exe (stock --no-mods / tip pack).
    bool tryPlayDevClient();
    /// `detachTip`: CIP tip Play (`startDetached` + quit). DEV keeps a waited child.
    void spawn(const QString& executable, const QString& workDir, const QString& revision,
               const QStringList& clientArgs = {}, bool detachTip = false);
    void considerSelfUpdate(const CipMetadata& metadata);
    QString packageDir() const;
    QString localVersion() const;
    QString localAssetsStamp() const;
    QString localModsStamp() const;
    QString localMinimapStamp() const;
    /// True when mods channel off/omitted or local mods.json.sha256 matches remote.
    bool modsStampOk() const;
    /// True when minimap channel off/omitted or local minimap.json.sha256 matches remote.
    bool minimapStampOk() const;
    bool clientModsChannelActive() const;
    bool minimapChannelActive() const;
    /// CipSoft install vocabulary: Unknown → PartiallyInstalled → InstalledAndUsable.
    QString installStatusLabel() const;
    void fetchRemoteStampsParallel();
    /// Stamps (+ optional package.json) GETs in one EventLoop (CIP pre-decide).
    void fetchRemoteStampsAndPackageParallel(bool includePackage = true);

    PackageClient* m_client = nullptr;
    std::function<void()> m_inProcessPlay;
    CipPackageInfo m_package;
    CipBuild m_build;
    CipPackageDoc m_packageDoc;
    QUrl m_hintsUrl;
    QString m_remoteVersion;
    QString m_remoteAssetsVersion;
    QString m_remoteModsVersion;
    QString m_remoteMinimapVersion;
    bool m_hasGaps = false;
    bool m_busy = false;
    QElapsedTimer m_timer;
    /// CipSoft onMeasureDownloadSpeed-shaped window (last sample → now).
    qint64 m_speedSampleBytes = 0;
    qint64 m_speedSampleMs = 0;
    double m_downloadRateBytesPerSec = 0.0;

    QStackedLayout* m_stack = nullptr;
    tibia::shared::QTextBrowserWithNetwork* m_news = nullptr;
    /// Feeds m_news's remote resources (CIP: the main window's download pool).
    std::shared_ptr<tibia::shared::TDownloadPool> m_hintDownloadPool;
    QLabel* m_logo = nullptr;
    QLabel* m_version = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_buildLabel = nullptr;
    QLabel* m_mark = nullptr;
    QCheckBox* m_defaultClient = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_delete = nullptr;
    QPushButton* m_action = nullptr;
    QGroupBox* m_packageBox = nullptr;
    QGroupBox* m_hintBox = nullptr;
#ifdef __EMSCRIPTEN__
    QWidget* m_stage = nullptr;
    QGraphicsView* m_stageView = nullptr;
    QGraphicsProxyWidget* m_stageProxy = nullptr;
#endif
#ifndef __EMSCRIPTEN__
    QPointer<QProcess> m_game;
#endif
};
