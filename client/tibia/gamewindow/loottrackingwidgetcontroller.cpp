#include "loottrackingwidgetcontroller.h"

#include "config/sidebaroptions.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/genericentertextdialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QJsonObject>
#include <QLocale>
#include <QQuickItem>

namespace tibia::gamewindow {

namespace {

constexpr auto OptionsKey = "lootTrackerWidgetOptions";
constexpr uint16_t GoldCoin = 3031;
constexpr uint16_t PlatinumCoin = 3035;
constexpr uint16_t CrystalCoin = 3043;

QString english(const qint64 value) { return QLocale(QLocale::English).toString(value); }

/// CIP 0x100bfbd70: the number and its " k"/"k", " kk"/"kk", ... forms as a
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

/// CIP FUN_100a62f40.
QString trackedItemName(const uint16_t typeId)
{
    switch (typeId) {
        case GoldCoin: return qtTrId("gold_coin");
        case PlatinumCoin: return qtTrId("platinum_coin");
        case CrystalCoin: return qtTrId("crystal_coin");
        default: break;
    }
    const ThingTypePtr& type = g_things.getThingType(typeId, ThingCategoryItem);
    if (type && !type->getName().empty())
        return QString::fromStdString(type->getName());
    return qtTrId("dummy_unknown");
}

QColor outfitColor(const int index)
{
    const Color c = Outfit::getColor(index);
    return QColor(c.r(), c.g(), c.b());
}

QString formatDateTime(const quint32 timestamp)
{
    // CIP 0x10095c790 with seconds.
    return QDateTime::fromSecsSinceEpoch(timestamp).toString(QStringLiteral("yyyy-MM-dd, HH:mm:ss"));
}

}

TQmlDroppedLoot::TQmlDroppedLoot(const cyclopedia::TItemInfoStorage::DroppedLoot& drop, QObject* parent)
    : cip::tibia::gamewindow::TQmlDroppedLoot(parent)
{
    m_monsterName = drop.monsterName;
    m_dropTimestamp = formatDateTime(drop.timestamp);
    m_dropCountString = english(drop.count);
    m_creatureOutfitId = static_cast<uint>(drop.outfit.getId());
    m_creatureHeadColor = outfitColor(drop.outfit.getHead());
    m_creatureTorsoColor = outfitColor(drop.outfit.getBody());
    m_creatureLegsColor = outfitColor(drop.outfit.getLegs());
    m_creatureDetailColor = outfitColor(drop.outfit.getFeet());
    m_creatureFirstAddOn = (drop.outfit.getAddons() & 1) != 0;
    m_creatureSecondAddOn = (drop.outfit.getAddons() & 2) != 0;
}

TQmlTrackedItem::TQmlTrackedItem(const cyclopedia::TItemInfoStorage::TrackedItem& item, const qint64 value, QObject* parent)
    : cip::tibia::gamewindow::TQmlTrackedItem(parent)
{
    m_appearanceTypeID = item.typeId;
    m_objectID = item.typeId;
    m_itemName = trackedItemName(item.typeId);
    m_trackingStartTime = formatDateTime(item.recordStartTimestamp);
    m_itemValueString = english(value);
    m_dropCount = static_cast<int>(item.dropCount);
    m_dropCountString = numberVariants(item.dropCount);
}

void TQmlTrackedItem::setDropCount(const quint32 count)
{
    if (m_dropCount == static_cast<int>(count))
        return;
    m_dropCount = static_cast<int>(count);
    m_dropCountString = numberVariants(count);
    Q_EMIT dropCountChanged();
}

void TQmlTrackedItem::setLootDrops(const std::vector<cyclopedia::TItemInfoStorage::DroppedLoot>& drops)
{
    const QList<QObject*> old = std::exchange(m_lootDrops, {});
    for (const cyclopedia::TItemInfoStorage::DroppedLoot& drop : drops)
        m_lootDrops.append(new TQmlDroppedLoot(drop, this));
    Q_EMIT lootDropsChanged();
    for (QObject* object : old)
        object->deleteLater();
}

TLootTrackingWidgetController::TLootTrackingWidgetController(cyclopedia::TItemInfoStorage* storage, QQuickItem* gameWindowItem,
                                                             QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_storage(storage)
{
    connect(m_storage, &cyclopedia::TItemInfoStorage::characterChanged, this, &TLootTrackingWidgetController::createLootTrackingList);
    connect(m_storage, &cyclopedia::TItemInfoStorage::trackingChanged, this, &TLootTrackingWidgetController::createLootTrackingList);
    connect(m_storage, &cyclopedia::TItemInfoStorage::pricesChanged, this, &TLootTrackingWidgetController::createLootTrackingList);
    connect(m_storage, &cyclopedia::TItemInfoStorage::itemDropTracked, this, &TLootTrackingWidgetController::onItemDropTracked);
    connect(m_storage, &cyclopedia::TItemInfoStorage::recentLootDropsChanged, this, &TLootTrackingWidgetController::onRecentLootDropsChanged);
    createLootTrackingList();
    widget()->createItem(QStringLiteral("LootTrackingWidget.qml"));
}

TLootTrackingWidgetController::~TLootTrackingWidgetController() { delete m_valueDialog; }

void TLootTrackingWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    if (!m_options)
        return;
    const QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    if (json.contains(QLatin1String("contentHeight")))
        widget()->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                       json.value(QLatin1String("contentMaximized")).toBool(true));
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

void TLootTrackingWidgetController::createLootTrackingList()
{
    const QList<QObject*> old = std::exchange(m_trackedItems, {});
    if (m_storage->hasCharacter()) {
        for (const cyclopedia::TItemInfoStorage::TrackedItem& item : m_storage->activeTrackedItems()) {
            auto* qmlItem = new TQmlTrackedItem(item, m_storage->lootPrice(item.typeId), this);
            qmlItem->setLootDrops(m_storage->lootDropsForTrackedItem(item.typeId));
            m_trackedItems.append(qmlItem);
        }
    }
    Q_EMIT trackedItemsChanged();
    for (QObject* object : old)
        object->deleteLater();
}

void TLootTrackingWidgetController::onItemDropTracked(const quint16 typeId, const quint32 dropCount)
{
    for (QObject* object : m_trackedItems) {
        auto* item = static_cast<TQmlTrackedItem*>(object);
        if (item->appearanceTypeID() == typeId) {
            item->setDropCount(dropCount);
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
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_CYCLOPEDIA));
    action->parameter = 0;
    input::publish(action);
}

void TLootTrackingWidgetController::requestContextMenu(const int TrackedItemIndex)
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    const auto typeId = static_cast<uint16_t>(TrackedItemIndex);
    menu->beginCustomMenu();
    // CIP 0x100cbbc10 / 0x100cbbd10.
    connect(menu->addCustomEntry(qtTrId("loottracker_remove_tracked")), &TContextMenuEntryItem::triggered, this,
            [this, typeId] { m_storage->setTracked(typeId, false); });
    connect(menu->addCustomEntry(qtTrId("loottracker_remove_all_tracked")), &TContextMenuEntryItem::triggered, this,
            [this] { m_storage->removeAllTracked(); });
    menu->showCustomMenu();
}

