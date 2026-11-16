#pragma once

#include <QVBoxLayout>

#include <memory>

class QCheckBox;
class QLabel;
class QProgressBar;
class QPushButton;

namespace launcher {

class TPackage;
class TPackageInstallation;

/// Download speed text: "%1 B/s", "%1 KB/s" or "%1 MB/s" (one decimal).
QString formatDownloadSpeed(quint64 bytesPerSecond);

/// One client package tile of the main window: logo, action button,
/// progress bar, name/version and the default-client checkbox.
class TClientPackageLayoutItem : public QVBoxLayout
{
    Q_OBJECT
public:
    TClientPackageLayoutItem(std::shared_ptr<TPackage> package, bool showPackageName);
    ~TClientPackageLayoutItem() override;

    void setDefaultChecked(bool isDefault);

Q_SIGNALS:
    void defaultChanged(std::shared_ptr<launcher::TPackage> package, bool isDefault);
    void buttonClicked(std::shared_ptr<launcher::TPackage> package);

public Q_SLOTS:
    void onInstallationOrUpdateStarted(launcher::TPackageInstallation* installationInProgress);
    void onInstallationOrUpdateFinished(launcher::TPackageInstallation* installationInProgress);
    void onLocalPackageStatusChanged();
    void updatePackageNameAndVersion();
    void updatePackageImageFromPackageStatus();
    void updateButtonFromPackageStatus();
    void updateDefaultClientCheckboxFromPackageStatus();

private:
    std::shared_ptr<TPackage> m_package;
    QCheckBox* m_defaultCheckBox = nullptr;
    QLabel* m_packageImage = nullptr;
    QLabel* m_packageName = nullptr;
    QLabel* m_packageVersion = nullptr;
    QPushButton* m_button = nullptr;
    QProgressBar* m_progressBar = nullptr;
};

} // namespace launcher
