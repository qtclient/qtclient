#include "exaltationforgedialogcontroller.h"
#include "gamewindow/smoothfiltering.h"

#include "container/containerslotinformation.h"
#include "game/gamesessionbase.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameactionhandlers.h"

#include <tibia/game/gamesession.h>
#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <QColor>
#include <QDateTime>
#include <QJsonObject>

#include <algorithm>
#include <functional>
#include <optional>
#include <tibia/game/thingsession_forward.h>

namespace tibia::exaltation {

namespace {

using Info = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;

/// CIP : the type's upgrade classification, if it has one.
std::optional<int> classificationOf(const uint16_t typeId)
{
    if (typeId == 0)
        return std::nullopt;
    const TAppearanceTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
    if (!type || type->getClassification() == 0)
        return std::nullopt;
    return type->getClassification();
}

}

TForgeObjectListModel::TForgeObjectListModel(container::TObjectCountStorage* counts, QObject* parent)
    : cip::tibia::gamewindow::TAppearanceInstanceListModel(parent)
    , m_counts(counts)
{
    connect(m_counts, &container::TObjectCountStorage::objectCountsChanged, this, &TForgeObjectListModel::onObjectCountsChanged);
}

int TForgeObjectListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_objects.size());
}

QVariant TForgeObjectListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
        return {};
    const auto [typeId, tier] = m_objects[index.row()];
    switch (role) {
    case TypeIdRole:
        return static_cast<uint>(typeId);
    case CumulativeCountRole:
        return 1u;
    case TypeCountRole:
        return m_counts->count(typeId, tier);
    case CountStringRole:
        return container::ammoText(m_counts->count(typeId, tier));
    case AlternativeSlotStringRole:
    case LootValueImageUnderItemRole:
    case LootValueImageOverItemRole:
    case ManagedContainerTooltipRole:
        return QString();
    case LiquidTypeRole:
        return QVariant::fromValue(static_cast<Info::ELiquidType>(0));
    case HookDirectionRole:
        return QVariant::fromValue(Info::EHookDirection::NONE);
    case UpgradeTierRole:
        return static_cast<uint>(tier);
    case DecoItemObjectIdRole:
        return 0u;
    default:
        return {};
    }
}

QHash<int, QByteArray> TForgeObjectListModel::roleNames() const
{
    return {
        { TypeIdRole, "typeID" },
        { CumulativeCountRole, "cumulativeCount" },
        { TypeCountRole, "typeCount" },
        { CountStringRole, "countString" },
        { AlternativeSlotStringRole, "alternativeSlotString" },
        { LiquidTypeRole, "liquideType" },
        { HookDirectionRole, "hookDirection" },
        { LootValueImageUnderItemRole, "lootvalueImageSourceUnderItem" },
        { LootValueImageOverItemRole, "lootvalueImageSourceOverItem" },
        { ManagedContainerTooltipRole, "managedContainerTooltip" },
        { UpgradeTierRole, "upgradeTier" },
        { DecoItemObjectIdRole, "decoItemObjectID" },
    };
}

void TForgeObjectListModel::setObjects(std::vector<std::pair<uint16_t, uint8_t>> objects)
{
    beginResetModel();
    m_objects = std::move(objects);
    endResetModel();
}

void TForgeObjectListModel::onObjectCountsChanged()
{
    if (!m_objects.empty())
        Q_EMIT dataChanged(index(0), index(rowCount() - 1), { TypeCountRole, CountStringRole });
}

TUpgradeableObjectModel::TUpgradeableObjectModel(TForgeObjectListModel* source, QObject* parent)
    : QSortFilterProxyModel(parent)
    , m_source(source)
{
    setSourceModel(source);
    sort(0, Qt::AscendingOrder);
}

void TUpgradeableObjectModel::setExcludedTypes(std::set<uint16_t> types)
{
    beginFilterChange();
    m_excluded = std::move(types);
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

bool TUpgradeableObjectModel::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    return !m_excluded.contains(m_source->object(sourceRow).first);
}

bool TUpgradeableObjectModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const auto [leftType, leftTier] = m_source->object(left.row());
    const auto [rightType, rightTier] = m_source->object(right.row());
    if (leftTier != rightTier)
        return leftTier > rightTier;
    const int leftClass = classificationOf(leftType).value_or(0);
    const int rightClass = classificationOf(rightType).value_or(0);
    if (leftClass != rightClass)
        return leftClass > rightClass;
    return leftType < rightType;
}

