#include "createcharacterdialogcontroller.h"

#include "gamewindow/messagedialogcontroller.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace tibia::gamewindow {

namespace {

/// CIP 0x100e13ab0.
QString formatDate(const qint64 timestamp)
{
    if (timestamp <= 0)
        return {};
    return QLocale().toString(QDateTime::fromSecsSinceEpoch(timestamp).date(), QLocale::ShortFormat);
}

/// CIP 0x100e13bb0.
QString battleEyeStatus(const bool initiallyActive, const qint64 activation)
{
    if (initiallyActive)
        return QStringLiteral("Protected since release");
    if (activation > 0 && activation <= QDateTime::currentSecsSinceEpoch())
        return QStringLiteral("Protected since %1").arg(formatDate(activation));
    return QStringLiteral("Unprotected");
}

/// CIP 0x100e13260.
CreationWorld parseWorld(const QJsonObject& o)
{
    CreationWorld w;
    w.name = o.value(QLatin1String("Name")).toString();
    w.region = o.value(QLatin1String("Region")).toString();
    w.pvpType = o.value(QLatin1String("PvPType")).toString();
    w.playersOnline = o.value(QLatin1String("PlayersOnline")).toInt();
    w.creationDate = formatDate(o.value(QLatin1String("CreationDate")).toInteger());
    w.battleEyeStatus = battleEyeStatus(o.value(QLatin1String("BattlEyeInitiallyActive")).toInt() == 1,
                                        o.value(QLatin1String("BattlEyeActivationTimestamp")).toInteger());
    w.premiumOnly = o.value(QLatin1String("PremiumOnly")).toInt() > 0;
    w.transferType = o.value(QLatin1String("TransferType")).toString();
    return w;
}

/// CIP 0x100e16b70.
bool succeeded(const QJsonObject& reply) { return reply.value(QLatin1String("errorCode")).toInt() == 0; }

/// CIP 0x100e16c30: "errorMessage", a string or a list joined by ", ".
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

}

TWorldSelectionModel::TWorldSelectionModel(QObject* parent) : cip::tibia::gamewindow::WorldSelectionModel(parent) {}

void TWorldSelectionModel::setWorlds(const QList<CreationWorld>& worlds)
{
    beginResetModel();
    m_worlds = worlds;
    m_count = int(m_worlds.size());
    endResetModel();
    Q_EMIT rowCountChanged();
}

int TWorldSelectionModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : int(m_worlds.size()); }

QVariant TWorldSelectionModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= m_worlds.size())
        return {};
    if (role == WorldNameRole || role == Qt::DisplayRole)
        return m_worlds[index.row()].name;
    return {};
}

QHash<int, QByteArray> TWorldSelectionModel::roleNames() const { return { { WorldNameRole, "WorldName" } }; }

TWorldSelectionSortProxyModel::TWorldSelectionSortProxyModel(TWorldSelectionModel* source, QObject* parent)
    : cip::tibia::gamewindow::TWorldSelectionSortProxyModel(parent)
    , m_source(source)
{
    setSourceModel(source);
    setSortRole(TWorldSelectionModel::WorldNameRole);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    sort(0);
}

QList<QObject*> TWorldSelectionSortProxyModel::filterOptions(const QStringList& values)
{
    QList<QObject*> options{ new TWorldFilterEntry(QStringLiteral("All"), {}, this) };
    for (const QString& value : values)
        options.append(new TWorldFilterEntry(value, value, this));
    return options;
}

void TWorldSelectionSortProxyModel::reload(const QString& recommendedWorld)
{
    QStringList regions;
    QStringList pvpTypes;
    for (const CreationWorld& w : m_source->worlds()) {
        if (!w.region.isEmpty() && !regions.contains(w.region))
            regions.append(w.region);
        if (!w.pvpType.isEmpty() && !pvpTypes.contains(w.pvpType))
            pvpTypes.append(w.pvpType);
    }
    regions.sort();
    pvpTypes.sort();
    qDeleteAll(m_worldRegionFilterModel);
    qDeleteAll(m_worldPvPTypeFilterModel);
    m_worldRegionFilterModel = filterOptions(regions);
    m_worldPvPTypeFilterModel = filterOptions(pvpTypes);
    Q_EMIT worldRegionsChanged();
    Q_EMIT worldPvPTypeChanged();
    if (m_recommendedWorld != recommendedWorld) {
        m_recommendedWorld = recommendedWorld;
        Q_EMIT recommendedWorldChanged();
    }
    resetFilter();
}

void TWorldSelectionSortProxyModel::setWorldRegionFilter(const QString& v)
{
    if (m_worldRegionFilter == v)
        return;
    beginFilterChange();
    m_worldRegionFilter = v;
    endFilterChange(Direction::Rows);
    Q_EMIT filterChanged();
}

