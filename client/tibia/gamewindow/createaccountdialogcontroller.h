#pragma once

#include "gamewindow/createcharacterdialogcontroller.h"
#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QJsonObject>
#include <QMap>
#include <QStringList>
#include <QUrl>

#include <functional>
#include <memory>
#include <vector>

class QEvent;
class QNetworkAccessManager;
class QNetworkReply;
class QObject;

namespace tibia {
namespace shared {
class TTibiaFileSystemHelper;
}
namespace gamewindow {

class TMessageDialogController;

/// CreateAccountDialog.qml's controller (CIP: TCreateAccountDialogController):
/// checks the e-mail, password and character name with the account service,
/// lets the player pick a world and creates account and first character.
class TCreateAccountDialogController : public TDialogController<cip::tibia::gamewindow::TCreateAccountDialogController>
{
    Q_OBJECT
public:
    explicit TCreateAccountDialogController(QObject* parent = nullptr);
    ~TCreateAccountDialogController() override;

    /// The account service (CIP accountcreationclientservices.php on the portal).
    void setServiceUrl(const QUrl& url) { m_serviceUrl = url; }
    void setTooltipContainer(QObject* container) { m_tooltipContainer = container; }
    /// The file system helper startPlaying reads the distribution marker with.
    void setTibiaFileSystemHelper(std::shared_ptr<shared::TTibiaFileSystemHelper> tibiaFileSystemHelper)
    {
        m_tibiaFileSystemHelper = std::move(tibiaFileSystemHelper);
    }

    /// Asks the account service whether accounts can be created; `onResult`
    /// gets true for an HTTP 200 answer.
    void checkIfAccountCreationEnabled(std::function<void(bool)> onResult);
    /// Closes the dialog and asks for the worlds; the answer shows it.
    void showDialog() override;
    bool eventFilter(QObject* watched, QEvent* event) override;

    /// A valid e-mail and password, both passwords alike, terms accepted.
    bool accountDataValid() const override;
    /// Account and character data valid.
    bool allDataValid() const override;
    /// An available name and a selected world.
    bool characterDataValid() const override;
    QVariantMap worldPvPTypesForRegion() const override;

    void setTermsAndServicesAccepted(bool Accepted) override;
    void setPersonalizedAdsAccepted(bool Accepted) override;
    /// Clears the account form and the worlds and closes the dialog.
    Q_INVOKABLE void cancel() override;
    Q_INVOKABLE void updateAccountEmail(const QString& Email) override;
    Q_INVOKABLE void updateAccountPasswords(const QString& Password01, const QString& Password02) override;
    Q_INVOKABLE void suggestCharacterName() override;
    Q_INVOKABLE void updateCharacterName(const QString& Name) override;
    Q_INVOKABLE void updateWorldRegionFilter(int SelectedIndex) override;
    Q_INVOKABLE void updateWorldPvPTypeFilter(int SelectedIndex) override;
    Q_INVOKABLE void updateSelectedWorld(const QString& SelectedWorld) override;
    /// Back to the recommended world and its filters.
    Q_INVOKABLE void resetWorldSelection() override;
    /// Ignored while a request is pending.
    Q_INVOKABLE void startPlaying(const QString& Recaptcha2Token, const QString& Recaptcha3Token) override;

private:
    using ReplyHandler = std::function<void(QNetworkReply*)>;

    void post(const QJsonObject& request, ReplyHandler onReply);
    void setPending(int pending);
    void setStartPlayingInProgress(bool inProgress);
    void requestAccountCreationStatus();
    void onAccountCreationStatus(QNetworkReply* reply);
    void onEMailChecked(QNetworkReply* reply);
    void onPasswordChecked(QNetworkReply* reply);
    void onCharacterNameGenerated(QNetworkReply* reply);
    void onCharacterNameChecked(QNetworkReply* reply);
    void onAccountCreated(QNetworkReply* reply);
    /// Selects `name` with the filters of its region and PvP type.
    void selectWorld(const QString& name);
    /// `name` the selected world and its details shown; empty if unknown.
    void applySelectedWorld(const QString& name);
    const WorldData* findWorld(const QString& name) const;
    void showError(const QString& message);

    QNetworkAccessManager* m_network;
    TWorldSelectionSortProxyModel* m_proxy;
    TMessageDialogController* m_error;
    QUrl m_serviceUrl;
    std::shared_ptr<shared::TTibiaFileSystemHelper> m_tibiaFileSystemHelper;
    std::vector<WorldData> m_worlds;
    /// Per region of worldRegions: "all" and its PvP types.
    QMap<QString, QStringList> m_pvpTypesByRegion;
    int m_pending = 0;
};

}
}
