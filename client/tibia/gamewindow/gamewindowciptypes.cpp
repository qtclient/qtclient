#include "gamewindowciptypes.h"

#include "gamewindow/actionbarcontroller.h"
#include "gamewindow/battlelistcontroller.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/gameactionhandlers.h"

#include <client/item.h>
#include <client/outfit.h>
#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>

#include <QColor>
#include <QGuiApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>

#include <algorithm>

namespace tibia::gamewindow {

namespace {
using A = input::EGameAction;
using TargetItem = cip::tibia::qmlcomponents::TTargetSelectionQuickItem;

TargetItem* targetItemAt(QQuickItem* item, const QPointF& scenePosition)
{
    QList<QQuickItem*> children = item->childItems();
    std::stable_sort(children.begin(), children.end(), [](const QQuickItem* a, const QQuickItem* b) { return a->z() < b->z(); });
    for (auto it = children.crbegin(); it != children.crend(); ++it) {
        QQuickItem* child = *it;
        if (!child->isVisible() || !child->isEnabled())
            continue;
        const QPointF local = child->mapFromScene(scenePosition);
        const bool inside = child->contains(local);
        if (child->clip() && !inside)
            continue;
        if (TargetItem* found = targetItemAt(child, scenePosition))
            return found;
        if (auto* target = qobject_cast<TargetItem*>(child); target && inside)
            return target;
    }
    return nullptr;
}
}

TPreventAutoCloseEventFilter::TPreventAutoCloseEventFilter(QObject* parent)
    : cip::tibia::gamewindow::TPreventAutoCloseEventFilter(parent)
{}

bool TPreventAutoCloseEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    // CIP 0x100b74020: any mouse or key input keeps the dialog open.
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
    case QEvent::MouseButtonDblClick:
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    case QEvent::Wheel:
        Q_EMIT preventAutoClose();
        break;
    default:
        break;
    }
    Q_UNUSED(watched);
    return false;
}

TTargetSelectionEventFilter::TTargetSelectionEventFilter(QQuickItem* root, QObject* parent)
    : cip::tibia::gamewindow::TTargetSelectionEventFilter(parent)
    , m_root(root)
{}

void TTargetSelectionEventFilter::setSelecting(const bool selecting)
{
    TMouseCursorController* cursor = TMouseCursorController::instance();
    if (!selecting) {
        if (cursor)
            cursor->setTargetSelection(false);
        if (m_filtering)
            qApp->removeEventFilter(this);
        m_filtering = false;
        return;
    }
    if (TDialogStackController* dialogs = TDialogStackController::instance(); dialogs && !dialogs->isEmpty()) {
        input::finishTargetSelection(nullptr);
        return;
    }
    if (cursor)
        cursor->setTargetSelection(true);
    m_initialButtons = QGuiApplication::mouseButtons();
    if (!m_filtering)
        qApp->installEventFilter(this);
    m_filtering = true;
}

void TTargetSelectionEventFilter::emitTargetSelected(const QPointF& scenePosition)
{
    QQuickWindow* window = m_root ? m_root->window() : nullptr;
    if (!window)
        return;
    if (TargetItem* target = targetItemAt(window->contentItem(), scenePosition)) {
        const QPointF local = target->mapFromScene(scenePosition);
        Q_EMIT target->targetSelected(static_cast<int>(local.x()), static_cast<int>(local.y()));
    }
}

bool TTargetSelectionEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    // CIP 0x100bdb630.
    const QEvent::Type type = event->type();
    bool consume = false;
    bool keepSelecting = true;
    if (type == QEvent::KeyRelease && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        keepSelecting = static_cast<QKeyEvent*>(event)->isAutoRepeat();
        consume = !keepSelecting;
    } else if (type == QEvent::KeyPress) {
        consume = static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape;
    }
    if (auto* mouse = dynamic_cast<QMouseEvent*>(event)) {
        if (type != QEvent::MouseButtonRelease)
            return type != QEvent::MouseMove;
        const Qt::MouseButton button = mouse->button();
        consume = true;
        if (!(button & ~m_initialButtons)) {
            m_initialButtons ^= button;
            consume = false;
        }
        if (button == Qt::LeftButton)
            emitTargetSelected(mouse->scenePosition());
        input::finishTargetSelection(nullptr);
        Q_EMIT targetSelectionFinished();
        return consume;
    }
    if (keepSelecting)
        return consume;
    input::finishTargetSelection(nullptr);
    Q_EMIT targetSelectionFinished();
    Q_UNUSED(watched);
    return consume;
}

