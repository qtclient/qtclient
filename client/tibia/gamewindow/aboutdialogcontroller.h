#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QUrl>

namespace tibia {
namespace gamewindow {

class TEditTextController;

/// AboutDialog.qml (CIP: TAboutDialogController, vtable ,
/// metacall), owned by the options dialog (CIP +0x3a8).
class TAboutDialogController final : public TDialogController<cip::tibia::gamewindow::TAboutDialogController>
{
    Q_OBJECT
    /// Host shown on the Website button (portal origin; CIP hardcodes www.tibia.com).
    Q_PROPERTY(QString websiteHost READ websiteHost CONSTANT)
public:
    explicit TAboutDialogController(QObject* parent = nullptr);

    /// Product: QCoreApplication::applicationName (CIP : regular client).
    QString clientType() const override;
    /// CIP .
    QString operatingSystemName() const override;
    /// CIP : client version, then revision when set.
    QString tibiaVersion() const override;
    /// CIP : "%1-%2" of 2002 and 2026.
    QString copyrightPeriod() const override;
    QString websiteHost() const;

    /// Portal origin for OPEN_URL_TIBIA_PAGE (LauncherEnv::loginBase).
    void setPortalBase(const QUrl& loginBase);

    /// CIP .
    void showDialog() override;
    /// CIP .
    Q_INVOKABLE void requestClose() override;
    /// CIP : OPEN_URL_TIBIA_PAGE (portal).
    Q_INVOKABLE void requestOpenWebsite() override;
    /// CIP : the licences as a read-only text captioned "aboutdialog_licenses".
    Q_INVOKABLE void requestShowLicensesDialog() override;

private:
    TEditTextController* m_licensesText;
    QUrl m_portalBase;
};

}
}
