#include "spelllistwidgetcontroller.h"

#include "config/characterconfigurationmanager.h"
#include "config/sidebaroptions.h"
#include "game/gamesessionbase.h"
#include "gamewindow/contextmenucontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <QQuickItem>

#include <array>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace tibia::gamewindow {

namespace {

using Proxy = TSpellListDataModelSortFilterProxy;
using Options = config::TSpellListWidgetOptions;

/// A filter box the widget options keep (not the rune and instant boxes):
/// its options field and the proxy's getter and setter, in the options' order.
struct TFilterBox
{
    bool Options::*field;
    bool (Proxy::*get)() const;
    void (Proxy::*set)(bool);
};

const std::array<TFilterBox, 13>& filterBoxes()
{
    static const std::array<TFilterBox, 13> boxes = { {
        { &Options::showOnlyCurrentVocation, &Proxy::showOnlyCurrentVocation, &Proxy::setShowOnlyCurrentVocation },
        { &Options::showOnlyCurrentLevel, &Proxy::showOnlyCurrentLevel, &Proxy::setShowOnlyCurrentLevel },
        { &Options::showUnknownSpells, &Proxy::showUnkownSpells, &Proxy::setShowUnkownSpells },
        { &Options::showDruidSpells, &Proxy::showDruidSpells, &Proxy::setShowDruidSpells },
        { &Options::showKnightSpells, &Proxy::showKnightSpells, &Proxy::setShowKnightSpells },
        { &Options::showPaladinSpells, &Proxy::showPaladinSpells, &Proxy::setShowPaladinSpells },
        { &Options::showSorcererSpells, &Proxy::showSorcererSpells, &Proxy::setShowSorcererSpells },
        { &Options::showMonkSpells, &Proxy::showMonkSpells, &Proxy::setShowMonkSpells },
        { &Options::showAttackSpellGroup, &Proxy::showAttackSpellGroup, &Proxy::setShowAttackSpellGroup },
        { &Options::showHealingSpellGroup, &Proxy::showHealingSpellGroup, &Proxy::setShowHealingSpellGroup },
        { &Options::showSupportSpellGroup, &Proxy::showSupportSpellGroup, &Proxy::setShowSupportSpellGroup },
        { &Options::showPremiumOnlySpells, &Proxy::showPremiumOnlySpells, &Proxy::setShowPremiumOnlySpells },
        { &Options::showFreeSpells, &Proxy::showFreeSpells, &Proxy::setShowFreeSpells },
    } };
    return boxes;
}

}

TSpellListWidgetController::TSpellListWidgetController(spells::TSpellInfoStorage* SpellInfoStorage, QQuickItem* gameWindowItem,
                                                       QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_proxy(new TSpellListDataModelSortFilterProxy(SpellInfoStorage, this))
{
    m_spellListModel = QVariant::fromValue<QObject*>(m_proxy);
    m_gameWindowQuickItem = gameWindowItem;
    widget()->createItem(QStringLiteral("spelllistwidget.qml"));
    if (QQuickItem* item = widget()->quickItem()) {
        m_focus.setClickArea(item);
        connect(item, &QObject::destroyed, this, &TSpellListWidgetController::onWidgetQuickItemDestroyed);
    }
    connect(&m_focus, &TUiFocusManager::focusChanged, this, &TSpellListWidgetController::focusChanged);
    connect(m_proxy, &TSpellListDataModelSortFilterProxy::configChanged, this, &TSpellListWidgetController::saveWidgetOptions);
}

QQuickItem* TSpellListWidgetController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }
QVariant TSpellListWidgetController::spellListModel() const { return m_spellListModel; }
bool TSpellListWidgetController::hasFocus() const { return m_focus.hasFocus(); }

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
    action->setSpellId(static_cast<uint16_t>(SpellID));
    Q_EMIT publishGameAction(action);
}

void TSpellListWidgetController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSessionConnections.disconnectAll();
    m_options = nullptr;
    m_gameSession = GameSession;
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get())) {
        if (config::TCharacterConfigurationManager* configuration = session->characterConfigurationManager().get())
            m_options = configuration->sidebarOptions();
        if (m_options) {
            m_gameSessionConnections.add(connect(m_options.data(), &config::TSidebarOptions::spellListWidgetOptionsChanged, this,
                                                 &TSpellListWidgetController::onWidgetOptionsChanged));
            onWidgetOptionsChanged();
        }
    }
    m_proxy->onGameSessionChanged();
}

void TSpellListWidgetController::saveWidgetOptions()
{
    if (!m_options || m_applyingOptions)
        return;
    config::TSpellListWidgetOptions options;
    options.setContentHeightAndMaximized(widget()->maximizedContentHeight(), widget()->isMaximized());
    for (const TFilterBox& box : filterBoxes())
        options.*box.field = (m_proxy->*box.get)();
    m_options->setSpellListWidgetOptions(options);
}

void TSpellListWidgetController::onWidgetOptionsChanged()
{
    if (!m_options)
        return;
    const config::TSpellListWidgetOptions& options = m_options->spellListWidgetOptions();
    widget()->setInitialContentHeightAndMaximized(options.contentHeight, options.contentMaximized);
    // Host: the proxy's configChanged saves the options; applying them does not.
    m_applyingOptions = true;
    for (const TFilterBox& box : filterBoxes())
        (m_proxy->*box.set)(options.*box.field);
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

// CIP TSidebarWidgetControllerWithFocusHandling: the widget's focus is its
// TUiFocusManager's (a click outside the widget gives it back).
void TSpellListWidgetController::takeFocus()
{
    m_focus.takeFocus();
}

void TSpellListWidgetController::releaseFocus()
{
    m_focus.releaseFocus();
}

TSpellListWidgetManager::TSpellListWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : QObject(parent)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{
}

TSpellListWidgetManager::~TSpellListWidgetManager() { delete m_controller; }

TSidebarWidget* TSpellListWidgetManager::open(const std::shared_ptr<game::IGameSession>& GameSession)
{
    if (!m_controller) {
        const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
        m_controller = new TSpellListWidgetController(session ? session->spellInfoStorage() : nullptr, m_gameWindowItem, m_engine);
        connect(m_controller, &TSpellListWidgetController::publishGameAction, this, &TSpellListWidgetManager::publishGameAction);
        m_controller->connectToGameSession(GameSession);
    }
    return m_controller->widget();
}

void TSpellListWidgetManager::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    if (m_controller)
        m_controller->connectToGameSession(GameSession);
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