TVipRenderInfo::TVipRenderInfo(const uint32_t characterId, QObject* parent)
    : cip::tibia::gamewindow::TVipRenderInfo(parent)
    , m_characterId(characterId)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &TVipRenderInfo::onHighlightedTimerTimeout);
}

void TVipRenderInfo::startHighlight(const int milliseconds)
{
    m_highlighted = true;
    m_timer.start(milliseconds);
    Q_EMIT vipRenderInfoChanged(m_characterId);
}

void TVipRenderInfo::clearHighlight()
{
    m_timer.stop();
    if (!m_highlighted)
        return;
    m_highlighted = false;
    Q_EMIT vipRenderInfoChanged(m_characterId);
}

void TVipRenderInfo::onHighlightedTimerTimeout()
{
    m_highlighted = false;
    Q_EMIT vipRenderInfoChanged(m_characterId);
}

TWhiteBlacklistQmlEntry::TWhiteBlacklistQmlEntry(QString name, const bool permanently, QObject* parent)
    : cip::tibia::gamewindow::TWhiteBlacklistQmlEntry(parent)
{
    setName(std::move(name));
    setPermanently(permanently);
}

TTextCopyController::TTextCopyController(QObject* parent)
    : cip::tibia::gamewindow::TTextCopyController(parent)
{}

void TTextCopyController::cutFromTextInputQmlItemIfPossible(QObject* pTextInput)
{
    copyOrCut(pTextInput, true);
}

void TTextCopyController::copyFromTextInputQmlItemIfPossible(QObject* pTextInput)
{
    copyOrCut(pTextInput, false);
}

void TTextCopyController::copyOrCut(QObject* textInput, const bool cut)
{
    if (!textInput)
        return;
    QString selected;
    QMetaObject::invokeMethod(textInput, "selectedText", Qt::DirectConnection, Q_RETURN_ARG(QString, selected));
    if (selected.isEmpty()) {
        QMetaObject::invokeMethod(textInput, "text", Qt::DirectConnection, Q_RETURN_ARG(QString, selected));
    }
    if (selected.isEmpty())
        return;
    if (QClipboard* clip = QGuiApplication::clipboard())
        clip->setText(selected);
    if (cut)
        QMetaObject::invokeMethod(textInput, "remove", Qt::DirectConnection);
    input::publish(input::makeAction(cut ? A::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD : A::COPY_TEXT_TO_CLIPBOARD));
}

TSpritesheetDataObject::TSpritesheetDataObject(QString id, QObject* parent)
    : cip::tibia::gamewindow::TSpritesheetDataObject(parent)
{
    setId(std::move(id));
}

TCyclopediaCreatureInfo::TCyclopediaCreatureInfo(const int raceId, QString raceName, const bool isUnknown, QObject* parent)
    : cip::tibia::gamewindow::TCyclopediaCreatureInfo(parent)
{
    m_raceID = raceId;
    m_raceName = std::move(raceName);
    m_isUnknown = isUnknown;
}

TCyclopediaCreatureOccurrencesInfo::TCyclopediaCreatureOccurrencesInfo(QObject* parent)
    : cip::tibia::gamewindow::TCyclopediaCreatureOccurrencesInfo(parent)
{}

void TCyclopediaCreatureOccurrencesInfo::setSubArea(const int id, const QString& name)
{
    m_subAreaID = id;
    m_subAreaName = name;
}

void TCyclopediaCreatureOccurrencesInfo::setCreatures(QList<QObject*> common, QList<QObject*> rare, QList<QObject*> dynamic)
{
    m_commonCreatures = std::move(common);
    m_rareCreatures = std::move(rare);
    m_dynamicCreatures = std::move(dynamic);
}

TCyclopediaMapSelectionInfoPassage::TCyclopediaMapSelectionInfoPassage(QObject* parent)
    : cip::tibia::gamewindow::TCyclopediaMapSelectionInfoPassage(parent)
{}

