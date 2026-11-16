#include "launchermainwindow.h"

#include "brand.h"
#include "clientpackagelayoutitem.h"
#include "cloudflarecdntrace.h"
#include "installationmanager.h"
#include "launcherselfupdater.h"
#include "package.h"
#include "packageinstallation.h"
#include "textbrowserwithnetwork.h"

#include <QDebug>
#include <QFile>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QStackedLayout>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

namespace launcher {

namespace {

QString image(const QString& fileName)
{
    return LauncherBrand::instance().imagePath(fileName);
}

/// Scroll bar styling shared by the EULA text and the hint browser.
QString scrollBarStyle()
{
    QString style = QStringLiteral(R"CSS(        /*inner area*/
        QScrollBar::vertical  {
          border-image: url(%IMG(scrollbar-texture-vertical.png)%) 0 0 0 0 repeat;
          border-top: 6px transparent;
          border-left: 6px transparent;
          border-bottom: 6px transparent;
          border-right: 6px transparent;
          border: 0px solid red;
          background: none;
          width: 12px;
          margin: 12px 0 12px 0; /* match top/lower area*/
        }
        /*the knob*/
        QScrollBar::handle:vertical {
          border-image: url(%IMG(scrollbar-handle-vertical.png)%) 6 6 6 6;
          border-top: 6px transparent;
          border-left: 6px transparent;
          border-bottom: 6px transparent;
          border-right: 6px transparent;
          min-height: 12px;
        }
        /*lower arrow area*/
        QScrollBar::add-line:vertical {
          border: 0px solid red;
          background: none;
          height: 12px;
          subcontrol-position: bottom;
          subcontrol-origin: margin;
        }
        /*top arrow area*/
        QScrollBar::sub-line:vertical {
          border: 0px solid red;
          background: none;
          height: 12px;
          subcontrol-position: top;
          subcontrol-origin: margin;
        }
        /*arrows*/
        QScrollBar::up-arrow:vertical  {
          image:  url(%IMG(scrollbar-buttonup-up.png)%);
        }
        /*pressed*/
        QScrollBar::up-arrow:vertical::pressed  {
          image:  url(%IMG(scrollbar-buttonup-down.png)%);
        }
        QScrollBar::down-arrow:vertical::pressed  {
          image:  url(%IMG(scrollbar-buttondown-down.png)%);
        }
        QScrollBar::down-arrow:vertical {
          image:  url(%IMG(scrollbar-buttondown-up.png)%);
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
          background: none;
        }
      )CSS");
    for (const char* name : {"scrollbar-buttondown-down.png", "scrollbar-buttondown-up.png",
                             "scrollbar-buttonup-down.png", "scrollbar-buttonup-up.png",
                             "scrollbar-handle-vertical.png", "scrollbar-texture-vertical.png"}) {
        const QString file = QString::fromLatin1(name);
        style.replace(QStringLiteral("%IMG(") + file + QStringLiteral(")%"), image(file));
    }
    return style;
}

const char* kSmallLabelStyle = R"(
          QLabel {
            font-family: "Verdana";
            font-size: 11px;
            font-weight: bold;
            color: #909090;
          }
        )";

} // namespace

