#include "createaccountdialogcontroller.h"

#include "gamewindow/messagedialogcontroller.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSysInfo>

namespace tibia::gamewindow {

namespace {

QJsonObject replyObject(QNetworkReply* reply)
{
    return QJsonDocument::fromJson(QString::fromUtf8(reply->readAll()).toUtf8()).object();
}

bool httpOk(QNetworkReply* reply)
{
    return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
}

bool succeeded(const QJsonObject& reply) { return reply.value(QLatin1String("errorCode")).toInt() == 0; }

/// "errorMessage", a string or a list joined by ", ".
QString errorMessage(const QJsonObject& reply)
{
    QString message = QString::fromUtf8("");
    const QJsonValue value = reply.value(QLatin1String("errorMessage"));
    if (value.isArray()) {
        const QJsonArray parts = value.toArray();
        if (!parts.isEmpty()) {
            message.append(parts[0].toString());
            for (qsizetype i = 1; i < parts.size(); ++i) {
                message.append(QStringLiteral(", "));
                message.append(parts[i].toString());
            }
        }
    }
    if (value.isString())
        message = value.toString();
    return message;
}

/// The help text of a PvP type; empty for an unknown one.
QString pvpTypeHelp(const QString& pvpType)
{
    if (pvpType == QLatin1String("Optional PvP"))
        return qtTrId("create_account_dialog_optional_pvp_help");
    if (pvpType == QLatin1String("Open PvP"))
        return qtTrId("create_account_dialog_open_pvp_help");
    if (pvpType == QLatin1String("Hardcore PvP"))
        return qtTrId("create_account_dialog_hardcore_pvp_help");
    if (pvpType == QLatin1String("Retro Open PvP"))
        return qtTrId("create_account_dialog_retro_open_pvp_help");
    if (pvpType == QLatin1String("Retro Hardcore PvP"))
        return qtTrId("create_account_dialog_retro_hardcore_pvp_help");
    return QString::fromUtf8("");
}

/// One entry of passwordCriteria.
QJsonObject passwordCriterion(const QString& key, const QJsonObject& requirements)
{
    return { { QStringLiteral("criterionKey"), key }, { QStringLiteral("criterionMet"), requirements[key].toBool() } };
}

}

TCreateAccountDialogController::TCreateAccountDialogController(QObject* parent)
    : TDialogController(QStringLiteral("createaccountandcharacter/CreateAccountDialog.qml"), parent)
    , m_network(new QNetworkAccessManager(this))
    , m_proxy(new TWorldSelectionSortProxyModel(this))
    , m_error(new TMessageDialogController(this))
{
    m_worldsModel = m_proxy;
    m_showRecaptchaV2 = false;
    m_recaptchaDisabled = false;
    m_passwordStrengthColor = QColor::fromString(QLatin1String("black"));
    m_characterSexMale = true;
    m_error->setCloseOnButtonClicked(true);
    m_error->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK, QKeySequence(QStringLiteral("Return"))) });
}

TCreateAccountDialogController::~TCreateAccountDialogController() = default;

void TCreateAccountDialogController::showDialog()
{
    dialog()->close();
    requestAccountCreationStatus();
}

bool TCreateAccountDialogController::eventFilter(QObject* watched, QEvent* event)
{
    // Host: CipSoft refreshes capsLock from the OS key state here; qtclient has
    // no caps lock query yet.
    return cip::tibia::gamewindow::TCreateAccountDialogController::eventFilter(watched, event);
}

bool TCreateAccountDialogController::accountDataValid() const
{
    return m_eMailDataValid && m_passwordValid && m_accountPassword == m_accountPasswordRepeat && m_termsAndServicesAccepted;
}

bool TCreateAccountDialogController::allDataValid() const { return accountDataValid() && m_characterNameValid && !m_selectedWorld.isEmpty(); }

bool TCreateAccountDialogController::characterDataValid() const { return m_characterNameValid && !m_selectedWorld.isEmpty(); }

QVariantMap TCreateAccountDialogController::worldPvPTypesForRegion() const
{
    QVariantMap map;
    for (auto it = m_pvpTypesByRegion.cbegin(); it != m_pvpTypesByRegion.cend(); ++it)
        map.insert(it.key(), QVariant::fromValue(it.value()));
    return map;
}