void TWorldSelectionSortProxyModel::setWorldPvPTypeFilter(const QString& v)
{
    if (m_worldPvPTypeFilter == v)
        return;
    beginFilterChange();
    m_worldPvPTypeFilter = v;
    endFilterChange(Direction::Rows);
    Q_EMIT filterChanged();
}

void TWorldSelectionSortProxyModel::resetFilter()
{
    beginFilterChange();
    m_worldRegionFilter.clear();
    m_worldPvPTypeFilter.clear();
    endFilterChange(Direction::Rows);
    Q_EMIT filterChanged();
}

bool TWorldSelectionSortProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    if (sourceRow >= m_source->worlds().size())
        return false;
    const CreationWorld& w = m_source->worlds()[sourceRow];
    return (m_worldRegionFilter.isEmpty() || w.region == m_worldRegionFilter) && (m_worldPvPTypeFilter.isEmpty() || w.pvpType == m_worldPvPTypeFilter);
}

const CreationWorld* TWorldSelectionSortProxyModel::worldAt(const QModelIndex& index) const
{
    const QModelIndex source = mapToSource(index);
    if (!source.isValid() || source.row() >= m_source->worlds().size())
        return nullptr;
    return &m_source->worlds()[source.row()];
}

QString TWorldSelectionSortProxyModel::worldName(const QModelIndex& index)
{
    const CreationWorld* w = worldAt(index);
    return w ? w->name : QString();
}

QString TWorldSelectionSortProxyModel::playersOnline(const QModelIndex& index)
{
    const CreationWorld* w = worldAt(index);
    return w ? QString::number(w->playersOnline) : QString();
}

QString TWorldSelectionSortProxyModel::worldPvPType(const QModelIndex& index)
{
    const CreationWorld* w = worldAt(index);
    return w ? w->pvpType : QString();
}

QString TWorldSelectionSortProxyModel::worldRegion(const QModelIndex& index)
{
    const CreationWorld* w = worldAt(index);
    return w ? w->region : QString();
}

QString TWorldSelectionSortProxyModel::worldCreationDate(const QModelIndex& index)
{
    const CreationWorld* w = worldAt(index);
    return w ? w->creationDate : QString();
}

QString TWorldSelectionSortProxyModel::worldBattleEyeStatus(const QModelIndex& index)
{
    const CreationWorld* w = worldAt(index);
    return w ? w->battleEyeStatus : QString();
}

QString TWorldSelectionSortProxyModel::worldPremiumOnly(const QModelIndex& index)
{
    const CreationWorld* w = worldAt(index);
    if (!w)
        return {};
    return w->premiumOnly ? qtTrId("yes") : qtTrId("no");
}

QString TWorldSelectionSortProxyModel::worldTransferType(const QModelIndex& index)
{
    const CreationWorld* w = worldAt(index);
    return w ? w->transferType : QString();
}

TCreateCharacterDialogController::TCreateCharacterDialogController(const QString& qmlFile, QObject* parent)
    : TDialogController(qmlFile, parent)
    , m_network(new QNetworkAccessManager(this))
    , m_worlds(new TWorldSelectionModel(this))
    , m_proxy(new TWorldSelectionSortProxyModel(m_worlds, this))
    , m_error(new TMessageDialogController(this))
{
    m_worldsModel = m_proxy;
    m_characterSexMale = true;
    m_playTutorial = true;
    m_canCancelCharacterCreation = true;
    m_error->setCloseOnButtonClicked(true);
    m_error->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK, QKeySequence(QStringLiteral("Return"))) });
}

TCreateCharacterDialogController::~TCreateCharacterDialogController() = default;

void TCreateCharacterDialogController::open(const bool accountHasNoCharacters)
{
    if (m_pending > 0 || dialog()->isInStack())
        return;
    m_noCharacters = accountHasNoCharacters;
    showDialog();
}

void TCreateCharacterDialogController::showDialog()
{
    m_canCancelCharacterCreation = !m_noCharacters;
    Q_EMIT characterCreationDataChanged();
    post({ { QStringLiteral("type"), QStringLiteral("getaccountcreationstatus") } }, [this](const QJsonObject& reply) { onStatus(reply); });
}

void TCreateCharacterDialogController::onStatus(const QJsonObject& reply)
{
    if (!succeeded(reply)) {
        showError(errorMessage(reply));
        return;
    }
    QList<CreationWorld> worlds;
    for (const QJsonValue& w : reply.value(QLatin1String("Worlds")).toArray())
        worlds.append(parseWorld(w.toObject()));
    m_worlds->setWorlds(worlds);
    m_proxy->reload(reply.value(QLatin1String("RecommendedWorld")).toString());
    const bool tutorial = reply.value(QLatin1String("PlayTutorialEnabled")).toBool(true);
    m_canChangePlayTutorial = tutorial;
    m_playTutorial = tutorial;
    Q_EMIT characterCreationDataChanged();
    suggestCharacterName();
    dialog()->show();
}

