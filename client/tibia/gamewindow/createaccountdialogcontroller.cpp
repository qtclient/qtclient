#include "createaccountdialogcontroller.h"

#include "gamewindow/messagedialogcontroller.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace tibia::gamewindow {

namespace {

bool succeeded(const QJsonObject& reply)
{
    return reply.value(QLatin1String("errorCode")).toInt() == 0;
}

QString errorMessage(const QJsonObject& reply)
{
    const QJsonValue value = reply.value(QLatin1String("errorMessage"));
    if (!value.isArray())
        return value.toString();
    QStringList parts;
    for (const QJsonValue& part : value.toArray())
        parts.append(part.toString());
    return parts.join(QStringLiteral(", "));
}

QString strengthLabel(const int strength)
{
    if (strength >= 2)
        return QStringLiteral("Strong");
    if (strength >= 1)
        return QStringLiteral("Medium");
    return QStringLiteral("Weak");
}

CreationWorld parseWorld(const QJsonObject& o)
{
    CreationWorld w;
    w.name = o.value(QLatin1String("Name")).toString();
    w.region = o.value(QLatin1String("Region")).toString();
    w.pvpType = o.value(QLatin1String("PvPType")).toString();
    w.playersOnline = o.value(QLatin1String("PlayersOnline")).toInt();
    w.creationDate = QString::number(o.value(QLatin1String("CreationDate")).toInteger());
    w.battleEyeStatus = o.value(QLatin1String("BattlEyeInitiallyActive")).toInt() == 1
        ? QStringLiteral("Protected since release")
        : QStringLiteral("Unprotected");
    w.premiumOnly = o.value(QLatin1String("PremiumOnly")).toInt() > 0;
    w.transferType = o.value(QLatin1String("TransferType")).toString();
    return w;
}

}

TCreateAccountDialogController::TCreateAccountDialogController(QObject* parent)
    : TDialogController(QStringLiteral("createaccountandcharacter/CreateAccountDialog.qml"), parent)
    , m_network(new QNetworkAccessManager(this))
    , m_worlds(new TWorldSelectionModel(this))
    , m_proxy(new TWorldSelectionSortProxyModel(m_worlds, this))
    , m_error(new TMessageDialogController(this))
{
    m_worldsModel = m_proxy;
    m_recaptchaDisabled = true;
    m_characterSexMale = true;
    m_error->setCloseOnButtonClicked(true);
    m_error->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK, QKeySequence(QStringLiteral("Return"))) });
    applyPasswordRequirements({}, false, {}, QStringLiteral("#EC644B"));
}

TCreateAccountDialogController::~TCreateAccountDialogController() = default;

void TCreateAccountDialogController::showDialog()
{
    if (m_pending > 0 || dialog()->isInStack())
        return;
    post({ { QStringLiteral("type"), QStringLiteral("getaccountcreationstatus") } }, [this](const QJsonObject& reply) { onStatus(reply); });
}

void TCreateAccountDialogController::onStatus(const QJsonObject& reply)
{
    if (!succeeded(reply)) {
        showError(errorMessage(reply));
        return;
    }
    m_recaptchaDisabled = reply.value(QLatin1String("IsCaptchaDeactivated")).toBool(true);
    Q_EMIT recaptchaDataChanged();

    QList<CreationWorld> worlds;
    for (const QJsonValue& w : reply.value(QLatin1String("Worlds")).toArray())
        worlds.append(parseWorld(w.toObject()));
    m_worlds->setWorlds(worlds);
    m_recommendedWorld = reply.value(QLatin1String("RecommendedWorld")).toString();
    m_proxy->reload(m_recommendedWorld);
    m_selectedWorld = m_recommendedWorld;
    m_worldRegions.clear();
    for (const CreationWorld& w : m_worlds->worlds()) {
        if (!w.region.isEmpty() && !m_worldRegions.contains(w.region))
            m_worldRegions.append(w.region);
    }
    m_worldRegions.sort();
    refreshSelectedWorldInfo();
    suggestCharacterName();
    recomputeValidity();
    dialog()->show();
}

void TCreateAccountDialogController::setTermsAndServicesAccepted(const bool Accepted)
{
    if (m_termsAndServicesAccepted == Accepted)
        return;
    m_termsAndServicesAccepted = Accepted;
    recomputeValidity();
}

void TCreateAccountDialogController::setPersonalizedAdsAccepted(const bool Accepted)
{
    if (m_personalizedAdsAccepted == Accepted)
        return;
    m_personalizedAdsAccepted = Accepted;
    Q_EMIT accountCreationDataChanged();
}

void TCreateAccountDialogController::setCharacterSexMale(const bool Male)
{
    if (m_characterSexMale == Male)
        return;
    m_characterSexMale = Male;
    Q_EMIT accountCreationDataChanged();
}