void TCreateAccountDialogController::setTermsAndServicesAccepted(const bool Accepted)
{
    m_termsAndServicesAccepted = Accepted;
    Q_EMIT accountCreationDataChanged();
}

void TCreateAccountDialogController::setPersonalizedAdsAccepted(const bool Accepted)
{
    m_personalizedAdsAccepted = Accepted;
    Q_EMIT accountCreationDataChanged();
}

void TCreateAccountDialogController::cancel()
{
    m_accountEmail = QString::fromUtf8("");
    m_eMailDataValid = false;
    m_accountPassword = QString::fromUtf8("");
    m_passwordValid = false;
    m_accountPasswordRepeat = QString::fromUtf8("");
    m_passwordStrength = QString::fromUtf8("");
    m_termsAndServicesAccepted = false;
    m_personalizedAdsAccepted = false;
    m_characterSexMale = true;
    m_showRecaptchaV2 = false;
    m_proxy->setWorlds({});
    m_asyncStartPlayingInProgress = false;
    dialog()->close();
}

void TCreateAccountDialogController::requestAccountCreationStatus()
{
    post({ { QStringLiteral("type"), QStringLiteral("getaccountcreationstatus") } },
         [this](QNetworkReply* reply) { onAccountCreationStatus(reply); });
}

void TCreateAccountDialogController::onAccountCreationStatus(QNetworkReply* reply)
{
    if (!httpOk(reply)) {
        showError(qtTrId("create_account_dialog_error_account_creation_disabled"));
        return;
    }
    const QJsonObject status = replyObject(reply);
    if (!succeeded(status)) {
        showError(errorMessage(status));
        return;
    }

    // The region and PvP type combo boxes: "all" first, then in the order the
    // worlds name them.
    m_worlds.clear();
    const QString all = QStringLiteral("all");
    m_worldRegions.clear();
    m_worldRegions.append(all);
    m_pvpTypesByRegion.clear();
    m_pvpTypesByRegion[all].append(all);
    m_selectedWorldRegionFilterIndex = 0;
    m_selectedWorldPvPTypeFilterIndex = 0;
    for (const QJsonValue& value : status[QLatin1String("Worlds")].toArray()) {
        WorldData world = parseWorldData(value);
        if (!m_worldRegions.contains(world.region)) {
            m_worldRegions.append(world.region);
            m_pvpTypesByRegion[world.region].append(all);
        }
        if (!m_pvpTypesByRegion[world.region].contains(world.pvpType))
            m_pvpTypesByRegion[world.region].append(world.pvpType);
        if (!m_pvpTypesByRegion[all].contains(world.pvpType))
            m_pvpTypesByRegion[all].append(world.pvpType);
        m_worlds.push_back(std::move(world));
    }
    m_proxy->setWorlds(m_worlds);

    const bool captchaDeactivated = status[QLatin1String("IsCaptchaDeactivated")].toBool();
    if (m_recaptchaDisabled != captchaDeactivated) {
        m_recaptchaDisabled = captchaDeactivated;
        Q_EMIT recaptchaDataChanged();
    }
    m_recommendedWorld = status[QLatin1String("RecommendedWorld")].toString();
    selectWorld(m_recommendedWorld);
    suggestCharacterName();
    dialog()->show();
}

void TCreateAccountDialogController::updateAccountEmail(const QString& Email)
{
    if (Email.isEmpty()) {
        m_eMailDataValid = false;
        Q_EMIT accountCreationDataChanged();
        return;
    }
    m_accountEmail = Email;
    post({ { QStringLiteral("type"), QStringLiteral("CheckEMail") }, { QStringLiteral("Email"), Email } },
         [this](QNetworkReply* reply) { onEMailChecked(reply); });
}

void TCreateAccountDialogController::onEMailChecked(QNetworkReply* reply)
{
    if (!httpOk(reply)) {
        showError(reply->errorString());
        return;
    }
    const QJsonObject answer = replyObject(reply);
    if (!succeeded(answer)) {
        m_eMailDataValid = false;
        Q_EMIT accountCreationDataChanged();
        return;
    }
    // Valid only for the address the player still has in the form.
    const bool valid = answer[QLatin1String("IsValid")].toBool();
    m_eMailDataValid = valid && answer[QLatin1String("EMail")].toString() == m_accountEmail;
    Q_EMIT accountCreationDataChanged();
}

