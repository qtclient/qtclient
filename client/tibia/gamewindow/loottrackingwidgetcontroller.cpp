#include "loottrackingwidgetcontroller.h"
#include "colorhelper.h"

#include "analysers/itempricestorage.h"
#include "config/sidebaroptions.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/genericentertextdialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <QJsonObject>
#include <QLocale>
#include <QQuickItem>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

constexpr auto OptionsKey = "lootTrackerWidgetOptions";
constexpr uint16_t GoldCoin = 3031;
constexpr uint16_t PlatinumCoin = 3035;
constexpr uint16_t CrystalCoin = 3043;

QString english(const qint64 value) { return QLocale(QLocale::English).toString(value); }

/// CIP : the number and its " k"/"k", " kk"/"kk", ... forms as a
/// multi string.
QString numberVariants(const qint64 value)
{
    QStringList variants{ english(value) };
    for (const auto& [divisor, unit] : { std::pair<qint64, const char*>{ 1000, "k" }, { 1000000, "kk" }, { 1000000000, "kkk" } }) {
        if (value < divisor)
            break;
        const QString shortened = english(value / divisor);
        variants << shortened + u' ' + QLatin1String(unit) << shortened + QLatin1String(unit);
    }
    return variants.join(QChar(0x9c));
}

/// CIP .
QString trackedItemName(const uint16_t typeId)
{
    switch (typeId) {
        case GoldCoin: return qtTrId("gold_coin");
        case PlatinumCoin: return qtTrId("platinum_coin");
        case CrystalCoin: return qtTrId("crystal_coin");
        default: break;
    }
    const TAppearanceTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
    if (type && !type->getName().empty())
        return QString::fromStdString(type->getName());
    return qtTrId("dummy_unknown");
}

QColor outfitColor(const int index)
{
    const QColor c = tibia::shared::outfitColor(index);
    return QColor(c.red(), c.green(), c.blue());
}

QString formatDateTime(const quint32 timestamp)
{
    // CIP with seconds.
    return QDateTime::fromSecsSinceEpoch(timestamp).toString(QStringLiteral("yyyy-MM-dd, HH:mm:ss"));
}

}

TQmlDroppedLoot::TQmlDroppedLoot(const cyclopedia::TDroppedLoot& drop, QObject* parent)
    : cip::tibia::gamewindow::TQmlDroppedLoot(parent)
{
    m_monsterName = drop.monsterName;
    m_dropTimestamp = formatDateTime(drop.timestamp);
    m_dropCountString = english(drop.count);
    m_outfit = drop.outfit;
}

uint TQmlDroppedLoot::creatureOutfitId() const { return static_cast<uint>(m_outfit.getId()); }
QColor TQmlDroppedLoot::creatureHeadColor() const { return outfitColor(m_outfit.getHead()); }
QColor TQmlDroppedLoot::creatureTorsoColor() const { return outfitColor(m_outfit.getBody()); }
QColor TQmlDroppedLoot::creatureLegsColor() const { return outfitColor(m_outfit.getLegs()); }
QColor TQmlDroppedLoot::creatureDetailColor() const { return outfitColor(m_outfit.getFeet()); }
bool TQmlDroppedLoot::creatureFirstAddOn() const { return (m_outfit.getAddons() & 1) != 0; }
bool TQmlDroppedLoot::creatureSecondAddOn() const { return (m_outfit.getAddons() & 2) != 0; }

TQmlTrackedItem::TQmlTrackedItem(const cyclopedia::TTrackedItem& item, QObject* parent)
    : cip::tibia::gamewindow::TQmlTrackedItem(parent)
{
    m_appearanceTypeID = item.objectType;
    m_objectID = item.objectType;
    m_itemName = trackedItemName(item.objectType);
    m_dropCount = static_cast<int>(item.dropCount);
    m_trackingStartTime = formatDateTime(item.recordStartTimestamp);
}

TQmlTrackedItem::~TQmlTrackedItem()
{
    const QList<QObject*> drops = std::exchange(m_lootDrops, {});
    Q_EMIT lootDropsChanged();
    qDeleteAll(drops);
}

QString TQmlTrackedItem::itemValueString() const { return english(m_itemValue); }

QString TQmlTrackedItem::dropCountString() const { return numberVariants(m_dropCount); }

void TQmlTrackedItem::setItemValue(const qint64 value)
{
    if (m_itemValue == value)
        return;
    m_itemValue = value;
    Q_EMIT itemValueChanged();
}

