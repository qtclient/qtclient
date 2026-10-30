#include "imbuingdialogcontroller.h"

#include "container/containerslotinformation.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>
#include <client/item.h>
#include <tibia/world/localplayer.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QLocale>

#include <algorithm>
#include <array>
#include <utility>

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

/// CIP 0x100927450.
QString imageSource(const int iconId)
{
    return QStringLiteral("image://%1/%2").arg(QStringLiteral("imbuement-icons")).arg(iconId);
}

/// CIP 0x100e53e90: the tier's name, empty for no imbuement.
QString categoryName(const Imbuement& imbuement)
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

/// CIP 0x10095e750 (below a day 0x10095e350), on minutes.
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

/// CIP 0x100883600: resizes with row inserts or removals, then reports
/// every row changed.
void TAvailableImbuementsListModel::setImbuements(std::vector<Imbuement> imbuements)
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

/// CIP 0x100e54090.
QVariant TAvailableImbuementsListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
        return {};
    const Imbuement& imbuement = m_imbuements[index.row()];
    switch (role) {
        case ImbuementIdRole: return static_cast<uint>(imbuement.id);
        case ImageSourceRole: return imageSource(imbuement.imageId);
        case CategoryRole: return categoryName(imbuement);
        default: return {};
    }
}

QHash<int, QByteArray> TAvailableImbuementsListModel::roleNames() const
{
    return { { ImbuementIdRole, "imbuementId" }, { ImageSourceRole, "imageSource" }, { CategoryRole, "category" } };
}

/// CIP 0x100883830.
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

/// CIP 0x100e53dd0.
QVariant TImbuementSlotsListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
        return {};
    const TImbuementSlot& slot = m_slots[index.row()];
    switch (role) {
        case SlotIndexRole: return static_cast<uint>(slot.index);
        case SlotImbuementIdRole: return static_cast<uint>(slot.imbuement.id);
        case SlotImageSourceRole: return imageSource(slot.imbuement.imageId);
        case SlotCategoryRole: return categoryName(slot.imbuement);
        default: return {};
    }
}

QHash<int, QByteArray> TImbuementSlotsListModel::roleNames() const
{
    return { { SlotIndexRole, "slotIndex" }, { SlotImbuementIdRole, "imbuementId" }, { SlotImageSourceRole, "imageSource" }, { SlotCategoryRole, "category" } };
}

/// CIP ctor 0x100e53230; resourceString 0x100e534d0 (count via 0x100960950),
/// hasEnoughToImbue 0x100e53660.
TQmlAstralSource::TQmlAstralSource(const QString& name, const uint typeId, const uint needed, const uint owned, QObject* parent)
    : cip::tibia::imbuing::TQmlAstralSource(parent)
{
    m_name = name;
    m_objectID = typeId;
    if (needed != 0)
        m_resourceString = qtTrId("imbuing_resource_count").arg(container::ammoText(owned)).arg(needed);
    m_hasEnoughToImbue = needed <= owned;
}

/// CIP 0x100e52e60.
TQmlImbuementData::TQmlImbuementData(const Imbuement& data, const QList<QObject*>& sources, const bool enough, QObject* parent)
    : cip::tibia::imbuing::TQmlImbuementData(parent)
{
    m_name = QString::fromStdString(data.name);
    m_description = QString::fromStdString(data.description);
    m_goldCost = static_cast<int>(data.cost);
    m_goldCostString = goldText(data.cost);
    m_astralSources = sources;
    for (QObject* source : sources)
        source->setParent(this);
    m_enoughAstralSourcesForImbuement = enough;
    m_isDummyWhenNoImbuementAvailable = false;
}

/// CIP 0x100e53670.
TQmlExistingImbuement::TQmlExistingImbuement(const TImbuementSlot& slot, QObject* parent)
    : cip::tibia::imbuing::TQmlExistingImbuement(parent)
{
    m_imbuementID = static_cast<int>(slot.imbuement.id);
    m_name = QString::fromStdString(slot.imbuement.name);
    m_clearingGoldCost = static_cast<int>(slot.clearingGoldCost);
    m_clearingGoldCostString = goldText(slot.clearingGoldCost);
    m_remainingDurationPercentage = slot.imbuement.duration ? static_cast<double>(slot.remainingDuration) / slot.imbuement.duration : 0.0;
    m_remainingDuration = durationText(slot.remainingDuration / 60);
}

}

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;
using imbuing::TImbuingStorage;

