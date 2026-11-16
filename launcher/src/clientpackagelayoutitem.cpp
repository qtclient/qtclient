#include "clientpackagelayoutitem.h"

#include "brand.h"
#include "package.h"
#include "packageinstallation.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>

namespace launcher {

namespace {

QString image(const QString& fileName)
{
    return LauncherBrand::instance().imagePath(fileName);
}

const char* kLabelStyle = R"(
        QLabel {
          font-family: "Verdana";
          font-size: 13px;
          font-weight: bold;
          color: #bfbfbf;
        }
      )";

} // namespace

QString formatDownloadSpeed(quint64 bytesPerSecond)
{
    if (bytesPerSecond > 0x100000)
        return QStringLiteral("%1 MB/s").arg(double(bytesPerSecond) * 9.5367431640625e-07, 0, 'f', 1);
    if (bytesPerSecond > 0x400)
        return QStringLiteral("%1 KB/s").arg(double(bytesPerSecond) * 0.0009765625, 0, 'f', 1);
    return QStringLiteral("%1 B/s").arg(bytesPerSecond);
}

TClientPackageLayoutItem::TClientPackageLayoutItem(std::shared_ptr<TPackage> package, bool showPackageName)
    : m_package(std::move(package))
{
    m_packageImage = new QLabel();
    m_packageImage->setFixedSize(QPixmap(image(QStringLiteral("TibiaLogo_Classic.png"))).size());
    updatePackageImageFromPackageStatus();

    m_button = new QPushButton();
    m_button->setFixedSize(QPixmap(image(QStringLiteral("Button_Blue_idle.png"))).size());
    updateButtonFromPackageStatus();
    connect(m_button, &QPushButton::clicked, this, [this]() { Q_EMIT buttonClicked(m_package); });

    m_progressBar = new QProgressBar();
    m_progressBar->setFixedSize(m_button->size());
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(1);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setStyleSheet(QStringLiteral(R"(
        QProgressBar {
          border-image: url(%1);
          font-family: "Verdana";
          font-size: 13px;
          font-weight: bold;
          color: #bfbfbf;
          text-align: center;
          border-top: 2px;
          border-left: 2px;
          border-bottom: 3px;
          border-right: 3px;
        }
        QProgressBar::chunk {
          border-image: url(%2);
        }
      )")
                                     .arg(image(QStringLiteral("ProgressBar_BG.png")),
                                          image(QStringLiteral("ProgressBar.png"))));

    if (showPackageName) {
        m_packageName = new QLabel();
        m_packageName->setStyleSheet(QString::fromUtf8(kLabelStyle));
    }
    m_packageVersion = new QLabel();
    m_packageVersion->setStyleSheet(QString::fromUtf8(kLabelStyle));
    updatePackageNameAndVersion();

    m_defaultCheckBox = new QCheckBox(QStringLiteral("Default Client"));
    QSizePolicy policy = m_defaultCheckBox->sizePolicy();
    policy.setRetainSizeWhenHidden(true);
    m_defaultCheckBox->setSizePolicy(policy);
    m_defaultCheckBox->setStyleSheet(QStringLiteral(R"(
        QCheckBox {
          font-family: "Verdana";
          font-size: 13px;
          font-weight: bold;
          color: #bfbfbf;
        }
        QCheckBox::indicator:unchecked {
          image: url(%1);
        }
        QCheckBox::indicator:checked {
          image: url(%2);
        }
      )")
                                         .arg(image(QStringLiteral("checkbox-unchecked.png")),
                                              image(QStringLiteral("checkbox-checked.png"))));
    connect(m_defaultCheckBox, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
        if (state == Qt::Unchecked)
            Q_EMIT defaultChanged(m_package, false);
        else if (state == Qt::Checked)
            Q_EMIT defaultChanged(m_package, true);
    });

    auto* box = new QGroupBox();
    box->setAlignment(Qt::AlignHCenter);
    box->setStyleSheet(QStringLiteral(R"(
        QGroupBox {
          border-image: url(%1) 4 4 4 4 repeat;
          border-top: 4px transparent;
          border-left: 4px transparent;
          border-bottom: 4px transparent;
          border-right: 4px transparent;
          background-attachment: fixed;
          padding-top: -6px;
          padding-left: -2px;
          padding-bottom: -8px;
          padding-right: -2px;
        }
      )")
                           .arg(image(QStringLiteral("clientpackage-background.png"))));
    auto* boxLayout = new QVBoxLayout();
    boxLayout->addWidget(m_progressBar, 0, Qt::AlignHCenter);
    boxLayout->addWidget(m_button, 0, Qt::AlignHCenter);
    boxLayout->addSpacing(4);
    if (showPackageName)
        boxLayout->addWidget(m_packageName, 0, Qt::AlignHCenter);
    boxLayout->addWidget(m_packageVersion, 0, Qt::AlignHCenter);
    boxLayout->addWidget(m_defaultCheckBox, 0, Qt::AlignHCenter);
    boxLayout->setSpacing(0);
    box->setLayout(boxLayout);

    addWidget(m_packageImage, 0, Qt::AlignHCenter | Qt::AlignTop);
    addSpacing(4);
    addWidget(box, 0, Qt::AlignHCenter | Qt::AlignTop);

    connect(m_package.get(), &TPackage::localPackageStatusChanged, this,
            &TClientPackageLayoutItem::onLocalPackageStatusChanged);
    m_progressBar->setVisible(false);
    m_button->setVisible(true);
    if (m_package)
        m_defaultCheckBox->setVisible(m_package->localStatus() == TPackage::InstalledAndUsable);
}

