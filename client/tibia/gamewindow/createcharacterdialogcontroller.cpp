#include "createcharacterdialogcontroller.h"

#include "gamewindow/messagedialogcontroller.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <set>

namespace tibia::gamewindow {

namespace {

/// A world's creation date: month and year.
QString worldCreationDateText(const int timestamp)
{
    QDateTime date;
    date.setSecsSinceEpoch(timestamp);
    return date.toString(QStringLiteral("MMM yyyy"));
}

/// Protected since the world's release, or since the month BattlEye came.
QString worldBattleEyeStatusText(const bool initiallyActive, const int activation)
{
    if (initiallyActive)
        return qtTrId("initially_protected");
    QDateTime date;
    date.setSecsSinceEpoch(activation);
    return qtTrId("create_account_dialog_protected_since_format").arg(date.toString(QStringLiteral("MMM yyyy")));
}

/// The reply body as a JSON object.
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

}

WorldData parseWorldData(const QJsonValue& value)
{
    const QJsonObject o = value.toObject();
    WorldData w;
    w.name = o[QLatin1String("Name")].toString();
    w.region = o[QLatin1String("Region")].toString();
    w.pvpType = o[QLatin1String("PvPType")].toString();
    w.playersOnline = o[QLatin1String("PlayersOnline")].toInt();
    w.creationDate = worldCreationDateText(o[QLatin1String("CreationDate")].toInt());
    w.battleEyeStatus = worldBattleEyeStatusText(o[QLatin1String("BattlEyeInitiallyActive")].toInt() == 1,
                                                 o[QLatin1String("BattlEyeActivationTimestamp")].toInt());
    w.premiumOnly = o[QLatin1String("PremiumOnly")].toInt() > 0;
    w.transferType = o[QLatin1String("TransferType")].toString();
    return w;
}

WorldSelectionModel::WorldSelectionModel(QObject* parent)
    : cip::tibia::gamewindow::WorldSelectionModel(parent)
{
    connect(this, &QAbstractItemModel::rowsInserted, this, &WorldSelectionModel::rowCountChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &WorldSelectionModel::rowCountChanged);
}

void WorldSelectionModel::setItems(std::vector<WorldData> worlds)
{
    int count = rowCount();
    const int newCount = int(worlds.size());
    if (newCount < count) {
        beginRemoveRows({}, newCount, count - 1);
        m_worlds = std::move(worlds);
        endRemoveRows();
        count = newCount;
    } else if (count < newCount) {
        beginInsertRows({}, count, newCount - 1);
        m_worlds = std::move(worlds);
        endInsertRows();
    } else {
        m_worlds = std::move(worlds);
    }
    if (count > 0)
        Q_EMIT dataChanged(index(0), index(count - 1));
}

int WorldSelectionModel::rowCount(const QModelIndex&) const { return int(m_worlds.size()); }

QVariant WorldSelectionModel::data(const QModelIndex& index, const int role) const
{
    if (index.row() < 0 || index.column() < 0 || !index.model() || index.row() >= int(m_worlds.size()))
        return {};
    const WorldData& w = m_worlds[size_t(index.row())];
    switch (role) {
    case WorldNameRole:
        return w.name;
    case PlayersOnlineRole:
        return w.playersOnline;
    case RegionRole:
        return w.region;
    case PvPTypeRole:
        return w.pvpType;
    case CreationDateRole:
        return w.creationDate;
    case BattleEyeStatusRole:
        return w.battleEyeStatus;
    case PremiumOnlyRole:
        return w.premiumOnly;
    case TransferTypeRole:
        return w.transferType;
    default:
        return {};
    }
}

QHash<int, QByteArray> WorldSelectionModel::roleNames() const
{
    return {
        { WorldNameRole, "WorldName" },
        { RegionRole, "Region" },
        { PvPTypeRole, "PvPType" },
        { PlayersOnlineRole, "PlayersOnline" },
        { CreationDateRole, "CreationDate" },
        { BattleEyeStatusRole, "BattleEyeStatus" },
        { PremiumOnlyRole, "PremiumOnly" },
        { TransferTypeRole, "TransferType" },
    };
}

TWorldSelectionSortProxyModel::TWorldSelectionSortProxyModel(QObject* parent)
    : cip::tibia::gamewindow::TWorldSelectionSortProxyModel(parent)
    , m_worlds(new WorldSelectionModel(this))
{
    setSourceModel(m_worlds);
    sort(0, Qt::AscendingOrder);
}

void TWorldSelectionSortProxyModel::setWorlds(const std::vector<WorldData>& worlds)
{
    m_worlds->setItems(worlds);

    // Every region and PvP type once, sorted, with "All" among them.
    const QString all = qtTrId("all");
    std::set<QString> regions{ all };
    std::set<QString> pvpTypes{ all };
    std::map<QString, std::set<QString>> pvpTypesByRegion;
    for (const WorldData& world : worlds) {
        regions.insert(world.region);
        pvpTypes.insert(world.pvpType);
        pvpTypesByRegion[world.region].insert(world.pvpType);
    }

    // "All" filters nothing: its value is empty.
    const auto option = [this, &all](const QString& text) -> QObject* {
        return new TWorldFilterEntry(text, text == all ? QString::fromUtf8("") : text, this);
    };
    const auto release = [](const QList<QObject*>& options) {
        for (QObject* entry : options)
            entry->deleteLater();
    };

    release(m_worldRegionFilterModel);
    m_worldRegionFilterModel.clear();
    for (const QString& region : regions)
        m_worldRegionFilterModel.append(option(region));

    release(m_worldPvPTypeFilterModel);
    m_worldPvPTypeFilterModel.clear();
    for (const QString& pvpType : pvpTypes)
        m_worldPvPTypeFilterModel.append(option(pvpType));

    for (const auto& [region, options] : m_worldPvPTypeFilterModelByRegion)
        release(options);
    m_worldPvPTypeFilterModelByRegion.clear();
    for (const auto& [region, types] : pvpTypesByRegion) {
        QList<QObject*>& options = m_worldPvPTypeFilterModelByRegion[region];
        for (const QString& pvpType : types)
            options.append(option(pvpType));
    }

    beginFilterChange();
    m_worldRegionFilter = QString::fromUtf8("");
    m_worldPvPTypeFilter = QString::fromUtf8("");
    endFilterChange(Direction::Both);
    invalidate();
}

void TWorldSelectionSortProxyModel::setRecommendedWorld(const QString& world)
{
    if (m_recommendedWorld == world)
        return;
    m_recommendedWorld = world;
    Q_EMIT recommendedWorldChanged();
    invalidate();
}

void TWorldSelectionSortProxyModel::setWorldRegionFilter(const QString& v)
{
    if (m_worldRegionFilter == v)
        return;
    // A new region drops the PvP type filter: its options are the region's.
    beginFilterChange();
    m_worldRegionFilter = v;
    m_worldPvPTypeFilter = QString::fromUtf8("");
    endFilterChange(Direction::Both);
    Q_EMIT filterChanged();
    Q_EMIT worldRegionsChanged();
    Q_EMIT worldPvPTypeChanged();
}

void TWorldSelectionSortProxyModel::setWorldPvPTypeFilter(const QString& v)
{
    if (m_worldPvPTypeFilter == v)
        return;
    beginFilterChange();
    m_worldPvPTypeFilter = v;
    endFilterChange(Direction::Both);
    Q_EMIT filterChanged();
    Q_EMIT worldRegionsChanged();
    Q_EMIT worldPvPTypeChanged();
}

QList<QObject*> TWorldSelectionSortProxyModel::worldPvPTypeFilterModel() const
{
    if (!m_worldRegionFilter.isEmpty()) {
        const auto it = m_worldPvPTypeFilterModelByRegion.find(m_worldRegionFilter);
        if (it != m_worldPvPTypeFilterModelByRegion.end())
            return it->second;
    }
    return m_worldPvPTypeFilterModel;
}

void TWorldSelectionSortProxyModel::resetFilter()
{
    beginFilterChange();
    m_worldRegionFilter = QString::fromUtf8("");
    m_worldPvPTypeFilter = QString::fromUtf8("");
    endFilterChange(Direction::Both);
}

bool TWorldSelectionSortProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    const QModelIndex world = m_worlds->index(sourceRow);
    if (!m_worldRegionFilter.isEmpty() && world.data(WorldSelectionModel::RegionRole).toString() != m_worldRegionFilter)
        return false;
    if (!m_worldPvPTypeFilter.isEmpty() && world.data(WorldSelectionModel::PvPTypeRole).toString() != m_worldPvPTypeFilter)
        return false;
    return true;
}

