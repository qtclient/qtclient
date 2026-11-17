#include "imbuingdialogcontroller.h"
#include "gamewindow/smoothfiltering.h"

#include "container/containerslotinformation.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameactionhandlers.h"

#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QLocale>

#include <algorithm>
#include <array>
#include <utility>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::imbuing {

namespace {

enum ERole
{
    ImbuementIdRole = Qt::UserRole + 1,
    ImageSourceRole,
    CategoryRole,
};

enum ESlotRole
{
    SlotIndexRole = Qt::UserRole + 1,
    SlotImbuementIdRole,
    SlotImageSourceRole,
    SlotCategoryRole,
};

/// The imbuement icon from the image provider.
QString imageSource(const uint32_t iconId)
{
    return QStringLiteral("image://%1/%2").arg(QStringLiteral("imbuement-icons")).arg(iconId);
}

/// The tier's name; empty for no imbuement or an unknown tier.
QString categoryName(const TImbuement& imbuement)
{
    if (imbuement.id == 0)
        return {};
    switch (imbuement.tier) {
        case 0: return qtTrId("imbuing_category_basic");
        case 1: return qtTrId("imbuing_category_intricate");
        case 2: return qtTrId("imbuing_category_powerful");
        default: return {};
    }
}

/// Minutes as "Xmin" / "Xh Ymin" up to a day, then days, hours and minutes.
QString durationText(const qint64 minutes)
{
    if (minutes <= 1440) {
        if (minutes < 60)
            return qtTrId("time_minutes_short").arg(minutes % 60);
        return qtTrId("time_hours_and_minutes_short").arg(minutes / 60).arg(minutes % 60);
    }
    return qtTrId("time_days_hours_and_minutes_short").arg(minutes / 1440).arg((minutes % 1440) / 60).arg(minutes % 60);
}

QString goldText(const qint64 gold)
{
    return QLocale(QLocale::English).toString(gold);
}

}

/// Resizes with row inserts or removals, then reports every row changed.
void TAvailableImbuementsListModel::setImbuements(std::vector<TImbuement> imbuements)
{
    const int previous = rowCount();
    const int count = static_cast<int>(imbuements.size());
    if (count < previous)
        beginRemoveRows({}, count, previous - 1);
    else if (count > previous)
        beginInsertRows({}, previous, count - 1);
    m_imbuements = std::move(imbuements);
    if (count < previous)
        endRemoveRows();
    else if (count > previous)
        endInsertRows();
    if (count > 0)
        Q_EMIT dataChanged(index(0, 0), index(count - 1, 0));
}

int TAvailableImbuementsListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_imbuements.size());
}

QVariant TAvailableImbuementsListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
        return {};
    const TImbuement& imbuement = m_imbuements[index.row()];
    switch (role) {
        case ImbuementIdRole: return static_cast<uint>(imbuement.id);
        case ImageSourceRole: return imageSource(imbuement.iconId);
        case CategoryRole: return categoryName(imbuement);
        default: return {};
    }
}

QHash<int, QByteArray> TAvailableImbuementsListModel::roleNames() const
{
    return { { ImbuementIdRole, "imbuementId" }, { ImageSourceRole, "imageSource" }, { CategoryRole, "category" } };
}

void TImbuementSlotsListModel::setSlots(std::vector<TImbuementSlot> slots)
{
    const int previous = rowCount();
    const int count = static_cast<int>(slots.size());
    if (count < previous)
        beginRemoveRows({}, count, previous - 1);
    else if (count > previous)
        beginInsertRows({}, previous, count - 1);
    m_slots = std::move(slots);
    if (count < previous)
        endRemoveRows();
    else if (count > previous)
        endInsertRows();
    if (count > 0)
        Q_EMIT dataChanged(index(0, 0), index(count - 1, 0));
}

int TImbuementSlotsListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_slots.size());
}

QVariant TImbuementSlotsListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
        return {};
    const TImbuementSlot& slot = m_slots[index.row()];
    switch (role) {
        case SlotIndexRole: return static_cast<uint>(slot.index);
        case SlotImbuementIdRole: return static_cast<uint>(slot.imbuement.id);
        case SlotImageSourceRole: return imageSource(slot.imbuement.iconId);
        case SlotCategoryRole: return categoryName(slot.imbuement);
        default: return {};
    }
}

