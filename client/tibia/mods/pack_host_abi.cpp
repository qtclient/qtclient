#include "mods/pack_host_abi.h"

#include "mods/attention_backend.h"
#include "mods/instance_backend.h"
#include "mods/mission_backend.h"

#include <qtclient/pack_host.h>

#include <QString>
#include <QQuickItem>

namespace tibia::mods {

namespace {

qtclient::PackHostWire* g_packWire = nullptr;
QtClientPackHostApiV1 g_packAbi{};

int ph_valid(void*)
{
    return (g_packWire && g_packWire->valid()) ? 1 : 0;
}

void ph_bind_quest_tracker(void*, void* mission_backend, QtClientBrowseFn browse, void* browse_ctx)
{
    if (!g_packWire || !g_packWire->bindQuestTracker || !mission_backend)
        return;
    g_packWire->bindQuestTracker(static_cast<MissionBackend*>(mission_backend),
                                 [browse, browse_ctx](const QString& filter) {
                                     if (browse && browse_ctx) {
                                         const QByteArray u = filter.toUtf8();
                                         browse(browse_ctx, u.constData());
                                     }
                                 });
}

void ph_bind_quest_log(void*, void* mission_backend, QtClientBrowseFn browse, void* browse_ctx)
{
    if (!g_packWire || !g_packWire->bindQuestLog || !mission_backend)
        return;
    g_packWire->bindQuestLog(static_cast<MissionBackend*>(mission_backend),
                             [browse, browse_ctx](const QString& filter) {
                                 if (browse && browse_ctx) {
                                     const QByteArray u = filter.toUtf8();
                                     browse(browse_ctx, u.constData());
                                 }
                             });
}

void ph_bind_instance_to_info_dialogs(void*, void* instance_backend)
{
    if (!g_packWire || !g_packWire->bindInstanceToInfoDialogs || !instance_backend)
        return;
    g_packWire->bindInstanceToInfoDialogs(static_cast<InstanceBackend*>(instance_backend));
}

void ph_bind_attention_to_button_bar(void*, void* attention_backend)
{
    if (!g_packWire || !g_packWire->bindAttentionToButtonBar || !attention_backend)
        return;
    g_packWire->bindAttentionToButtonBar(static_cast<AttentionBackend*>(attention_backend));
}

void ph_add_product_options_page(void*, int category, void* options_page)
{
    if (!g_packWire || !g_packWire->addProductOptionsPage || !options_page)
        return;
    g_packWire->addProductOptionsPage(category, options_page);
}

void ph_open_options_page(void*, int page)
{
    if (!g_packWire || !g_packWire->openOptionsPage)
        return;
    g_packWire->openOptionsPage(page);
}

void ph_open_store(void*)
{
    if (!g_packWire || !g_packWire->openStore)
        return;
    g_packWire->openStore();
}

void ph_request_logout_via_button_bar(void*)
{
    if (!g_packWire || !g_packWire->requestLogoutViaButtonBar)
        return;
    g_packWire->requestLogoutViaButtonBar();
}

void ph_set_social_button_on_button_bar(void*, void* controller)
{
    if (!g_packWire || !g_packWire->setSocialButtonOnButtonBar)
        return;
    g_packWire->setSocialButtonOnButtonBar(static_cast<QObject*>(controller));
}

void ph_set_active_chat_item(void*, void* qquick_item)
{
    if (!g_packWire || !g_packWire->setActiveChatItem)
        return;
    g_packWire->setActiveChatItem(static_cast<QQuickItem*>(qquick_item));
}

void ph_install_create_character_controller(void*, void* controller)
{
    if (!g_packWire || !g_packWire->installCreateCharacterController)
        return;
    g_packWire->installCreateCharacterController(static_cast<QObject*>(controller));
}

void* ph_blessings_dialog(void*)
{
    if (!g_packWire || !g_packWire->blessingsDialog)
        return nullptr;
    return g_packWire->blessingsDialog();
}

void* ph_npc_dialog(void*)
{
    if (!g_packWire || !g_packWire->npcDialog)
        return nullptr;
    return g_packWire->npcDialog();
}

void* ph_chat_controller(void*)
{
    if (!g_packWire || !g_packWire->chatController)
        return nullptr;
    return g_packWire->chatController();
}

void* ph_chat_storage(void*)
{
    if (!g_packWire || !g_packWire->chatStorage)
        return nullptr;
    return g_packWire->chatStorage();
}

void* ph_store_messages(void*)
{
    if (!g_packWire || !g_packWire->storeMessages)
        return nullptr;
    return g_packWire->storeMessages();
}

void* ph_store_image_manager(void*)
{
    if (!g_packWire || !g_packWire->storeImageManager)
        return nullptr;
    return g_packWire->storeImageManager();
}

void* ph_minimap_controller(void*)
{
    if (!g_packWire || !g_packWire->minimapController)
        return nullptr;
    return g_packWire->minimapController();
}

void* ph_sidebar_manager(void*)
{
    if (!g_packWire || !g_packWire->sidebarManager)
        return nullptr;
    return g_packWire->sidebarManager();
}

void* ph_sidebar_widgets(void*)
{
    if (!g_packWire || !g_packWire->sidebarWidgets)
        return nullptr;
    return g_packWire->sidebarWidgets();
}

void* ph_containers(void*)
{
    if (!g_packWire || !g_packWire->containers)
        return nullptr;
    return g_packWire->containers();
}

void* ph_button_bar(void*)
{
    if (!g_packWire || !g_packWire->buttonBar)
        return nullptr;
    return g_packWire->buttonBar();
}

void* ph_imbuing_dialog(void*)
{
    if (!g_packWire || !g_packWire->imbuingDialog)
        return nullptr;
    return g_packWire->imbuingDialog();
}

void* ph_imbuing_storage(void*)
{
    if (!g_packWire || !g_packWire->imbuingStorage)
        return nullptr;
    return g_packWire->imbuingStorage();
}

void* ph_session_bootstrap(void*)
{
    if (!g_packWire || !g_packWire->sessionBootstrap)
        return nullptr;
    return g_packWire->sessionBootstrap();
}

void* ph_minimap_panel_item(void*)
{
    if (!g_packWire || !g_packWire->minimapPanelItem)
        return nullptr;
    return g_packWire->minimapPanelItem();
}

void* ph_inventory_panel_item(void*)
{
    if (!g_packWire || !g_packWire->inventoryPanelItem)
        return nullptr;
    return g_packWire->inventoryPanelItem();
}

void* ph_button_bar_panel_item(void*)
{
    if (!g_packWire || !g_packWire->buttonBarPanelItem)
        return nullptr;
    return g_packWire->buttonBarPanelItem();
}

} // namespace

void fillPackHostAbiFromPackHostWire(qtclient::PackHostWire* wire)
{
    g_packWire = wire;
    g_packAbi = QtClientPackHostApiV1{
        .valid = &ph_valid,
        .bind_quest_tracker = &ph_bind_quest_tracker,
        .bind_quest_log = &ph_bind_quest_log,
        .bind_instance_to_info_dialogs = &ph_bind_instance_to_info_dialogs,
        .bind_attention_to_button_bar = &ph_bind_attention_to_button_bar,
        .add_product_options_page = &ph_add_product_options_page,
        .open_options_page = &ph_open_options_page,
        .open_store = &ph_open_store,
        .request_logout_via_button_bar = &ph_request_logout_via_button_bar,
        .set_social_button_on_button_bar = &ph_set_social_button_on_button_bar,
        .set_active_chat_item = &ph_set_active_chat_item,
        .install_create_character_controller = &ph_install_create_character_controller,
        .blessings_dialog = &ph_blessings_dialog,
        .npc_dialog = &ph_npc_dialog,
        .chat_controller = &ph_chat_controller,
        .chat_storage = &ph_chat_storage,
        .store_messages = &ph_store_messages,
        .store_image_manager = &ph_store_image_manager,
        .minimap_controller = &ph_minimap_controller,
        .sidebar_manager = &ph_sidebar_manager,
        .sidebar_widgets = &ph_sidebar_widgets,
        .containers = &ph_containers,
        .button_bar = &ph_button_bar,
        .imbuing_dialog = &ph_imbuing_dialog,
        .imbuing_storage = &ph_imbuing_storage,
        .session_bootstrap = &ph_session_bootstrap,
        .minimap_panel_item = &ph_minimap_panel_item,
        .inventory_panel_item = &ph_inventory_panel_item,
        .button_bar_panel_item = &ph_button_bar_panel_item,
    };
}

const QtClientPackHostApiV1* packHostAbiTable()
{
    return &g_packAbi;
}

} // namespace tibia::mods
