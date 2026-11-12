#include "hitpointmanabarcontroller.h"

#include "app/clientoptions.h"
#include "gamewindow/contextmenucontroller.h"

#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

constexpr auto StatusBarEnabled = "statusBarEnabled";
constexpr auto StatusPanelEnabled = "statusPanelEnabled";

}

THitpointManabarController::THitpointManabarController(QObject* parent)
    : cip::tibia::gamewindow::THitpointManabarController(parent)
{
    m_visible = app::ClientOptions::load().options().value(QLatin1String(StatusPanelEnabled)).toBool(true);
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this,
            &THitpointManabarController::onOptionChanged);

    const auto onPlayer = [this](GameObject*, const std::vector<std::any>&) {
        updateFromPlayer();
        return std::any{};
    };
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onHealthChange", onPlayer);
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onManaChange", onPlayer);
    updateFromPlayer();
}

// b1/4519a356916e9e7d
void THitpointManabarController::updateFromPlayer()
{
    if (!tibia::game::localPlayer())
        return;
    const auto update = [this](int& field, const uint32_t value, void (THitpointManabarController::*signal)()) {
        if (field == static_cast<int>(value))
            return;
        field = static_cast<int>(value);
        Q_EMIT(this->*signal)();
    };
    update(m_currentHitpoints, tibia::game::localPlayerHealth(), &THitpointManabarController::currentHitpointsChanged);
    update(m_maxHitpoints, tibia::game::localPlayerMaxHealth(), &THitpointManabarController::maxHitpointsChanged);
    update(m_currentMana, tibia::game::localPlayerMana(), &THitpointManabarController::currentManaChanged);
    update(m_maxMana, tibia::game::localPlayerMaxMana(), &THitpointManabarController::maxManaChanged);
}

// CIP .
void THitpointManabarController::onOptionChanged(const QString& name)
{
    if (name != QLatin1String(StatusPanelEnabled))
        return;
    m_visible = app::ClientOptions::load().options().value(name).toBool(true);
    Q_EMIT visibilityChanged();
}

// CIP .
// b1/d212c222008f1ce6
void THitpointManabarController::requestContextMenu()
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    const QJsonObject options = app::ClientOptions::load().options();
    const auto addOption = [&](const char* label, const char* name, const bool fallback) {
        const bool enabled = options.value(QLatin1String(name)).toBool(fallback);
        TContextMenuEntryItem* item = menu->addCustomEntry(qtTrId(label));
        item->setCheckable(true);
        item->setChecked(enabled);
        connect(item, &TContextMenuEntryItem::triggered, this,
                [name, enabled] { app::ClientOptions::store(QLatin1String(name), !enabled); });
    };
    addOption("optionsmenu_statusbar_enabled", StatusBarEnabled, true);
    addOption("optionsmenu_statuspanel_enabled", StatusPanelEnabled, false);
    menu->showCustomMenu();
}

}