QHash<int, QByteArray> TImbuementSlotsListModel::roleNames() const
{
    return { { SlotIndexRole, "slotIndex" }, { SlotImbuementIdRole, "imbuementId" }, { SlotImageSourceRole, "imageSource" }, { SlotCategoryRole, "category" } };
}

TQmlAstralSource::TQmlAstralSource(const QString& name, const uint needed, const uint owned, const uint objectTypeId, QObject* parent)
    : cip::tibia::imbuing::TQmlAstralSource(parent)
{
    m_name = name;
    m_objectID = objectTypeId;
    // "owned/needed", nothing for a source that takes none.
    m_resourceString = needed == 0 ? QStringLiteral("") : qtTrId("imbuing_resource_count").arg(container::ammoText(owned)).arg(needed);
    m_hasEnoughToImbue = needed <= owned;
}

TQmlImbuementData::TQmlImbuementData(const TImbuement& imbuement, const QList<QObject*>& astralSources, const bool enoughAstralSources,
                                     QObject* parent)
    : cip::tibia::imbuing::TQmlImbuementData(parent)
{
    m_name = imbuement.name;
    m_description = imbuement.description;
    m_goldCost = static_cast<int>(imbuement.goldCost);
    m_goldCostString = goldText(m_goldCost);
    m_astralSources = astralSources;
    for (QObject* source : astralSources)
        source->setParent(this);
    m_enoughAstralSourcesForImbuement = enoughAstralSources;
    m_isDummyWhenNoImbuementAvailable = false;
}

TQmlExistingImbuement::TQmlExistingImbuement(const uint32_t imbuementId, const QString& name, const uint64_t remainingDuration,
                                             const uint64_t totalDuration, const uint32_t clearingGoldCost, QObject* parent)
    : cip::tibia::imbuing::TQmlExistingImbuement(parent)
{
    m_imbuementID = static_cast<int>(imbuementId);
    m_name = name;
    m_clearingGoldCost = static_cast<int>(clearingGoldCost);
    m_clearingGoldCostString = goldText(m_clearingGoldCost);
    m_remainingDurationPercentage = static_cast<double>(remainingDuration) / static_cast<double>(totalDuration);
    m_remainingDuration = durationText(static_cast<qint64>(remainingDuration) / 60);
}

}

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;
using imbuing::TImbuement;
using imbuing::TImbuementSlot;
using imbuing::TImbuementSource;
using imbuing::TImbuingStorage;

/// The blank imbuement scroll shown in scroll mode.
constexpr uint16_t BlankImbuementScrollTypeId = 51442;

/// The list order of imbuement ids 1..72; others sort as 25.
constexpr std::array<uint32_t, 72> ImbuementSortKeys = {
    11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 15, 1, 1, 1, 2, 2, 2, 3, 3, 3,
    21, 21, 21, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 20, 20, 20, 23, 23, 23, 5, 5, 5,
    4, 4, 4, 6, 6, 6, 7, 7, 7, 10, 10, 10, 8, 8, 8, 24, 24, 24, 22, 22, 22, 9, 9, 9,
};

uint32_t sortKey(const uint32_t id)
{
    return id - 1 < ImbuementSortKeys.size() ? ImbuementSortKeys[id - 1] : 25;
}

void sortImbuements(std::vector<TImbuement>& imbuements)
{
    std::stable_sort(imbuements.begin(), imbuements.end(),
                     [](const TImbuement& a, const TImbuement& b) { return sortKey(a.id) < sortKey(b.id); });
}

/// The storage's imbuements of one tier, in the server's order.
std::vector<TImbuement> imbuementsOfTier(const TImbuingStorage& storage, const uint32_t tier)
{
    std::vector<TImbuement> imbuements;
    for (const TImbuement& imbuement : storage.imbuements()) {
        if (imbuement.tier == tier)
            imbuements.push_back(imbuement);
    }
    return imbuements;
}