void TCreateAccountDialogController::updateAccountPasswords(const QString& Password01, const QString& Password02)
{
    if (Password01.isEmpty()) {
        m_passwordValid = false;
        m_passwordStrength = QString::fromUtf8("");
        Q_EMIT accountCreationDataChanged();
    } else {
        post({ { QStringLiteral("Type"), QStringLiteral("checkpassword") }, { QStringLiteral("Password1"), Password01 } },
             [this](QNetworkReply* reply) { onPasswordChecked(reply); });
    }
    m_accountPassword = Password01;
    m_accountPasswordRepeat = Password02;
}

void TCreateAccountDialogController::onPasswordChecked(QNetworkReply* reply)
{
    if (!httpOk(reply)) {
        showError(reply->errorString());
        return;
    }
    const QJsonObject answer = replyObject(reply);
    if (!succeeded(answer)) {
        showError(errorMessage(answer));
        return;
    }
    // An answer for a password the player already changed is dropped.
    if (answer[QLatin1String("Password1")].toString() != m_accountPassword)
        return;
    m_passwordValid = answer[QLatin1String("PasswordValid")].toBool();
    const int strength = answer[QLatin1String("PasswordStrength")].toInt();
    m_passwordStrength = qtTrId(QStringLiteral("create_account_dialog_password_strength_%1").arg(strength).toStdString().c_str());
    m_passwordStrengthColor = QColor::fromString(answer[QLatin1String("PasswordStrengthColor")].toString());
    const QJsonObject requirements = answer[QLatin1String("PasswordRequirements")].toObject();
    m_passwordCriteria = QJsonArray();
    m_passwordCriteria.append(passwordCriterion(QStringLiteral("PasswordLength"), requirements));
    m_passwordCriteria.append(passwordCriterion(QStringLiteral("InvalidCharacters"), requirements));
    m_passwordCriteria.append(passwordCriterion(QStringLiteral("HasLowerCase"), requirements));
    m_passwordCriteria.append(passwordCriterion(QStringLiteral("HasUpperCase"), requirements));
    m_passwordCriteria.append(passwordCriterion(QStringLiteral("HasNumber"), requirements));
    Q_EMIT accountCreationDataChanged();
}

void TCreateAccountDialogController::suggestCharacterName()
{
    post({ { QStringLiteral("Type"), QStringLiteral("GenerateCharacterName") } }, [this](QNetworkReply* reply) { onCharacterNameGenerated(reply); });
}

void TCreateAccountDialogController::onCharacterNameGenerated(QNetworkReply* reply)
{
    if (!httpOk(reply)) {
        showError(reply->errorString());
        return;
    }
    const QJsonObject answer = replyObject(reply);
    if (!succeeded(answer)) {
        showError(errorMessage(answer));
        return;
    }
    m_characterName = answer[QLatin1String("GeneratedName")].toString();
    Q_EMIT characterNameChanged();
    if (answer.contains(QLatin1String("Error")))
        m_characterNameError = answer[QLatin1String("Error")].toString();
    else
        m_characterNameError = QString::fromUtf8("");
    m_characterNameValid = true;
    Q_EMIT accountCreationDataChanged();
}

void TCreateAccountDialogController::updateCharacterName(const QString& Name)
{
    m_characterName = Name;
    if (!m_characterName.isEmpty()) {
        post({ { QStringLiteral("Type"), QStringLiteral("CheckCharacterName") }, { QStringLiteral("CharacterName"), m_characterName } },
             [this](QNetworkReply* reply) { onCharacterNameChecked(reply); });
        return;
    }
    m_characterNameError = QString::fromUtf8("");
    m_characterNameValid = false;
    Q_EMIT accountCreationDataChanged();
}

