#include "playerguidewidgetcontroller.h"

#include "config/sidebaroptions.h"

#include <QJsonObject>

#include <any>
#include <vector>

namespace tibia::gamewindow {

namespace {

constexpr auto OptionsKey = "playerGuideWidgetOptions";
using Args = std::vector<std::any>;

}

TPlayerGuideWidgetController::TPlayerGuideWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
{
    // Empty CipSoft shell: player-guide entries pane with no unlocked Newhaven rows.
    // CipSoft has no CIP player-guide data events; related unlocks go through onUnlockedSpells.
    m_isShowPlayerGuideEntries = true;
    m_hasLatestUnlockedPlayerGuideEntry = false;
    m_hasPreviousUnlockedPlayerGuideEntries = false;
    m_isShowLevelUp = false;
    m_isShowAutoSpellList = false;
    widget()->createItem(QStringLiteral("PlayerGuideWidget.qml"));
}

void TPlayerGuideWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    if (!m_options)
        return;
    const QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    if (json.contains(QLatin1String("contentHeight")))
        widget()->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                       json.value(QLatin1String("contentMaximized")).toBool(true));
}

void TPlayerGuideWidgetController::onUnlockedSpells(const std::vector<uint16_t>& spellIds)
{
    // CipSoft has no player-guide entry catalogue; keep the empty entries shell.
    // Remember that unlocks arrived so CipSoft can later bind an auto-spell list.
    Q_UNUSED(spellIds);
    Q_EMIT playerGuideEntriesChanged();
}

void TPlayerGuideWidgetController::saveWidgetOptions()
{
    if (!m_options)
        return;
    QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    m_options->setSection(QLatin1String(OptionsKey), json);
}

TPlayerGuideWidgetManager::TPlayerGuideWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : QObject(parent)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{
    // Related CipSoft event while Newhaven guide packets are absent.
    m_hooks.emplace_back("g_game", "onUnlockedSpells", [this](LuaObject*, const Args& args) {
        const auto* spellIds = args.empty() ? nullptr : std::any_cast<std::vector<uint16_t>>(&args[0]);
        if (m_controller && spellIds)
            m_controller->onUnlockedSpells(*spellIds);
        return std::any{};
    });
}

TPlayerGuideWidgetManager::~TPlayerGuideWidgetManager() { delete m_controller; }

TSidebarWidget* TPlayerGuideWidgetManager::open(config::TSidebarOptions* options)
{
    if (!m_controller) {
        m_controller = new TPlayerGuideWidgetController(m_gameWindowItem, m_engine);
        m_controller->setSidebarOptions(options);
    }
    return m_controller->widget();
}

void TPlayerGuideWidgetManager::close()
{
    if (!m_controller)
        return;
    m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TPlayerGuideWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

}
