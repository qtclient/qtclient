// Host API v9: the chat table (abi/chat_api.h) over the session's CipSoft
// TChatChannelStorage, the game window's TChatController (chat mode), its
// TNpcDialogController (the host's external NPC conversation) and the map window's
// TSpeechBubbleOverlayQuickItem (status message inset).
#include "mods/abi_internal.h"

#include "chat/chatchannel.h"
#include "chat/chatgameactionhandler.h"
#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"
#include "gamewindow/chatcontroller.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/npcdialogcontroller.h"
#include "qmlcomponents/speechbubbleoverlay.h"

#include <client/outfit.h>

#include <QColor>
#include <QPointer>
#include <QQuickItem>
#include <QString>

#include <exception>
#include <functional>
#include <memory>
#include <vector>

namespace tibia::mods {
namespace {

gamewindow::TNpcDialogController* npcDialog()
{
    const HostObjects* host = hostObjects();
    return host ? host->npcDialog : nullptr;
}

void chatAddNpcEntry(int message_mode, const char* text_utf8, const char* speaker_utf8)
{
    game::TGameSessionBase* session = game::currentGameSession();
    if (!session || !session->chatChannelStorage() || !text_utf8)
        return;
    QColor color;
    try {
        color = chat::consoleColor(message_mode);
    } catch (const std::exception&) {
        return;
    }
    session->chatChannelStorage()->addNpcEntry(color, QString::fromUtf8(text_utf8),
                                               QString::fromUtf8(speaker_utf8 ? speaker_utf8 : ""));
}

uint32_t chatConsoleColor(int message_mode)
{
    try {
        return chat::consoleColor(message_mode).rgba();
    } catch (const std::exception&) {
        return 0;
    }
}

int chatMode()
{
    const HostObjects* host = hostObjects();
    return host && host->chat ? host->chat->chatMode() : QTCLIENT_CHAT_MODE_OFF;
}

struct TChatWatch
{
    Subscribers<std::function<void(int)>> modeListeners;
    QPointer<gamewindow::TChatController> bound;
    QMetaObject::Connection modeConnection;
    int statusMargin = 0;
    /// The external conversation the pack opened (its `ended` runs once).
    struct TConversation
    {
        void* user = nullptr;
        void (*ended)(void*) = nullptr;
    };
    std::shared_ptr<TConversation> conversation;
    QPointer<gamewindow::TNpcDialogController> boundNpc;
    std::vector<QMetaObject::Connection> npcConnections;
};

TChatWatch& chatWatch()
{
    static TChatWatch w;
    return w;
}

void endConversation()
{
    TChatWatch& w = chatWatch();
    if (std::shared_ptr<TChatWatch::TConversation> c = std::exchange(w.conversation, nullptr); c && c->ended)
        c->ended(c->user);
}

/// The NPC dialog dropped the external conversation (closed, the game window going).
void checkConversation()
{
    gamewindow::TNpcDialogController* npc = npcDialog();
    if (chatWatch().conversation && (!npc || !npc->hasExternalConversation()))
        endConversation();
}

void bindChat()
{
    TChatWatch& w = chatWatch();
    const HostObjects* host = hostObjects();
    gamewindow::TChatController* chat = host ? host->chat : nullptr;
    if (w.bound != chat) {
        QObject::disconnect(w.modeConnection);
        w.bound = chat;
        if (chat)
            w.modeConnection = QObject::connect(chat, &cip::tibia::gamewindow::TChatController::chatModeChanged, chat,
                                                [] { chatWatch().modeListeners.call(chatMode()); });
    }
    gamewindow::TNpcDialogController* npc = npcDialog();
    if (w.boundNpc != npc) {
        for (const QMetaObject::Connection& c : w.npcConnections)
            QObject::disconnect(c);
        w.npcConnections.clear();
        w.boundNpc = npc;
        if (npc) {
            w.npcConnections.push_back(QObject::connect(npc, &cip::tibia::gamewindow::TDialogControllerBase::dialogClosed, npc,
                                                        [] { checkConversation(); }, Qt::QueuedConnection));
            w.npcConnections.push_back(QObject::connect(npc, &cip::tibia::gamewindow::TNpcDialogController::talkPartnersChanged,
                                                        npc, [] { checkConversation(); }, Qt::QueuedConnection));
        }
    }
    checkConversation();
}

int chatSubscribeModeChanged(void (*fn)(void* user, int mode), void* user)
{
    if (!fn)
        return 0;
    bindChat();
    return chatWatch().modeListeners.add([fn, user](const int mode) { fn(user, mode); });
}

void chatUnsubscribeModeChanged(int id)
{
    chatWatch().modeListeners.remove(id);
}

Outfit outfitFromAbi(const QtClientCreatureOutfitInfo& info)
{
    Outfit outfit;
    if (!info.exists)
        return outfit;
    if (info.is_object)
        outfit.setAuxId(info.looktype);
    else
        outfit.setId(info.looktype);
    outfit.setHead(info.head);
    outfit.setBody(info.body);
    outfit.setLegs(info.legs);
    outfit.setFeet(info.feet);
    outfit.setAddons(info.addons);
    return outfit;
}

int chatOpenExternal(const QtClientExternalNpcConversationV1* conversation)
{
    gamewindow::TNpcDialogController* npc = npcDialog();
    if (!npc || !conversation)
        return 0;
    bindChat();
    // A new conversation replaces the one shown.
    endConversation();
    auto record = std::make_shared<TChatWatch::TConversation>(TChatWatch::TConversation{ conversation->user, conversation->ended });
    gamewindow::TExternalNpcConversation external;
    external.name = QString::fromUtf8(conversation->name_utf8 ? conversation->name_utf8 : "");
    external.outfit = outfitFromAbi(conversation->outfit);
    for (size_t i = 0; conversation->keywords && i < conversation->keyword_count; ++i) {
        const QtClientNpcKeyword& k = conversation->keywords[i];
        external.keywordButtons.push_back({ k.id, QString::fromUtf8(k.text_utf8 ? k.text_utf8 : "") });
    }
    void* user = conversation->user;
    if (auto* say = conversation->say)
        external.say = [say, user](const QString& text) {
            const QByteArray utf8 = text.toUtf8();
            say(user, utf8.constData());
        };
    if (auto* close = conversation->close)
        external.close = [close, user] { close(user); };
    chatWatch().conversation = record;
    npc->openExternalConversation(std::move(external));
    return 1;
}

void chatCloseExternal()
{
    if (gamewindow::TNpcDialogController* npc = npcDialog())
        npc->closeExternalConversation();
    endConversation();
}

int chatNpcHasExternal()
{
    gamewindow::TNpcDialogController* npc = npcDialog();
    return npc && npc->hasExternalConversation() ? 1 : 0;
}

int chatNpcTalkPartnerNames(char* buf, size_t buf_len)
{
    gamewindow::TNpcDialogController* npc = npcDialog();
    return writeSizedOut(npc ? npc->talkPartnerNames() : QString(), buf, buf_len);
}

void chatNpcClose()
{
    if (gamewindow::TNpcDialogController* npc = npcDialog())
        npc->closeButtonPressed();
}

void applyStatusMargin()
{
    const HostObjects* host = hostObjects();
    QQuickItem* pane = host ? host->mapWindowPane : nullptr;
    if (!pane)
        return;
    if (auto* overlay = pane->findChild<qmlcomponents::TSpeechBubbleOverlayQuickItem*>(QStringLiteral("speechBubbleOverlay")))
        overlay->setStatusBottomMargin(chatWatch().statusMargin);
}

void chatSetStatusBottomMargin(int px)
{
    chatWatch().statusMargin = std::max(0, px);
    applyStatusMargin();
}

const QtClientChatApiV1 g_chatApi = {
    .struct_size = sizeof(QtClientChatApiV1),
    .add_npc_entry = &chatAddNpcEntry,
    .console_color_rgba = &chatConsoleColor,
    .chat_mode = &chatMode,
    .subscribe_chat_mode_changed = &chatSubscribeModeChanged,
    .unsubscribe_chat_mode_changed = &chatUnsubscribeModeChanged,
    .open_external_npc_conversation = &chatOpenExternal,
    .close_external_npc_conversation = &chatCloseExternal,
    .npc_dialog_has_external_conversation = &chatNpcHasExternal,
    .npc_dialog_talk_partner_names = &chatNpcTalkPartnerNames,
    .npc_dialog_close = &chatNpcClose,
    .set_status_bottom_margin = &chatSetStatusBottomMargin,
};

} // namespace

void bindChatWatchers()
{
    bindChat();
    if (chatWatch().statusMargin != 0)
        applyStatusMargin();
}

const QtClientChatApiV1* chatApiV1()
{
    return &g_chatApi;
}

} // namespace tibia::mods