TLauncherMainWindow::TLauncherMainWindow(const std::vector<std::shared_ptr<TPackage>>& packages,
                                         TInstallationManager* installationManager, bool showPackageNames,
                                         std::shared_ptr<QNetworkAccessManager> networkAccessManager,
                                         bool eulaWasAccepted, std::shared_ptr<host::TDownloadPool> downloadPool,
                                         std::shared_ptr<TCloudflareCdnTrace> cdnTrace)
    : m_downloadPool(std::move(downloadPool))
    , m_networkAccessManager(std::move(networkAccessManager))
    , m_cdnTrace(std::move(cdnTrace))
{
    setWindowTitle(LauncherBrand::instance().productName);
    setWindowFlags(Qt::WindowFlags(0xa005100));
    setFixedWidth(800);
    setMaximumHeight(600);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QScreen* screen = this->screen() ? this->screen() : QGuiApplication::primaryScreen();
    const QRect available = screen->geometry();
    move(available.x() + (available.width() - width()) / 2, available.y() + (available.height() - height()) / 2);
    setStyleSheet(QStringLiteral(R"(
        QMainWindow {
          border-image: url(%1) 0 0 450 0;
          border-top: 0px transparent;
          border-left: 0px transparent;
          border-bottom: 450px transparent;
          border-right: 0px transparent;
        }
      )")
                      .arg(image(QStringLiteral("BackgroundImage.jpg"))));

    auto* central = new QWidget();
    setCentralWidget(central);
    auto* packagesRow = new QHBoxLayout();
    auto* grid = new QGridLayout();
    packagesRow->addStretch();
    packagesRow->addLayout(grid);
    packagesRow->addStretch();
    int column = 0;
    int row = 0;
    for (const std::shared_ptr<TPackage>& package : packages) {
        auto* item = new TClientPackageLayoutItem(package, showPackageNames);
        m_items[package->type()] = item;
        item->setDefaultChecked(package->isDefault());
        grid->addItem(item, row, column);
        connect(item, &TClientPackageLayoutItem::defaultChanged, this,
                &TLauncherMainWindow::onDefaultClientPackageCheckboxChanged);
        connect(item, &TClientPackageLayoutItem::buttonClicked, this,
                &TLauncherMainWindow::clientPackageButtonClicked);
        // Four tiles per row.
        if (column > 2) {
            column = 0;
            ++row;
        } else {
            ++column;
        }
    }

    auto* footer = new QHBoxLayout();
    footer->addWidget(createVersionLabel());
    footer->addStretch();
    footer->addWidget(createAboutLabel());

    auto* eulaPage = new QVBoxLayout();
    eulaPage->addSpacing(11);
    eulaPage->addWidget(createEulaText());
    eulaPage->addWidget(createEulaAcceptButton());
    auto* mainPage = new QVBoxLayout();
    mainPage->addSpacing(11);
    mainPage->addLayout(packagesRow);
    mainPage->addSpacing(6);
    mainPage->addWidget(createHintBox(), 0, Qt::AlignHCenter);
    auto* eulaWidget = new QWidget();
    eulaWidget->setLayout(eulaPage);
    auto* mainWidget = new QWidget();
    mainWidget->setLayout(mainPage);
    m_pages = new QStackedLayout();
    m_pages->addWidget(eulaWidget);
    m_pages->addWidget(mainWidget);
    m_pages->setCurrentIndex(eulaWasAccepted ? 1 : 0);

    auto* layout = new QVBoxLayout();
    layout->addLayout(m_pages);
    layout->addSpacing(6);
    layout->addWidget(createCipSoftLogo(), 0, Qt::AlignHCenter);
    layout->addSpacing(-15);
    layout->addLayout(footer);
    central->setLayout(layout);

    connect(installationManager, &TInstallationManager::installationOrUpdateStarted, this,
            &TLauncherMainWindow::onInstallationOrUpdateStarted);
    connect(installationManager, &TInstallationManager::installationOrUpdateFinished, this,
            &TLauncherMainWindow::onInstallationOrUpdateFinished);
    const std::shared_ptr<TPackage> defaultPackage = installationManager->defaultPackage();
    m_defaultPackageType = defaultPackage ? defaultPackage->type() : -1;
}

TLauncherMainWindow::~TLauncherMainWindow() = default;

QWidget* TLauncherMainWindow::createVersionLabel()
{
    auto* label = new QLabel(launcherVersion(), this);
    label->setStyleSheet(QString::fromUtf8(kSmallLabelStyle));
    return label;
}

QWidget* TLauncherMainWindow::createAboutLabel()
{
    auto* label = new QLabel(
        QStringLiteral("<a href=\"about\" style=\"color: #909090; text-decoration: none;\">About</a>"), this);
    label->setStyleSheet(QString::fromUtf8(kSmallLabelStyle));
    connect(label, &QLabel::linkActivated, this, &TLauncherMainWindow::onAboutDialogRequested);
    return label;
}