void TCyclopediaMapSelectionInfoPassage::setReachableCreatureOccurrences(QList<QObject*> occurrences)
{
    m_reachableCreatureOccurrences = std::move(occurrences);
    Q_EMIT infoChanged();
}

TQMLCharacterData::TQMLCharacterData(QObject* parent)
    : cip::tibia::gamewindow::TQMLCharacterData(parent)
{}

void TQMLCharacterData::setCharacter(const QString& name, const QString& vocation, const int level, const QString& worldType, QObject* outfit)
{
    m_characterName = name;
    m_characterVocation = vocation;
    m_characterLevel = level;
    m_characterWorldType = worldType;
    m_outfitConfiguration = outfit;
    Q_EMIT characterDataChanged();
}

TQmlChildTransaction::TQmlChildTransaction(QString date, QString description, const EMarketTransactionCurrency currency,
                                           const qlonglong balanceChange, QObject* parent)
    : cip::tibia::gamewindow::TQmlChildTransaction(parent)
{
    m_dateString = std::move(date);
    m_description = std::move(description);
    m_currency = currency;
    m_balanceChange = balanceChange;
}

TQmlPossibleMainChar::TQmlPossibleMainChar(QString name, QString world, QString vocation, const int level, QObject* parent)
    : cip::tibia::gamewindow::TQmlPossibleMainChar(parent)
{
    m_characterName = std::move(name);
    m_worldName = std::move(world);
    m_vocation = std::move(vocation);
    m_level = level;
}

TQmlTargetWorld::TQmlTargetWorld(QString worldName, QString detailedWorldName, const bool premiumOnly, const bool lockedForTransfer,
                                 QObject* parent)
    : cip::tibia::gamewindow::TQmlTargetWorld(parent)
{
    m_worldName = std::move(worldName);
    m_detailedWorldName = std::move(detailedWorldName);
    m_premiumOnly = premiumOnly;
    m_lockedForTransfer = lockedForTransfer;
}

TQmlWorldTransferCondition::TQmlWorldTransferCondition(const bool meets, QString text, QObject* parent)
    : cip::tibia::gamewindow::TQmlWorldTransferCondition(parent)
{
    m_meetsCondition = meets;
    m_conditionText = std::move(text);
}

TAccountCharactersModel::TAccountCharactersModel(QObject* parent)
    : cip::tibia::gamewindow::TAccountCharactersModel(parent)
{}

int TAccountCharactersModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant TAccountCharactersModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_rows.size()))
        return {};
    const TQMLCharacterData* row = m_rows[static_cast<size_t>(index.row())];
    if (!row)
        return {};
    switch (role) {
    case NameRole:
        return row->characterName();
    case LevelRole:
        return row->characterLevel();
    case VocationRole:
        return row->characterVocation();
    case WorldRole:
        return row->characterWorldType();
    default:
        return {};
    }
}

QHash<int, QByteArray> TAccountCharactersModel::roleNames() const
{
    return { { NameRole, "characterName" }, { LevelRole, "characterLevel" }, { VocationRole, "characterVocation" },
             { WorldRole, "characterWorldType" } };
}

void TAccountCharactersModel::setCharacters(std::vector<TQMLCharacterData*> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

TPassiveAbilityListDataModel::TPassiveAbilityListDataModel(QObject* parent)
    : cip::tibia::gamewindow::TPassiveAbilityListDataModel(parent)
{}

int TPassiveAbilityListDataModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TPassiveAbilityListDataModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const Entry& e = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
    case NameRole:
        return e.name;
    case IdRole:
    case EnumIdRole:
        return e.id;
    case IconUrlRole:
        return e.iconUrl;
    case UsableRole:
        return e.usableByCurrentVocation;
    default:
        return {};
    }
}

QHash<int, QByteArray> TPassiveAbilityListDataModel::roleNames() const
{
    return { { NameRole, "name" },
             { IdRole, "id" },
             { EnumIdRole, "enumId" },
             { IconUrlRole, "iconUrl" },
             { UsableRole, "usableByCurrentVocation" } };
}