bool TWorldSelectionSortProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    // The recommended world first, then by name.
    const QString leftName = left.data(WorldSelectionModel::WorldNameRole).toString();
    const QString rightName = right.data(WorldSelectionModel::WorldNameRole).toString();
    const bool leftNotRecommended = leftName != m_recommendedWorld;
    const bool rightNotRecommended = rightName != m_recommendedWorld;
    if (leftNotRecommended != rightNotRecommended)
        return leftNotRecommended < rightNotRecommended;
    return QString::compare(leftName, rightName, Qt::CaseSensitive) < 0;
}

QString TWorldSelectionSortProxyModel::roleText(const QModelIndex& index, const int role) const
{
    return data(index, role).toString();
}

QString TWorldSelectionSortProxyModel::worldName(const QModelIndex& index) { return roleText(index, WorldSelectionModel::WorldNameRole); }

QString TWorldSelectionSortProxyModel::playersOnline(const QModelIndex& index)
{
    return roleText(index, WorldSelectionModel::PlayersOnlineRole);
}

QString TWorldSelectionSortProxyModel::worldPvPType(const QModelIndex& index) { return roleText(index, WorldSelectionModel::PvPTypeRole); }

QString TWorldSelectionSortProxyModel::worldRegion(const QModelIndex& index) { return roleText(index, WorldSelectionModel::RegionRole); }