/// The blank imbuement scroll (CIP 0x100c69c90's scroll-mode appearance).
constexpr uint BlankImbuementScrollTypeId = 51442;

/// CIP 0x100c68c50's sort keys for imbuement ids 1..72; others sort as 25.
constexpr std::array<int, 72> ImbuementSortKeys = {
    11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 15, 1, 1, 1, 2, 2, 2, 3, 3, 3,
    21, 21, 21, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 20, 20, 20, 23, 23, 23, 5, 5, 5,
    4, 4, 4, 6, 6, 6, 7, 7, 7, 10, 10, 10, 8, 8, 8, 24, 24, 24, 22, 22, 22, 9, 9, 9,
};

int sortKey(const uint32_t id)
{
    return id >= 1 && id <= ImbuementSortKeys.size() ? ImbuementSortKeys[id - 1] : 25;
}

bool localPlayerPremium()
{
    const LocalPlayerPtr player = g_game.getLocalPlayer();
    return player && player->isPremium();
}

QString typeName(const uint typeId)
{
    const ThingTypePtr& type = g_things.getThingType(typeId, ThingCategoryItem);
    return type ? QString::fromStdString(type->getName()) : QString();
}

/// CIP 0x101018f90: each space-separated word's first letter upper-cased.
QString capitalizeWords(const QString& text)
{
    QStringList words = text.split(QLatin1Char(' '));
    for (QString& word : words) {
        if (!word.isEmpty())
            word[0] = word[0].toUpper();
    }
    return words.join(QLatin1Char(' '));
}