void TCreateAccountDialogController::cancel()
{
    m_accountEmail.clear();
    m_accountPassword.clear();
    m_accountPasswordRepeat.clear();
    m_eMailDataValid = false;
    m_passwordValid = false;
    m_characterName.clear();
    m_characterNameError.clear();
    m_characterNameValid = false;
    m_generatedName.clear();
    m_termsAndServicesAccepted = false;
    m_personalizedAdsAccepted = false;
    m_characterSexMale = true;
    m_selectedWorld.clear();
    m_worlds->setWorlds({});
    applyPasswordRequirements({}, false, {}, QStringLiteral("#EC644B"));
    recomputeValidity();
    if (dialog()->isInStack())
        dialog()->close();
}

void TCreateAccountDialogController::updateAccountEmail(const QString& Email)
{
    m_accountEmail = Email.trimmed();
    if (m_accountEmail.isEmpty()) {
        m_eMailDataValid = false;
        recomputeValidity();
        return;
    }
    post({ { QStringLiteral("type"), QStringLiteral("checkemail") }, { QStringLiteral("Email"), m_accountEmail } }, [this](const QJsonObject& reply) {
        const QString answered = reply.value(QLatin1String("EMail")).toString();
        if (!answered.isEmpty() && answered.compare(m_accountEmail, Qt::CaseInsensitive) != 0)
            return;
        m_eMailDataValid = reply.value(QLatin1String("IsValid")).toBool();
        recomputeValidity();
    });
}

void TCreateAccountDialogController::updateAccountPasswords(const QString& Password01, const QString& Password02)
{
    m_accountPassword = Password01;
    m_accountPasswordRepeat = Password02;
    if (Password01.isEmpty()) {
        applyPasswordRequirements({}, false, {}, QStringLiteral("#EC644B"));
        recomputeValidity();
        return;
    }
    post({ { QStringLiteral("type"), QStringLiteral("checkpassword") }, { QStringLiteral("Password1"), Password01 } }, [this](const QJsonObject& reply) {
        const QJsonObject requirements = reply.value(QLatin1String("PasswordRequirements")).toObject();
        const bool valid = reply.value(QLatin1String("PasswordValid")).toBool();
        const int strength = reply.value(QLatin1String("PasswordStrength")).toInt();
        const QString color = reply.value(QLatin1String("PasswordStrengthColor")).toString(QStringLiteral("#EC644B"));
        applyPasswordRequirements(requirements, valid, strengthLabel(strength), color);
        recomputeValidity();
    });
}

void TCreateAccountDialogController::suggestCharacterName()
{
    post({ { QStringLiteral("Type"), QStringLiteral("GenerateCharacterName") } }, [this](const QJsonObject& reply) {
        if (!succeeded(reply)) {
            showError(errorMessage(reply));
            return;
        }
        m_generatedName = reply.value(QLatin1String("GeneratedName")).toString();
        m_characterName = m_generatedName;
        m_characterNameError.clear();
        m_characterNameValid = !m_generatedName.isEmpty();
        Q_EMIT characterNameChanged();
        recomputeValidity();
    });
}

void TCreateAccountDialogController::updateCharacterName(const QString& Name)
{
    m_characterName = Name;
    if (Name.isEmpty()) {
        m_characterNameError.clear();
        m_characterNameValid = false;
        recomputeValidity();
        return;
    }
    post({ { QStringLiteral("Type"), QStringLiteral("CheckCharacterName") }, { QStringLiteral("CharacterName"), Name } }, [this](const QJsonObject& reply) {
        if (reply.value(QLatin1String("CharacterName")).toString() != m_characterName) {
            if (!succeeded(reply))
                showError(errorMessage(reply));
            return;
        }
        m_characterNameValid = reply.value(QLatin1String("IsAvailable")).toBool();
        m_characterNameError = errorMessage(reply);
        recomputeValidity();
    });
}

void TCreateAccountDialogController::updateWorldRegionFilter(const int SelectedIndex)
{
    m_selectedWorldRegionFilterIndex = SelectedIndex;
    const QList<QObject*> options = m_proxy->worldRegionFilterModel();
    QString value;
    if (SelectedIndex >= 0 && SelectedIndex < options.size())
        value = options[SelectedIndex]->property("value").toString();
    m_proxy->setWorldRegionFilter(value);
    Q_EMIT accountCreationDataChanged();
}

void TCreateAccountDialogController::updateWorldPvPTypeFilter(const int SelectedIndex)
{
    m_selectedWorldPvPTypeFilterIndex = SelectedIndex;
    const QList<QObject*> options = m_proxy->worldPvPTypeFilterModel();
    QString value;
    if (SelectedIndex >= 0 && SelectedIndex < options.size())
        value = options[SelectedIndex]->property("value").toString();
    m_proxy->setWorldPvPTypeFilter(value);
    Q_EMIT accountCreationDataChanged();
}

void TCreateAccountDialogController::updateSelectedWorld(const QString& SelectedWorld)
{
    m_selectedWorld = SelectedWorld;
    refreshSelectedWorldInfo();
    recomputeValidity();
}

void TCreateAccountDialogController::resetWorldSelection()
{
    m_proxy->resetFilter();
    m_selectedWorldRegionFilterIndex = 0;
    m_selectedWorldPvPTypeFilterIndex = 0;
    m_selectedWorld = m_recommendedWorld;
    refreshSelectedWorldInfo();
    recomputeValidity();
}

