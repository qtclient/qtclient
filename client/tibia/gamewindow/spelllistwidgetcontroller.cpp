#include "spelllistwidgetcontroller.h"

#include "config/sidebaroptions.h"
#include "gamewindow/contextmenucontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <QJsonObject>
#include <QQuickItem>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace tibia::gamewindow {

namespace {

constexpr auto OptionsKey = "spellListWidgetOptions";
using Proxy = TSpellListDataModelSortFilterProxy;

/// The filter boxes the widget options keep (not the rune and instant boxes).
const std::vector<std::pair<const char*, bool (Proxy::*)() const>>& filterBoxes()
{
    static const std::vector<std::pair<const char*, bool (Proxy::*)() const>> boxes = {
        { "showOnlyCurrentVocation", &Proxy::showOnlyCurrentVocation },
        { "showOnlyCurrentLevel", &Proxy::showOnlyCurrentLevel },
        { "showUnkownSpells", &Proxy::showUnkownSpells },
        { "showDruidSpells", &Proxy::showDruidSpells },
        { "showKnightSpells", &Proxy::showKnightSpells },
        { "showPaladinSpells", &Proxy::showPaladinSpells },
        { "showSorcererSpells", &Proxy::showSorcererSpells },
        { "showMonkSpells", &Proxy::showMonkSpells },
        { "showAttackSpellGroup", &Proxy::showAttackSpellGroup },
        { "showHealingSpellGroup", &Proxy::showHealingSpellGroup },
        { "showSupportSpellGroup", &Proxy::showSupportSpellGroup },
        { "showPremiumOnlySpells", &Proxy::showPremiumOnlySpells },
        { "showFreeSpells", &Proxy::showFreeSpells },
    };
    return boxes;
}

const std::vector<std::pair<const char*, void (Proxy::*)(bool)>>& filterBoxSetters()
{
    static const std::vector<std::pair<const char*, void (Proxy::*)(bool)>> boxes = {
        { "showOnlyCurrentVocation", &Proxy::setShowOnlyCurrentVocation },
        { "showOnlyCurrentLevel", &Proxy::setShowOnlyCurrentLevel },
        { "showUnkownSpells", &Proxy::setShowUnkownSpells },
        { "showDruidSpells", &Proxy::setShowDruidSpells },
        { "showKnightSpells", &Proxy::setShowKnightSpells },
        { "showPaladinSpells", &Proxy::setShowPaladinSpells },
        { "showSorcererSpells", &Proxy::setShowSorcererSpells },
        { "showMonkSpells", &Proxy::setShowMonkSpells },
        { "showAttackSpellGroup", &Proxy::setShowAttackSpellGroup },
        { "showHealingSpellGroup", &Proxy::setShowHealingSpellGroup },
        { "showSupportSpellGroup", &Proxy::setShowSupportSpellGroup },
        { "showPremiumOnlySpells", &Proxy::setShowPremiumOnlySpells },
        { "showFreeSpells", &Proxy::setShowFreeSpells },
    };
    return boxes;
}

}

TSpellListWidgetController::TSpellListWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_proxy(new TSpellListDataModelSortFilterProxy(this))
{
    m_spellListModel = QVariant::fromValue<QObject*>(m_proxy);
    m_gameWindowQuickItem = gameWindowItem;
    widget()->createItem(QStringLiteral("spelllistwidget.qml"));
    if (QQuickItem* item = widget()->quickItem()) {
        m_focus.setClickArea(item);
        connect(item, &QObject::destroyed, this, &TSpellListWidgetController::onWidgetQuickItemDestroyed);
    }
    connect(m_proxy, &TSpellListDataModelSortFilterProxy::configChanged, this, &TSpellListWidgetController::saveWidgetOptions);
}

void TSpellListWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    onWidgetOptionsChanged();
}

QQuickItem* TSpellListWidgetController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }
QVariant TSpellListWidgetController::spellListModel() const { return m_spellListModel; }
bool TSpellListWidgetController::hasFocus() const { return m_hasFocus; }

