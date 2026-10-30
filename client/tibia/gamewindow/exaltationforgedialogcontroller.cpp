#include "exaltationforgedialogcontroller.h"

#include "app/clientoptions.h"
#include "container/containerslotinformation.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QColor>
#include <QDateTime>
#include <QJsonObject>

#include <algorithm>
#include <functional>
#include <optional>

namespace tibia::exaltation {

namespace {

using Info = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;

/// CIP 0x100941070: the type's upgrade classification, if it has one.
std::optional<int> classificationOf(const uint16_t typeId)
{
    if (typeId == 0)
        return std::nullopt;
    const ThingTypePtr& type = g_things.getThingType(typeId, ThingCategoryItem);
    if (!type || type->getClassification() == 0)
        return std::nullopt;
    return type->getClassification();
}

}

void TObjectCounts::set(std::map<std::pair<uint16_t, uint8_t>, uint32_t> counts)
{
    m_counts = std::move(counts);
    Q_EMIT countsChanged();
}

void TObjectCounts::clear()
{
    if (m_counts.empty())
        return;
    m_counts.clear();
    Q_EMIT countsChanged();
}

uint32_t TObjectCounts::count(const uint16_t typeId, const uint8_t tier) const
{
    const auto it = m_counts.find({ typeId, tier });
    return it == m_counts.end() ? 0 : it->second;
}

TForgeObjectListModel::TForgeObjectListModel(TObjectCounts* counts, QObject* parent)
    : cip::tibia::gamewindow::TAppearanceInstanceListModel(parent)
    , m_counts(counts)
{
    connect(m_counts, &TObjectCounts::countsChanged, this, &TForgeObjectListModel::onObjectCountsChanged);
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

int TExaltationHistoryListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TExaltationHistoryListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
        return {};
    const THistoryEntry& entry = m_entries[index.row()];
    switch (role) {
    case DateStringRole:
        // CIP TTextHelperQmlService::formatDateTime 0x10095c790, with seconds.
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
    return {
        { DateStringRole, "dateString" },
        { ActionStringRole, "actionString" },
        { DetailsStringRole, "detailsString" },
    };
}

void TExaltationHistoryListModel::setEntries(std::vector<THistoryEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

}

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using exaltation::classificationOf;
using ObjectKey = std::pair<uint16_t, uint8_t>;

void publish(const A type)
{
    input::publish(input::makeAction(type));
}

/// Option antialiasingMode is AntialiasingModeAntialiasing (CIP 0x100dc6bb0,
/// 0x100dd0560).
bool smoothTextureFilteringOption()
{
    return app::ClientOptions::load().options().value(QLatin1String("antialiasingMode")).toInt()
           == static_cast<int>(cip::tibia::qmlcomponents::enums::EAntialiasingMode::AntialiasingModeAntialiasing);
}

}

// ---------------------------------------------------------------------------
// Fusion

TExaltationForgeFusionPageController::TExaltationForgeFusionPageController(exaltation::TExaltationStorage* storage, QObject* parent)
    : cip::TExaltationForgeFusionPageController(parent)
    , m_storage(storage)
    , m_counts(new exaltation::TObjectCounts(this))
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
    connect(m_storage, &exaltation::TExaltationStorage::fusionGroupsChanged, this,
            &TExaltationForgeFusionPageController::onFusionGroupsChanged);
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

uint TExaltationForgeFusionPageController::resourceObjectCount() const
{
    return m_counts->count(m_resourceObjectTypeId, m_fusionObjectUpgradeTier);
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

/// CIP 0x100dc7c60.
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

/// CIP 0x100dc7e00.
void TExaltationForgeFusionPageController::requestFusion(const bool SuccessChanceIncreased, const bool TierLossImproved)
{
    auto action = input::mutableClone(*input::makeAction(A::EXALTATION_FORGE_FUSION));
    action->exaltationTypeId = m_fusionObjectTypeId;
    action->exaltationTier = m_fusionObjectUpgradeTier;
    action->exaltationSecondTypeId = m_resourceObjectTypeId;
    action->exaltationSuccessIncreased = SuccessChanceIncreased;
    action->exaltationTierLossImproved = TierLossImproved;
    input::publish(action);
    m_fusionObjectTypeId = 0;
    m_fusionObjectUpgradeTier = 0;
    m_resourceObjectTypeId = 0;
    Q_EMIT fusionObjectChanged();
    Q_EMIT resourceObjectChanged();
}

/// CIP 0x100dc7f90.
void TExaltationForgeFusionPageController::requestConvergenceFusion()
{
    auto action = input::mutableClone(*input::makeAction(A::EXALTATION_FORGE_CONVERGENCE_FUSION));
    action->exaltationTypeId = m_fusionObjectTypeId;
    action->exaltationTier = m_fusionObjectUpgradeTier;
    action->exaltationSecondTypeId = m_resourceObjectTypeId;
    input::publish(action);
    m_fusionObjectTypeId = 0;
    m_fusionObjectUpgradeTier = 0;
    m_resourceObjectTypeId = 0;
    Q_EMIT fusionObjectChanged();
    Q_EMIT resourceObjectChanged();
}

/// CIP 0x100dc8110: every object of every group, counted.
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
    m_counts->set(std::move(counts));
}

/// CIP 0x100dc8740: the other objects of the selected object's groups (a
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

/// CIP 0x100dc8ed0.
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

TExaltationForgeTransferPageController::TExaltationForgeTransferPageController(exaltation::TExaltationStorage* storage, QObject* parent)
    : cip::TExaltationForgeTransferPageController(parent)
    , m_storage(storage)
    , m_sourceCounts(new exaltation::TObjectCounts(this))
    , m_targetCounts(new exaltation::TObjectCounts(this))
    , m_sourceObjects(new exaltation::TForgeObjectListModel(m_sourceCounts, this))
    , m_targetObjects(new exaltation::TForgeObjectListModel(m_targetCounts, this))
    , m_sourceProxy(new exaltation::TUpgradeableObjectModel(m_sourceObjects, this))
    , m_targetProxy(new exaltation::TUpgradeableObjectModel(m_targetObjects, this))
{
    m_sourceObjectsModel = QVariant::fromValue<QObject*>(m_sourceProxy);
    m_targetObjectsModel = QVariant::fromValue<QObject*>(m_targetProxy);
    connect(m_storage, &exaltation::TExaltationStorage::transferGroupsChanged, this,
            &TExaltationForgeTransferPageController::onTransferGroupsChanged);
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

uint TExaltationForgeTransferPageController::sourceObjectCount() const
{
    return m_sourceCounts->count(m_sourceObjectTypeId, m_sourceObjectUpgradeTier);
}

QString TExaltationForgeTransferPageController::sourceObjectName() const
{
    if (m_sourceObjectTypeId != 0) {
        if (const ThingTypePtr& type = g_things.getThingType(m_sourceObjectTypeId, ThingCategoryItem); type && type->getId() != 0)
            return QString::fromStdString(type->getName());
    }
    return qtTrId("dummy_unknown");
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

/// CIP 0x100dc9a30: the receivers of the groups the source may give to.
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
    m_targetCounts->set(std::move(counts));
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

/// CIP 0x100dca000.
void TExaltationForgeTransferPageController::requestTransfer()
{
    auto action = input::mutableClone(*input::makeAction(A::EXALTATION_FORGE_TRANSFER));
    action->exaltationTypeId = m_sourceObjectTypeId;
    action->exaltationTier = m_sourceObjectUpgradeTier;
    action->exaltationSecondTypeId = m_targetObjectTypeId;
    action->exaltationConvergence = m_convergenceTransferEnabled;
    input::publish(action);
}

/// CIP 0x100dca150: every donor of every group, counted.
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
    m_sourceCounts->set(std::move(counts));
}

/// CIP 0x100dca9f0.
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

TExaltationForgeConversionPageController::TExaltationForgeConversionPageController(exaltation::TExaltationStorage* storage, QObject* parent)
    : cip::TExaltationForgeConversionPageController(parent)
    , m_storage(storage)
{
    m_convertDustToSliversCost = 60;
    m_convertDustToSliversResult = 3;
    m_convertSliversToCoresCost = 50;
    m_convertSliversToCoresResult = 1;
    connect(m_storage, &exaltation::TExaltationStorage::currentDustLimitChanged, this,
            &TExaltationForgeConversionPageController::increaseDustLimitValueChanged);
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
    g_game.forgeRequest(Otc::ForgeAction_t::DUST2SLIVER);
}

void TExaltationForgeConversionPageController::convertSlivers()
{
    g_game.forgeRequest(Otc::ForgeAction_t::SLIVER2CORE);
}

void TExaltationForgeConversionPageController::increaseDustLimit()
{
    g_game.forgeRequest(Otc::ForgeAction_t::INCREASELIMIT);
}

// ---------------------------------------------------------------------------
// History

TExaltationForgeHistoryPageController::TExaltationForgeHistoryPageController(exaltation::TExaltationStorage* storage, QObject* parent)
    : cip::TExaltationForgeHistoryPageController(parent)
    , m_storage(storage)
    , m_history(new exaltation::TExaltationHistoryListModel(this))
{
    m_exaltationHistoryModel = QVariant::fromValue<QObject*>(m_history);
    connect(m_storage, &exaltation::TExaltationStorage::historyChanged, this, &TExaltationForgeHistoryPageController::onHistoryChanged);
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
    publish(A::REQUEST_EXALTATION_FORGE_HISTORY_FIRST_PAGE);
    onHistoryChanged();
}

void TExaltationForgeHistoryPageController::hideDialogTab()
{
    m_history->setEntries({});
}

void TExaltationForgeHistoryPageController::requestPreviousPage()
{
    publish(A::REQUEST_EXALTATION_FORGE_HISTORY_PREVIOUS_PAGE);
}

void TExaltationForgeHistoryPageController::requestNextPage()
{
    publish(A::REQUEST_EXALTATION_FORGE_HISTORY_NEXT_PAGE);
}

/// Newest first.
void TExaltationForgeHistoryPageController::onHistoryChanged()
{
    std::vector<exaltation::THistoryEntry> entries = m_storage->history();
    std::ranges::stable_sort(entries, std::greater<>{}, &exaltation::THistoryEntry::timestamp);
    m_history->setEntries(std::move(entries));
    Q_EMIT historyChanged();
}

// ---------------------------------------------------------------------------
// Result dialog

TExaltationResultDialogController::TExaltationResultDialogController(exaltation::TExaltationStorage* storage, QObject* parent)
    : TDialogController(QStringLiteral("ExaltationResultDialog.qml"), parent)
    , m_storage(storage)
{
    m_smoothTextureFiltering = smoothTextureFilteringOption();
}

uchar TExaltationResultDialogController::dustCost() const
{
    return 100;
}

/// CIP 0x100dd05b0.
qulonglong TExaltationResultDialogController::goldCost() const
{
    const std::optional<int> classification = classificationOf(m_inputObjectTypeId);
    return classification ? m_storage->fusionPrice(*classification, m_inputObjectUpgradeTier) : 0;
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

TExaltationForgeDialogController::TExaltationForgeDialogController(exaltation::TExaltationStorage* storage, QObject* parent)
    : TDialogController(QStringLiteral("ExaltationForgeDialog.qml"), parent)
    , m_storage(storage)
    , m_resultDialog(new TExaltationResultDialogController(storage, this))
    , m_errorDialog(new TMessageDialogController(this))
{
    auto* fusion = new TExaltationForgeFusionPageController(storage, this);
    auto* transfer = new TExaltationForgeTransferPageController(storage, this);
    m_fusionController = fusion;
    m_transferController = transfer;
    m_conversionController = new TExaltationForgeConversionPageController(storage, this);
    m_historyController = new TExaltationForgeHistoryPageController(storage, this);
    m_smoothTextureFiltering = smoothTextureFilteringOption();
    m_errorDialog->setCloseOnButtonClicked(true);
    connect(this, &TExaltationForgeDialogController::prepareOpenDialog, fusion, &TExaltationForgeFusionPageController::prepareOpenDialog);
    connect(this, &TExaltationForgeDialogController::prepareOpenDialog, transfer, &TExaltationForgeTransferPageController::prepareOpenDialog);

    input::setActionHandler(A::REQUEST_OPEN_EXALTATION_FORGE_DIALOG, [this](const input::TGameAction&) {
        showDialog(true);
        return true;
    });
    input::setActionHandler(A::CLOSE_EXALTATION_FORGE_DIALOG, [this](const input::TGameAction&) {
        closeDialog();
        return true;
    });
    input::setActionHandler(A::OPEN_OR_UPDATE_EXALTATION_FORGE_DIALOG, [this](const input::TGameAction&) {
        if (dialog()->item())
            return true;
        closeDialog();
        m_readOnlyMode = false;
        if (!dialog()->item()) {
            dialog()->create();
            dialog()->show();
        }
        Q_EMIT prepareOpenDialog();
        setTab(Fusion);
        return true;
    });
    input::setActionHandler(A::SHOW_EXALTATION_RESULT_DIALOG, [this](const input::TGameAction&) {
        m_resultDialog->showDialog(m_storage->lastResult());
        return true;
    });
    input::setActionHandler(A::SHOW_EXALTATION_ERROR_DIALOG, [this](const input::TGameAction& action) {
        m_errorDialog->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK, QKeySequence(QStringLiteral("Return"))) });
        m_errorDialog->showMessage(qtTrId("exaltation_forge_info_dialog_caption"), action.text);
        return true;
    });
}

