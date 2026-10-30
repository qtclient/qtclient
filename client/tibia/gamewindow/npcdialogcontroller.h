#pragma once

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/npckeywordbuttonmodel.h"
#include "gamewindow/npctradecontroller.h"
#include "gamewindow/uifocusmanager.h"

#include <cip_gen.h>

#include <client/outfit.h>

#include <QColor>
#include <QVariantMap>

#include <functional>
#include <optional>
#include <vector>

namespace tibia::chat {
class TChatChannelStorage;
}

namespace tibia::gamewindow {

class TChatChannelController;
class TChatController;

/// NpcDialog.qml's npcOutfit (CIP: the QObject at vtable 0x1026801e0 made in
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
/// TNpcDialogController, ctor 0x100e3da40, vtable 0x102635488). The NPC chat
/// window message (talk partners and keyword buttons) opens and closes it;
/// without one it opens and closes with the trade. The conversation pane
/// shows the NPCs channel. product remote sessions bring their own keyword
/// buttons. Focus (CIP TDialogControllerWithFocusHandlingBase, take
/// 0x100e47bc0 / release 0x100e47c20 with an application event filter): taken
/// when the dialog opens and on a press inside it, released on a press
/// outside it. The trade pane has its own TUiFocusManager (+0x160), active
/// while the trade shows, for its search field.
class TNpcDialogController final : public TDialogController<cip::tibia::gamewindow::TNpcDialogController>
{
    Q_OBJECT
public:
    explicit TNpcDialogController(trade::TNPCTradeStorage* storage, QObject* parent = nullptr);
    ~TNpcDialogController() override;

    /// v18 (CIP 0x100e47a70).
    bool hasPersonalMouseShield() const override { return true; }

    /// CIP ctor's chat controller (+0x138, chatController): ChatInput's history and chat
    /// mode, typed lines, and the conversation pane's channel (chatChannelModel, the NPCs
    /// channel). Before the first show.
    void setChatController(TChatController* controller);

    bool isShown() const { return dialog()->isShown(); }
    /// CIP 0x100e3fa00 (PRESS_ENTER_IN_CHAT while focused, 0x100e3f250): sends the input
    /// and keeps what the chat leaves of it.
    void pressEnter();
    /// CIP 0x100e3f500 (SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT while shown): fills the input,
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
    /// CIP 0x100e3fc20 (v14): closes the trade and the dialog.
    void closeButtonPressed() override;
    QString sendChatMessage(const QString& Message, bool EnforceWritesToLocalChat) override;
    QString sendChatMessage(const QString& Message) override;
    void sendKeyword(const QString& Keyword) override;
    void onRequestOpenNPCTrade() override;
    void onRequestCloseNPCTrade() override;
    void onPositionChanged(int x, int y) override;
    void onSizeChanged(int NewDialogWidth, int NewContentHeight) override;
    void onTradeRequestTakeFocus() override;
    void onTradeRequestReleaseFocus() override;
    void takeFocus() override;
    void releaseFocus() override;

Q_SIGNALS:
    /// product: chat HUD leaves the bubble closed while this dialog is shown so
    /// Enter / temporary chat-on focuses the NPC input instead of HudChat.
    void shownChanged(bool shown);

protected:
    void afterShow() override;
    void afterHide() override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setShowTrade(bool show);
    void setTalkPartnerNames(const QString& names);
    void bindNpcChannel();
    /// CIP 0x100e3e580 (onNpcTalkPartnersChanged) and 0x100e3efa0
    /// (onKeywordButtonsChanged): no talk partners closes the dialog, else the
    /// first partner's outfit, the names and the buttons, then shows it.
    void onNpcChatWindow(const std::vector<uint32_t>& npcIds, std::vector<TNpcKeywordButtonModel::Button> buttons);
    /// CIP 0x100e3fe40: "A", "A, B and C"; a partner not on screen is the fallback name.
    static QString partnerNamesOf(const std::vector<uint32_t>& npcIds);

    QPointer<trade::TNPCTradeStorage> m_storage;
    std::vector<uint32_t> m_talkPartners;
    std::vector<client::gamecore::Hook> m_hooks;
    TNPCTradeController m_trade;
    TUiFocusManager m_tradeFocus;
    TNpcDialogOutfit m_outfit;
    TNpcKeywordButtonModel m_keywordButtons;
    QPointer<TChatController> m_chatControl;
    QPointer<chat::TChatChannelStorage> m_chatStorage;
    TChatChannelController* m_chat = nullptr;
    std::optional<TRemoteNpcSession> m_remote;
};

}