void TPassiveAbilityListDataModel::setEntries(std::vector<Entry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

TPassiveAbilityListDataModelSortFilterProxy::TPassiveAbilityListDataModelSortFilterProxy(QObject* parent)
    : cip::tibia::gamewindow::TPassiveAbilityListDataModelSortFilterProxy(parent)
{}

QVariant TPassiveAbilityListDataModelSortFilterProxy::getDataWithRoleName(const int RowIndex, const QByteArray& RoleName)
{
    const QModelIndex idx = index(RowIndex, 0);
    const int role = roleNames().key(RoleName, -1);
    return role >= 0 ? data(idx, role) : QVariant{};
}

void TPassiveAbilityListDataModelSortFilterProxy::onPlayerDataChanged()
{
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT configChanged();
}

void TPassiveAbilityListDataModelSortFilterProxy::setNameFilter(const QString& filter)
{
    if (m_nameFilter == filter)
        return;
    beginFilterChange();
    m_nameFilter = filter;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT configChanged();
}

bool TPassiveAbilityListDataModelSortFilterProxy::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    if (m_nameFilter.isEmpty())
        return true;
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    const QString name = sourceModel()->data(idx, TPassiveAbilityListDataModel::NameRole).toString();
    return name.contains(m_nameFilter, Qt::CaseInsensitive);
}

TSimpleNameAndCountModel::TSimpleNameAndCountModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int TSimpleNameAndCountModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TSimpleNameAndCountModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const Entry& e = m_entries[static_cast<size_t>(index.row())];
    if (role == NameRole)
        return e.name;
    if (role == CountRole)
        return e.count;
    return {};
}

QHash<int, QByteArray> TSimpleNameAndCountModel::roleNames() const
{
    return { { NameRole, "name" }, { CountRole, "count" } };
}

void TSimpleNameAndCountModel::setEntries(std::vector<Entry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

THouseItemModel::THouseItemModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int THouseItemModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant THouseItemModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const Entry& e = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
    case NameRole:
        return e.name;
    case CountRole:
        return e.count;
    case ObjectIdRole:
        return e.objectId;
    default:
        return {};
    }
}

QHash<int, QByteArray> THouseItemModel::roleNames() const
{
    return { { NameRole, "name" }, { CountRole, "count" }, { ObjectIdRole, "objectId" } };
}

void THouseItemModel::setEntries(std::vector<Entry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

TBattleListDataModelSortFilterProxy::TBattleListDataModelSortFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

bool TBattleListDataModelSortFilterProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString ln = sourceModel()->data(left, Qt::DisplayRole).toString();
    const QString rn = sourceModel()->data(right, Qt::DisplayRole).toString();
    if (ln != rn)
        return QString::localeAwareCompare(ln, rn) < 0;
    return left.row() < right.row();
}

TPartyHuntMembersListModelSortFilterProxy::TPartyHuntMembersListModelSortFilterProxy(QObject* parent)
    : cip::tibia::gamewindow::TPartyHuntMembersListModelSortFilterProxy(parent)
{
    setDynamicSortFilter(true);
}

bool TPartyHuntMembersListModelSortFilterProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString ln = sourceModel()->data(left, Qt::UserRole + 1).toString();
    const QString rn = sourceModel()->data(right, Qt::UserRole + 1).toString();
    return QString::localeAwareCompare(ln, rn) < 0;
}

TBattleListGameActionHandler::TBattleListGameActionHandler(TBattleListControllerStorage* storage, QObject* parent)
    : cip::tibia::gamewindow::TBattleListGameActionHandler(parent)
    , m_storage(storage)
{}

void TBattleListGameActionHandler::attackFirstTarget()
{
    if (m_storage)
        m_storage->attackFirstTarget();
}

void TBattleListGameActionHandler::attackNextOrPreviousTarget(const bool Previous)
{
    if (m_storage)
        m_storage->attackNextOrPreviousTarget(Previous);
}

void TBattleListGameActionHandler::handleRemoveAllTutorialMarkersGameAction() {}

TAssignActionDialogControllerBase::TAssignActionDialogControllerBase(QObject* parent)
    : cip::tibia::gamewindow::TAssignActionDialogControllerBase(parent)
{}

