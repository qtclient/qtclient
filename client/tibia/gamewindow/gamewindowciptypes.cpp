#include "gamewindowciptypes.h"
#include "creatures/creaturestorage.h"
#include "game/inspectedobject.h"
#include "qmlcomponents/texthelper.h"
#include "store/storestorage.h"
#include "game/worldtype.h"
#include "colorhelper.h"

#include <QWheelEvent>

#include "config/clientoptions.h"
#include "gamewindow/smoothfiltering.h"
#include "gamewindow/actionbarcontroller.h"
#include "gamewindow/battlelistcontroller.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/mousecursorcontroller.h"
#include "game/gameciptypes.h"
#include "game/gamesessionbase.h"
#include "input/gameaction.h"
#include "input/gameactioncontext.h"
#include "protocol/gameactioncontexthelper.h"
#include "trade/npctradestorage.h"
#include "trade/tradeciptypes.h"
#include "input/gameactionhandlers.h"

#include <client/outfit.h>
#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>
#include <QColor>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QKeyEvent>
#include <QMetaObject>
#include <protocol.pb.h>

#include <memory>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>

#include <algorithm>
#include <tibia/game/gamenotify.h>

namespace tibia::gamewindow {

namespace {
using A = input::EGameAction;
using TargetItem = cip::tibia::qmlcomponents::TTargetSelectionQuickItem;

/// The topmost visible, enabled target selection item under `scenePosition`
/// among `items` and their children (a clipping item hides children outside
/// its clip rectangle).
TargetItem* targetItemAt(QList<QQuickItem*> items, const QPointF& scenePosition)
{
    std::stable_sort(items.begin(), items.end(), [](const QQuickItem* a, const QQuickItem* b) { return a->z() < b->z(); });
    for (auto it = items.crbegin(); it != items.crend(); ++it) {
        QQuickItem* item = *it;
        if (!item->isVisible() || !item->isEnabled())
            continue;
        if (item->clip() && !item->clipRect().contains(item->mapFromScene(scenePosition)))
            continue;
        if (!item->childItems().isEmpty())
            if (TargetItem* found = targetItemAt(item->childItems(), scenePosition))
                return found;
        if (item->contains(item->mapFromScene(scenePosition)))
            if (auto* target = qobject_cast<TargetItem*>(item))
                return target;
    }
    return nullptr;
}

/// The target selection item under `scenePosition` among the root's children.
TargetItem* targetSelectionItemAt(const QPointF& scenePosition, QQuickItem* root)
{
    return root ? targetItemAt(root->childItems(), scenePosition) : nullptr;
}
}

TPreventAutoCloseEventFilter::TPreventAutoCloseEventFilter(QObject* parent)
    : cip::tibia::gamewindow::TPreventAutoCloseEventFilter(parent)
{}

bool TPreventAutoCloseEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    // CIP: key and wheel events and mouse presses and releases keep the dialog open.
    Q_UNUSED(watched);
    if (event
        && (dynamic_cast<QKeyEvent*>(event) || dynamic_cast<QWheelEvent*>(event) || event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::MouseButtonRelease))
        Q_EMIT preventAutoClose();
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
    if (TargetItem* target = targetSelectionItemAt(scenePosition, m_root)) {
        const QPointF local = target->mapFromScene(scenePosition);
        Q_EMIT target->targetSelected(static_cast<int>(local.x()), static_cast<int>(local.y()));
    }
}

bool TTargetSelectionEventFilter::eventFilter(QObject* watched, QEvent* event)
{
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
        Q_EMIT targetSelectionFinished();
        return consume;
    }
    if (keepSelecting)
        return consume;
    Q_EMIT targetSelectionFinished();
    Q_UNUSED(watched);
    return consume;
}

TWhiteBlacklistQmlEntry::TWhiteBlacklistQmlEntry(QString name, const bool permanently, QObject* parent)
    : cip::tibia::gamewindow::TWhiteBlacklistQmlEntry(parent)
{
    setName(std::move(name));
    setPermanently(permanently);
}

TTextCopyController::TTextCopyController(QObject* parent)
    : cip::tibia::gamewindow::TTextCopyController(parent)
{
}