void TQmlTrackedItem::setDropCount(const quint32 count)
{
    if (m_dropCount == static_cast<int>(count))
        return;
    m_dropCount = static_cast<int>(count);
    Q_EMIT dropCountChanged();
}

void TQmlTrackedItem::setLootDrops(const std::vector<cyclopedia::TDroppedLoot>& drops)
{
    const QList<QObject*> old = std::exchange(m_lootDrops, {});
    for (const cyclopedia::TDroppedLoot& drop : drops)
        m_lootDrops.append(new TQmlDroppedLoot(drop, this));
    Q_EMIT lootDropsChanged();
    qDeleteAll(old);
}

TLootTrackingWidgetController::TLootTrackingWidgetController(analysers::TAnalyserStorages* storages, QQuickItem* gameWindowItem,
                                                             QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_storages(storages)
    , m_storage(storages->itemTracking())
    , m_valueDialog(new TGenericEnterTextDialogController(qtTrId("loottracker_valueinput_caption"),
                                                          qtTrId("loottracker_valueinput_description"), 9, this))
{
    m_valueDialog->setIntOnly(true);
    connect(m_valueDialog, &TGenericEnterTextDialogController::textEntered, this, &TLootTrackingWidgetController::onAutoTrackLootValueChanged);
    analysers::TItemPriceStorage* prices = analysers::TItemPriceStorage::instance();
    connect(m_storages, &analysers::TAnalyserStorages::configurationLoadedForCharacter, this,
            &TLootTrackingWidgetController::onCharacterConfigurationLoaded);
    connect(m_storages, &analysers::TAnalyserStorages::premiumTimeChanged, this,
            &TLootTrackingWidgetController::onCharacterConfigurationLoaded);
    connect(m_storage, &cyclopedia::TItemTrackingStorage::configurationChangedFromUserInput, this,
            &TLootTrackingWidgetController::onCharacterConfigurationLoaded);
    connect(prices, &analysers::TItemPriceStorage::configurationChangedFromUserInput, this,
            &TLootTrackingWidgetController::onCharacterConfigurationLoaded);
    connect(prices, &analysers::TItemPriceStorage::marketPricesAvailable, this,
            &TLootTrackingWidgetController::onCharacterConfigurationLoaded);
    connect(m_storage, &cyclopedia::TItemTrackingStorage::itemDropTracked, this, &TLootTrackingWidgetController::onItemDropTracked);
    connect(m_storage, &cyclopedia::TItemTrackingStorage::recentLootDropAddedOrRemoved, this,
            &TLootTrackingWidgetController::onRecentLootDropsChanged);
    onCharacterConfigurationLoaded();
    widget()->createItem(QStringLiteral("LootTrackingWidget.qml"));
    if (QQuickItem* item = quickItem())
        connect(item, &QObject::destroyed, this, &TLootTrackingWidgetController::onWidgetQuickItemDestroyed);
}

TLootTrackingWidgetController::~TLootTrackingWidgetController()
{
    // CIP: empties trackedItems, announces it, then deletes the old items.
    const QList<QObject*> old = std::exchange(m_trackedItems, {});
    Q_EMIT trackedItemsChanged();
    qDeleteAll(old);
}

QList<QObject*> TLootTrackingWidgetController::trackedItems() const { return m_trackedItems; }

QQuickItem* TLootTrackingWidgetController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }

void TLootTrackingWidgetController::onCloseClicked() { onRequestClose(); }
void TLootTrackingWidgetController::onRequestClose() { Q_EMIT widgetClosed(); }
void TLootTrackingWidgetController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
}
void TLootTrackingWidgetController::onSetDragActive(const bool Active) { widget()->setDragActive(Active); }
void TLootTrackingWidgetController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }
void TLootTrackingWidgetController::onLevelUpClicked() {}
void TLootTrackingWidgetController::setCursorToResizeVertical(const bool Enabled)
{
    widget()->setCursorToResizeVertical(Enabled);
}
void TLootTrackingWidgetController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }
void TLootTrackingWidgetController::onWidgetQuickItemDestroyed() {}

void TLootTrackingWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    onWidgetOptionsChanged();
}

void TLootTrackingWidgetController::saveWidgetOptions()
{
    if (!m_options)
        return;
    QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    m_options->setSection(QLatin1String(OptionsKey), json);
}

/// CIP: the stored content height and maximized state are applied to the widget.
void TLootTrackingWidgetController::onWidgetOptionsChanged()
{
    if (!m_options)
        return;
    const QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    if (json.contains(QLatin1String("contentHeight")))
        widget()->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                       json.value(QLatin1String("contentMaximized")).toBool(true));
}