QVariant TExaltationHistoryListModel::itemData(const THistoryEntry& entry, const int role) const
{
    switch (role) {
    case DateStringRole:
        // CIP TTextHelperQmlService::formatDateTime , with seconds.
        return QDateTime::fromSecsSinceEpoch(entry.timestamp).toString(QStringLiteral("yyyy-MM-dd, HH:mm:ss"));
    case ActionStringRole:
        if (!entry.bonus)
            return entry.action;
        return QStringLiteral("<font color=\"%1\">%2</font>").arg(QColor(0x3f, 0xbb, 0xca).name(), entry.action);
    case DetailsStringRole:
        return entry.description;
    default:
        return {};
    }
}

QHash<int, QByteArray> TExaltationHistoryListModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
    roles[DateStringRole] = "dateString";
    roles[ActionStringRole] = "actionString";
    roles[DetailsStringRole] = "detailsString";
    return roles;
}

}

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using exaltation::classificationOf;
using ObjectKey = std::pair<uint16_t, uint8_t>;

input::IGameActionPtr simpleAction(const A type)
{
    return input::makeGameAction(type);
}

/// The session's exaltation storage, or null without a session.
exaltation::TExaltationStorage* exaltationStorageOf(const std::shared_ptr<game::IGameSession>& gameSession)
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(gameSession.get());
    return session ? session->exaltationStorage().get() : nullptr;
}

}

// ---------------------------------------------------------------------------
// Fusion

TExaltationForgeFusionPageController::TExaltationForgeFusionPageController(QObject* parent)
    : cip::TExaltationForgeFusionPageController(parent)
    , m_counts(new container::TObjectCountStorage(this))
    , m_fusionObjects(new exaltation::TForgeObjectListModel(m_counts, this))
    , m_resourceObjects(new exaltation::TForgeObjectListModel(m_counts, this))
    , m_fusionProxy(new exaltation::TUpgradeableObjectModel(m_fusionObjects, this))
    , m_resourceProxy(new exaltation::TUpgradeableObjectModel(m_resourceObjects, this))
{
    m_sucessChance = 50;
    m_sucessChanceBonus = 15;
    m_baseTierLoss = 100;
    m_improvedTierLoss = 50;
    m_fusionObjectsModel = QVariant::fromValue<QObject*>(m_fusionProxy);
    m_resourceObjectsModel = QVariant::fromValue<QObject*>(m_resourceProxy);
}

/// The session's exaltation storage's fusion groups.
void TExaltationForgeFusionPageController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    if (m_storage)
        disconnect(m_storage, nullptr, this, nullptr);
    m_gameSession = GameSession;
    m_storage = exaltationStorageOf(m_gameSession);
    if (m_storage)
        connect(m_storage, &exaltation::TExaltationStorage::fusionGroupsChanged, this,
                &TExaltationForgeFusionPageController::onFusionGroupsChanged);
}

uchar TExaltationForgeFusionPageController::sucessChance() const
{
    return m_sucessChance;
}

uchar TExaltationForgeFusionPageController::sucessChanceBonus() const
{
    return m_sucessChanceBonus;
}

uchar TExaltationForgeFusionPageController::baseTierLoss() const
{
    return m_baseTierLoss;
}

uchar TExaltationForgeFusionPageController::improvedTierLoss() const
{
    return m_improvedTierLoss;
}

QVariant TExaltationForgeFusionPageController::fusionObjectsModel() const
{
    return m_fusionObjectsModel;
}

QVariant TExaltationForgeFusionPageController::resourceObjectsModel() const
{
    return m_resourceObjectsModel;
}

uchar TExaltationForgeFusionPageController::dustCostForFusion() const
{
    return m_convergenceFusionEnabled ? 130 : 100;
}

qulonglong TExaltationForgeFusionPageController::fusionGoldCost() const
{
    const std::optional<int> classification = classificationOf(m_fusionObjectTypeId);
    if (!classification)
        return 0;
    return m_convergenceFusionEnabled ? m_storage->convergenceFusionPrice(m_fusionObjectUpgradeTier)
                                      : m_storage->fusionPrice(*classification, m_fusionObjectUpgradeTier);
}

ushort TExaltationForgeFusionPageController::fusionObjectTypeId() const
{
    return m_fusionObjectTypeId;
}

uchar TExaltationForgeFusionPageController::fusionObjectUpgradeTier() const
{
    return m_fusionObjectUpgradeTier;
}

ushort TExaltationForgeFusionPageController::resourceObjectTypeId() const
{
    return m_resourceObjectTypeId;
}

uint TExaltationForgeFusionPageController::resourceObjectCount() const
{
    return m_counts->count(m_resourceObjectTypeId, m_fusionObjectUpgradeTier);
}

bool TExaltationForgeFusionPageController::convergenceFusionEnabled() const
{
    return m_convergenceFusionEnabled;
}