QString TWorldSelectionSortProxyModel::worldCreationDate(const QModelIndex& index)
{
    return roleText(index, WorldSelectionModel::CreationDateRole);
}

QString TWorldSelectionSortProxyModel::worldBattleEyeStatus(const QModelIndex& index)
{
    return roleText(index, WorldSelectionModel::BattleEyeStatusRole);
}

QString TWorldSelectionSortProxyModel::worldPremiumOnly(const QModelIndex& index)
{
    return roleText(index, WorldSelectionModel::PremiumOnlyRole);
}

QString TWorldSelectionSortProxyModel::worldTransferType(const QModelIndex& index)
{
    return roleText(index, WorldSelectionModel::TransferTypeRole);
}

TCreateCharacterDialogController::TCreateCharacterDialogController(const QString& qmlFile, QObject* parent)
    : TDialogController(qmlFile, parent)
    , m_network(new QNetworkAccessManager(this))
    , m_proxy(new TWorldSelectionSortProxyModel(this))
    , m_error(new TMessageDialogController(this))
{
    m_worldsModel = m_proxy;
    m_characterSexMale = true;
    m_playTutorial = true;
    setAccountHasNoCharacters(true);
    m_error->setCloseOnButtonClicked(true);
    m_error->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK, QKeySequence(QStringLiteral("Return"))) });
}

TCreateCharacterDialogController::~TCreateCharacterDialogController() = default;

void TCreateCharacterDialogController::setAccountHasNoCharacters(const bool noCharacters)
{
    // The first character can neither be cancelled nor skip the tutorial.
    m_noCharacters = noCharacters;
    m_canChangePlayTutorial = !noCharacters;
    m_canCancelCharacterCreation = !noCharacters;
}

void TCreateCharacterDialogController::open(const bool accountHasNoCharacters)
{
    if (m_pending > 0)
        return;
    setAccountHasNoCharacters(accountHasNoCharacters);
    dialog()->close();
    requestAccountCreationStatus();
}

void TCreateCharacterDialogController::showDialog()
{
    dialog()->close();
    requestAccountCreationStatus();
}

void TCreateCharacterDialogController::cancel()
{
    m_characterSexMale = true;
    m_playTutorial = true;
    setAccountHasNoCharacters(false);
    m_proxy->setWorlds({});
    Q_EMIT characterCreationDataChanged();
    dialog()->close();
}

void TCreateCharacterDialogController::requestAccountCreationStatus()
{
    post({ { QStringLiteral("type"), QStringLiteral("getaccountcreationstatus") } },
         [this](QNetworkReply* reply) { onAccountCreationStatus(reply); });
}