void TLootTrackingWidgetController::requestConfigurationMenu()
{
    if (!m_valueDialog) {
        // CIP ctor 0x100cb9130: the prompt at +0xc0.
        m_valueDialog = new TGenericEnterTextDialogController();
        m_valueDialog->setCaption(qtTrId("loottracker_valueinput_caption"));
        m_valueDialog->setDescription(qtTrId("loottracker_valueinput_description"));
        m_valueDialog->setMaxInputLength(9);
        m_valueDialog->setIntOnly(true);
        connect(m_valueDialog, &TGenericEnterTextDialogController::textEntered, this, &TLootTrackingWidgetController::onAutoTrackLootValueChanged);
    }
    m_valueDialog->setText(QString::number(m_storage->autoTrackAboveValue()));
    m_valueDialog->dialog()->show();
}

TLootTrackingWidgetManager::TLootTrackingWidgetManager(cyclopedia::TItemInfoStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                       QObject* parent)
    : QObject(parent)
    , m_storage(storage)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{}

TLootTrackingWidgetManager::~TLootTrackingWidgetManager() { delete m_controller; }

TSidebarWidget* TLootTrackingWidgetManager::open(config::TSidebarOptions* options)
{
    if (!m_storage)
        return nullptr;
    if (!m_controller) {
        m_controller = new TLootTrackingWidgetController(m_storage, m_gameWindowItem, m_engine);
        m_controller->setSidebarOptions(options);
    }
    return m_controller->widget();
}

void TLootTrackingWidgetManager::close()
{
    if (!m_controller)
        return;
    m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TLootTrackingWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

}