void TSpellListWidgetController::onShowContextMenu()
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    TSpellListDataModelSortFilterProxy* proxy = m_proxy;
    menu->beginCustomMenu();
    const auto addBox = [this, menu](const QString& text, const bool checked, std::function<void(bool)> toggle) {
        TContextMenuEntryItem* entry = menu->addCustomEntry(text, QString());
        entry->setCheckable(true);
        entry->setChecked(checked);
        connect(entry, &TContextMenuEntryItem::toggled, this, std::move(toggle));
    };
    addBox(qtTrId("spelllist_only_current_vocation"), proxy->showOnlyCurrentVocation(),
           [proxy](const bool on) { proxy->setShowOnlyCurrentVocation(on); });
    addBox(qtTrId("spelllist_only_current_level"), proxy->showOnlyCurrentLevel(),
           [proxy](const bool on) { proxy->setShowOnlyCurrentLevel(on); });
    addBox(qtTrId("spelllist_show_only_known_spells"), !proxy->showUnkownSpells(),
           [proxy](const bool on) { proxy->setShowUnkownSpells(!on); });
    menu->addCustomSeparator();
    addBox(qtTrId("druid"), proxy->showDruidSpells(), [proxy](const bool on) { proxy->setShowDruidSpells(on); });
    addBox(qtTrId("knight"), proxy->showKnightSpells(), [proxy](const bool on) { proxy->setShowKnightSpells(on); });
    addBox(qtTrId("paladin"), proxy->showPaladinSpells(), [proxy](const bool on) { proxy->setShowPaladinSpells(on); });
    addBox(qtTrId("sorcerer"), proxy->showSorcererSpells(), [proxy](const bool on) { proxy->setShowSorcererSpells(on); });
    addBox(qtTrId("monk"), proxy->showMonkSpells(), [proxy](const bool on) { proxy->setShowMonkSpells(on); });
    addBox(qtTrId("spelllist_all_vocations"),
           proxy->showDruidSpells() && proxy->showKnightSpells() && proxy->showPaladinSpells() && proxy->showSorcererSpells()
               && proxy->showMonkSpells(),
           [proxy](const bool on) {
               proxy->setShowDruidSpells(on);
               proxy->setShowKnightSpells(on);
               proxy->setShowPaladinSpells(on);
               proxy->setShowSorcererSpells(on);
               proxy->setShowMonkSpells(on);
           });
    menu->addCustomSeparator();
    addBox(qtTrId("spell_attack"), proxy->showAttackSpellGroup(), [proxy](const bool on) { proxy->setShowAttackSpellGroup(on); });
    addBox(qtTrId("spell_healing"), proxy->showHealingSpellGroup(), [proxy](const bool on) { proxy->setShowHealingSpellGroup(on); });
    addBox(qtTrId("spell_support"), proxy->showSupportSpellGroup(), [proxy](const bool on) { proxy->setShowSupportSpellGroup(on); });
    addBox(qtTrId("spelllist_all_spell_groups"),
           proxy->showAttackSpellGroup() && proxy->showHealingSpellGroup() && proxy->showSupportSpellGroup(), [proxy](const bool on) {
               proxy->setShowAttackSpellGroup(on);
               proxy->setShowHealingSpellGroup(on);
               proxy->setShowSupportSpellGroup(on);
           });
    menu->addCustomSeparator();
    addBox(qtTrId("spelllist_premium_only"), proxy->showPremiumOnlySpells(), [proxy](const bool on) { proxy->setShowPremiumOnlySpells(on); });
    addBox(qtTrId("spelllist_free_account"), proxy->showFreeSpells(), [proxy](const bool on) { proxy->setShowFreeSpells(on); });
    menu->addCustomSeparator();
    addBox(qtTrId("spelllist_rune_spells"), proxy->showRuneSpells(), [proxy](const bool on) { proxy->setShowRuneSpells(on); });
    addBox(qtTrId("spelllist_instant_spells"), proxy->showInstantSpells(), [proxy](const bool on) { proxy->setShowInstantSpells(on); });
    menu->showCustomMenu();
}

void TSpellListWidgetController::onShowMoreForSpell(const int SpellID)
{
    // The Magical Archive tab with the spell.
    auto action = std::make_shared<input::TGameActionOpenCyclopedia>(8);
    action->setTypeId(static_cast<uint16_t>(uint16_t(SpellID)));
    input::publish(action);
}

void TSpellListWidgetController::saveWidgetOptions()
{
    if (!m_options || m_applyingOptions)
        return;
    QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    for (const auto& [key, getter] : filterBoxes())
        json.insert(QLatin1String(key), (m_proxy->*getter)());
    m_options->setSection(QLatin1String(OptionsKey), json);
}

void TSpellListWidgetController::onWidgetOptionsChanged()
{
    if (!m_options)
        return;
    const QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    if (json.contains(QLatin1String("contentHeight")))
        widget()->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                       json.value(QLatin1String("contentMaximized")).toBool(true));
    // Every box defaults to ticked; applying them does not store them back.
    m_applyingOptions = true;
    for (const auto& [key, setter] : filterBoxSetters())
        (m_proxy->*setter)(json.value(QLatin1String(key)).toBool(true));
    m_applyingOptions = false;
}

void TSpellListWidgetController::onWidgetQuickItemDestroyed() {}

void TSpellListWidgetController::onCloseClicked() { onRequestClose(); }
void TSpellListWidgetController::onRequestClose() { Q_EMIT widgetClosed(); }
void TSpellListWidgetController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
    Q_EMIT widgetWantsToMoveToSidebar(TargetSidebar, widget());
}
void TSpellListWidgetController::onSetDragActive(const bool Active)
{
    widget()->setDragActive(Active);
    Q_EMIT widgetDragChanged(parentSidebarId(), Active);
}
void TSpellListWidgetController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }
void TSpellListWidgetController::onLevelUpClicked() {}
void TSpellListWidgetController::setCursorToResizeVertical(const bool Enabled)
{
    widget()->setCursorToResizeVertical(Enabled);
}
void TSpellListWidgetController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }

void TSpellListWidgetController::takeFocus()
{
    if (m_hasFocus)
        return;
    m_hasFocus = true;
    m_focus.takeFocus();
    Q_EMIT focusChanged();
    Q_EMIT requestWidgetControlToTakeFocus();
}

void TSpellListWidgetController::releaseFocus()
{
    if (!m_hasFocus)
        return;
    m_hasFocus = false;
    m_focus.releaseFocus();
    Q_EMIT focusChanged();
}

TSpellListWidgetManager::TSpellListWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : QObject(parent)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{
}

TSpellListWidgetManager::~TSpellListWidgetManager() { delete m_controller; }

TSidebarWidget* TSpellListWidgetManager::open(config::TSidebarOptions* options)
{
    if (!m_controller) {
        m_controller = new TSpellListWidgetController(m_gameWindowItem, m_engine);
        m_controller->setSidebarOptions(options);
    }
    return m_controller->widget();
}

void TSpellListWidgetManager::close(const bool saveWidgetOptions)
{
    if (!m_controller)
        return;
    if (saveWidgetOptions)
        m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TSpellListWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

}