void TCreateCharacterDialogController::onAccountCreationStatus(QNetworkReply* reply)
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
    std::vector<WorldData> worlds;
    for (const QJsonValue& world : status[QLatin1String("Worlds")].toArray())
        worlds.push_back(parseWorldData(world));
    m_proxy->setWorlds(worlds);
    m_proxy->setRecommendedWorld(status[QLatin1String("RecommendedWorld")].toString());
    // Product: the portal can switch the tutorial off for every new character.
    if (!status[QLatin1String("PlayTutorialEnabled")].toBool(true)) {
        m_canChangePlayTutorial = false;
        m_playTutorial = false;
    }
    suggestCharacterName();
    dialog()->show();
    Q_EMIT characterCreationDataChanged();
}

void TCreateCharacterDialogController::suggestCharacterName()
{
    post({ { QStringLiteral("Type"), QStringLiteral("GenerateCharacterName") } }, [this](QNetworkReply* reply) { onCharacterNameGenerated(reply); });
}

void TCreateCharacterDialogController::onCharacterNameGenerated(QNetworkReply* reply)
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
    m_generatedName = m_characterName;
    m_characterNameValid = true;
    Q_EMIT characterCreationDataChanged();
}

void TCreateCharacterDialogController::updateCharacterName(const QString& Name)
{
    m_characterName = Name;
    if (!m_characterName.isEmpty()) {
        post({ { QStringLiteral("Type"), QStringLiteral("CheckCharacterName") }, { QStringLiteral("CharacterName"), m_characterName } },
             [this](QNetworkReply* reply) { onCharacterNameChecked(reply); });
        return;
    }
    m_characterNameError = QString::fromUtf8("");
    m_characterNameValid = false;
    Q_EMIT characterCreationDataChanged();
}

void TCreateCharacterDialogController::onCharacterNameChecked(QNetworkReply* reply)
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
        Q_EMIT characterCreationDataChanged();
    } else if (!succeeded(answer)) {
        showError(errorMessage(answer));
    }
}

void TCreateCharacterDialogController::createCharacter(const QString& CharacterName, const bool CharacterSexMale, const bool PlayTutorial,
                                                       const QString& WorldName)
{
    if (m_pending > 0)
        return;
    QJsonObject request{
        { QStringLiteral("SessionKey"), m_sessionKey },
        { QStringLiteral("Type"), QStringLiteral("createCharacter") },
        { QStringLiteral("CharacterName"), CharacterName },
        { QStringLiteral("CharacterSex"), CharacterSexMale ? QStringLiteral("male") : QStringLiteral("female") },
        { QStringLiteral("SelectedWorld"), WorldName },
        { QStringLiteral("PlayTutorial"), PlayTutorial },
        { QStringLiteral("ClientType"), QStringLiteral("Client") },
        { QStringLiteral("NameIsGenerated"), m_generatedName == CharacterName },
        // Host: "Epic" for the Epic Games distribution, which qtclient is not.
        { QStringLiteral("DistributionChannel"), QStringLiteral("Regular") },
    };
    addCreateCharacterFields(request);
    post(request, [this](QNetworkReply* reply) { onCharacterCreated(reply); });
}

void TCreateCharacterDialogController::onCharacterCreated(QNetworkReply* reply)
{
    if (!httpOk(reply)) {
        showError(reply->errorString());
        return;
    }
    const QJsonObject answer = replyObject(reply);
    if (answer[QLatin1String("Success")].toBool()) {
        // The reply carries the account's characters; the host applies them.
        Q_EMIT characterCreated(answer[QLatin1String("CharacterName")].toString(), answer);
        cancel();
        return;
    }
    if (succeeded(answer))
        return;
    showError(errorMessage(answer));
    // Product: the portal's codes differ from CipSoft's (which closes the dialog
    // unless errorCode is 2), so the form always stays for a correction.
}

void TCreateCharacterDialogController::post(const QJsonObject& request, ReplyHandler onReply)
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

void TCreateCharacterDialogController::setPending(const int pending)
{
    m_pending = std::max(0, pending);
    m_asyncOperationInProgress = m_pending > 0;
    Q_EMIT asyncOperationStatusChanged();
}

void TCreateCharacterDialogController::showError(const QString& message)
{
    m_error->showMessage(qtTrId("error"), message);
}

}