const TImbuement* findImbuement(const TImbuingStorage& storage, const uint32_t id)
{
    const auto& imbuements = storage.imbuements();
    const auto it = std::find_if(imbuements.begin(), imbuements.end(), [id](const TImbuement& imbuement) { return imbuement.id == id; });
    return it != imbuements.end() ? &*it : nullptr;
}

/// The owned resources the storage imbuement `id` needs, and whether there
/// is enough of each. False when the storage does not offer `id`; true for
/// an imbuement that needs nothing.
std::vector<TImbuementSource> ownedAstralSources(const TImbuingStorage& storage, const uint32_t id, bool& enough)
{
    std::vector<TImbuementSource> owned;
    const TImbuement* imbuement = findImbuement(storage, id);
    enough = imbuement != nullptr;
    if (!imbuement)
        return owned;
    for (const TImbuementSource& source : imbuement->astralSources) {
        const auto& ownedObjects = storage.ownedObjects();
        const auto it = std::find_if(ownedObjects.begin(), ownedObjects.end(),
                                     [&source](const TImbuementSource& o) { return o.objectTypeId == source.objectTypeId; });
        if (it == ownedObjects.end()) {
            enough = false;
            continue;
        }
        enough = enough && source.count <= it->count;
        owned.push_back(*it);
    }
    return owned;
}

bool localPlayerPremium()
{
    return tibia::game::hasLocalPlayer() && tibia::game::localPlayerIsPremium();
}

/// An object type's name with each space-separated word capitalised.
QString objectTypeName(const uint16_t typeId)
{
    const ThingTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
    return type ? QString::fromStdString(tibia::game::formatCreatureName(type->getName())) : QString();
}

uint32_t imbuementSlotCount(const uint16_t typeId)
{
    const ThingTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
    return type ? type->getImbueSlots() : 0;
}

void publishOpenPremiumStore()
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_STORE));
    action->parameter = 1;
    input::publish(action);
}

}

bool TImbuingDialogController::clientSettingSmoothFiltering() const { return smoothFilteringEnabled(); }

TImbuingDialogController::TImbuingDialogController(imbuing::TImbuingStorage* storage, QObject* parent)
    : TDialogController(QStringLiteral("ImbuingDialog.qml"), parent)
    , m_storage(storage)
    , m_available(new imbuing::TAvailableImbuementsListModel(this))
    , m_slotsModel(new imbuing::TImbuementSlotsListModel(this))
    , m_resultMessage(new TMessageDialogController(this))
    , m_imbueConfirm(new TMessageDialogController(this))
    , m_clearConfirm(new TMessageDialogController(this))
{
    m_availableImbuements = m_available;
    m_imbuementSlots = m_slotsModel;
    m_resultMessage->setCloseOnButtonClicked(true);
    m_imbueConfirm->setCloseOnButtonClicked(true);
    m_clearConfirm->setCloseOnButtonClicked(true);
    connect(m_imbueConfirm, &TMessageDialogController::acceptClicked, this, &TImbuingDialogController::onImbueSlotConfirmed);
    connect(m_clearConfirm, &TMessageDialogController::acceptClicked, this, &TImbuingDialogController::onClearSlotConfirmed);

    connect(m_storage, &TImbuingStorage::imbuingDataChanged, this, &TImbuingDialogController::onImbuingStorageDataChanged);
    connect(m_storage, &TImbuingStorage::requestShowOrRefreshImbuingDialog, this, &TImbuingDialogController::showDialog);
    connect(m_storage, &TImbuingStorage::requestCloseImbuingDialog, this, &TImbuingDialogController::closeDialog);
    connect(m_storage, &TImbuingStorage::requestShowResultDialog, this, &TImbuingDialogController::onDisplayResultMessage);

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPremiumChange", [this](GameObject*, const Args&) {
        Q_EMIT premiumStatusChanged();
        Q_EMIT imbuingQmlDataChanged();
        return std::any{};
    });
}

bool TImbuingDialogController::itemSelectionValid() const
{
    return m_storage->mode() == TImbuingStorage::MODE_ITEM || m_storage->mode() == TImbuingStorage::MODE_SCROLL;
}

