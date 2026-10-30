#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QUrl>

namespace tibia::gamewindow {

class TMessageDialogController;

/// AboutDialog.qml (CIP: TAboutDialogController, vtable 0x10262c140,
/// metacall 0x100801cf0), owned by the options dialog (CIP +0x3a8).
class TAboutDialogController final : public TDialogController<cip::tibia::gamewindow::TAboutDialogController>
{
    Q_OBJECT
    /// Host shown on the Website button (portal origin; CIP hardcodes www.tibia.com).
    Q_PROPERTY(QString websiteHost READ websiteHost CONSTANT)
public:
    explicit TAboutDialogController(QObject* parent = nullptr);

    /// Product: QCoreApplication::applicationName() (CIP FUN_100c4b210: regular client).
    QString clientType() const override;
    /// CIP FUN_100c4b240.
    QString operatingSystemName() const override;
    /// CIP FUN_100c4b2c0 (FUN_101019610): client version, then revision when set.
    QString tibiaVersion() const override;
    /// CIP FUN_100c4b2f0: "%1-%2" of 2002 and 2026.
    QString copyrightPeriod() const override;
    QString websiteHost() const;

    /// Portal origin for OPEN_URL_TIBIA_PAGE (LauncherEnv::loginBase).
    void setPortalBase(const QUrl& loginBase);

    /// CIP 0x100c4b4d0.
    void showDialog() override;
    /// CIP 0x100c4b030.
    Q_INVOKABLE void requestClose() override;
    /// CIP 0x100c4b040: OPEN_URL_TIBIA_PAGE (portal).
    Q_INVOKABLE void requestOpenWebsite() override;
    /// CIP 0x100c4b120: a message box captioned "aboutdialog_licenses".
    Q_INVOKABLE void requestShowLicensesDialog() override;

private:
    TMessageDialogController* m_licensesMessage;
    QUrl m_portalBase;
};

}