void TCreateAccountDialogController::startPlaying(const QString&, const QString&)
{
    if (m_asyncStartPlayingInProgress || !m_allDataValid)
        return;
    setStartPlayingPending(true);
    QJsonObject request{
        { QStringLiteral("type"), QStringLiteral("createaccountandcharacter") },
        { QStringLiteral("EMail"), m_accountEmail },
        { QStringLiteral("Password"), m_accountPassword },
        { QStringLiteral("CharacterName"), m_characterName },
        { QStringLiteral("CharacterSex"), m_characterSexMale ? QStringLiteral("male") : QStringLiteral("female") },
        { QStringLiteral("SelectedWorld"), m_selectedWorld },
    };
    const QString email = m_accountEmail;
    const QString password = m_accountPassword;
    post(request, [this, email, password](const QJsonObject& reply) {
        setStartPlayingPending(false);
        if (!reply.value(QLatin1String("Success")).toBool()) {
            const QString message = errorMessage(reply);
            showError(message.isEmpty() ? QStringLiteral("The account could not be created.") : message);
            return;
        }
        Q_EMIT accountCreated(email, password);
        cancel();
    });
}

void TCreateAccountDialogController::refreshSelectedWorldInfo()
{
    const CreationWorld* found = nullptr;
    for (const CreationWorld& w : m_worlds->worlds()) {
        if (w.name == m_selectedWorld) {
            found = &w;
            break;
        }
    }
    if (!found) {
        m_selectedWorldPlayersOnline = 0;
        m_selectedWorldPvPType.clear();
        m_selectedWorldRegion.clear();
        m_selectedWorldCreationDate.clear();
        m_selectedWorldBattleEyeStatus.clear();
        m_selectedWorldPremiumOnly = false;
        m_selectedWorldTransferType.clear();
        return;
    }
    m_selectedWorldPlayersOnline = found->playersOnline;
    m_selectedWorldPvPType = found->pvpType;
    m_selectedWorldRegion = found->region;
    m_selectedWorldCreationDate = found->creationDate;
    m_selectedWorldBattleEyeStatus = found->battleEyeStatus;
    m_selectedWorldPremiumOnly = found->premiumOnly;
    m_selectedWorldTransferType = found->transferType;
}

void TCreateAccountDialogController::applyPasswordRequirements(const QJsonObject& requirements, const bool valid, const QString& strength,
                                                               const QString& color)
{
    static const QStringList keys{ QStringLiteral("PasswordLength"), QStringLiteral("HasLowerCase"), QStringLiteral("HasUpperCase"),
                                   QStringLiteral("HasNumber"), QStringLiteral("InvalidCharacters") };
    QJsonArray criteria;
    for (const QString& key : keys) {
        criteria.append(QJsonObject{ { QStringLiteral("criterionKey"), key },
                                     { QStringLiteral("criterionMet"), requirements.value(key).toBool() } });
    }
    m_passwordCriteria = criteria;
    m_passwordValid = valid;
    m_passwordStrength = strength;
    m_passwordStrengthColor = QColor(color);
}

void TCreateAccountDialogController::recomputeValidity()
{
    const bool passwordsMatch = !m_accountPassword.isEmpty() && m_accountPassword == m_accountPasswordRepeat;
    m_accountDataValid = m_eMailDataValid && m_passwordValid && passwordsMatch && m_termsAndServicesAccepted;
    m_characterDataValid = m_characterNameValid;
    m_allDataValid = m_accountDataValid && m_characterDataValid && !m_selectedWorld.isEmpty();
    Q_EMIT accountCreationDataChanged();
}

void TCreateAccountDialogController::post(const QJsonObject& request, ReplyHandler onReply)
{
    QNetworkRequest httpRequest(m_serviceUrl);
    httpRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply* reply = m_network->post(httpRequest, QJsonDocument(request).toJson(QJsonDocument::Compact));
    setPending(m_pending + 1);
    connect(reply, &QNetworkReply::finished, this, [this, reply, onReply = std::move(onReply)] {
        reply->deleteLater();
        setPending(m_pending - 1);
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 200) {
            setStartPlayingPending(false);
            showError(reply->errorString());
            return;
        }
        onReply(QJsonDocument::fromJson(reply->readAll()).object());
    });
}

void TCreateAccountDialogController::setPending(const int pending)
{
    m_pending = std::max(0, pending);
    const bool busy = m_pending > 0;
    if (m_asyncOperationInProgress == busy)
        return;
    m_asyncOperationInProgress = busy;
    Q_EMIT asyncOperationStatusChanged();
}

void TCreateAccountDialogController::setStartPlayingPending(const bool pending)
{
    if (m_asyncStartPlayingInProgress == pending)
        return;
    m_asyncStartPlayingInProgress = pending;
    Q_EMIT asyncStartPlayingStatusChanged();
}

void TCreateAccountDialogController::showError(const QString& message)
{
    m_error->showMessage(qtTrId("create_account_dialog_caption"), message);
}

}