void TTextCopyController::createInstance(QQmlEngine* Engine)
{
    static std::shared_ptr<TTextCopyController> instance;
    auto* controller = new TTextCopyController();
    Engine->rootContext()->setContextProperty(QStringLiteral("tibiaTextCopyController"), controller);
    instance.reset(controller);
    // Host: released with the engine, before the application goes.
    QObject::connect(Engine, &QObject::destroyed, [] { instance.reset(); });
}

/// Cuts the selection of a QML text input with normal echo that is not read
/// only: removes it, deselects and copies it through the clipboard action.
void TTextCopyController::cutFromTextInputQmlItemIfPossible(QObject* pTextInput)
{
    if (!pTextInput->property("hasSelection").toBool() || pTextInput->property("echoMode").toInt() != 0
        || pTextInput->property("readOnly").toBool())
        return;
    const QString text = pTextInput->property("selectedText").toString();
    const int selectionStart = pTextInput->property("selectionStart").toInt();
    const int selectionEnd = pTextInput->property("selectionEnd").toInt();
    QMetaObject::invokeMethod(pTextInput, "remove", Q_ARG(int, selectionStart), Q_ARG(int, selectionEnd));
    QMetaObject::invokeMethod(pTextInput, "deselect");
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionCopyTextToClipboard>(text));
}

/// Copies the selection of a QML text input with normal echo through the
/// clipboard action.
void TTextCopyController::copyFromTextInputQmlItemIfPossible(QObject* pTextInput)
{
    if (!pTextInput->property("hasSelection").toBool() || pTextInput->property("echoMode").toInt() != 0)
        return;
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionCopyTextToClipboard>(pTextInput->property("selectedText").toString()));
}

TSpritesheetDataObject::TSpritesheetDataObject(QString id, QObject* parent)
    : cip::tibia::gamewindow::TSpritesheetDataObject(parent)
{
    setId(std::move(id));
}

void TSpritesheetDataObject::setId(const QString& v) { m_id = v; }

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

TCyclopediaCreatureOccurrencesInfo::TCyclopediaCreatureOccurrencesInfo(const TCyclopediaCreatureOccurrencesData& data, QObject* parent)
    : cip::tibia::gamewindow::TCyclopediaCreatureOccurrencesInfo(parent)
{
    m_subAreaID = data.subAreaId;
    m_subAreaName = data.subAreaName;
    QList<QObject*>* lists[] = { &m_commonCreatures, &m_rareCreatures, &m_dynamicCreatures };
    for (int occurrence = 0; occurrence < 3; ++occurrence) {
        const auto found = data.creatures.find(occurrence);
        if (found == data.creatures.end())
            continue;
        for (const TCyclopediaCreatureData& creature : found->second)
            lists[occurrence]->append(new TCyclopediaCreatureInfo(creature.raceId, creature.raceName, creature.isUnknown, this));
    }
}

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

TQmlChildTransaction::TQmlChildTransaction(const store::TChildTransactionDetails& child, QObject* parent)
    : cip::tibia::gamewindow::TQmlChildTransaction(parent)
{
    m_dateString = qmlcomponents::TTextHelperQmlService().formatDateTime(static_cast<int>(child.date), true);
    m_balanceChange = child.balanceChange;
    m_description = child.description;
    m_currency = child.type == 1 ? Gold : TibiaCoins;
}

TQmlPossibleMainChar::TQmlPossibleMainChar(QString name, QString world, QString vocation, const int level, QObject* parent)
    : cip::tibia::gamewindow::TQmlPossibleMainChar(parent)
{
    m_characterName = std::move(name);
    m_worldName = std::move(world);
    m_vocation = std::move(vocation);
    m_level = level;
}

TQmlTargetWorld::TQmlTargetWorld(QString worldName, const bool premiumOnly, const bool lockedForTransfer, const int pvpType, QObject* parent)
    : cip::tibia::gamewindow::TQmlTargetWorld(parent)
    , m_pvpType(pvpType)
{
    m_worldName = std::move(worldName);
    m_premiumOnly = premiumOnly;
    m_lockedForTransfer = lockedForTransfer;
}