bool TImbuingDialogController::scrollIsAvailable() const { return m_storage->scrollAvailable(); }
bool TImbuingDialogController::imbueModeItem() const { return m_storage->mode() == TImbuingStorage::MODE_ITEM; }
bool TImbuingDialogController::imbueModeScroll() const { return m_storage->mode() == TImbuingStorage::MODE_SCROLL; }
bool TImbuingDialogController::isPremium() const { return localPlayerPremium(); }

QString TImbuingDialogController::imbuementFilter() const
{
    switch (m_filter) {
        case 0: return qtTrId("imbuing_category_basic");
        case 1: return qtTrId("imbuing_category_intricate");
        case 2: return qtTrId("imbuing_category_powerful");
        default: return QStringLiteral("Unknown imbuing category");
    }
}

void TImbuingDialogController::setImbuementFilter(const QString& v)
{
    uint32_t filter = 0;
    if (v == qtTrId("imbuing_category_intricate"))
        filter = 1;
    else if (v == qtTrId("imbuing_category_powerful"))
        filter = 2;
    if (filter == m_filter)
        return;
    m_filter = filter;
    m_selectedImbuementId = 0;
    updateAvailableImbuements();
    selectImbuement(static_cast<uint32_t>(m_selectedImbuementId));
    Q_EMIT imbuingQmlDataChanged();
}

QString TImbuingDialogController::selectedItemText() const
{
    switch (m_storage->mode()) {
        case TImbuingStorage::MODE_SCROLL: return objectTypeName(BlankImbuementScrollTypeId);
        case TImbuingStorage::MODE_ITEM: return objectTypeName(m_storage->itemTypeId());
        default: return QStringLiteral("");
    }
}

void TImbuingDialogController::showDialog()
{
    if (!dialog()->item())
        dialog()->show();
}

void TImbuingDialogController::closeDialog()
{
    m_resultMessage->dialog()->close();
    m_imbueConfirm->dialog()->close();
    m_clearConfirm->dialog()->close();
    Q_EMIT imbuingQmlDataChanged();
    dialog()->close();
    tibia::game::closeImbuingWindow();
    m_imbuedItemObjectID = 0;
    m_imbuedItemObjectCount = 0;
    m_imbuedItemObjectUpgradeTier = 0;
    m_picking = false;
    m_selectedImbuementSlotIndex = 0;
    m_selectedImbuementId = 0;
    m_filter = 0;
}

void TImbuingDialogController::requestClose() { closeDialog(); }

void TImbuingDialogController::requestImbue()
{
    if (m_selectedImbuementId == 0)
        return;
    const TImbuement* imbuement = findImbuement(*m_storage, static_cast<uint32_t>(m_selectedImbuementId));
    if (!imbuement)
        return;
    m_imbueConfirm->setYesNoButtons();
    m_imbueConfirm->showMessage(qtTrId("imbuing_confirm_imbuing_title"),
                                qtTrId("imbuing_confirm_imbuing_text").arg(imbuement->name).arg(imbuing::goldText(imbuement->goldCost)));
}

void TImbuingDialogController::requestClearImbuement()
{
    const auto* existing = qobject_cast<imbuing::TQmlExistingImbuement*>(m_existingImbuement);
    const auto* selected = qobject_cast<imbuing::TQmlImbuementData*>(m_selectedImbuement);
    if (!existing || !selected)
        return;
    m_clearConfirm->setYesNoButtons();
    m_clearConfirm->showMessage(qtTrId("imbuing_confirm_clearing_title"),
                                qtTrId("imbuing_confirm_clearing_text").arg(imbuing::goldText(existing->clearingGoldCost())).arg(selected->name()));
}

void TImbuingDialogController::requestOpenStoreWithPremiumCategory() { publishOpenPremiumStore(); }

void TImbuingDialogController::openPremiumStore()
{
    publishOpenPremiumStore();
    closeDialog();
}

void TImbuingDialogController::handleSelectImbuementSlot(const uint SlotIndex)
{
    if (SlotIndex == m_selectedImbuementSlotIndex)
        return;
    m_selectedImbuementSlotIndex = SlotIndex;
    updateExistingImbuement();
    updateAvailableImbuements();
    const TImbuementSlot* slot = selectedSlot();
    selectImbuement(slot && !slot->isEmpty ? slot->imbuement.id : 0);
    Q_EMIT imbuingQmlDataChanged();
}