void TAssignActionDialogControllerBase::bindActionButton(TActionBarController* bar, const int buttonId)
{
    if (!bar)
        return;
    m_actionBarId = uint(bar->actionBarId());
    m_buttonId = uint(buttonId);
    m_buttonName = qtTrId("actionbar_button_identifier").arg(bar->actionBarId()).arg(buttonId);
    m_showActionBarTarget = true;
    Q_EMIT showActionBarTargetChanged();
}

void TAssignActionDialogControllerBase::onCloseClicked()
{
    Q_EMIT abort();
    Q_EMIT dialogClosed();
}

TDialogControllerWithFocusHandlingBase::TDialogControllerWithFocusHandlingBase(QObject* parent)
    : cip::tibia::gamewindow::TDialogControllerWithFocusHandlingBase(parent)
{}

void TDialogControllerWithFocusHandlingBase::takeFocus()
{
    if (m_hasFocus)
        return;
    m_hasFocus = true;
    Q_EMIT focusChanged();
}

void TDialogControllerWithFocusHandlingBase::releaseFocus()
{
    if (!m_hasFocus)
        return;
    m_hasFocus = false;
    Q_EMIT focusChanged();
}

TSidebarPanelControllerBase::TSidebarPanelControllerBase(QObject* parent)
    : cip::tibia::gamewindow::TSidebarPanelControllerBase(parent)
{}

void TSidebarPanelControllerBase::onSidebarDestroyed()
{
    Q_EMIT requestClose();
}

void TSidebarPanelControllerBase::onPanelQuickItemDestroyed()
{
    Q_EMIT requestClose();
}

