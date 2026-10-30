#include "spelllistwidgetcontroller.h"

#include "config/sidebaroptions.h"
#include "gamewindow/contextmenucontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <QJsonObject>
#include <QQuickItem>

#include <any>
#include <memory>
#include <vector>

namespace tibia::gamewindow {

namespace {

constexpr auto OptionsKey = "spellListWidgetOptions";
using Args = std::vector<std::any>;

}

TSpellListWidgetController::TSpellListWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_model(new TMagicalArchiveSpellListModel(this))
    , m_proxy(new TSpellListDataModelSortFilterProxy(this))
{
    m_proxy->setSourceModel(m_model);
    m_proxy->resetFilter();
    m_proxy->sort(0);
    m_spellListModel = QVariant::fromValue<QObject*>(m_proxy);
    m_gameWindowQuickItem = gameWindowItem;
    widget()->createItem(QStringLiteral("spelllistwidget.qml"));
    if (QQuickItem* item = widget()->quickItem())
        m_focus.setClickArea(item);
}

void TSpellListWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    if (!m_options)
        return;
    const QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    if (json.contains(QLatin1String("contentHeight")))
        widget()->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                       json.value(QLatin1String("contentMaximized")).toBool(true));
}

void TSpellListWidgetController::reloadArchive()
{
    if (m_model)
        m_model->reload();
}

void TSpellListWidgetController::onShowContextMenu()
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    connect(menu->addCustomEntry(qtTrId("spelllist_open_magical_archive")), &TContextMenuEntryItem::triggered, this, [] {
        auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_CYCLOPEDIA));
        action->parameter = 8; // Magical Archive tab
        input::publish(action);
    });
    connect(menu->addCustomEntry(qtTrId("spelllist_reset_filter")), &TContextMenuEntryItem::triggered, this, [this] {
        m_proxy->resetFilter();
    });
    menu->showCustomMenu();
}

void TSpellListWidgetController::onShowMoreForSpell(const int SpellID)
{
    Q_UNUSED(SpellID);
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_CYCLOPEDIA));
    action->parameter = 8; // Magical Archive tab
    input::publish(action);
}

void TSpellListWidgetController::saveWidgetOptions()
{
    if (!m_options)
        return;
    QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    m_options->setSection(QLatin1String(OptionsKey), json);
}

void TSpellListWidgetController::takeFocus()
{
    if (m_hasFocus)
        return;
    m_hasFocus = true;
    m_focus.takeFocus();
    Q_EMIT focusChanged();
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
    // CipSoft archive model: CastPossibleRole reads LocalPlayer spells; reload on unlock / mods.
    const auto reload = [this](LuaObject*, const Args&) {
        if (m_controller)
            m_controller->reloadArchive();
        return std::any{};
    };
    m_hooks.emplace_back("g_game", "onUnlockedSpells", reload);
    m_hooks.emplace_back("g_game", "onSpellDataModification", reload);
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

void TSpellListWidgetManager::close()
{
    if (!m_controller)
        return;
    m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TSpellListWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

}