void TCreateCharacterDialogController::resetForm()
{
    m_characterSexMale = true;
    m_playTutorial = true;
    m_characterName.clear();
    m_characterNameError.clear();
    m_characterNameValid = false;
    m_generatedName.clear();
    m_creating = false;
    m_worlds->setWorlds({});
    Q_EMIT characterNameChanged();
    Q_EMIT characterCreationDataChanged();
}

void TCreateCharacterDialogController::cancel()
{
    resetForm();
    if (dialog()->isInStack())
        dialog()->close();
}

void TCreateCharacterDialogController::suggestCharacterName()
{
    post({ { QStringLiteral("Type"), QStringLiteral("GenerateCharacterName") } }, [this](const QJsonObject& reply) {
        if (!succeeded(reply)) {
            showError(errorMessage(reply));
            return;
        }
        // CIP 0x100e17c10.
        m_generatedName = reply.value(QLatin1String("GeneratedName")).toString();
        m_characterName = m_generatedName;
        m_characterNameError = errorMessage(reply);
        m_characterNameValid = !m_generatedName.isEmpty();
        Q_EMIT characterNameChanged();
        Q_EMIT characterCreationDataChanged();
    });
}

void TCreateCharacterDialogController::updateCharacterName(const QString& Name)
{
    m_characterName = Name;
    if (Name.isEmpty()) {
        m_characterNameError.clear();
        m_characterNameValid = false;
        Q_EMIT characterCreationDataChanged();
        return;
    }
    post({ { QStringLiteral("Type"), QStringLiteral("CheckCharacterName") }, { QStringLiteral("CharacterName"), Name } }, [this](const QJsonObject& reply) {
        // CIP 0x100e17710: an answer for an older name only reports errors.
        if (reply.value(QLatin1String("CharacterName")).toString() != m_characterName) {
            if (!succeeded(reply))
                showError(errorMessage(reply));
            return;
        }
        m_characterNameValid = reply.value(QLatin1String("IsAvailable")).toBool();
        m_characterNameError = errorMessage(reply);
        Q_EMIT characterCreationDataChanged();
    });
}

void TCreateCharacterDialogController::createCharacter(const QString& CharacterName, const bool CharacterSexMale, const bool PlayTutorial,
                                                       const QString& WorldName)
{
    if (m_creating)
        return;
    m_creating = true;
    QJsonObject request{
        { QStringLiteral("SessionKey"), m_sessionKey },
        { QStringLiteral("Type"), QStringLiteral("createCharacter") },
        { QStringLiteral("CharacterName"), CharacterName },
        { QStringLiteral("CharacterSex"), CharacterSexMale ? QStringLiteral("male") : QStringLiteral("female") },
        { QStringLiteral("SelectedWorld"), WorldName },
        { QStringLiteral("PlayTutorial"), PlayTutorial },
        { QStringLiteral("ClientType"), QStringLiteral("Client") },
        { QStringLiteral("NameIsGenerated"), !m_generatedName.isEmpty() && CharacterName == m_generatedName },
        { QStringLiteral("DistributionChannel"), QStringLiteral("Regular") },
    };
    addCreateCharacterFields(request);
    post(request, [this, CharacterName](const QJsonObject& reply) {
        m_creating = false;
        // CIP 0x100e18c80 closes the dialog after most errors; product's
        // service codes differ from CIP's, so the form stays for a correction.
        if (!reply.value(QLatin1String("Success")).toBool()) {
            const QString message = errorMessage(reply);
            showError(message.isEmpty() ? QStringLiteral("The character could not be created.") : message);
            return;
        }
        const QString name = reply.value(QLatin1String("CharacterName")).toString(CharacterName);
        cancel();
        Q_EMIT characterCreated(name, reply);
    });
}

void TCreateCharacterDialogController::post(const QJsonObject& request, ReplyHandler onReply)
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
            m_creating = false;
            showError(reply->errorString());
            return;
        }
        onReply(QJsonDocument::fromJson(reply->readAll()).object());
    });
}

void TCreateCharacterDialogController::setPending(const int pending)
{
    m_pending = std::max(0, pending);
    const bool busy = m_pending > 0;
    if (m_asyncOperationInProgress == busy)
        return;
    m_asyncOperationInProgress = busy;
    Q_EMIT asyncOperationStatusChanged();
}

void TCreateCharacterDialogController::showError(const QString& message)
{
    m_error->showMessage(qtTrId("create_character_dialog_caption"), message);
}

}