TClientPackageLayoutItem::~TClientPackageLayoutItem() = default;

void TClientPackageLayoutItem::setDefaultChecked(bool isDefault)
{
    if (m_defaultCheckBox->isChecked() == isDefault)
        return;
    m_defaultCheckBox->setChecked(isDefault);
}

void TClientPackageLayoutItem::updatePackageImageFromPackageStatus()
{
    QString fileName = QStringLiteral("TibiaLogo_");
    const TPackageConfiguration* configuration = m_package->configurationForCurrentPlatform();
    if (configuration && configuration->generation() == 1) {
        fileName.append(QLatin1String("Classic"));
    } else {
        switch (m_package->variant()) {
        case TPackage::VariantPreview:
            fileName.append(QLatin1String("Preview"));
            break;
        case TPackage::VariantBeta:
            fileName.append(QLatin1String("Beta"));
            break;
        case TPackage::VariantTest:
            fileName.append(QLatin1String("Test"));
            break;
        default:
            fileName.append(QLatin1String("QT"));
        }
    }
    if (m_package->localStatus() != TPackage::InstalledAndUsable)
        fileName.append(QLatin1String("_disabled"));
    fileName.append(QLatin1String(".png"));
    const QString path = image(fileName);
    if (!m_package->logoLinkTarget().isEmpty()) {
        m_packageImage->setText(QStringLiteral("<a href=\"") + m_package->logoLinkTarget().toString()
                                + QStringLiteral("\"><img src=\"") + path + QStringLiteral("\"></a>"));
        m_packageImage->setTextFormat(Qt::RichText);
        m_packageImage->setOpenExternalLinks(true);
    } else {
        m_packageImage->setStyleSheet(QStringLiteral("\n          QLabel {\n            image: url(") + path
                                      + QStringLiteral(");\n          }\n        "));
    }
    m_packageImage->setToolTip(m_package->name());
}