TSidebarWidgetControllerBase::TSidebarWidgetControllerBase(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : cip::tibia::gamewindow::TSidebarWidgetControllerBase(parent)
    , m_widget(new TSidebarWidget(this, gameWindowItem, engine))
{}

QQuickItem* TSidebarWidgetControllerBase::gameWindowQuickItem() const
{
    return m_widget->gameWindowItem();
}

uint TSidebarWidgetControllerBase::parentSidebarId() const
{
    return m_widget->parentSidebarId();
}

void TSidebarWidgetControllerBase::onCloseClicked()
{
    onRequestClose();
}

void TSidebarWidgetControllerBase::onRequestClose()
{
    Q_EMIT widgetClosed();
}

void TSidebarWidgetControllerBase::onRequestMoveToSidebar(const int TargetSidebar)
{
    m_widget->requestMoveToSidebar(TargetSidebar);
}

void TSidebarWidgetControllerBase::onSetDragActive(const bool Active)
{
    m_widget->setDragActive(Active);
}

void TSidebarWidgetControllerBase::onSidebarDestroyed()
{
    m_widget->onSidebarDestroyed();
}

void TSidebarWidgetControllerBase::setCursorToResizeVertical(const bool Enabled)
{
    m_widget->setCursorToResizeVertical(Enabled);
}

TSidebarWidgetControllerWithFocusHandling::TSidebarWidgetControllerWithFocusHandling(QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                                                     QObject* parent)
    : cip::tibia::gamewindow::TSidebarWidgetControllerWithFocusHandling(parent)
    , m_widget(new TSidebarWidget(this, gameWindowItem, engine))
{}

QQuickItem* TSidebarWidgetControllerWithFocusHandling::gameWindowQuickItem() const
{
    return m_widget->gameWindowItem();
}

uint TSidebarWidgetControllerWithFocusHandling::parentSidebarId() const
{
    return m_widget->parentSidebarId();
}

void TSidebarWidgetControllerWithFocusHandling::takeFocus()
{
    if (m_hasFocus)
        return;
    m_hasFocus = true;
    Q_EMIT focusChanged();
    Q_EMIT requestWidgetControlToTakeFocus();
}

void TSidebarWidgetControllerWithFocusHandling::releaseFocus()
{
    if (!m_hasFocus)
        return;
    m_hasFocus = false;
    Q_EMIT focusChanged();
}

void TSidebarWidgetControllerWithFocusHandling::onCloseClicked()
{
    onRequestClose();
}

void TSidebarWidgetControllerWithFocusHandling::onRequestClose()
{
    Q_EMIT widgetClosed();
}

void TSidebarWidgetControllerWithFocusHandling::onRequestMoveToSidebar(const int TargetSidebar)
{
    m_widget->requestMoveToSidebar(TargetSidebar);
}

void TSidebarWidgetControllerWithFocusHandling::onSetDragActive(const bool Active)
{
    m_widget->setDragActive(Active);
}

void TSidebarWidgetControllerWithFocusHandling::onSidebarDestroyed()
{
    m_widget->onSidebarDestroyed();
}

void TSidebarWidgetControllerWithFocusHandling::setCursorToResizeVertical(const bool Enabled)
{
    m_widget->setCursorToResizeVertical(Enabled);
}

namespace {

using Args = std::vector<std::any>;
constexpr uint8_t CyclopediaInspectionType = 1;
constexpr int InspectSlots = 10;
constexpr int InspectPlayerSlot = 99;
constexpr int InspectRightHandSlot = 5;
constexpr int InspectLeftHandSlot = 6;

const char* const InspectSlotBackgrounds[] = {
    nullptr,
    "/images/inventory-head.png",
    "/images/inventory-neck.png",
    "/images/inventory-back.png",
    "/images/inventory-torso.png",
    "/images/inventory-right-hand.png",
    "/images/inventory-left-hand.png",
    "/images/inventory-legs.png",
    "/images/inventory-feet.png",
    "/images/inventory-finger.png",
    "/images/inventory-hip.png",
};

QColor inspectOutfitColor(const int index)
{
    const Color c = Outfit::getColor(index);
    return QColor(c.r(), c.g(), c.b());
}

class TInspectObjectRow final : public cip::tibia::gamewindow::TInspectObjectQMLData
{
public:
    using Descriptions = std::vector<std::pair<std::string, std::string>>;

    TInspectObjectRow(const int slot, QObject* parent)
        : cip::tibia::gamewindow::TInspectObjectQMLData(parent)
    {
        m_slotID = slot;
        if (slot > 0 && slot < static_cast<int>(std::size(InspectSlotBackgrounds)))
            m_backgroundImageUrl = QString::fromLatin1(InspectSlotBackgrounds[slot]);
    }

    void setDetails(const QString& name, const Descriptions& descriptions)
    {
        static const QString Row = QStringLiteral(
            "<tr>  <td width=\"150\" style=\"text-align:right; padding-right:%3px; padding-top:%4px\">%1:</td>"
            "  <td style=\"padding-top:%4px\">%2</td></tr>");
        setObjectName(name);
        m_objectInformation = QStringLiteral("<table width=\"100%\">");
        m_clipboardReadyText = qtTrId("inspect_inspecting").arg(name) + QLatin1Char('\n');
        bool first = true;
        for (const auto& [label, value] : descriptions) {
            const QString key = QString::fromStdString(label);
            const QString text = QString::fromStdString(value);
            m_objectInformation += Row.arg(key, text).arg(5).arg(first ? 0 : 5);
            m_clipboardReadyText += QStringLiteral("%1: %2\n").arg(key, text);
            first = false;
        }
        m_objectInformation += QStringLiteral("</table>");
    }

    void setFromItem(const ItemInspectionData& data)
    {
        setDetails(QString::fromStdString(data.name), data.descriptions);
        if (data.item) {
            m_appearanceID = data.item->getId();
            m_objectCount = static_cast<uint>(std::max(1, data.item->getCount()));
            if (m_objectCount > 1)
                m_textOnSprite = QString::number(m_objectCount);
            m_objectUpgradeTier = data.item->getTier();
            if (data.item->isFluidContainer() || data.item->isSplash())
                m_liquidType = static_cast<decltype(m_liquidType)>(data.item->getSubType());
            if (data.item->isHookSouth())
                m_hookDirection = static_cast<decltype(m_hookDirection)>(1);
            else if (data.item->isHookEast())
                m_hookDirection = static_cast<decltype(m_hookDirection)>(2);
            m_hasProficiency = data.item->getProficiencyId() != 0;
        }
        for (const uint16_t icon : data.imbuements)
            m_imbumentIcons << QStringLiteral("image://imbuement-icons/%1").arg(icon);
    }

    void setFromInventory(const InspectionInventoryItem& entry)
    {
        setDetails(QString::fromStdString(entry.name), entry.descriptions);
        if (entry.item) {
            m_appearanceID = entry.item->getId();
            m_objectCount = static_cast<uint>(std::max(1, entry.item->getCount()));
            if (m_objectCount > 1)
                m_textOnSprite = QString::number(m_objectCount);
            m_objectUpgradeTier = entry.item->getTier();
            if (entry.item->isFluidContainer() || entry.item->isSplash())
                m_liquidType = static_cast<decltype(m_liquidType)>(entry.item->getSubType());
            if (entry.item->isHookSouth())
                m_hookDirection = static_cast<decltype(m_hookDirection)>(1);
            else if (entry.item->isHookEast())
                m_hookDirection = static_cast<decltype(m_hookDirection)>(2);
            m_hasProficiency = entry.item->getProficiencyId() != 0;
        }
        for (const uint16_t icon : entry.imbuements)
            m_imbumentIcons << QStringLiteral("image://imbuement-icons/%1").arg(icon);
    }

    void setFromPlayer(const CyclopediaCharacterInspection& data)
    {
        setDetails(QString::fromStdString(data.playerName), data.playerDescriptions);
        m_appearanceID = data.outfit.getId();
        m_objectCount = 1;
        m_headColor = inspectOutfitColor(data.outfit.getHead());
        m_torsoColor = inspectOutfitColor(data.outfit.getBody());
        m_legsColor = inspectOutfitColor(data.outfit.getLegs());
        m_detailColor = inspectOutfitColor(data.outfit.getFeet());
        m_firstAddOn = (data.outfit.getAddons() & 1) != 0;
        m_secondAddOn = (data.outfit.getAddons() & 2) != 0;
    }
};

void clearInspectList(QList<QObject*>& list)
{
    qDeleteAll(list);
    list.clear();
}

} // namespace

TInspectObjectDialogController* TInspectObjectDialogController::s_instance = nullptr;

TInspectObjectDialogController::TInspectObjectDialogController(QObject* parent)
    : TDialogController(QStringLiteral("InspectObjectDialog.qml"), parent)
{
    s_instance = this;
    m_hooks.emplace_back("g_game", "onParseItemDetail", [this](LuaObject*, const Args& args) {
        const auto* data = args.empty() ? nullptr : std::any_cast<ItemInspectionData>(&args[0]);
        if (!data || data->inspectionType == CyclopediaInspectionType)
            return std::any{};
        showObject(*data);
        return std::any{};
    });
}

TInspectObjectDialogController* TInspectObjectDialogController::instance()
{
    return s_instance;
}

void TInspectObjectDialogController::showObject(const ItemInspectionData& data)
{
    clearInspectList(m_inspectObjectList);
    auto* row = new TInspectObjectRow(0, this);
    row->setFromItem(data);
    m_inspectObjectList.append(row);
    m_showProficiencyButton = row->hasProficiency();
    Q_EMIT inspectObjectListChanged();
    dialog()->create();
    dialog()->show();
}

void TInspectObjectDialogController::closeDialog()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

void TInspectObjectDialogController::okButtonClicked()
{
    closeDialog();
}

void TInspectObjectDialogController::copyToClipboarClicked()
{
    if (!m_inspectObjectList.isEmpty()) {
        if (auto* row = static_cast<TInspectObjectRow*>(m_inspectObjectList.front())) {
            if (QClipboard* clip = QGuiApplication::clipboard())
                clip->setText(row->clipboardReadyText());
        }
    }
    input::publish(input::makeAction(A::COPY_TEXT_TO_CLIPBOARD));
}

TInspectPlayerDialogController* TInspectPlayerDialogController::s_instance = nullptr;

TInspectPlayerDialogController::TInspectPlayerDialogController(QObject* parent)
    : TDialogController(QStringLiteral("InspectPlayerDialog.qml"), parent)
{
    s_instance = this;
    // CipSoft Character Info keeps onParseCyclopediaCharacterInspection; look/inspect
    // packets publish onParseCharacterInspection (and dual-emit the Cyclopedia name).
    m_hooks.emplace_back("g_game", "onParseCharacterInspection", [this](LuaObject*, const Args& args) {
        const auto* data = args.empty() ? nullptr : std::any_cast<CyclopediaCharacterInspection>(&args[0]);
        if (data)
            showPlayer(*data);
        return std::any{};
    });
}

TInspectPlayerDialogController* TInspectPlayerDialogController::instance()
{
    return s_instance;
}

void TInspectPlayerDialogController::showPlayer(const CyclopediaCharacterInspection& data)
{
    clearInspectList(m_inspectObjectList);
    for (int slot = 1; slot <= InspectSlots; ++slot)
        m_inspectObjectList.append(new TInspectObjectRow(slot, this));
    auto* player = new TInspectObjectRow(InspectPlayerSlot, this);
    m_inspectObjectList.append(player);

    bool rightHandEmpty = true;
    ItemPtr leftHand;
    for (const InspectionInventoryItem& entry : data.inventoryItems) {
        if (entry.slot < 1 || entry.slot > InspectSlots)
            continue;
        static_cast<TInspectObjectRow*>(m_inspectObjectList[entry.slot - 1])->setFromInventory(entry);
        if (entry.slot == InspectRightHandSlot && entry.item)
            rightHandEmpty = false;
        if (entry.slot == InspectLeftHandSlot)
            leftHand = entry.item;
    }
    player->setFromPlayer(data);
    const bool dualWield = leftHand && leftHand->isDualWield();
    m_hasDualWielding = dualWield && rightHandEmpty;
    m_dualWieldingObjectID = dualWield ? static_cast<int>(leftHand->getId()) : 0;
    m_isOwnPlayer = g_game.getLocalPlayer() && g_game.getLocalPlayer()->getId() == data.creatureId;
    Q_EMIT inspectObjectListChanged();
    dialog()->create();
    dialog()->show();
}

void TInspectPlayerDialogController::closeDialog()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

void TInspectPlayerDialogController::okButtonClicked()
{
    closeDialog();
}

void TInspectPlayerDialogController::copyToClipboarClicked(const QString& Text)
{
    if (QClipboard* clip = QGuiApplication::clipboard())
        clip->setText(Text);
    input::publish(input::makeAction(A::COPY_TEXT_TO_CLIPBOARD));
}

TChatChannelFilterController::TChatChannelFilterController(QObject* parent)
    : cip::tibia::gamewindow::TChatChannelFilterController(parent)
{}

void TChatChannelFilterController::setChannelModel(QAbstractItemModel* source)
{
    setSourceModel(source);
}

void TChatChannelFilterController::syncFromChannel(const bool canSubscribe, const bool subscribed, const bool writesLocal, const bool canClose,
                                                   const QString& name)
{
    m_canSubscribeToServerLog = canSubscribe;
    m_isSubscribedToServerLog = subscribed;
    m_writesToLocalChat = writesLocal;
    m_canBeClosed = canClose;
    m_channelName = name;
    Q_EMIT chatChannelPropertyChanged();
    Q_EMIT chatChannelChanged();
}

bool TChatChannelFilterController::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    Q_UNUSED(sourceRow);
    Q_UNUSED(sourceParent);
    // CIP 0x100bd71d0: currently accepts every channel entry row.
    return true;
}

TChatChannelTabRenderInfo::TChatChannelTabRenderInfo(QObject* parent)
    : cip::tibia::gamewindow::TChatChannelTabRenderInfo(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &TChatChannelTabRenderInfo::onFlashTimerTimeout);
}

void TChatChannelTabRenderInfo::startFlash(const int milliseconds)
{
    m_timer.start(milliseconds);
}

void TChatChannelTabRenderInfo::stopFlash()
{
    m_timer.stop();
}

void TChatChannelTabRenderInfo::onFlashTimerTimeout()
{
    Q_EMIT flashFinished();
}

TChatTutorialMarkerRenderInfo::TChatTutorialMarkerRenderInfo(const int markerId, const int startIndex, const int length, QObject* parent)
    : cip::tibia::gamewindow::TChatTutorialMarkerRenderInfo(parent)
{
    m_tutorialMarkerID = markerId;
    m_startIndex = startIndex;
    m_length = length;
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
    // Custom-ctor CIP types are constructed on live gamewindow controllers.
}

} // namespace tibia::gamewindow