void TExaltationForgeFusionPageController::setConvergenceFusionEnabled(const bool v)
{
    if (v == m_convergenceFusionEnabled)
        return;
    m_convergenceFusionEnabled = v;
    onFusionGroupsChanged();
    Q_EMIT convergenceFusionEnabledChanged();
    Q_EMIT fusionObjectChanged();
    Q_EMIT resourceObjectChanged();
}

void TExaltationForgeFusionPageController::showDialogTab()
{
    onFusionGroupsChanged();
}

void TExaltationForgeFusionPageController::hideDialogTab()
{
    reset();
}

void TExaltationForgeFusionPageController::prepareOpenDialog()
{
    m_convergenceFusionEnabled = false;
}

/// CIP .
void TExaltationForgeFusionPageController::fusionObjectSelected(const int TypeID, const int UpgradeTier)
{
    if (TypeID == m_fusionObjectTypeId && UpgradeTier == m_fusionObjectUpgradeTier)
        return;
    m_fusionObjectTypeId = static_cast<ushort>(TypeID);
    m_fusionObjectUpgradeTier = static_cast<uchar>(UpgradeTier);
    Q_EMIT fusionObjectChanged();
    setResourceModel();
    ushort resource = 0;
    if (!m_convergenceFusionEnabled && m_resourceProxy->rowCount() == 1)
        resource = static_cast<ushort>(m_resourceProxy->data(m_resourceProxy->index(0, 0), exaltation::TForgeObjectListModel::TypeIdRole).toInt());
    if (resource != m_resourceObjectTypeId) {
        m_resourceObjectTypeId = resource;
        Q_EMIT resourceObjectChanged();
    }
}

void TExaltationForgeFusionPageController::resourceObjectSelected(const int TypeID)
{
    if (TypeID == m_resourceObjectTypeId)
        return;
    m_resourceObjectTypeId = static_cast<ushort>(TypeID);
    Q_EMIT resourceObjectChanged();
}

/// CIP .
void TExaltationForgeFusionPageController::requestFusion(const bool SuccessChanceIncreased, const bool TierLossImproved)
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionExaltationForgeFusion>(
        static_cast<uint16_t>(m_fusionObjectTypeId), static_cast<uint8_t>(m_fusionObjectUpgradeTier),
        static_cast<uint16_t>(m_resourceObjectTypeId), SuccessChanceIncreased, TierLossImproved));
    m_fusionObjectTypeId = 0;
    m_fusionObjectUpgradeTier = 0;
    m_resourceObjectTypeId = 0;
    Q_EMIT fusionObjectChanged();
    Q_EMIT resourceObjectChanged();
}

/// CIP .
void TExaltationForgeFusionPageController::requestConvergenceFusion()
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionExaltationForgeConvergenceFusion>(
        static_cast<uint16_t>(m_fusionObjectTypeId), static_cast<uint8_t>(m_fusionObjectUpgradeTier),
        static_cast<uint16_t>(m_resourceObjectTypeId)));
    m_fusionObjectTypeId = 0;
    m_fusionObjectUpgradeTier = 0;
    m_resourceObjectTypeId = 0;
    Q_EMIT fusionObjectChanged();
    Q_EMIT resourceObjectChanged();
}

/// CIP : every object of every group, counted.
void TExaltationForgeFusionPageController::onFusionGroupsChanged()
{
    reset();
    const auto& groups = m_convergenceFusionEnabled ? m_storage->convergenceFusionGroups() : m_storage->fusionGroups();
    std::map<ObjectKey, uint32_t> counts;
    std::vector<ObjectKey> objects;
    for (const exaltation::TFusionGroup& group : groups) {
        for (const exaltation::TExaltationObject& object : group) {
            counts[{ object.typeId, object.tier }] = object.count;
            if (classificationOf(object.typeId))
                objects.emplace_back(object.typeId, object.tier);
        }
    }
    m_fusionObjects->setObjects(std::move(objects));
    m_counts->setCounts(std::move(counts));
}

/// CIP : the other objects of the selected object's groups (a
/// convergence fusion's of the same tier), and the object itself when there
/// are two.
void TExaltationForgeFusionPageController::setResourceModel()
{
    const auto& groups = m_convergenceFusionEnabled ? m_storage->convergenceFusionGroups() : m_storage->fusionGroups();
    const uint32_t selectedCount = m_counts->count(m_fusionObjectTypeId, m_fusionObjectUpgradeTier);
    std::vector<ObjectKey> objects;
    for (const exaltation::TFusionGroup& group : groups) {
        const bool containsSelected = std::ranges::any_of(group, [this](const exaltation::TExaltationObject& object) {
            return object.typeId == m_fusionObjectTypeId && object.tier == m_fusionObjectUpgradeTier;
        });
        if (!containsSelected)
            continue;
        for (const exaltation::TExaltationObject& object : group) {
            if (!classificationOf(object.typeId))
                continue;
            if (m_convergenceFusionEnabled && object.tier != m_fusionObjectUpgradeTier)
                continue;
            if (object.typeId != m_fusionObjectTypeId || selectedCount > 1)
                objects.emplace_back(object.typeId, object.tier);
        }
    }
    m_resourceObjects->setObjects(std::move(objects));
}

