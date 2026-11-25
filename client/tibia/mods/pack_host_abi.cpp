// Host API v9: the game window chrome table (abi/pack_host_api.h): the options dialog's
// extension pages, store / options / logout, the chat item, CipSoft's sidebar panels and
// the map window's overlay input. Reads the game window's objects (mods/abi_internal.h).
#include "mods/pack_host_abi.h"
#include "mods/abi_internal.h"

#include "config/optionspages.h"
#include "gamewindow/chatcontroller.h"
#include "gamewindow/mapwindowcontroller.h"
#include "gamewindow/optionsmenudialogcontroller.h"
#include "gamewindow/sidebarpanelsmanager.h"
#include "gamewindow/storedialogcontroller.h"

#include <QPointer>
#include <QQuickItem>

#include <cstring>
#include <memory>
#include <vector>

namespace tibia::mods {

namespace {

/// A pack's options page (CipSoft TOptionsPage over the pack's load / apply).
class TAbiOptionsPage final : public config::TOptionsPage
{
public:
    explicit TAbiOptionsPage(const QtClientOptionsPageV1& page)
        : m_page(page)
    {
    }
    void load(const config::TClientOptions&) override
    {
        if (m_page.load)
            m_page.load(m_page.user);
    }
    void apply(config::TClientOptions&) const override
    {
        if (m_page.apply)
            m_page.apply(m_page.user);
    }
    int category() const { return m_page.category; }

    /// The options dialog the page was added to (a new game window adds it again).
    QPointer<gamewindow::TOptionsMenuDialogController> addedTo;

private:
    QtClientOptionsPageV1 m_page;
};

std::vector<std::unique_ptr<TAbiOptionsPage>>& optionsPages()
{
    static std::vector<std::unique_ptr<TAbiOptionsPage>> pages;
    return pages;
}

void addPagesToDialog()
{
    const HostObjects* host = hostObjects();
    if (!host || !host->optionsDialog)
        return;
    for (const auto& page : optionsPages()) {
        if (page->addedTo == host->optionsDialog)
            continue;
        host->optionsDialog->addExtensionPage(page->category(), page.get());
        page->addedTo = host->optionsDialog;
    }
}

int ph_valid(void*)
{
    return hostObjects() ? 1 : 0;
}

void ph_add_options_page(void*, const QtClientOptionsPageV1* page)
{
    if (!page)
        return;
    QtClientOptionsPageV1 copy{};
    std::memcpy(&copy, page, std::min(page->struct_size ? page->struct_size : sizeof(copy), sizeof(copy)));
    optionsPages().push_back(std::make_unique<TAbiOptionsPage>(copy));
    addPagesToDialog();
}

void ph_open_options_page(void*, int page)
{
    if (const HostObjects* host = hostObjects(); host && host->optionsDialog)
        host->optionsDialog->open(page);
}

void ph_open_store(void*)
{
    if (const HostObjects* host = hostObjects(); host && host->storeDialog)
        host->storeDialog->open();
}

void ph_request_logout_via_button_bar(void*)
{
    if (const HostObjects* host = hostObjects(); host && host->requestLogoutViaButtonBar)
        host->requestLogoutViaButtonBar();
}

void ph_set_active_chat_item(void*, QtClientQQuickItemPtr item)
{
    if (const HostObjects* host = hostObjects(); host && host->chat && item)
        host->chat->setChatItem(static_cast<QQuickItem*>(item));
}

QtClientQQuickItemPtr ph_minimap_panel_item(void*)
{
    const HostObjects* host = hostObjects();
    return host && host->sidebarPanels ? host->sidebarPanels->minimapItem() : nullptr;
}

QtClientQQuickItemPtr ph_inventory_panel_item(void*)
{
    const HostObjects* host = hostObjects();
    return host && host->sidebarPanels ? host->sidebarPanels->inventoryItem() : nullptr;
}

QtClientQQuickItemPtr ph_button_bar_panel_item(void*)
{
    const HostObjects* host = hostObjects();
    return host && host->sidebarPanels ? host->sidebarPanels->buttonBarItem() : nullptr;
}

gamewindow::TMapWindowController* mapWindow()
{
    const HostObjects* host = hostObjects();
    return host ? host->mapWindowController : nullptr;
}

void ph_map_overlay_hover(void*, int hovering, double x, double y)
{
    if (gamewindow::TMapWindowController* map = mapWindow())
        map->setOverlayHover(hovering != 0, x, y);
}

void ph_map_overlay_click(void*, int x, int y, int mouse_button, int keyboard_modifiers)
{
    if (gamewindow::TMapWindowController* map = mapWindow())
        map->onMapWindowClicked(x, y, mouse_button, keyboard_modifiers);
}

void ph_map_overlay_start_drag(void*, int x, int y)
{
    if (gamewindow::TMapWindowController* map = mapWindow())
        map->onMapWindowStartDrag(x, y, nullptr);
}

const QtClientPackHostApiV1 g_packAbi = {
    .valid = &ph_valid,
    .add_options_page = &ph_add_options_page,
    .open_options_page = &ph_open_options_page,
    .open_store = &ph_open_store,
    .request_logout_via_button_bar = &ph_request_logout_via_button_bar,
    .set_active_chat_item = &ph_set_active_chat_item,
    .minimap_panel_item = &ph_minimap_panel_item,
    .inventory_panel_item = &ph_inventory_panel_item,
    .button_bar_panel_item = &ph_button_bar_panel_item,
    .map_overlay_hover = &ph_map_overlay_hover,
    .map_overlay_click = &ph_map_overlay_click,
    .map_overlay_start_drag = &ph_map_overlay_start_drag,
};

} // namespace

void bindPackHostToHost()
{
    addPagesToDialog();
}

const QtClientPackHostApiV1* packHostAbiTable()
{
    return &g_packAbi;
}

} // namespace tibia::mods