void TImbuingDialogController::handleSelectImbuement(const uint ImbuementId)
{
    selectImbuement(ImbuementId);
    Q_EMIT imbuingQmlDataChanged();
}

void TImbuingDialogController::handlePickFilterBasic() { setFilter(0); }
void TImbuingDialogController::handlePickFilterIntricate() { setFilter(1); }
void TImbuingDialogController::handlePickFilterPowerful() { setFilter(2); }

void TImbuingDialogController::setFilter(const uint32_t filter)
{
    m_filter = filter;
    updateAvailableImbuements();
    Q_EMIT imbuingQmlDataChanged();
}

void TImbuingDialogController::handlePickItemToImbue()
{
    m_picking = true;
    dialog()->hide();
    // The picked object goes to the server; when the selection ends the
    // dialog comes back if it is still there.
    input::startTargetSelection([this](const input::TGameActionPtr& target) {
        if (!m_picking)
            return;
        if (target && target->objectClientId) {
            const uint8_t stackpos = target->stackPosition >= 0 ? static_cast<uint8_t>(target->stackPosition) : 0;
            tibia::game::selectImbuementItem(target->objectClientId, target->position, stackpos);
        }
        m_picking = false;
        if (dialog()->item() && !dialog()->isShown())
            dialog()->show();
    });
}

void TImbuingDialogController::handleImbueScroll() { tibia::game::selectImbuementScroll(); }

void TImbuingDialogController::onImbuingStorageDataChanged()
{
    updateImbuedItem();
    updateExistingImbuement();
    updateAvailableImbuements();
    selectImbuement(static_cast<uint32_t>(m_selectedImbuementId));
    Q_EMIT imbuingQmlDataChanged();
}

void TImbuingDialogController::onDisplayResultMessage(const QString& message)
{
    m_resultMessage->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK, QKeySequence(QStringLiteral("Return"))) });
    m_resultMessage->showMessage(qtTrId("info"), message);
}

void TImbuingDialogController::onImbueSlotConfirmed()
{
    tibia::game::applyImbuement(static_cast<uint8_t>(m_selectedImbuementSlotIndex), static_cast<uint32_t>(m_selectedImbuementId), false);
}

void TImbuingDialogController::onClearSlotConfirmed()
{
    tibia::game::clearImbuement(static_cast<uint8_t>(m_selectedImbuementSlotIndex));
}

const TImbuementSlot* TImbuingDialogController::selectedSlot() const
{
    const auto& slots = m_storage->slots();
    return m_selectedImbuementSlotIndex < slots.size() ? &slots[m_selectedImbuementSlotIndex] : nullptr;
}

void TImbuingDialogController::updateImbuedItem()
{
    switch (m_storage->mode()) {
        case TImbuingStorage::MODE_NONE:
            m_imbuedItemObjectID = 0;
            m_imbuedItemObjectUpgradeTier = 0;
            m_imbuedItemObjectCount = 0;
            m_selectedImbuementSlotIndex = 0;
            break;
        case TImbuingStorage::MODE_ITEM:
            m_imbuedItemObjectID = m_storage->itemTypeId();
            m_imbuedItemObjectUpgradeTier = m_storage->itemTier().value_or(0);
            m_imbuedItemObjectCount = 0;
            // An item without imbuement slots, or fewer than the index, starts at the first.
            if (imbuementSlotCount(m_storage->itemTypeId()) <= m_selectedImbuementSlotIndex)
                m_selectedImbuementSlotIndex = 0;
            break;
        case TImbuingStorage::MODE_SCROLL:
            m_imbuedItemObjectID = BlankImbuementScrollTypeId;
            m_imbuedItemObjectUpgradeTier = 0;
            m_imbuedItemObjectCount = 0;
            m_selectedImbuementSlotIndex = 0;
            break;
        default:
            break;
    }
}