TExaltationForgeDialogController::~TExaltationForgeDialogController()
{
    for (const A type : { A::REQUEST_OPEN_EXALTATION_FORGE_DIALOG, A::CLOSE_EXALTATION_FORGE_DIALOG, A::OPEN_OR_UPDATE_EXALTATION_FORGE_DIALOG,
                          A::SHOW_EXALTATION_RESULT_DIALOG, A::SHOW_EXALTATION_ERROR_DIALOG })
        input::setActionHandler(type, nullptr);
}

/// CIP 0x100dc6be0.
void TExaltationForgeDialogController::requestClose()
{
    closeDialog();
    publish(A::EXALTATION_FORGE_DIALOG_CLOSED);
}

/// CIP 0x100dc6cd0.
void TExaltationForgeDialogController::requestTabSwitch(const EDialogTab NewTab)
{
    if (NewTab != m_dialogTab)
        setTab(NewTab);
}

/// CIP 0x100dc6e40: read-only (the button bar) asks for the balances and
/// starts on the conversion page.
void TExaltationForgeDialogController::showDialog(const bool ReadOnly)
{
    closeDialog();
    m_readOnlyMode = ReadOnly;
    if (ReadOnly) {
        for (const int resource : { 0, 1, 0x46, 0x47, 0x48 }) {
            auto action = input::mutableClone(*input::makeAction(A::REQUEST_RESOURCE_BALANCE));
            action->parameter = resource;
            input::publish(action);
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

/// CIP 0x100dc6cf0.
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
