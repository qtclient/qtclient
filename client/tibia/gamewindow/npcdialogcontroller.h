#pragma once

#include "input/gameactiondispatch.h"
#include "client/gamecore.h"
#include "gamewindow/chatchannelfiltercontroller.h"
#include "gamewindow/chatcontroller.h"
#include "gamewindow/dialogcontrollerwithfocushandlingbase.h"
#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"
#include "gamewindow/npckeywordbuttonmodel.h"
#include "gamewindow/npctradecontroller.h"
#include "gamewindow/uifocusmanager.h"

#include <cip_gen.h>

#include <client/outfit.h>

#include <QColor>
#include <QVariantMap>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

class QQmlEngine;
class QQuickItem;

namespace tibia {
namespace config {
class TClientOptions;
}
namespace chat {
class TChatChannelStorage;
}
namespace game {
class IGameSession;
}
namespace input {
class TEventFilterManager;
class TGameActionMapper;
}
}

namespace tibia {
namespace gamewindow {

/// NpcDialog.qml's npcOutfit (CIP: the QObject at vtable made in
/// TNpcDialogController's ctor).
class TNpcDialogOutfit final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int outfitID READ outfitID NOTIFY changed FINAL)
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY changed FINAL)
    Q_PROPERTY(bool firstAddOn READ firstAddOn NOTIFY changed FINAL)
    Q_PROPERTY(bool secondAddOn READ secondAddOn NOTIFY changed FINAL)
    Q_PROPERTY(bool isObjectOutfit READ isObjectOutfit NOTIFY changed FINAL)
public:
    using QObject::QObject;

    int outfitID() const { return m_outfitId; }
    QVariantMap colors() const { return m_colors; }
    bool firstAddOn() const { return m_addons & 1; }
    bool secondAddOn() const { return m_addons & 2; }
    bool isObjectOutfit() const { return m_isObject; }

    /// The outfit of the creature named `name` on screen, or none.
    void setFromCreatureNamed(const QString& name);
    void setOutfit(const Outfit& outfit);

Q_SIGNALS:
    void changed();

private:
    int m_outfitId = 0;
    QVariantMap m_colors;
    int m_addons = 0;
    bool m_isObject = false;
};

/// product: a conversation with an NPC that is not on screen (product opcode 120). While it
/// is open the dialog shows its name, outfit and keyword buttons, typed text and keywords
/// go to `say`, the close button to `close`, and the server's trade close keeps the dialog
/// (the NPC reopens its shop).
struct TRemoteNpcSession
{
    QString name;
    Outfit outfit;
    std::vector<TNpcKeywordButtonModel::Button> keywordButtons;
    std::function<void(const QString& text)> say;
    std::function<void()> close;
};

/// The NPC conversation window with the trade beside it, NpcDialog.qml (CIP:
/// TNpcDialogController, ctor , vtable). The NPC chat
/// window message (talk partners and keyword buttons) opens and closes it;
/// without one it opens and closes with the trade. The conversation pane
/// shows the NPCs channel through a time filter: opening the dialog hides what
/// was said before it (half a second of margin). product remote sessions bring their own keyword
/// buttons. Focus: TDialogControllerWithFocusHandlingBase (taken when the dialog
/// shows and on a click inside it, given back on a click elsewhere). The trade
/// pane has its own TUiFocusManager (+0x160), active while the trade shows; while
/// it has the keyboard the dialog's focus is not allowed. Losing the focus puts
/// the chat mode back to the one it had when the dialog took it.
class TNpcDialogController final : public TDialogControllerWithFocusHandlingBase<cip::tibia::gamewindow::TNpcDialogController>
{
    Q_OBJECT
public:
    /// The client options (NPC dialog rectangle), the game action mapper (the
    /// NPCs channel's clicks), the chat controller (+0x138: ChatInput's history
    /// and chat mode, typed lines, the conversation pane's channel), the event
    /// filter manager (the trade's focus), the QML engine and the dialog parent
    /// item the dialog and the trade are made with.
    TNpcDialogController(config::TClientOptions* ClientOptions, std::shared_ptr<input::TGameActionMapper> GameActionMapper,
                         TChatController* ChatController, input::TEventFilterManager* EventFilterManager, QQmlEngine* Engine,
                         QQuickItem* DialogParentItem, QObject* parent = nullptr);
    ~TNpcDialogController() override;
    /// CIP connector: the dialog closes; then it follows `GameSession`'s NPC
    /// conversation, NPCs channel and NPC trade.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    bool isShown() const { return dialog()->isShown(); }
    /// CIP: more than one talk partner.
    bool isMultiNpcTalk() const override { return m_talkPartners.size() > 1; }
    /// CIP: the trade pane's focus manager has the keyboard.
    bool tradeHasFocus() const override { return m_tradeFocus.hasFocus(); }
    /// CIP: the chat controller's chat mode (0 without one).
    int chatMode() const override;
    /// CIP (PRESS_ENTER_IN_CHAT while focused,): sends the input
    /// and keeps what the chat leaves of it.
    void pressEnter();
    /// CIP: PRESS_ENTER_IN_CHAT and the text actions while focused, the chat
    /// select-all for the NPCs channel and the input text while shown.
    bool handleGameAction(const input::IGameAction& action);
    /// CIP (SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT while shown): fills the input,
    /// or sends `text` (spells to the local chat) and clears it.
    void setInputText(const QString& text, bool send, bool isSpell, int spellCastType);