/// CIP .
void TExaltationForgeFusionPageController::reset()
{
    m_fusionObjectTypeId = 0;
    m_fusionObjectUpgradeTier = 0;
    m_resourceObjectTypeId = 0;
    m_fusionObjects->setObjects({});
    m_resourceObjects->setObjects({});
    m_counts->clear();
    Q_EMIT fusionObjectChanged();
    Q_EMIT resourceObjectChanged();
}

// ---------------------------------------------------------------------------
// Transfer

TExaltationForgeTransferPageController::TExaltationForgeTransferPageController(QObject* parent)
    : cip::TExaltationForgeTransferPageController(parent)
    , m_sourceCounts(new container::TObjectCountStorage(this))
    , m_targetCounts(new container::TObjectCountStorage(this))
    , m_sourceObjects(new exaltation::TForgeObjectListModel(m_sourceCounts, this))
    , m_targetObjects(new exaltation::TForgeObjectListModel(m_targetCounts, this))
    , m_sourceProxy(new exaltation::TUpgradeableObjectModel(m_sourceObjects, this))
    , m_targetProxy(new exaltation::TUpgradeableObjectModel(m_targetObjects, this))
{
    m_sourceObjectsModel = QVariant::fromValue<QObject*>(m_sourceProxy);
    m_targetObjectsModel = QVariant::fromValue<QObject*>(m_targetProxy);
}

/// The session's exaltation storage's transfer groups.
void TExaltationForgeTransferPageController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    if (m_storage)
        disconnect(m_storage, nullptr, this, nullptr);
    m_gameSession = GameSession;
    m_storage = exaltationStorageOf(m_gameSession);
    if (m_storage)
        connect(m_storage, &exaltation::TExaltationStorage::transferGroupsChanged, this,
                &TExaltationForgeTransferPageController::onTransferGroupsChanged);
}

QVariant TExaltationForgeTransferPageController::sourceObjectsModel() const
{
    return m_sourceObjectsModel;
}

QVariant TExaltationForgeTransferPageController::targetObjectsModel() const
{
    return m_targetObjectsModel;
}

uchar TExaltationForgeTransferPageController::dustCostForTransfer() const
{
    return m_convergenceTransferEnabled ? 160 : 100;
}

qulonglong TExaltationForgeTransferPageController::transferGoldCost() const
{
    const std::optional<int> classification = classificationOf(m_sourceObjectTypeId);
    if (!classification)
        return 0;
    return m_convergenceTransferEnabled ? m_storage->convergenceTransferPrice(m_sourceObjectUpgradeTier)
                                        : m_storage->transferPrice(*classification, m_sourceObjectUpgradeTier);
}

ushort TExaltationForgeTransferPageController::transferCoreCost() const
{
    if (m_sourceObjectTypeId == 0 || m_targetObjectUpgradeTier == 0)
        return 0;
    return m_storage->transferCoreCost(m_targetObjectUpgradeTier);
}

ushort TExaltationForgeTransferPageController::sourceObjectTypeId() const
{
    return m_sourceObjectTypeId;
}

uchar TExaltationForgeTransferPageController::sourceObjectUpgradeTier() const
{
    return m_sourceObjectUpgradeTier;
}

uint TExaltationForgeTransferPageController::sourceObjectCount() const
{
    return m_sourceCounts->count(m_sourceObjectTypeId, m_sourceObjectUpgradeTier);
}

QString TExaltationForgeTransferPageController::sourceObjectName() const
{
    if (m_sourceObjectTypeId != 0) {
        if (const TAppearanceTypePtr& type = tibia::game::thingType(m_sourceObjectTypeId, ThingCategoryItem); type && type->getId() != 0)
            return QString::fromStdString(type->getName());
    }
    return qtTrId("dummy_unknown");
}

ushort TExaltationForgeTransferPageController::targetObjectTypeId() const
{
    return m_targetObjectTypeId;
}

uchar TExaltationForgeTransferPageController::targetObjectUpgradeTier() const
{
    return m_targetObjectUpgradeTier;
}

bool TExaltationForgeTransferPageController::convergenceTransferEnabled() const
{
    return m_convergenceTransferEnabled;
}