void TCreateAccountDialogController::onCharacterNameChecked(QNetworkReply* reply)
{
    if (!httpOk(reply)) {
        showError(reply->errorString());
        return;
    }
    const QJsonObject answer = replyObject(reply);
    // An answer for a name the player already changed only reports errors.
    if (answer[QLatin1String("CharacterName")].toString() == m_characterName) {
        m_characterNameValid = answer[QLatin1String("IsAvailable")].toBool();
        m_characterNameError = errorMessage(answer);
        Q_EMIT accountCreationDataChanged();
    } else if (!succeeded(answer)) {
        showError(errorMessage(answer));
    }
}

void TCreateAccountDialogController::updateWorldRegionFilter(const int SelectedIndex)
{
    if (SelectedIndex < 0 || SelectedIndex >= m_worldRegions.size() || SelectedIndex == m_selectedWorldRegionFilterIndex)
        return;
    m_selectedWorldRegionFilterIndex = SelectedIndex;
    const QString region = m_worldRegions[SelectedIndex];
    if (region == QLatin1String("all")) {
        m_proxy->setWorldRegionFilter(QString::fromUtf8(""));
    } else {
        m_proxy->setWorldRegionFilter(region);
        // A selected world of another region is dropped.
        const WorldData* selected = findWorld(m_selectedWorld);
        if (selected && selected->region != region)
            m_selectedWorld = QString::fromUtf8("");
    }
    Q_EMIT accountCreationDataChanged();
}

void TCreateAccountDialogController::updateWorldPvPTypeFilter(const int SelectedIndex)
{
    const int regionIndex = m_selectedWorldRegionFilterIndex;
    if (regionIndex >= 0 && regionIndex < m_worldRegions.size()) {
        const QStringList& pvpTypes = m_pvpTypesByRegion[m_worldRegions[regionIndex]];
        if (SelectedIndex >= 0 && SelectedIndex < pvpTypes.size()) {
            if (m_selectedWorldPvPTypeFilterIndex == SelectedIndex)
                return;
            m_selectedWorldPvPTypeFilterIndex = SelectedIndex;
            const QString pvpType = pvpTypes[SelectedIndex];
            if (pvpType == QLatin1String("all")) {
                m_proxy->setWorldPvPTypeFilter(QString::fromUtf8(""));
            } else {
                m_proxy->setWorldPvPTypeFilter(pvpType);
                // A selected world of another PvP type is dropped.
                const WorldData* selected = findWorld(m_selectedWorld);
                if (selected && selected->pvpType != pvpType)
                    m_selectedWorld = QString::fromUtf8("");
            }
        }
    }
    Q_EMIT accountCreationDataChanged();
}

void TCreateAccountDialogController::updateSelectedWorld(const QString& SelectedWorld)
{
    if (m_selectedWorld == SelectedWorld)
        return;
    applySelectedWorld(SelectedWorld);
    Q_EMIT accountCreationDataChanged();
}

void TCreateAccountDialogController::resetWorldSelection() { selectWorld(m_recommendedWorld); }

void TCreateAccountDialogController::selectWorld(const QString& name)
{
    const WorldData* world = findWorld(name);
    if (!world)
        return;
    m_proxy->setWorldRegionFilter(world->region);
    m_selectedWorldRegionFilterIndex = int(m_worldRegions.indexOf(world->region));
    m_proxy->setWorldPvPTypeFilter(world->pvpType);
    m_selectedWorldPvPTypeFilterIndex = int(m_pvpTypesByRegion[m_worldRegions.value(m_selectedWorldRegionFilterIndex)].indexOf(world->pvpType));
    applySelectedWorld(name);
    Q_EMIT accountCreationDataChanged();
}

void TCreateAccountDialogController::applySelectedWorld(const QString& name)
{
    const WorldData* world = findWorld(name);
    if (!world) {
        m_selectedWorld = QString::fromUtf8("");
        return;
    }
    m_selectedWorld = name;
    m_selectedWorldPlayersOnline = world->playersOnline;
    m_selectedWorldPvPType = world->pvpType;
    m_selectedWorldPvPTypeHelp = pvpTypeHelp(world->pvpType);
    m_selectedWorldRegion = world->region;
    m_selectedWorldCreationDate = world->creationDate;
    m_selectedWorldBattleEyeStatus = world->battleEyeStatus;
    m_selectedWorldPremiumOnly = world->premiumOnly;
    m_selectedWorldTransferType = world->transferType;
}