    /// product remote NPC session (TRemoteNpcSession): opens the dialog.
    void openRemoteSession(TRemoteNpcSession session);
    /// Ends the remote session and closes the dialog and the trade.
    void closeRemoteSession();
    bool hasRemoteSession() const { return m_remote.has_value(); }
    /// The remote session's `say`; false without a session.
    bool sayToRemoteNpc(const QString& text);

public Q_SLOTS:
    /// CIP (v14): closes the trade and the dialog.
    void closeButtonPressed() override;
    /// CIP: when the dialog is not created yet, the conversation pane starts
    /// half a second before now (session clock); then the dialog is created and shown.
    void showDialog() override;
    QString sendChatMessage(const QString& Message, bool EnforceWritesToLocalChat) override;
    QString sendChatMessage(const QString& Message) override;
    void sendKeyword(const QString& Keyword) override;
    /// positionAndSize follows the client options' NPC dialog rectangle.
    void onNpcDialogOptionsChanged() override;
    void onRequestOpenNPCTrade() override;
    void onRequestCloseNPCTrade() override;
    void onPositionChanged(int x, int y) override;
    void onSizeChanged(int NewDialogWidth, int NewContentHeight) override;
    void onTradeRequestTakeFocus() override;
    void onTradeRequestReleaseFocus() override;

Q_SIGNALS:
    /// product: chat HUD leaves the bubble closed while this dialog is shown so
    /// Enter / temporary chat-on focuses the NPC input instead of HudChat.
    void shownChanged(bool shown);

protected:
    void afterCreate() override;
    void beforeDestroy() override;
    void beforeHide() override;
    void afterShow() override;
    void afterHide() override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setShowTrade(bool show);
    /// CIP: the trade, the NPC outfit and the keyword buttons go.
    void resetConversation();
    /// CIP: losing the focus restores the chat mode saved when taking it.
    void onFocusChanged();
    /// REQUEST_SELECT_ALL_TEXT: an empty input selects the conversation, else the input.
    void selectAllText();
    /// CIP: the NPCs channel (type 2) selects the conversation since the filter's start.
    void selectAllInChannelOfType(int channelType);
    /// REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD: the input's selection, else the conversation's.
    void copySelectedText();
    void setTalkPartnerNames(const QString& names);
    void bindNpcChannel();
    /// CIP (onNpcTalkPartnersChanged) and 
    /// (onKeywordButtonsChanged): no talk partners closes the dialog, else the
    /// first partner's outfit, the names and the buttons, then shows it.
    void onNpcChatWindow(const std::vector<uint32_t>& npcIds, std::vector<TNpcKeywordButtonModel::Button> buttons);
    /// CIP : "A", "A, B and C"; a partner not on screen is the fallback name.
    static QString partnerNamesOf(const std::vector<uint32_t>& npcIds);
    /// CIP: "npc_dialog_talking_to" in the NPCs channel when the talk partners change.
    void postTalkingTo(const QString& names);

    QPointer<trade::TNPCTradeStorage> m_storage;
    std::vector<uint32_t> m_talkPartners;
    TNPCTradeController m_trade;
    /// CIP +0x210: the current session.
    std::shared_ptr<game::IGameSession> m_gameSession;
    /// CIP +0x50.
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
    TUiFocusManager m_tradeFocus;
    TNpcDialogOutfit m_outfit;
    TNpcKeywordButtonModel m_keywordButtons;
    /// CIP +0x70: the NPCs channel.
    TChatChannelController m_chat;
    /// CIP +0x110: the conversation pane's model, m_chat from the dialog's opening on.
    TChatChannelFilterController m_filter;
    QPointer<TChatController> m_chatControl;
    /// CIP +0x128.
    config::TClientOptions* m_clientOptions;
    /// CipSoft hands these to the NPCs channel controller and the trade's focus
    /// manager; qtclient's take neither yet.
    std::shared_ptr<input::TGameActionMapper> m_pGameActionMapper;
    input::TEventFilterManager* m_pEventFilterManager;
    QPointer<chat::TChatChannelStorage> m_chatStorage;
    std::optional<TRemoteNpcSession> m_remote;
    int m_savedChatMode = 0;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