void TExaltationForgeTransferPageController::setConvergenceTransferEnabled(const bool v)
{
    if (v == m_convergenceTransferEnabled)
        return;
    m_convergenceTransferEnabled = v;
    onTransferGroupsChanged();
    Q_EMIT convergenceTransferEnabledChanged();
    Q_EMIT sourceObjectChanged();
    Q_EMIT targetObjectChanged();
}

void TExaltationForgeTransferPageController::showDialogTab()
{
    onTransferGroupsChanged();
}

void TExaltationForgeTransferPageController::hideDialogTab()
{
    reset();
}

void TExaltationForgeTransferPageController::prepareOpenDialog()
{
    m_convergenceTransferEnabled = false;
}

/// CIP : the receivers of the groups the source may give to.
void TExaltationForgeTransferPageController::sourceSelected(const int TypeID, const int UpgradeTier)
{
    if (TypeID == m_sourceObjectTypeId && UpgradeTier == m_sourceObjectUpgradeTier)
        return;
    m_sourceObjectTypeId = static_cast<ushort>(TypeID);
    m_sourceObjectUpgradeTier = static_cast<uchar>(UpgradeTier);
    m_targetObjectUpgradeTier = static_cast<uchar>(UpgradeTier + (m_convergenceTransferEnabled ? 1 : 0) - 1);
    Q_EMIT sourceObjectChanged();
    m_targetProxy->setExcludedTypes(m_sourceObjectTypeId == 0 ? std::set<uint16_t>{} : std::set<uint16_t>{ m_sourceObjectTypeId });

    const auto& groups = m_convergenceTransferEnabled ? m_storage->convergenceTransferGroups() : m_storage->transferGroups();
    std::map<ObjectKey, uint32_t> counts;
    std::vector<ObjectKey> objects;
    for (const exaltation::TTransferGroup& group : groups) {
        const bool donates = std::ranges::any_of(group.donors, [this](const exaltation::TExaltationObject& donor) {
            return donor.typeId == m_sourceObjectTypeId && donor.tier == m_sourceObjectUpgradeTier;
        });
        if (!donates)
            continue;
        for (const exaltation::TExaltationObject& receiver : group.receivers) {
            counts[{ receiver.typeId, receiver.tier }] = receiver.count;
            if (classificationOf(receiver.typeId))
                objects.emplace_back(receiver.typeId, 0);
        }
    }
    m_targetObjects->setObjects(std::move(objects));
    m_targetCounts->setCounts(std::move(counts));
    if (m_targetObjectTypeId != 0) {
        m_targetObjectTypeId = 0;
        Q_EMIT targetObjectChanged();
    }
}

void TExaltationForgeTransferPageController::targetSelected(const int TypeID)
{
    if (TypeID == m_targetObjectTypeId)
        return;
    m_targetObjectTypeId = static_cast<ushort>(TypeID);
    Q_EMIT targetObjectChanged();
}

/// CIP .
void TExaltationForgeTransferPageController::requestTransfer()
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionExaltationForgeTransfer>(
        static_cast<uint16_t>(m_sourceObjectTypeId), static_cast<uint8_t>(m_sourceObjectUpgradeTier),
        static_cast<uint16_t>(m_targetObjectTypeId), m_convergenceTransferEnabled));
}

/// CIP : every donor of every group, counted.
void TExaltationForgeTransferPageController::onTransferGroupsChanged()
{
    reset();
    const auto& groups = m_convergenceTransferEnabled ? m_storage->convergenceTransferGroups() : m_storage->transferGroups();
    std::map<ObjectKey, uint32_t> counts;
    std::vector<ObjectKey> objects;
    for (const exaltation::TTransferGroup& group : groups) {
        for (const exaltation::TExaltationObject& donor : group.donors) {
            counts[{ donor.typeId, donor.tier }] = donor.count;
            if (classificationOf(donor.typeId))
                objects.emplace_back(donor.typeId, donor.tier);
        }
    }
    m_sourceObjects->setObjects(std::move(objects));
    m_sourceCounts->setCounts(std::move(counts));
}

/// CIP .
void TExaltationForgeTransferPageController::reset()
{
    m_sourceObjectTypeId = 0;
    m_sourceObjectUpgradeTier = 0;
    m_targetObjectTypeId = 0;
    m_targetObjectUpgradeTier = 0;
    m_sourceObjects->setObjects({});
    m_targetObjects->setObjects({});
    m_targetProxy->setExcludedTypes({});
    m_sourceCounts->clear();
    m_targetCounts->clear();
    Q_EMIT sourceObjectChanged();
    Q_EMIT targetObjectChanged();
}

// ---------------------------------------------------------------------------
// Conversion