QString TQmlTargetWorld::detailedWorldName() const
{
    if (!m_premiumOnly && !m_lockedForTransfer)
        return qtTrId("characterworldtransfer_world_name").arg(m_worldName).arg(::tibia::game::worldTypeName(m_pvpType));
    const QString detail = qtTrId("characterworldtransfer_world_name_detail").arg(m_worldName).arg(::tibia::game::worldTypeName(m_pvpType));
    if (!m_premiumOnly)
        return detail.arg(qtTrId("characterworldtransfer_world_locked"));
    if (!m_lockedForTransfer)
        return detail.arg(qtTrId("characterworldtransfer_world_premium"));
    return detail.arg(qtTrId("characterworldtransfer_world_premium_and_locked"));
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

/// The default roles with "name" and "count".
QHash<int, QByteArray> TSimpleNameAndCountModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
    roles[NameRole] = "name";
    roles[CountRole] = "count";
    return roles;
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
    case AppearanceTypeIdRole:
        return uint(e.appearanceTypeId);
    case NameRole:
        return e.name;
    case CountRole:
        return e.count;
    default:
        return {};
    }
}

QHash<int, QByteArray> THouseItemModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
    roles.insert(AppearanceTypeIdRole, "appearanceTypeID");
    roles.insert(NameRole, "name");
    roles.insert(CountRole, "count");
    return roles;
}

void THouseItemModel::setEntries(std::vector<Entry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

TPartyHuntMembersListModelSortFilterProxy::TPartyHuntMembersListModelSortFilterProxy(QObject* parent)
    : cip::tibia::gamewindow::TPartyHuntMembersListModelSortFilterProxy(parent)
{
    setDynamicSortFilter(true);
}

/// CIP: every member row with a valid index.
bool TPartyHuntMembersListModelSortFilterProxy::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    if (!sourceModel())
        return false;
    return sourceModel()->index(sourceRow, 0, sourceParent).isValid();
}

bool TPartyHuntMembersListModelSortFilterProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    // By the member model's name role (characterID is Qt::UserRole + 1),
    // ignoring case.
    if (!sourceModel())
        return false;
    const QString ln = left.data(Qt::UserRole + 2).toString();
    const QString rn = right.data(Qt::UserRole + 2).toString();
    return QString::compare(ln, rn, Qt::CaseInsensitive) < 0;
}

TDialogControllerBase::TDialogControllerBase(QObject* parent)
    : cip::tibia::gamewindow::TDialogControllerBase(parent)
{}

