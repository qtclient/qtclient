#include "aboutdialogcontroller.h"

#include "app/launcherenv.h"
#include "client/gameclient.h"
#include "gamewindow/edittextcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <QCoreApplication>
#include <QSysInfo>
#include <QtGlobal>

namespace tibia::gamewindow {

TAboutDialogController::TAboutDialogController(QObject* parent)
    : TDialogController(QStringLiteral("AboutDialog.qml"), parent)
    // CipSoft makes it with the about dialog's engine and dialog parent item; both
    // are the dialog stack's here (TDialog's fallback for null).
    , m_licensesText(new TEditTextController(nullptr, nullptr, this))
{
}

QString TAboutDialogController::clientType() const
{
    return QCoreApplication::applicationName();
}

QString TAboutDialogController::operatingSystemName() const
{
    const QString product = QSysInfo::productType();
    if (product == QLatin1String("windows"))
        return qtTrId("aboutdialog_windows");
    if (product == QLatin1String("macos"))
        return qtTrId("aboutdialog_macosx");
    if (QSysInfo::kernelType() == QLatin1String("linux"))
        return qtTrId("aboutdialog_linux");
    return QString();
}

QString TAboutDialogController::tibiaVersion() const
{
    constexpr int version = client::TGameClient::ClientVersion;
    QString text = QStringLiteral("%1.%2").arg(version / 100).arg(version % 100, 2, 10, QLatin1Char('0'));
    const QString revision = app::LauncherEnv::load(QString()).revision;
    if (!revision.isEmpty())
        text += QLatin1Char('.') + revision;
    return text;
}

QString TAboutDialogController::copyrightPeriod() const
{
    return QStringLiteral("%1-%2").arg(2002).arg(2026);
}

QString TAboutDialogController::websiteHost() const
{
    if (!m_portalBase.isEmpty() && !m_portalBase.host().isEmpty())
        return m_portalBase.host() + (m_portalBase.port() > 0 ? QStringLiteral(":%1").arg(m_portalBase.port()) : QString());
    return QCoreApplication::applicationName();
}

void TAboutDialogController::setPortalBase(const QUrl& loginBase)
{
    m_portalBase = loginBase;
}

void TAboutDialogController::showDialog()
{
    dialog()->close();
    dialog()->show();
}

void TAboutDialogController::requestClose()
{
    dialog()->close();
}

void TAboutDialogController::requestOpenWebsite()
{
    Q_EMIT publishGameAction(input::makeGameAction(input::EGameAction::OPEN_URL_TIBIA_PAGE));
}

void TAboutDialogController::requestShowLicensesDialog()
{
    // CIP concatenates the bundled third-party licence files; the product
    // lists the libraries it links.
    m_licensesText->showReadOnlyText(qtTrId("aboutdialog_licenses"),
                                     QStringLiteral("Qt %1 (LGPLv3), zlib, miniaudio (public domain), protobuf (BSD)")
                                         .arg(QLatin1StringView(qVersion())));
}

}