void TClientPackageLayoutItem::updateButtonFromPackageStatus()
{
    QString color;
    QString action;
    switch (m_package->localStatus()) {
    case TPackage::Unknown:
        color = QStringLiteral("Red");
        break;
    case TPackage::NotInstalled:
        color = QStringLiteral("Blue");
        action = QStringLiteral("Download");
        break;
    case TPackage::PartiallyInstalled:
        color = QStringLiteral("Orange");
        action = QStringLiteral("Update");
        break;
    default:
        color = QStringLiteral("Green");
        action = QStringLiteral("Play");
    }
    m_button->setToolTip(action);
    m_button->setStyleSheet(QStringLiteral(R"(
        QPushButton {
          background-image: url(%1);
          image: url(%2);
          border: none;
        }
        QPushButton:hover {
          background-image: url(%3);
        }
        QPushButton:pressed {
          background-image: url(%4);
          padding-top: 1px;
          padding-left: 1px;
          padding-bottom: -1px;
          padding-right: -1px;
        }
        QToolTip {
          border: 1px solid black;
          background: #c0c0c0;
          padding-top: 1px;
          padding-left: 1px;
          padding-bottom: 1px;
          padding-right: 1px;
          font-family: "Verdana";
          font-size: 11px;
          font-weight: bold;
          color: #3F3F3F;
        }
      )")
                                .arg(image(QStringLiteral("Button_%1_idle.png").arg(color)),
                                     image(QStringLiteral("Icon_%1.png").arg(action)),
                                     image(QStringLiteral("Button_%1_hover.png").arg(color)),
                                     image(QStringLiteral("Button_%1_pressed.png").arg(color))));
}

void TClientPackageLayoutItem::updatePackageNameAndVersion()
{
    if (m_packageName)
        m_packageName->setText(m_package->name());
    QString version;
    if (const TPackageConfiguration* configuration = m_package->configurationForCurrentPlatform())
        version = configuration->packageVersion();
    m_packageVersion->setText(version);
}

void TClientPackageLayoutItem::onLocalPackageStatusChanged()
{
    updatePackageImageFromPackageStatus();
    updateButtonFromPackageStatus();
    updatePackageNameAndVersion();
    if (!m_package)
        return;
    m_defaultCheckBox->setVisible(m_package->localStatus() == TPackage::InstalledAndUsable);
}

void TClientPackageLayoutItem::updateDefaultClientCheckboxFromPackageStatus()
{
    if (!m_package)
        return;
    m_defaultCheckBox->setVisible(m_package->localStatus() == TPackage::InstalledAndUsable);
}

void TClientPackageLayoutItem::onInstallationOrUpdateStarted(TPackageInstallation* installationInProgress)
{
    TPackageFileDownloader* downloader = installationInProgress->downloader();
    m_progressBar->setValue(0);
    m_progressBar->setMaximum(1);
    m_progressBar->setFormat(QStringLiteral("Working..."));
    connect(downloader, &TPackageFileDownloader::downloadProgress, this,
            [this, downloader](TFileSize bytesDownloaded, TFileSize bytesTotal) {
                // Kilobytes keep large packages inside the progress bar's int range.
                m_progressBar->setValue(int(bytesDownloaded / 1024));
                m_progressBar->setMaximum(int(qMax<TFileSize>(1, bytesTotal / 1024)));
                m_progressBar->setFormat(downloader->hasDownloadSpeed()
                                             ? formatDownloadSpeed(downloader->downloadSpeed())
                                             : QStringLiteral("Working..."));
                m_packageVersion->setText(QStringLiteral("%1/%2 MB")
                                              .arg(double(bytesDownloaded) * 9.5367431640625e-07, 0, 'f', 1)
                                              .arg(double(bytesTotal) * 9.5367431640625e-07, 0, 'f', 1));
            });
    m_button->setVisible(false);
    m_progressBar->setVisible(true);
}

void TClientPackageLayoutItem::onInstallationOrUpdateFinished(TPackageInstallation*)
{
    m_progressBar->setVisible(false);
    m_button->setVisible(true);
    m_progressBar->setValue(0);
    updatePackageImageFromPackageStatus();
    updateButtonFromPackageStatus();
    updatePackageNameAndVersion();
}

} // namespace launcher