TSidebarWidgetControllerBase::TSidebarWidgetControllerBase(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : cip::tibia::gamewindow::TSidebarWidgetControllerBase(parent)
    , m_widget(new TSidebarWidget(this, gameWindowItem, engine, [this](uint id) { m_parentSidebarId = id; }))
{}

QQuickItem* TSidebarWidgetControllerBase::gameWindowQuickItem() const
{
    return m_widget->gameWindowItem();
}

uint TSidebarWidgetControllerBase::parentSidebarId() const
{
    return m_parentSidebarId;
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
    , m_widget(new TSidebarWidget(this, gameWindowItem, engine, [this](uint id) { m_parentSidebarId = id; }))
{
    connect(&m_focus, &TUiFocusManager::focusChanged, this, &TSidebarWidgetControllerWithFocusHandling::focusChanged);
}

/// CIP: the focus goes back first.
TSidebarWidgetControllerWithFocusHandling::~TSidebarWidgetControllerWithFocusHandling() { m_focus.releaseFocus(); }

bool TSidebarWidgetControllerWithFocusHandling::hasFocus() const { return m_focus.hasFocus(); }

QQuickItem* TSidebarWidgetControllerWithFocusHandling::gameWindowQuickItem() const
{
    return m_widget->gameWindowItem();
}

uint TSidebarWidgetControllerWithFocusHandling::parentSidebarId() const
{
    return m_parentSidebarId;
}

void TSidebarWidgetControllerWithFocusHandling::takeFocus() { m_focus.takeFocus(); }

void TSidebarWidgetControllerWithFocusHandling::releaseFocus() { m_focus.releaseFocus(); }

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
        if (data.typeId != 0) {
            m_appearanceID = data.typeId;
            m_objectCount = static_cast<uint>(std::max<uint16_t>(1, data.count));
            if (m_objectCount > 1)
                m_textOnSprite = QString::number(m_objectCount);
            m_objectUpgradeTier = data.tier;
            if (data.fluidOrSplash)
                m_liquidType = static_cast<decltype(m_liquidType)>(data.fluidOrSubtype);
            if (data.hookSouth)
                m_hookDirection = static_cast<decltype(m_hookDirection)>(1);
            else if (data.hookEast)
                m_hookDirection = static_cast<decltype(m_hookDirection)>(2);
            m_hasProficiency = data.hasProficiency;
        }
        for (const uint16_t icon : data.imbuements)
            m_imbumentIcons << QStringLiteral("image://imbuement-icons/%1").arg(icon);
    }

    void setFromInventory(const InspectionInventoryItem& entry)
    {
        setDetails(QString::fromStdString(entry.name), entry.descriptions);
        if (entry.typeId != 0) {
            m_appearanceID = entry.typeId;
            m_objectCount = static_cast<uint>(std::max<uint16_t>(1, entry.count));
            if (m_objectCount > 1)
                m_textOnSprite = QString::number(m_objectCount);
            m_objectUpgradeTier = entry.tier;
            if (entry.fluidOrSplash)
                m_liquidType = static_cast<decltype(m_liquidType)>(entry.fluidOrSubtype);
            if (entry.hookSouth)
                m_hookDirection = static_cast<decltype(m_hookDirection)>(1);
            else if (entry.hookEast)
                m_hookDirection = static_cast<decltype(m_hookDirection)>(2);
            m_hasProficiency = entry.hasProficiency;
        }
        for (const uint16_t icon : entry.imbuements)
            m_imbumentIcons << QStringLiteral("image://imbuement-icons/%1").arg(icon);
    }

    void setFromPlayer(const CyclopediaCharacterInspection& data)
    {
        setDetails(QString::fromStdString(data.playerName), data.playerDescriptions);
        m_appearanceID = data.outfit.getId();
        m_objectCount = 1;
        m_headColor = tibia::shared::outfitColor(data.outfit.getHead());
        m_torsoColor = tibia::shared::outfitColor(data.outfit.getBody());
        m_legsColor = tibia::shared::outfitColor(data.outfit.getLegs());
        m_detailColor = tibia::shared::outfitColor(data.outfit.getFeet());
        m_firstAddOn = (data.outfit.getAddons() & 1) != 0;
        m_secondAddOn = (data.outfit.getAddons() & 2) != 0;
    }
};

/// Empties the list, tells QML, then deletes the rows.
template <class Controller>
void clearInspectList(Controller& controller, QList<QObject*>& list)
{
    const QList<QObject*> rows = std::exchange(list, {});
    Q_EMIT controller.inspectObjectListChanged();
    qDeleteAll(rows);
}

} // namespace

TOutfitQMLInformation::TOutfitQMLInformation(const int id, const int head, const int torso, const int legs, const int detail,
                                             const int addons, QObject* parent)
    : cip::tibia::gamewindow::TOutfitQMLInformation(parent)
{
    const auto color = [](const int index) { return tibia::shared::outfitColor(index); };
    m_id = id;
    m_headColor = color(head);
    m_torsoColor = color(torso);
    m_legsColor = color(legs);
    m_detailColor = color(detail);
    m_firstAddOn = (addons & 1) != 0;
    m_secondAddOn = (addons & 2) != 0;
}

bool smoothFilteringEnabled() { return config::TClientOptions::instance()->antialiasingMode() == 1; }

namespace {
/// The inspect dialogs read the option from the client options they keep.
bool smoothFilteringEnabled(const config::TClientOptions* options)
{
    return options && options->antialiasingMode() == 1;
}
} // namespace

bool TInspectObjectDialogController::clientSettingSmoothFiltering() const { return smoothFilteringEnabled(m_clientOptions); }
bool TInspectPlayerDialogController::clientSettingSmoothFiltering() const { return smoothFilteringEnabled(m_clientOptions); }