QWidget* TLauncherMainWindow::createEulaText()
{
    auto* text = new QTextEdit();
    text->setReadOnly(true);
    text->setWordWrapMode(QTextOption::WordWrap);
    text->setFixedHeight(330);
    text->setStyleSheet(QStringLiteral(R"CSS(
        QTextEdit {
          background-color: transparent;
          color: #dfdfdf;
          font-family: "Verdana";
          font-size: 11px;
          font-weight: bold;
          border: 1px solid #909090;
        }
)CSS") + scrollBarStyle());
    QFile eula(QStringLiteral(":/documents/tibia-eula.md"));
    eula.open(QIODevice::ReadOnly);
    text->setMarkdown(QString::fromUtf8(eula.readAll()));
    return text;
}

QWidget* TLauncherMainWindow::createEulaAcceptButton()
{
    auto* button = new QPushButton(QStringLiteral("Accept"));
    connect(button, &QPushButton::clicked, this, &TLauncherMainWindow::onEulaAcceptClicked);
    return button;
}

QWidget* TLauncherMainWindow::createHintBox()
{
    m_hintBrowser = new shared::QTextBrowserWithNetwork(this);
    m_hintBrowser->setOpenExternalLinks(true);
    m_hintBrowser->setContextMenuPolicy(Qt::NoContextMenu);
    m_hintBrowser->setFixedWidth(542);
    m_hintBrowser->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_hintBrowser->setMaximumHeight(250);
    m_hintBrowser->viewport()->setAutoFillBackground(false);
    m_hintBrowser->document()->setDefaultStyleSheet(QStringLiteral(R"(
        a {
          text-decoration:unterline;
          color: #c0c0c0;
        }
      )"));
    m_hintBrowser->setStyleSheet(QStringLiteral(R"CSS(
        QTextBrowser {
          font-family: "Verdana";
          font-size: 11px;
          font-weight: bold;
          color: #c0c0c0;
          border-style: none;
        }
)CSS") + scrollBarStyle());
    m_hintBox = new QGroupBox(QStringLiteral("Hint"));
    m_hintBox->setAlignment(Qt::AlignHCenter);
    m_hintBox->setStyleSheet(QStringLiteral(R"(
        QGroupBox {
          border-image: url(%1) 17 4 4 4 repeat;
          border-top: 17px transparent;
          border-left: 4px transparent;
          border-bottom: 4px transparent;
          border-right: 4px transparent;
          background-attachment: fixed;
          font-family: "Verdana";
          font-size: 11px;
          font-weight: bold;
        }
        QGroupBox::title {
          color: #909090;
          margin-top: 2px transparent;
          background-color: transparent;
        }
      )")
                                 .arg(image(QStringLiteral("hint-background.png"))));
    auto* layout = new QStackedLayout();
    layout->addWidget(m_hintBrowser);
    m_hintBox->setLayout(layout);
    return m_hintBox;
}

QWidget* TLauncherMainWindow::createCipSoftLogo()
{
    auto* label = new QLabel(this);
    label->setText(QStringLiteral("<a href=\"%1\"><img src=\"%2\"></a>")
                       .arg(LauncherBrand::instance().productUrl, image(QStringLiteral("CipSoftLogo.png"))));
    label->setTextFormat(Qt::RichText);
    label->setOpenExternalLinks(true);
    return label;
}

TClientPackageLayoutItem* TLauncherMainWindow::itemOf(const TPackage& package) const
{
    return m_items.at(package.type());
}

void TLauncherMainWindow::setHint(THint hint)
{
    if (!m_hintBrowser)
        return;
    const bool visible = !hint.richText.trimmed().isEmpty();
    m_hintBrowser->setVisible(visible);
    m_hintBox->setVisible(visible);
    m_hintBrowser->setHtml(hint.richText);
    m_hintBox->setTitle(hint.title.isEmpty() ? QStringLiteral("Hint") : hint.title);
    adjustSize();
}

void TLauncherMainWindow::onDefaultClientPackageCheckboxChanged(std::shared_ptr<TPackage> package, bool isDefault)
{
    const int type = package->type();
    int newDefault = -1;
    if (!isDefault) {
        if (m_defaultPackageType != type)
            return;
    } else {
        if (m_defaultPackageType == type)
            return;
        newDefault = type;
    }
    m_defaultPackageType = newDefault;
    for (const auto& [itemType, item] : m_items)
        item->setDefaultChecked(itemType == type && isDefault);
    Q_EMIT defaultChanged(package, isDefault);
}

void TLauncherMainWindow::setDefault(std::shared_ptr<TPackage> package, bool isDefault)
{
    m_defaultPackageType = isDefault ? package->type() : -1;
    for (const auto& [itemType, item] : m_items)
        item->setDefaultChecked(itemType == package->type() && isDefault);
}

void TLauncherMainWindow::onInstallationOrUpdateStarted(TPackageInstallation* installationInProgress)
{
    itemOf(*installationInProgress->package())->onInstallationOrUpdateStarted(installationInProgress);
}

void TLauncherMainWindow::onInstallationOrUpdateFinished(TPackageInstallation* installationInProgress)
{
    itemOf(*installationInProgress->package())->onInstallationOrUpdateFinished(installationInProgress);
    const QString name = installationInProgress->package()->name();
    if (installationInProgress->errorReason() == DownloadFailed) {
        showDownloadFailure(name);
    } else if (installationInProgress->errorReason() == DiskAccessFailed) {
        QMessageBox::critical(this, QStringLiteral("Installation failed"),
                              QStringLiteral("The installation of %1 failed.\n\nMake sure you have enough free disk "
                                             "space and the application isn't currently running, then try again.")
                                  .arg(name));
    }
    qDebug() << "Installation of package" << name << "finished.";
}

void TLauncherMainWindow::showDownloadFailure(const QString& packageName)
{
    if (!m_downloadFailureBox)
        m_downloadFailureBox = new QMessageBox(this);
    QString diagnostics;
    if (!m_cdnTrace->received() && !m_cdnTrace->failed()) {
        m_cdnTrace->request();
        connect(
            m_cdnTrace.get(), &TCloudflareCdnTrace::diagnosticsTraceReceived, this,
            [this, packageName]() {
                // Refresh the open dialog once the trace is in.
                if (m_downloadFailureBox->isVisible())
                    showDownloadFailure(packageName);
            },
            Qt::SingleShotConnection);
        diagnostics = QStringLiteral("Loading, please wait...");
    }
    if (m_cdnTrace->received())
        diagnostics = formatCdnTrace(m_cdnTrace->trace());
    else if (m_cdnTrace->failed())
        diagnostics = QStringLiteral("Failed to obtain information.");
    m_downloadFailureBox->setWindowTitle(QStringLiteral("Download failed"));
    m_downloadFailureBox->setText(
        QStringLiteral("The installation of %1 failed.\n\nNot all files necessary could be downloaded. Please try "
                       "again later.\n\nDiagnostic information: %2")
            .arg(packageName, diagnostics));
    m_downloadFailureBox->setIcon(QMessageBox::Critical);
    m_downloadFailureBox->show();
}

void TLauncherMainWindow::onEulaAcceptClicked()
{
    m_pages->setCurrentIndex(1);
    Q_EMIT eulaAccepted();
}

void TLauncherMainWindow::onAboutDialogRequested(const QString&)
{
    QMessageBox about(this);
    about.setWindowTitle(QStringLiteral("About ") + LauncherBrand::instance().productName);
    about.setIcon(QMessageBox::Information);
    about.setTextFormat(Qt::RichText);
    about.setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    about.setText(QStringLiteral(R"(<p>Copyright (C) %1-%2. All rights reserved.</p>
        <p>This software uses Qt 6. The Qt 6 library is licensed under the GNU Lesser General Public License,
       version 3. You can obtain a copy at <a href="https://www.gnu.org/licenses/lgpl-3.0.en.html">
        https://www.gnu.org/licenses/lgpl-3.0.en.html</a>.</p>
       <p>Source code, including modifications, at <a href="http://qt.tibia.com">http://qt.tibia.com</a>.</p>)")
                      .arg(QString::number(2002), QString::number(2026)));
    about.exec();
}

void TLauncherMainWindow::mousePressEvent(QMouseEvent* event)
{
    m_dragPosition = event->globalPosition();
}

void TLauncherMainWindow::mouseReleaseEvent(QMouseEvent*)
{
    m_dragPosition = QPointF();
}

void TLauncherMainWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (qAbs(m_dragPosition.x()) > 1e-12 || qAbs(m_dragPosition.y()) > 1e-12) {
        const QPointF delta = event->globalPosition() - m_dragPosition;
        move((QPointF(pos()) + delta).toPoint());
    }
    m_dragPosition = event->globalPosition();
}

} // namespace launcher
