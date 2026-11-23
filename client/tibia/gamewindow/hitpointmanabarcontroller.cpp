#include "hitpointmanabarcontroller.h"

#include "app/clientoptions.h"
#include "gamewindow/contextmenucontroller.h"

#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

constexpr auto StatusBarEnabled = "statusBarEnabled";
constexpr auto StatusPanelEnabled = "statusPanelEnabled";

bool option(const char* name)
{
    return app::ClientOptions::load().options().value(QLatin1String(name)).toBool(false);
}

void toggleOption(const char* name)
{
    app::ClientOptions::store(QLatin1String(name), !option(name));
}

}

THitpointManabarController::THitpointManabarController(QObject* parent)
    : TSidebarPanelController(parent)
{
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this,
            &THitpointManabarController::onOptionChanged);
    setVisible(option(StatusPanelEnabled));

    const auto onPlayer = [this](GameObject*, const std::vector<std::any>&) {
        onPlayerDataChanged();
        return std::any{};
    };
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onHealthChange", onPlayer);
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onManaChange", onPlayer);
    onPlayerDataChanged();
}

void THitpointManabarController::onPlayerDataChanged()
{
    if (!tibia::game::hasLocalPlayer())
        return;
    setHitpoints(static_cast<int>(tibia::game::localPlayerHealth()),
                 static_cast<int>(tibia::game::localPlayerMaxHealth()));
    setMana(static_cast<int>(tibia::game::localPlayerMana()), static_cast<int>(tibia::game::localPlayerMaxMana()));
}

void THitpointManabarController::setHitpoints(const int current, const int max)
{
    if (m_currentHitpoints != current) {
        m_currentHitpoints = current;
        Q_EMIT currentHitpointsChanged();
    }
    if (m_maxHitpoints == max)
        return;
    m_maxHitpoints = max;
    Q_EMIT maxHitpointsChanged();
}

void THitpointManabarController::setMana(const int current, const int max)
{
    if (m_currentMana != current) {
        m_currentMana = current;
        Q_EMIT currentManaChanged();
    }
    if (m_maxMana == max)
        return;
    m_maxMana = max;
    Q_EMIT maxManaChanged();
}

void THitpointManabarController::setVisible(const bool visible)
{
    m_visible = visible;
    Q_EMIT visibilityChanged();
}

void THitpointManabarController::onOptionChanged()
{
    setVisible(option(StatusPanelEnabled));
}

void THitpointManabarController::requestContextMenu()
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    const auto addOption = [&](const char* label, const char* name) {
        TContextMenuEntryItem* item = menu->addCustomEntry(qtTrId(label));
        // The option is read again when the entry is picked.
        connect(item, &TContextMenuEntryItem::triggered, this, [name] { toggleOption(name); });
        item->setCheckable(true);
        item->setChecked(option(name));
    };
    addOption("optionsmenu_statusbar_enabled", StatusBarEnabled);
    addOption("optionsmenu_statuspanel_enabled", StatusPanelEnabled);
    menu->showCustomMenu();
}

}