TInspectObjectDialogController::TInspectObjectDialogController(config::TClientOptions* ClientOptions, QQmlEngine* Engine,
                                                               QQuickItem* DialogParentItem, QObject* parent)
    : TDialogController(QStringLiteral("InspectObjectDialog.qml"), Engine, DialogParentItem, parent)
    , m_clientOptions(ClientOptions)
{
}

void TInspectObjectDialogController::handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session || !pGameAction || pGameAction->gameActionType() != input::EGameAction::INSPECT_OBJECT)
        return;
    // The next inspection result opens the dialog. The protocol handler reports
    // the Cyclopedia's item inspections the same way; those stay with the Cyclopedia.
    disconnect(m_inspectionConnection);
    m_inspectionConnection = connect(session->gameProtocolMessageHandler().get(), &game::TGameProtocolMessageHandler::inspectObjectDataReceived,
                                     this, [this](const std::vector<TInspectObjectData>& list, const game::EInspectionListWindow window) {
                                         if (window != CyclopediaInspectionType)
                                             showObject(game::toItemInspectionData(list, window));
                                     });
    const auto* inspect = dynamic_cast<const input::TGameActionInspectObject*>(pGameAction.get());
    if (!inspect)
        return;
    const input::TGameActionContextPtr& location = inspect->location();
    const input::EGameActionContextType type = location ? location->gameActionContextType() : input::EGameActionContextType::LOCATION_UNKNOWN;
    if (type == input::EGameActionContextType::LOCATION_NPC_TRADE) {
        const auto* trade = dynamic_cast<const input::TLocationNpcTradeGameActionContext*>(location.get());
        if (trade && trade->good() && session->npcTradeStorage())
            session->npcTradeStorage()->inspect(*trade->good());
        return;
    }
    if (type == input::EGameActionContextType::LOCATION_PLAYER_TRADE) {
        if (const auto* trade = dynamic_cast<const input::TLocationPlayerTradeGameActionContext*>(location.get()))
            session->playerTradeProtocolMessageHandler()->lookTrade(trade->index(), trade->side());
        return;
    }
    // CIP: an InspectObject (type 0) whose ObjectPosition the TGameActionContextHelper fills from the location,
    // sent by the session's game protocol message handler.
    if (inspect->position().isValid()) {
        ::tibia::protobuf::protocol::GameclientMessageInspectObject message;
        message.set_inspection_type(0);
        protocol::TGameActionContextHelper().fillObjectPosition(location, message.mutable_object_position());
        Q_EMIT session->gameProtocolMessageHandler()->sendInspectObject(message);
    }
}

void TInspectObjectDialogController::showObject(const ItemInspectionData& data)
{
    // Any open inspection closes first.
    closeDialog();
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
    clearInspectList(*this, m_inspectObjectList);
    dialog()->close();
}

void TInspectObjectDialogController::okButtonClicked()
{
    closeDialog();
}

void TInspectObjectDialogController::copyToClipboarClicked()
{
    QString text;
    for (QObject* object : m_inspectObjectList) {
        if (const auto* row = qobject_cast<cip::tibia::gamewindow::TInspectObjectQMLData*>(object))
            text.append(row->clipboardReadyText());
    }
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionCopyTrustedTextToClipboard>(text));
}

void TInspectObjectDialogController::openWeaponProficiencyDialogClicked()
{
    if (m_inspectObjectList.isEmpty())
        return;
    if (const auto* row = qobject_cast<cip::tibia::gamewindow::TInspectObjectQMLData*>(m_inspectObjectList.front())) {
        auto action = std::make_shared<input::gameactions::TGameActionOpenWeaponProficiencyDialog>(static_cast<uint16_t>(static_cast<uint16_t>(row->appearanceID())));
        Q_EMIT publishGameAction(action);
    }
}

TInspectPlayerDialogController::TInspectPlayerDialogController(config::TClientOptions* ClientOptions, QQmlEngine* Engine,
                                                               QQuickItem* DialogParentItem, QObject* parent)
    : TDialogController(QStringLiteral("InspectPlayerDialog.qml"), Engine, DialogParentItem, parent)
    , m_clientOptions(ClientOptions)
{
}

void TInspectObjectDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    closeDialog();
    m_gameSession = GameSession;
}

void TInspectPlayerDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    closeDialog();
    for (const QMetaObject::Connection& connection : m_gameSessionConnections)
        disconnect(connection);
    m_gameSessionConnections.clear();
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    m_gameSessionConnections.push_back(connect(session->gameProtocolMessageHandler().get(),
                                               &game::TGameProtocolMessageHandler::requestOpenInspectPlayerDialog, this,
                                               [this](const std::vector<TInspectObjectData>& list, const creatures::TCreatureID playerId) {
                                                   showPlayer(game::toCharacterInspection(list, playerId));
                                               }));
}

void TInspectPlayerDialogController::showPlayer(const CyclopediaCharacterInspection& data)
{
    // Any open inspection closes first; then one row per inventory slot and
    // one for the player.
    closeDialog();
    for (int slot = 1; slot <= InspectSlots; ++slot)
        m_inspectObjectList.append(new TInspectObjectRow(slot, this));
    auto* player = new TInspectObjectRow(InspectPlayerSlot, this);
    m_inspectObjectList.append(player);

    // Dual wielding: a dual-wield weapon in the left hand and the right hand empty.
    bool rightHandEmpty = true;
    uint16_t leftHandTypeId = 0;
    bool leftHandDualWield = false;
    for (const InspectionInventoryItem& entry : data.inventoryItems) {
        if (entry.slot < 1 || entry.slot > InspectSlots)
            continue;
        static_cast<TInspectObjectRow*>(m_inspectObjectList[entry.slot - 1])->setFromInventory(entry);
        if (entry.slot == InspectRightHandSlot && entry.typeId != 0)
            rightHandEmpty = false;
        if (entry.slot == InspectLeftHandSlot && entry.typeId != 0) {
            leftHandTypeId = entry.typeId;
            if (TAppearanceType* type = tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, entry.typeId))
                leftHandDualWield = type->isDualWield();
        }
    }
    player->setFromPlayer(data);
    m_hasDualWielding = leftHandDualWield && rightHandEmpty;
    m_dualWieldingObjectID = leftHandDualWield ? static_cast<int>(leftHandTypeId) : 0;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    m_isOwnPlayer = session && session->hasLocalPlayer() && session->creatureStorage()
        && session->creatureStorage()->playerID() == data.creatureId;
    Q_EMIT inspectObjectListChanged();
    m_inspectedPlayerId = data.creatureId;
    dialog()->create();
    dialog()->show();
}

void TInspectPlayerDialogController::closeDialog()
{
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->setPointingHand(false);
    clearInspectList(*this, m_inspectObjectList);
    dialog()->close();
}

void TInspectPlayerDialogController::okButtonClicked()
{
    closeDialog();
}

void TInspectPlayerDialogController::openCharacterInfoClicked()
{
    // The Cyclopedia's character tab.
    auto action = std::make_shared<input::TGameActionOpenCyclopedia>(2);
    Q_EMIT publishGameAction(action);
    closeDialog();
}

void TInspectPlayerDialogController::copyToClipboarClicked(const QString& Text)
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionCopyTrustedTextToClipboard>(Text));
}

void TInspectPlayerDialogController::openSkillWheelDialog()
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session || !session->hasLocalPlayer() || m_inspectObjectList.isEmpty() || m_inspectedPlayerId == 0)
        return;
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionCreature>(A::REQUEST_SKILL_WHEEL, m_inspectedPlayerId));
}

void TInspectPlayerDialogController::openWeaponProficiencyDialogClicked(const uint AppearanceTypeID)
{
    auto action = std::make_shared<input::gameactions::TGameActionOpenWeaponProficiencyDialog>(static_cast<uint16_t>(static_cast<uint16_t>(AppearanceTypeID)));
    Q_EMIT publishGameAction(action);
}

TChatTutorialMarkerRenderInfo::TChatTutorialMarkerRenderInfo(const int markerId, const int startIndex, const int length, QObject* parent)
    : cip::tibia::gamewindow::TChatTutorialMarkerRenderInfo(parent)
{
    m_tutorialMarkerID = markerId;
    m_startIndex = startIndex;
    m_length = length;
}

} // namespace tibia::gamewindow