void publishOpenPremiumStore()
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_STORE));
    action->parameter = 1;
    input::publish(action);
}

}

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
    m_clientSettingSmoothFiltering = true;
    m_resultMessage->setCloseOnButtonClicked(true);
    m_imbueConfirm->setCloseOnButtonClicked(true);
    m_clearConfirm->setCloseOnButtonClicked(true);
    connect(m_imbueConfirm, &TMessageDialogController::acceptClicked, this, &TImbuingDialogController::onImbueSlotConfirmed);
    connect(m_clearConfirm, &TMessageDialogController::acceptClicked, this, &TImbuingDialogController::onClearSlotConfirmed);

    connect(m_storage, &TImbuingStorage::imbuingDataChanged, this, &TImbuingDialogController::onImbuingStorageDataChanged);
    connect(m_storage, &TImbuingStorage::requestShowOrRefreshImbuingDialog, this, &TImbuingDialogController::showDialog);
    connect(m_storage, &TImbuingStorage::requestCloseImbuingDialog, this, &TImbuingDialogController::closeDialog);
    connect(m_storage, &TImbuingStorage::requestShowResultDialog, this, &TImbuingDialogController::onDisplayResultMessage);

    m_hooks.emplace_back("object", "onPremiumChange", [this](LuaObject* self, const Args&) {
        if (self == g_game.getLocalPlayer().get()) {
            Q_EMIT premiumStatusChanged();
            Q_EMIT imbuingQmlDataChanged();
        }
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
    int filter = 0;
    if (v == qtTrId("imbuing_category_intricate"))
        filter = 1;
    else if (v == qtTrId("imbuing_category_powerful"))
        filter = 2;
    if (filter == m_filter)
        return;
    m_filter = filter;
    m_selectedImbuementId = 0;
    updateAvailable();
    select(0);
    Q_EMIT imbuingQmlDataChanged();
}

QString TImbuingDialogController::selectedItemText() const
{
    switch (m_storage->mode()) {
        case TImbuingStorage::MODE_SCROLL: return typeName(BlankImbuementScrollTypeId);
        case TImbuingStorage::MODE_ITEM: return capitalizeWords(typeName(m_storage->itemTypeId()));
        default: return {};
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
    g_game.closeImbuingWindow();
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
    const auto& imbuements = m_storage->imbuements();
    const auto it = std::find_if(imbuements.begin(), imbuements.end(), [this](const Imbuement& i) { return static_cast<int>(i.id) == m_selectedImbuementId; });
    if (it == imbuements.end())
        return;
    m_imbueConfirm->setYesNoButtons();
    m_imbueConfirm->showMessage(qtTrId("imbuing_confirm_imbuing_title"),
                                qtTrId("imbuing_confirm_imbuing_text").arg(QString::fromStdString(it->name)).arg(imbuing::goldText(it->cost)));
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
    updateExisting();
    updateAvailable();
    const imbuing::TImbuementSlot* slot = selectedSlot();
    select(slot && !slot->isEmpty ? static_cast<int>(slot->imbuement.id) : 0);
    Q_EMIT imbuingQmlDataChanged();
}

void TImbuingDialogController::handleSelectImbuement(const uint ImbuementId)
{
    select(static_cast<int>(ImbuementId));
    Q_EMIT imbuingQmlDataChanged();
}

void TImbuingDialogController::handlePickFilterBasic() { setFilter(0); }
void TImbuingDialogController::handlePickFilterIntricate() { setFilter(1); }
void TImbuingDialogController::handlePickFilterPowerful() { setFilter(2); }

void TImbuingDialogController::setFilter(const int filter)
{
    m_filter = filter;
    updateAvailable();
    Q_EMIT imbuingQmlDataChanged();
}

void TImbuingDialogController::handlePickItemToImbue()
{
    m_picking = true;
    dialog()->hide();
    input::startTargetSelection([this](const input::TGameActionPtr& target) {
        if (!m_picking)
            return;
        if (target && target->object && target->object->isItem()) {
            const ItemPtr item = target->object->static_self_cast<Item>();
            g_game.selectImbuementItem(static_cast<uint16_t>(item->getId()), item->getPosition(), static_cast<uint8_t>(item->getStackPos()));
        }
        m_picking = false;
        if (dialog()->item() && !dialog()->isShown())
            dialog()->show();
    });
}

void TImbuingDialogController::handleImbueScroll() { g_game.selectImbuementScroll(); }

void TImbuingDialogController::onImbuingStorageDataChanged()
{
    updateItem();
    updateExisting();
    updateAvailable();
    select(m_selectedImbuementId);
    Q_EMIT imbuingQmlDataChanged();
}

void TImbuingDialogController::onDisplayResultMessage(const QString& message)
{
    m_resultMessage->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK, QKeySequence(QStringLiteral("Return"))) });
    m_resultMessage->showMessage(qtTrId("info"), message);
}

void TImbuingDialogController::onImbueSlotConfirmed()
{
    g_game.applyImbuement(static_cast<uint8_t>(m_selectedImbuementSlotIndex), static_cast<uint32_t>(m_selectedImbuementId), false);
}

void TImbuingDialogController::onClearSlotConfirmed()
{
    g_game.clearImbuement(static_cast<uint8_t>(m_selectedImbuementSlotIndex));
}

const imbuing::TImbuementSlot* TImbuingDialogController::selectedSlot() const
{
    const auto& slots = m_storage->slots();
    return m_selectedImbuementSlotIndex < slots.size() ? &slots[m_selectedImbuementSlotIndex] : nullptr;
}

void TImbuingDialogController::updateItem()
{
    switch (m_storage->mode()) {
        case TImbuingStorage::MODE_ITEM:
            m_imbuedItemObjectID = m_storage->itemTypeId();
            m_imbuedItemObjectUpgradeTier = m_storage->itemTier();
            m_imbuedItemObjectCount = 0;
            if (m_selectedImbuementSlotIndex >= m_storage->slots().size())
                m_selectedImbuementSlotIndex = 0;
            break;
        case TImbuingStorage::MODE_SCROLL:
            m_imbuedItemObjectID = BlankImbuementScrollTypeId;
            m_imbuedItemObjectUpgradeTier = 0;
            m_imbuedItemObjectCount = 0;
            m_selectedImbuementSlotIndex = 0;
            break;
        default:
            m_imbuedItemObjectID = 0;
            m_imbuedItemObjectUpgradeTier = 0;
            m_imbuedItemObjectCount = 0;
            m_selectedImbuementSlotIndex = 0;
            break;
    }
}