TExaltationForgeConversionPageController::TExaltationForgeConversionPageController(QObject* parent)
    : cip::TExaltationForgeConversionPageController(parent)
{
    m_convertDustToSliversCost = 60;
    m_convertDustToSliversResult = 3;
    m_convertSliversToCoresCost = 50;
    m_convertSliversToCoresResult = 1;
}

/// The session's exaltation storage's dust limit.
void TExaltationForgeConversionPageController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    if (m_storage)
        disconnect(m_storage, nullptr, this, nullptr);
    m_gameSession = GameSession;
    m_storage = exaltationStorageOf(m_gameSession);
    if (m_storage)
        connect(m_storage, &exaltation::TExaltationStorage::currentDustLimitChanged, this,
                &TExaltationForgeConversionPageController::increaseDustLimitValueChanged);
}

int TExaltationForgeConversionPageController::convertDustToSliversCost() const
{
    return m_convertDustToSliversCost;
}

int TExaltationForgeConversionPageController::convertDustToSliversResult() const
{
    return m_convertDustToSliversResult;
}

int TExaltationForgeConversionPageController::convertSliversToCoresCost() const
{
    return m_convertSliversToCoresCost;
}

int TExaltationForgeConversionPageController::convertSliversToCoresResult() const
{
    return m_convertSliversToCoresResult;
}

int TExaltationForgeConversionPageController::increaseDustLimitCost() const
{
    const int raw = m_storage->dustLimitRaw();
    return raw == 225 ? 0x7fffffff : std::min(raw, 225) + 25;
}

int TExaltationForgeConversionPageController::currentDustLimit() const
{
    return m_storage->currentDustLimit();
}

int TExaltationForgeConversionPageController::increaseDustLimitResult() const
{
    return (std::min<int>(m_storage->dustLimitRaw(), 224) + 1) * 20 + 100;
}

bool TExaltationForgeConversionPageController::maxDustLimitReached() const
{
    return m_storage->dustLimitRaw() > 224;
}

void TExaltationForgeConversionPageController::convertDust()
{
    tibia::game::forgeRequest(tibia::client::ForgeAction_t::DUST2SLIVER);
}

void TExaltationForgeConversionPageController::convertSlivers()
{
    tibia::game::forgeRequest(tibia::client::ForgeAction_t::SLIVER2CORE);
}

void TExaltationForgeConversionPageController::increaseDustLimit()
{
    tibia::game::forgeRequest(tibia::client::ForgeAction_t::INCREASELIMIT);
}

void TExaltationForgeConversionPageController::showDialogTab()
{
    Q_EMIT increaseDustLimitValueChanged();
}

void TExaltationForgeConversionPageController::hideDialogTab()
{
}

// ---------------------------------------------------------------------------
// History

TExaltationForgeHistoryPageController::TExaltationForgeHistoryPageController(QObject* parent)
    : cip::TExaltationForgeHistoryPageController(parent)
    , m_history(new exaltation::TExaltationHistoryListModel(this))
{
    m_exaltationHistoryModel = QVariant::fromValue<QObject*>(m_history);
}

/// The session's exaltation storage's history.
void TExaltationForgeHistoryPageController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    if (m_storage)
        disconnect(m_storage, nullptr, this, nullptr);
    m_gameSession = GameSession;
    m_storage = exaltationStorageOf(m_gameSession);
    if (m_storage)
        connect(m_storage, &exaltation::TExaltationStorage::historyChanged, this, &TExaltationForgeHistoryPageController::onHistoryChanged);
}

QVariant TExaltationForgeHistoryPageController::exaltationHistoryModel() const
{
    return m_exaltationHistoryModel;
}

ushort TExaltationForgeHistoryPageController::currentPageId() const
{
    return m_storage->currentPage();
}

ushort TExaltationForgeHistoryPageController::numberOfPages() const
{
    return m_storage->numberOfPages();
}

void TExaltationForgeHistoryPageController::showDialogTab()
{
    Q_EMIT publishGameAction(simpleAction(A::REQUEST_EXALTATION_FORGE_HISTORY_FIRST_PAGE));
    onHistoryChanged();
}

void TExaltationForgeHistoryPageController::hideDialogTab()
{
    m_history->clear();
}

void TExaltationForgeHistoryPageController::requestPreviousPage()
{
    Q_EMIT publishGameAction(simpleAction(A::REQUEST_EXALTATION_FORGE_HISTORY_PREVIOUS_PAGE));
}

void TExaltationForgeHistoryPageController::requestNextPage()
{
    Q_EMIT publishGameAction(simpleAction(A::REQUEST_EXALTATION_FORGE_HISTORY_NEXT_PAGE));
}

