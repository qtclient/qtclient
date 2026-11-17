#include "clientoptionsmodeswitcher.h"

#include "config/clientoptions.h"
#include "config/sidebaroptions.h"
#include "input/gameactionhandlers.h"

namespace tibia::config {

TClientOptionsModeSwitcher::TClientOptionsModeSwitcher(TClientOptions* options, QObject* parent)
    : cip::tibia::config::TClientOptionsModeSwitcher(parent)
    , m_options(options)
{
    input::setActionHandler(input::EGameAction::SWITCH_CLIENTOPTIONS_MODE, [this](const input::TGameAction& action) {
        handleGameAction(action);
        return true;
    });
}

TClientOptionsModeSwitcher::~TClientOptionsModeSwitcher()
{
    input::setActionHandler(input::EGameAction::SWITCH_CLIENTOPTIONS_MODE, nullptr);
}

void TClientOptionsModeSwitcher::connectToGameSession(std::function<TSidebarOptions*()> sidebarOptions)
{
    m_sidebarOptions = std::move(sidebarOptions);
}

void TClientOptionsModeSwitcher::handleGameAction(const input::TGameAction& action)
{
    if (action.type != input::EGameAction::SWITCH_CLIENTOPTIONS_MODE)
        return;
    switchClientOptionsMode(m_options->clientOptionsMode(), action.parameter);
}

void TClientOptionsModeSwitcher::switchClientOptionsMode(const int currentMode, const int requestedMode)
{
    if (currentMode == 0 && requestedMode == 1) {
        m_options->saveToJsonFile();
        m_options->loadDefaultClientOptions();
        m_options->setLeftSmartClickControlScheme();
        m_options->setClientOptionsMode(1);
        m_options->setIntOption(EClientOption::gameWindowUpperPaneHeight, 362);
        m_options->setIntOption(EClientOption::gameWindowLowerPaneHeight, 216);
        m_options->setIntOption(EClientOption::gameWindowResizingMode, 0);
        m_options->announceAllOptions();
        setControlButtonsVisible(false);
        Q_EMIT configureSidebarFromClientOptions();
    } else if (currentMode == 1 && requestedMode == 0) {
        m_options->load();
        m_options->setLeftSmartClickControlScheme();
        m_options->setClientOptionsMode(0);
        m_options->announceAllOptions();
        setControlButtonsVisible(true);
        Q_EMIT configureSidebarFromClientOptions();
    }
}

void TClientOptionsModeSwitcher::setControlButtonsVisible(const bool visible)
{
    TSidebarOptions* sidebar = m_sidebarOptions ? m_sidebarOptions() : nullptr;
    if (!sidebar)
        return;
    SidebarPanelsOptions panels = sidebar->sidebarPanelsOptions();
    panels.controlButtonsVisible = visible;
    sidebar->setSidebarPanelsOptions(panels);
}

}