void TImbuingDialogController::updateExisting()
{
    QObject* existing = nullptr;
    switch (m_storage->mode()) {
        case TImbuingStorage::MODE_ITEM:
            if (const imbuing::TImbuementSlot* slot = selectedSlot(); slot && !slot->isEmpty) {
                existing = new imbuing::TQmlExistingImbuement(*slot, this);
                m_selectedImbuementId = static_cast<int>(slot->imbuement.id);
            }
            m_slotsModel->setSlots(m_storage->slots());
            break;
        case TImbuingStorage::MODE_SCROLL:
            m_selectedImbuementSlotIndex = 0;
            m_slotsModel->setSlots({});
            break;
        default:
            m_slotsModel->setSlots({});
            break;
    }
    replaceObject(m_existingImbuement, existing);
}

void TImbuingDialogController::updateAvailable()
{
    const auto byTier = [this] {
        std::vector<Imbuement> list;
        for (const Imbuement& imbuement : m_storage->imbuements()) {
            if (imbuement.tier == m_filter)
                list.push_back(imbuement);
        }
        return list;
    };
    const auto sorted = [](std::vector<Imbuement> list) {
        std::stable_sort(list.begin(), list.end(), [](const Imbuement& a, const Imbuement& b) { return sortKey(a.id) < sortKey(b.id); });
        return list;
    };

    switch (m_storage->mode()) {
        case TImbuingStorage::MODE_ITEM: {
            const imbuing::TImbuementSlot* slot = selectedSlot();
            const bool occupied = slot && !slot->isEmpty;
            if (occupied)
                m_filter = slot->imbuement.tier;
            else if (!localPlayerPremium())
                m_filter = 0;
            std::vector<Imbuement> list = occupied ? std::vector<Imbuement>{ slot->imbuement } : byTier();
            m_available->setImbuements(sorted(std::move(list)));
            break;
        }
        case TImbuingStorage::MODE_SCROLL: {
            if (m_filter == 0)
                m_filter = 1;
            std::vector<Imbuement> list = sorted(byTier());
            if (m_selectedImbuementId == 0 && !list.empty())
                m_selectedImbuementId = static_cast<int>(list.front().id);
            m_available->setImbuements(std::move(list));
            break;
        }
        default:
            m_filter = 0;
            m_available->setImbuements({});
            break;
    }
}

void TImbuingDialogController::select(const int imbuementId)
{
    m_selectedImbuementId = imbuementId;
    const auto& imbuements = m_storage->imbuements();
    const auto it = std::find_if(imbuements.begin(), imbuements.end(), [imbuementId](const Imbuement& i) { return static_cast<int>(i.id) == imbuementId; });
    const Imbuement* data = it != imbuements.end() ? &*it : nullptr;
    if (!data && m_storage->mode() == TImbuingStorage::MODE_ITEM) {
        if (const imbuing::TImbuementSlot* slot = selectedSlot(); slot && !slot->isEmpty)
            data = &slot->imbuement;
    }
    if (!data) {
        replaceObject(m_selectedImbuement, nullptr);
        return;
    }

    QList<QObject*> sources;
    bool enough = true;
    for (const auto& [item, name] : data->sources) {
        const uint typeId = item ? item->getId() : 0;
        const uint needed = item ? item->getCount() : 0;
        bool listed = false;
        const uint owned = m_storage->ownedCount(static_cast<uint16_t>(typeId), &listed);
        enough = enough && listed && needed <= owned;
        sources.append(new imbuing::TQmlAstralSource(QString::fromStdString(name), typeId, needed, owned, nullptr));
    }
    replaceObject(m_selectedImbuement, new imbuing::TQmlImbuementData(*data, sources, enough, this));
}

void TImbuingDialogController::replaceObject(QObject*& member, QObject* object)
{
    QObject* previous = std::exchange(member, object);
    if (previous)
        previous->deleteLater();
}

}