/// Newest first.
void TExaltationForgeHistoryPageController::onHistoryChanged()
{
    std::vector<exaltation::THistoryEntry> entries = m_storage->history();
    std::ranges::stable_sort(entries, std::greater<>{}, &exaltation::THistoryEntry::timestamp);
    m_history->setItems(entries);
    Q_EMIT historyChanged();
}

// ---------------------------------------------------------------------------
// Result dialog

TExaltationResultDialogController::TExaltationResultDialogController(QObject* parent)
    : TDialogController(QStringLiteral("ExaltationResultDialog.qml"), parent)
{
}

void TExaltationResultDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSession = GameSession;
    m_storage = exaltationStorageOf(m_gameSession);
}

bool TExaltationResultDialogController::smoothTextureFiltering() const { return gamewindow::smoothFilteringEnabled(); }

uchar TExaltationResultDialogController::dustCost() const
{
    return 100;
}

/// The fusion price of the input object's classification and tier.
qulonglong TExaltationResultDialogController::goldCost() const
{
    const std::optional<int> classification = classificationOf(m_inputObjectTypeId);
    return classification && m_storage ? m_storage->fusionPrice(*classification, m_inputObjectUpgradeTier) : 0;
}

void TExaltationResultDialogController::showDialog(const exaltation::TExaltationResult& result)
{
    dialog()->close();
    m_isFusion = result.isFusion;
    m_isConvergence = result.isConvergence;
    m_wasSuccessful = result.wasSuccessful;
    m_inputObjectTypeId = result.inputTypeId;
    m_inputObjectUpgradeTier = result.inputTier;
    m_resultObjectTypeId = result.resultTypeId;
    m_resultObjectUpgradeTier = result.resultTier;
    m_fusionBonus = static_cast<cip::tibia::qmlcomponents::enums::EFusionBonusEffect>(result.fusionBonus);
    m_coreCount = result.coreCount;
    m_bonusObjectTypeId = result.bonusTypeId;
    m_bonusObjectUpgradeTier = result.bonusTier;
    if (!dialog()->item()) {
        dialog()->create();
        dialog()->show();
    }
}

void TExaltationResultDialogController::requestClose()
{
    dialog()->close();
    m_isFusion = false;
    m_isConvergence = false;
    m_wasSuccessful = false;
    m_inputObjectTypeId = 0;
    m_inputObjectUpgradeTier = 0;
    m_resultObjectTypeId = 0;
    m_resultObjectUpgradeTier = 0;
    m_fusionBonus = {};
    m_coreCount = 0;
    m_bonusObjectTypeId = 0;
    m_bonusObjectUpgradeTier = 0;
}

// ---------------------------------------------------------------------------
// Forge dialog

bool TExaltationForgeDialogController::smoothTextureFiltering() const { return gamewindow::smoothFilteringEnabled(); }

TExaltationForgeDialogController::TExaltationForgeDialogController(QObject* parent)
    : TDialogController(QStringLiteral("ExaltationForgeDialog.qml"), parent)
    , m_resultDialog(new TExaltationResultDialogController(this))
    , m_errorDialog(new TMessageDialogController(this))
{
    auto* fusion = new TExaltationForgeFusionPageController(this);
    auto* transfer = new TExaltationForgeTransferPageController(this);
    m_fusionController = fusion;
    m_transferController = transfer;
    m_conversionController = new TExaltationForgeConversionPageController(this);
    auto* history = new TExaltationForgeHistoryPageController(this);
    m_historyController = history;
    connect(fusion, &TExaltationForgeFusionPageController::publishGameAction, this, &TExaltationForgeDialogController::publishGameAction);
    connect(transfer, &TExaltationForgeTransferPageController::publishGameAction, this, &TExaltationForgeDialogController::publishGameAction);
    connect(history, &TExaltationForgeHistoryPageController::publishGameAction, this,
            &TExaltationForgeDialogController::publishGameAction);
    m_readOnlyMode = true;
    m_errorDialog->setCloseOnButtonClicked(true);
    connect(this, &TExaltationForgeDialogController::prepareOpenDialog, fusion, &TExaltationForgeFusionPageController::prepareOpenDialog);
    connect(this, &TExaltationForgeDialogController::prepareOpenDialog, transfer, &TExaltationForgeTransferPageController::prepareOpenDialog);

    for (const A type : { A::REQUEST_OPEN_EXALTATION_FORGE_DIALOG, A::CLOSE_EXALTATION_FORGE_DIALOG, A::OPEN_OR_UPDATE_EXALTATION_FORGE_DIALOG,
                          A::SHOW_EXALTATION_RESULT_DIALOG, A::SHOW_EXALTATION_ERROR_DIALOG })
        m_gameActions.on(type, [this](const input::IGameAction& action) { return handleGameAction(action); });
}