const WorldData* TCreateAccountDialogController::findWorld(const QString& name) const
{
    for (const WorldData& world : m_worlds) {
        if (world.name == name)
            return &world;
    }
    return nullptr;
}

void TCreateAccountDialogController::startPlaying(const QString& Recaptcha2Token, const QString& Recaptcha3Token)
{
    if (m_pending > 0)
        return;
    qDebug() << "Start playing( V2:" << !Recaptcha2Token.isEmpty() << ", V3:" << !Recaptcha3Token.isEmpty() << " )";
    const QJsonObject request{
        { QStringLiteral("Type"), QStringLiteral("CreateAccountAndCharacter") },
        { QStringLiteral("ReCaptcha2Token"), Recaptcha2Token },
        { QStringLiteral("ReCaptcha3Token"), Recaptcha3Token },
        { QStringLiteral("EMail"), m_accountEmail },
        { QStringLiteral("Password"), m_accountPassword },
        { QStringLiteral("CharacterName"), m_characterName },
        { QStringLiteral("CharacterSex"), m_characterSexMale ? QStringLiteral("male") : QStringLiteral("female") },
        { QStringLiteral("SelectedWorld"), m_selectedWorld },
        { QStringLiteral("ClientType"), QStringLiteral("Client") },
        { QStringLiteral("PersonalizedAdsAccepted"), m_personalizedAdsAccepted },
        // Host: "Epic" for the Epic Games distribution, which qtclient is not.
        { QStringLiteral("DistributionChannel"), QStringLiteral("Regular") },
        { QStringLiteral("OperatingSystem"), QSysInfo::prettyProductName() },
    };
    if (!m_asyncStartPlayingInProgress) {
        m_asyncStartPlayingInProgress = true;
        Q_EMIT asyncStartPlayingStatusChanged();
    }
    post(request, [this](QNetworkReply* reply) { onAccountCreated(reply); });
}

void TCreateAccountDialogController::onAccountCreated(QNetworkReply* reply)
{
    setStartPlayingInProgress(false);
    if (!httpOk(reply)) {
        showError(reply->errorString());
        return;
    }
    const QJsonObject answer = replyObject(reply);
    if (answer[QLatin1String("Success")].toBool()) {
        Q_EMIT accountCreated(m_accountEmail, m_accountPassword);
        cancel();
        return;
    }
    const bool recaptchaV2Requested = answer[QLatin1String("IsRecaptcha2Requested")].toBool();
    if (m_showRecaptchaV2 != recaptchaV2Requested) {
        m_showRecaptchaV2 = recaptchaV2Requested;
        Q_EMIT recaptchaDataChanged();
    }
    if (m_showRecaptchaV2) {
        Q_EMIT accountCreationDataChanged();
        return;
    }
    if (succeeded(answer))
        return;
    showError(errorMessage(answer));
    // Product: the portal's codes differ from CipSoft's (which closes the dialog
    // unless errorCode is 2), so the form always stays for a correction.
}

void TCreateAccountDialogController::setStartPlayingInProgress(const bool inProgress)
{
    if (m_asyncStartPlayingInProgress == inProgress)
        return;
    m_asyncStartPlayingInProgress = inProgress;
    Q_EMIT asyncStartPlayingStatusChanged();
}

void TCreateAccountDialogController::post(const QJsonObject& request, ReplyHandler onReply)
{
    QNetworkRequest httpRequest;
    httpRequest.setUrl(m_serviceUrl);
    httpRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    setPending(m_pending + 1);
    QNetworkReply* reply = m_network->post(httpRequest, QJsonDocument(request).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, onReply = std::move(onReply)] {
        reply->deleteLater();
        setPending(m_pending - 1);
        onReply(reply);
    });
}

void TCreateAccountDialogController::setPending(const int pending)
{
    m_pending = std::max(0, pending);
    m_asyncOperationInProgress = m_pending > 0;
    Q_EMIT asyncOperationStatusChanged();
}

void TCreateAccountDialogController::showError(const QString& message)
{
    m_error->showMessage(qtTrId("error"), message);
}

}
