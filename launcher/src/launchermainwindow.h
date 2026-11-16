#pragma once

#include "launcherconfigurationhandler.h"

#include <QMainWindow>
#include <QPointF>

#include <map>
#include <memory>
#include <vector>

class QGroupBox;
class QMessageBox;
class QNetworkAccessManager;
class QStackedLayout;

namespace shared {
class QTextBrowserWithNetwork;
}

namespace launcher {

class TClientPackageLayoutItem;
class TCloudflareCdnTrace;
class TInstallationManager;
class TPackage;
class TPackageInstallation;

/// The launcher window: EULA page, client package tiles, hint box.
/// The frameless window is dragged with the mouse.
class TLauncherMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    TLauncherMainWindow(const std::vector<std::shared_ptr<TPackage>>& packages,
                        TInstallationManager* installationManager, bool showPackageNames,
                        std::shared_ptr<QNetworkAccessManager> networkAccessManager, bool eulaAccepted,
                        std::shared_ptr<host::TDownloadPool> downloadPool,
                        std::shared_ptr<TCloudflareCdnTrace> cdnTrace);
    ~TLauncherMainWindow() override;

Q_SIGNALS:
    void defaultChanged(std::shared_ptr<launcher::TPackage> package, bool isDefault);
    void clientPackageButtonClicked(std::shared_ptr<launcher::TPackage> package);
    void eulaAccepted();

public Q_SLOTS:
    void setHint(launcher::THint hint);
    void onDefaultClientPackageCheckboxChanged(std::shared_ptr<launcher::TPackage> package, bool isDefault);
    void setDefault(std::shared_ptr<launcher::TPackage> package, bool isDefault);
    void onInstallationOrUpdateStarted(launcher::TPackageInstallation* installationInProgress);
    void onInstallationOrUpdateFinished(launcher::TPackageInstallation* installationInProgress);
    void onEulaAcceptClicked();
    void onAboutDialogRequested(const QString& link);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QWidget* createVersionLabel();
    QWidget* createAboutLabel();
    QWidget* createEulaText();
    QWidget* createEulaAcceptButton();
    QWidget* createHintBox();
    QWidget* createCipSoftLogo();
    TClientPackageLayoutItem* itemOf(const TPackage& package) const;
    /// Shows why a download failed, with the CDN trace once it is known.
    void showDownloadFailure(const QString& packageName);

    std::shared_ptr<host::TDownloadPool> m_downloadPool;
    std::map<int, TClientPackageLayoutItem*> m_items;
    shared::QTextBrowserWithNetwork* m_hintBrowser = nullptr;
    QGroupBox* m_hintBox = nullptr;
    QMessageBox* m_downloadFailureBox = nullptr;
    QStackedLayout* m_pages = nullptr;
    int m_defaultPackageType = -1;
    std::shared_ptr<QNetworkAccessManager> m_networkAccessManager;
    std::shared_ptr<TCloudflareCdnTrace> m_cdnTrace;
    QPointF m_dragPosition;
};

} // namespace launcher