/// CIP: without a loaded character the list stays as it is.
void TLootTrackingWidgetController::onCharacterConfigurationLoaded()
{
    if (m_storages->hasCharacter())
        rebuildTrackedItems();
}

/// CIP: one TQmlTrackedItem per actively tracked item, with its recent drops
/// and its value; the previous items are deleted after trackedItemsChanged.
void TLootTrackingWidgetController::rebuildTrackedItems()
{
    const QList<QObject*> old = std::exchange(m_trackedItems, {});
    analysers::TItemPriceStorage* prices = analysers::TItemPriceStorage::instance();
    for (const cyclopedia::TTrackedItem& item : m_storage->activeTrackedItems()) {
        auto* qmlItem = new TQmlTrackedItem(item, this);
        qmlItem->setItemValue(static_cast<qint64>(prices->lootPrice(item.objectType)));
        qmlItem->setLootDrops(m_storage->lootDropsForTrackedItem(item.objectType));
        m_trackedItems.append(qmlItem);
    }
    Q_EMIT trackedItemsChanged();
    qDeleteAll(old);
}

void TLootTrackingWidgetController::onItemDropTracked(const cyclopedia::TTrackedItem& trackedItem, const cyclopedia::TDroppedLoot&)
{
    for (QObject* object : m_trackedItems) {
        auto* item = static_cast<TQmlTrackedItem*>(object);
        if (item->appearanceTypeID() == trackedItem.objectType) {
            item->setDropCount(trackedItem.dropCount);
            break;
        }
    }
    if (QQuickItem* item = quickItem())
        QMetaObject::invokeMethod(item, "highlightFlash");
}

void TLootTrackingWidgetController::onRecentLootDropsChanged()
{
    for (QObject* object : m_trackedItems) {
        auto* item = static_cast<TQmlTrackedItem*>(object);
        item->setLootDrops(m_storage->lootDropsForTrackedItem(static_cast<uint16_t>(item->appearanceTypeID())));
    }
}

void TLootTrackingWidgetController::onAutoTrackLootValueChanged(const QString& text)
{
    bool ok = false;
    const int value = text.toInt(&ok);
    if (ok)
        m_storage->setAutoTrackAboveValue(static_cast<quint32>(value));
}

void TLootTrackingWidgetController::requestOpenItemInfoDialog()
{
    auto action = std::make_shared<input::TGameActionOpenCyclopedia>(0);
    Q_EMIT publishGameAction(action);
}

void TLootTrackingWidgetController::requestContextMenu(const int TrackedItemIndex)
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    const auto typeId = static_cast<uint16_t>(TrackedItemIndex);
    menu->beginCustomMenu();
    // CIP.
    connect(menu->addCustomEntry(qtTrId("loottracker_remove_tracked")), &TContextMenuEntryItem::triggered, this,
            [this, typeId] { m_storage->untrackItem(typeId); });
    connect(menu->addCustomEntry(qtTrId("loottracker_remove_all_tracked")), &TContextMenuEntryItem::triggered, this,
            [this] { m_storage->removeAllTrackedItems(); });
    menu->showCustomMenu();
}

void TLootTrackingWidgetController::requestConfigurationMenu()
{
    m_valueDialog->showWithText(QString::number(m_storage->autoTrackAboveValue()));
}

TLootTrackingWidgetManager::TLootTrackingWidgetManager(analysers::TAnalyserStorages* storages, QQuickItem* gameWindowItem,
                                                       QQmlEngine* engine, QObject* parent)
    : QObject(parent)
    , m_storages(storages)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{}

TLootTrackingWidgetManager::~TLootTrackingWidgetManager() { delete m_controller; }

TSidebarWidget* TLootTrackingWidgetManager::open(config::TSidebarOptions* options)
{
    if (!m_storages)
        return nullptr;
    if (!m_controller) {
        m_controller = new TLootTrackingWidgetController(m_storages, m_gameWindowItem, m_engine);
        connect(m_controller, &TLootTrackingWidgetController::publishGameAction, this, &TLootTrackingWidgetManager::publishGameAction);
        m_controller->setSidebarOptions(options);
    }
    return m_controller->widget();
}

void TLootTrackingWidgetManager::close(const bool saveWidgetOptions)
{
    if (!m_controller)
        return;
    if (saveWidgetOptions)
        m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TLootTrackingWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

}
