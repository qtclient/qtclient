#pragma once

#include "gamewindow/createcharacterdialogcontroller.h"
#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QJsonObject>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;

namespace tibia::gamewindow {

class TMessageDialogController;

/// CreateAccountDialog.qml's controller (CIP: TCreateAccountDialogController).
/// Posts account + first character creation to the portal's create-account API.
class TCreateAccountDialogController : public TDialogController<cip::tibia::gamewindow::TCreateAccountDialogController>
{
    Q_OBJECT
public:
    explicit TCreateAccountDialogController(QObject* parent = nullptr);
    ~TCreateAccountDialogController() override;

    void setServiceUrl(const QUrl& url) { m_serviceUrl = url; }
    void setTooltipContainer(QObject* container) { m_tooltipContainer = container; }

    void showDialog() override;
    void setTermsAndServicesAccepted(bool Accepted) override;
    void setPersonalizedAdsAccepted(bool Accepted) override;
    void setCharacterSexMale(bool Male) override;
    Q_INVOKABLE void cancel() override;
    Q_INVOKABLE void updateAccountEmail(const QString& Email) override;
    Q_INVOKABLE void updateAccountPasswords(const QString& Password01, const QString& Password02) override;
    Q_INVOKABLE void suggestCharacterName() override;
    Q_INVOKABLE void updateCharacterName(const QString& Name) override;
    Q_INVOKABLE void updateWorldRegionFilter(int SelectedIndex) override;
    Q_INVOKABLE void updateWorldPvPTypeFilter(int SelectedIndex) override;
    Q_INVOKABLE void updateSelectedWorld(const QString& SelectedWorld) override;
    Q_INVOKABLE void resetWorldSelection() override;
    Q_INVOKABLE void startPlaying(const QString& Recaptcha2Token, const QString& Recaptcha3Token) override;

private:
    using ReplyHandler = std::function<void(const QJsonObject&)>;

    void post(const QJsonObject& request, ReplyHandler onReply);
    void setPending(int pending);
    void setStartPlayingPending(bool pending);
    void showError(const QString& message);
    void recomputeValidity();
    void applyPasswordRequirements(const QJsonObject& requirements, bool valid, const QString& strength, const QString& color);
    void onStatus(const QJsonObject& reply);
    void refreshSelectedWorldInfo();

    QNetworkAccessManager* m_network;
    TWorldSelectionModel* m_worlds;
    TWorldSelectionSortProxyModel* m_proxy;
    TMessageDialogController* m_error;
    QUrl m_serviceUrl;
    QString m_generatedName;
    int m_pending = 0;
};

}