void TImbuingDialogController::updateExistingImbuement()
{
    switch (m_storage->mode()) {
        case TImbuingStorage::MODE_NONE:
            m_slotsModel->setSlots({});
            replaceObject(m_existingImbuement, nullptr);
            break;
        case TImbuingStorage::MODE_ITEM: {
            // Only an imbueable item has slots to show.
            if (imbuementSlotCount(m_storage->itemTypeId()) == 0)
                break;
            const TImbuementSlot* slot = selectedSlot();
            if (slot && !slot->isEmpty) {
                replaceObject(m_existingImbuement,
                              new imbuing::TQmlExistingImbuement(slot->imbuement.id, slot->imbuement.name, slot->remainingDuration,
                                                                 slot->imbuement.duration, slot->clearingGoldCost, this));
                m_selectedImbuementId = static_cast<int>(slot->imbuement.id);
            } else {
                replaceObject(m_existingImbuement, nullptr);
            }
            m_slotsModel->setSlots(m_storage->slots());
            break;
        }
        case TImbuingStorage::MODE_SCROLL:
            m_selectedImbuementSlotIndex = 0;
            replaceObject(m_existingImbuement, nullptr);
            m_slotsModel->setSlots({});
            break;
        default:
            break;
    }
}

void TImbuingDialogController::updateAvailableImbuements()
{
    switch (m_storage->mode()) {
        case TImbuingStorage::MODE_NONE:
            m_filter = 0;
            m_available->setImbuements({});
            break;
        case TImbuingStorage::MODE_ITEM: {
            // An occupied slot lists only its imbuement, under its tier; a
            // free account sees basic imbuements only.
            const TImbuementSlot* slot = selectedSlot();
            const bool occupied = slot && !slot->isEmpty;
            if (occupied)
                m_filter = slot->imbuement.tier;
            else if (!localPlayerPremium())
                m_filter = 0;
            std::vector<TImbuement> imbuements = occupied ? std::vector<TImbuement>{ slot->imbuement } : imbuementsOfTier(*m_storage, m_filter);
            sortImbuements(imbuements);
            m_available->setImbuements(std::move(imbuements));
            break;
        }
        case TImbuingStorage::MODE_SCROLL: {
            // Scrolls have no basic imbuements.
            if (m_filter == 0)
                m_filter = 1;
            std::vector<TImbuement> imbuements = imbuementsOfTier(*m_storage, m_filter);
            sortImbuements(imbuements);
            if (m_selectedImbuementId == 0 && !imbuements.empty())
                m_selectedImbuementId = static_cast<int>(imbuements.front().id);
            m_available->setImbuements(std::move(imbuements));
            break;
        }
        default:
            break;
    }
}

void TImbuingDialogController::selectImbuement(const uint32_t imbuementId)
{
    m_selectedImbuementId = static_cast<int>(imbuementId);
    bool enough = false;
    const std::vector<TImbuementSource> owned = ownedAstralSources(*m_storage, imbuementId, enough);
    const TImbuement* imbuement = findImbuement(*m_storage, imbuementId);
    if (!imbuement && m_storage->mode() == TImbuingStorage::MODE_ITEM) {
        // The selected slot's imbuement, which the server may not offer.
        if (const TImbuementSlot* slot = selectedSlot(); slot && !slot->isEmpty)
            imbuement = &slot->imbuement;
    }
    if (!imbuement) {
        replaceObject(m_selectedImbuement, nullptr);
        return;
    }
    QList<QObject*> astralSources;
    for (const TImbuementSource& source : imbuement->astralSources) {
        const auto it = std::find_if(owned.begin(), owned.end(),
                                     [&source](const TImbuementSource& o) { return o.objectTypeId == source.objectTypeId; });
        const uint ownedCount = it != owned.end() ? it->count : 0;
        astralSources.append(new imbuing::TQmlAstralSource(source.name, source.count, ownedCount, source.objectTypeId));
    }
    replaceObject(m_selectedImbuement, new imbuing::TQmlImbuementData(*imbuement, astralSources, enough, this));
}

void TImbuingDialogController::replaceObject(QObject*& member, QObject* object)
{
    QObject* previous = std::exchange(member, object);
    if (previous)
        previous->deleteLater();
}

}