/// Closes the forge; its pages and the result dialog follow the new session.
void TExaltationForgeDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    closeDialog();
    m_gameSession = GameSession;
    m_storage = exaltationStorageOf(m_gameSession);
    qobject_cast<TExaltationForgeFusionPageController*>(m_fusionController)->connectToGameSession(m_gameSession);
    qobject_cast<TExaltationForgeTransferPageController*>(m_transferController)->connectToGameSession(m_gameSession);
    qobject_cast<TExaltationForgeConversionPageController*>(m_conversionController)->connectToGameSession(m_gameSession);
    qobject_cast<TExaltationForgeHistoryPageController*>(m_historyController)->connectToGameSession(m_gameSession);
    m_resultDialog->connectToGameSession(m_gameSession);
}

/// The result travels in the storage instead of the action.
bool TExaltationForgeDialogController::handleGameAction(const input::IGameAction& action)
{
    switch (action.gameActionType()) {
    case A::REQUEST_OPEN_EXALTATION_FORGE_DIALOG:
        showDialog(true);
        break;
    case A::CLOSE_EXALTATION_FORGE_DIALOG:
        closeDialog();
        break;
    case A::OPEN_OR_UPDATE_EXALTATION_FORGE_DIALOG:
        if (dialog()->item())
            break;
        closeDialog();
        m_readOnlyMode = false;
        if (!dialog()->item()) {
            dialog()->create();
            dialog()->show();
        }
        Q_EMIT prepareOpenDialog();
        setTab(Fusion);
        break;
    case A::SHOW_EXALTATION_RESULT_DIALOG:
        m_resultDialog->showDialog(m_storage->lastResult());
        break;
    case A::SHOW_EXALTATION_ERROR_DIALOG:
        m_errorDialog->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK, QKeySequence(QStringLiteral("Return"))) });
        if (const auto* message = input::gameActionAs<input::TGameActionShowMessageDialog>(&action))
            m_errorDialog->showMessage(qtTrId("exaltation_forge_info_dialog_caption"), message->text());
        break;
    default:
        return false;
    }
    return true;
}

TExaltationForgeDialogController::~TExaltationForgeDialogController()
{
}

/// Closes and tells the storage (EXALTATION_FORGE_DIALOG_CLOSED).
void TExaltationForgeDialogController::requestClose()
{
    closeDialog();
    Q_EMIT publishGameAction(simpleAction(A::EXALTATION_FORGE_DIALOG_CLOSED));
}

void TExaltationForgeDialogController::requestTabSwitch(const EDialogTab NewTab)
{
    if (NewTab != m_dialogTab)
        setTab(NewTab);
}

/// Read-only (the button bar) asks for the balances and starts on the
/// conversion page.
void TExaltationForgeDialogController::showDialog(const bool ReadOnly)
{
    closeDialog();
    m_readOnlyMode = ReadOnly;
    if (ReadOnly) {
        for (const int resource : { 0, 1, 0x46, 0x47, 0x48 }) {
            auto action = std::make_shared<input::TGameActionRequestResourceBalance>(resource);
            Q_EMIT publishGameAction(action);
        }
    }
    if (!dialog()->item()) {
        dialog()->create();
        dialog()->show();
    }
    Q_EMIT prepareOpenDialog();
    setTab(ReadOnly ? Conversion : Fusion);
}

void TExaltationForgeDialogController::closeDialog()
{
    m_errorDialog->dialog()->close();
    if (auto* current = page(m_dialogTab))
        current->hideDialogTab();
    m_dialogTab = NoTabInitialState;
    Q_EMIT dialogTabChanged();
    m_resultDialog->requestClose();
    dialog()->close();
}

cip::tibia::gamewindow::IDialogTabPage* TExaltationForgeDialogController::page(const EDialogTab tab) const
{
    switch (tab) {
    case Fusion: return static_cast<cip::tibia::gamewindow::IDialogTabPage*>(m_fusionController);
    case Transfer: return static_cast<cip::tibia::gamewindow::IDialogTabPage*>(m_transferController);
    case Conversion: return static_cast<cip::tibia::gamewindow::IDialogTabPage*>(m_conversionController);
    case History: return static_cast<cip::tibia::gamewindow::IDialogTabPage*>(m_historyController);
    default: return nullptr;
    }
}

/// Hides the current page, shows the new one.
void TExaltationForgeDialogController::setTab(const EDialogTab tab)
{
    if (auto* current = page(m_dialogTab))
        current->hideDialogTab();
    m_dialogTab = tab;
    if (auto* next = page(tab))
        next->showDialogTab();
    Q_EMIT dialogTabChanged();
}

}
