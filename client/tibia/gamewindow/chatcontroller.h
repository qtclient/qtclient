#pragma once

#include "chat/chatchannel.h"
#include "chat/chatgameactionhandler.h"
#include "chat/chatmodemanager.h"
#include "chat/chatprotocolmessagehandler.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/genericentertextdialogcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/whiteblacklistdialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionmapper.h"

#include <cip_gen.h>

#include <QFont>
#include <QPointer>
#include <QSize>
#include <QStringList>
#include <QTimer>

#include <memory>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

class QQuickItem;

namespace tibia::client {
class TGameClient;
}

namespace tibia::gamewindow {

class TNpcDialogController;

/// One channel's entries for ChatOutput.qml (CIP: TChatChannelController,
/// data 0x100bd28c0, roleNames 0x100bd5070). Row 0 is the newest entry: the
/// view lays out bottom to top and CIP inserts at 0 (onIdRangeChanged
/// 0x100bd1e10).
class TChatChannelController final : public cip::tibia::gamewindow::TChatChannelController
{
    Q_OBJECT
public:
    enum Role
    {
        ChatTextRole = Qt::UserRole + 1,
        SelectionStartRole,
        SelectionEndRole,
        TutorialMarkerRole,
        DateTimeRole,
    };

    explicit TChatChannelController(QObject* parent = nullptr);

    chat::TChatChannel* channel() const { return m_channel; }
    void setChannel(chat::TChatChannel* channel);

    /// The entry of a row (row 0 newest), or null.
    const chat::TChatEntry* entryAt(int row) const;
    QString plainTextAt(int row) const;
    /// The selected text, oldest line first.
    QString selectedText() const;
    bool hasSelection() const;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool canSubscribeToServerLog() const override;
    bool isSubscribedToServerLog() const override;
    bool writesToLocalChat() const override;
    bool canBeClosed() const override;
    QString channelName() const override;

public Q_SLOTS:
    void onChatChannelControllerOptionsChanged() override;
    /// CIP 0x100bd4d80: every line is formatted again.
    void onEntryFormatChanged(bool showTimestamp, bool showTimestampSeconds, bool showLevel) override;

public:
    void startNewSelection(int entryIndex, int cursorPosition) override;
    void updateSelection(int entryIndex, int cursorPosition) override;
    void selectAll() override;
    void selectTextPassage(int entryIndex, int passageStart, int passageEnd) override;
    void setChatWidth(int width) override;
    void setChatFont(const QFont& font) override;
    void setAutoScrollToEnd(bool autoScroll) override;
    void setSubscribeToServerLog(bool subscribe) override;
    void closeChannel() override;
    void onChannelClicked(int entryId, int mouseButtonRaw, int keyboardModifiersRaw) override;

Q_SIGNALS:
    void closeRequested(chat::TChatChannel* channel);
    void entryClicked(int row, int mouseButton, int keyboardModifiers);

private:
    struct Row
    {
        QString html;
        mutable QString plain;
        mutable QSize size;
        int selectionStart = -1;
        int selectionEnd = -1;
    };

    void rebuild();
    void onEntryAdded();
    void onOldestEntryRemoved();
    Row makeRow(const chat::TChatEntry& entry) const;
    int plainLength(int row) const;
    void applySelection(int anchorRow, int anchorPos, int focusRow, int focusPos);
    void clearSelection();
    void invalidateSizes();

    QPointer<chat::TChatChannel> m_channel;
    std::vector<Row> m_rows;
    TChatChannelFilterController m_filter;
    bool m_showTimestamps = true;
    bool m_showTimestampSeconds = true;
    bool m_showLevels = true;
    int m_width = 0;
    QFont m_font;
    bool m_autoScroll = true;
    int m_anchorRow = -1;
    int m_anchorPos = 0;
};

/// The channel tabs (CIP: TChatChannelTabController, roles 0x101 channelName,
/// 0x102 tabHighlight; new entries 0x100bd8b30, flash end 0x100bd9f00).
class TChatChannelTabController final : public cip::tibia::gamewindow::TChatChannelTabController
{
    Q_OBJECT
public:
    /// gamewindow::EChatChannelTabHighlight.
    enum EHighlight : uint
    {
        NONE = 0,
        CURRENT = 1,
        FLASH_NEW_ENTRY = 2,
        NEW_ENTRY = 3,
    };

    explicit TChatChannelTabController(chat::TChatChannelStorage& storage, QObject* parent = nullptr);

    chat::TChatChannel* currentChannel() const;
    chat::TChatChannel* channelAt(int row) const;
    void makeCurrent(chat::TChatChannel* channel);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

public Q_SLOTS:
    void onMakeChannelWithRowIdCurrent(int rowId) override;
    void onMakeNextChannelCurrent() override;
    void onMakePreviousChannelCurrent() override;

public:
    void onChannelTabClicked(int rowId, int mouseButtonRaw, int keyboardModifiersRaw) override;
    void setNumberOfHiddenTabs(uint hiddenLeft, uint hiddenRight) override;
    void moveTab(int sourceIndex, int targetIndex) override;

Q_SIGNALS:
    void currentChannelChanged(chat::TChatChannel* channel);
    void tabClicked(chat::TChatChannel* channel, int mouseButton, int keyboardModifiers);

private:
    void onChannelAdded(chat::TChatChannel* channel, bool makeCurrent);
    void onChannelRemoved(chat::TChatChannel* channel);
    void onEntryAdded(chat::TChatChannel* channel, bool fromOthers);
    void setCurrentRow(int row);
    int rowOf(const chat::TChatChannel* channel) const;
    uint highlightOf(int row) const;
    void updateButtonHighlights();

    chat::TChatChannelStorage& m_storage;
    std::vector<chat::TChatChannel*> m_rows;
    std::unordered_map<const chat::TChatChannel*, uint> m_highlights;
    std::unordered_map<const chat::TChatChannel*, TChatChannelTabRenderInfo*> m_flashInfos;
    uint m_hiddenLeft = 0;
    uint m_hiddenRight = 0;
};

/// The channel list (CIP: TOpenChatChannelDialogController,
/// OpenChatChannelDialog.qml): the server's channels, then the NPCs and Loot
/// channels (FUN_100998810).
class TOpenChatChannelDialogController final : public TDialogController<cip::tibia::gamewindow::TOpenChatChannelDialogController>
{
    Q_OBJECT
public:
    explicit TOpenChatChannelDialogController(QObject* parent = nullptr);

    void open(const std::vector<chat::TChatChannelIdentifier>& channels);

    QVariantMap initialDialogProperties() const override;

public Q_SLOTS:
    void onCancelClicked() override;
    void onControllerFinished() override;
    void onOpenChannelFromList(int rowIndex) override;
    void onOpenPrivateChannelTo(const QString& recipient) override;

Q_SIGNALS:
    void channelChosen(const chat::TChatChannelIdentifier& channel);
    void privateChannelChosen(const QString& recipient);

private:
    std::vector<chat::TChatChannelIdentifier> m_channels;
};

/// The chat pane (CIP: TChatController; sendChatMessage 0x100b9e320, prefix
/// parsing 0x100b9f4a0, handleGameAction 0x100ba1070, openChannelsFromConfig
/// 0x100b9dff0, tab click 0x100b9c130). Owns the channel storage, the
/// console side of the protocol, the ignore list and the chat mode.
class TChatController final : public cip::tibia::gamewindow::TChatController
{
    Q_OBJECT
public:
    TChatController(QQuickItem* chatItem, client::TGameClient& gameClient, QObject* parent = nullptr);
    ~TChatController() override;

    /// Bind the active Chat / HudChat item (classic dock vs floated HUD).
    void setChatItem(QQuickItem* chatItem);
    QQuickItem* chatItem() const { return m_chatItem; }

    bool messageIgnoredFlash() const override { return m_ignoredFlash; }
    /// CIP 0x100ba0020: the console's exiva options button is enabled on an
    /// Optional PvP world.
    bool exivaOptionsAvailable() const override;
    int chatMode() const override { return m_mode.chatMode(); }

    chat::TChatChannelStorage& storage() { return m_storage; }
    /// CIP TWhiteBlacklist shared with the game-message (map bubble) handler.
    chat::TWhiteBlacklist& ignoreList() { return m_blacklist; }
    void setControlScheme(int scheme) { m_mapper.setControlScheme(scheme); }

    /// CIP 0x100ba2b90 (+0x58): the NPC dialog. While it has focus it takes
    /// PRESS_ENTER_IN_CHAT, while it is shown SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT
    /// (0x100ba1070), and during its product remote session the NPCs channel talks to
    /// that NPC (Lua game_remote_dialog's console hook).
    void setNpcDialog(TNpcDialogController* dialog);
    /// True while the NPC conversation dialog is on screen (product: bubble stays
    /// closed; Enter / temporary chat-on focuses that dialog instead of HudChat).
    bool npcDialogShown() const;
    /// CIP 0x100b9e320 as the NPC dialog calls it (0x100e40830): the line goes to
    /// `channel` rather than the current tab.
    QString sendChatMessageTo(chat::TChatChannel* channel, const QString& chatMessage, bool isSpell = false, int spellCastType = 0);
    /// CIP 0x100ba07b0: the chat mode's message-sent step.
    void onMessageSent() { m_mode.onMessageSent(); }

public Q_SLOTS:
    void openChannelsFromConfig() override;
    void onMessageFiltered() override;
    /// CIP 0x100ba1030 (EXIVA_WAS_SUPPRESSED): the exiva button flashes for a second.
    void onExivaSuppressed() override;
    /// CIP 0x100ba0a90: OPEN_WHITEBLACKLIST_DIALOG.
    void openWhiteBlacklistDialog() override;
    /// CIP 0x100ba0b70: OPEN_EXIVA_OPTIONS_DIALOG.
    void openExivaOptionsDialog() override;
    QString sendChatMessage(const QString& chatMessage, int chatVolume) override;
    void sendKeyword(const QString& keyword) override;
    QString getOlderChatInputHistoryEntry() override;
    QString getNewerChatInputHistoryEntry() override;
    void addEntryToChatInputHistory(const QString& chatInputText) override;
    void requestChannelList() override;
    void onChatModeButtonClicked() override;
    void onChatInputClicked() override;
    void onChatMessageSendDirectly() override;
    void onSecondaryChannelClicked(int mouseButtonRaw, int keyboardModifiersRaw) override;
    void onSelectTabAsSecondaryChannel(int rowId) override;

private:
    /// What a typed line asks for (FUN_100b9f4a0's return value).
    enum class EInput
    {
        Empty = 0,
        Default = 1,
        Say = 2,
        Yell = 3,
        Whisper = 4,
        Broadcast = 5,
        RedChannel = 6,
        Private = 7,
        RedPrivate = 8,
        Invite = 9,
        Exclude = 10,
    };
    static EInput parseInput(QString& text, QString& recipient);

    void onWorldEntered();
    void onWorldLeft();
    void bindItem();
    void reloadOptions();
    void onOptionChanged(const QString& name);
    void onChannelAdded(chat::TChatChannel* channel, bool makeCurrent);
    void onChannelListReceived(const std::vector<std::pair<uint16_t, QString>>& channels);
    void storeOpenChannels() const;

    bool handleAction(const input::TGameAction& action);
    /// CIP 0x100b9e320 with its spell arguments: a spell always goes to the
    /// local chat, with its ESpellCastOptionalTargetType. A line holding the
    /// account password is not sent; the account data warning shows instead.
    QString sendChatMessage(const QString& chatMessage, int chatVolume, bool isSpell, int spellCastType,
                            chat::TChatChannel* target = nullptr);
    void pressEnter();
    void setInputText(const QString& text, bool sendDirectly);
    void closeChannel(chat::TChatChannel* channel);
    void setSecondary(chat::TChatChannel* channel);
    void sayInChannel(chat::TChatChannel* channel, const QString& text, int volume, int spellCastType = 0);
    void sendPrivate(const QString& recipient, const QString& text, bool gamemaster);

    void showEntryMenu(TChatChannelController* model, int row, int button, int modifiers);
    void showTabMenu(chat::TChatChannel* channel, int button, int modifiers);
    void showMenu(const input::TGameActionList& candidates, std::span<const input::EGameAction> order, int context, int button,
                  int modifiers, TChatChannelController* model, chat::TChatChannel* channel);
    void runMenuAction(const input::TGameAction& action, TChatChannelController* model, chat::TChatChannel* channel);
    /// CIP 0x100ba1740: the channel's lines as its pane shows them, appended to
    /// <app data>/log/<channel>.txt.
    void saveChannel(chat::TChatChannel* channel);
    /// CIP 0x100ba03f0 / 0x100ba05d0: the name prompt for the channel.
    void openInviteDialog(chat::TChatChannel* channel, bool exclude);
    /// CIP 0x100ba2960.
    void showRecentlyTraded(const QString& name);
    void addLogMessage(const QString& text);

    QPointer<QQuickItem> m_chatItem;
    chat::TChatChannelStorage m_storage;
    chat::TChatGameActionHandler m_gameActionHandler;
    chat::TChatStorage m_chatStorage;
    chat::TWhiteBlacklist m_blacklist;
    chat::TChatModeManager m_mode;
    std::unique_ptr<chat::TChatProtocolMessageHandler> m_protocol;
    TChatChannelController m_primary;
    TChatChannelController m_secondary;
    TChatChannelTabController m_tabs;
    TOpenChatChannelDialogController m_openDialog;
    input::TGameActionMapper m_mapper;
    TWhiteBlacklistDialogController m_whiteBlacklistDialog;
    TGenericEnterTextDialogController m_inviteDialog;
    TGenericEnterTextDialogController m_excludeDialog;
    /// CIP +0x438: "recently traded" and the account data warning.
    TMessageDialogController m_messageDialog;
    QPointer<chat::TChatChannel> m_inviteTarget;
    QTimer m_ignoredFlashTimer;
    bool m_ignoredFlash = false;
    QTimer m_exivaSuppressedTimer;
    client::TGameClient& m_gameClient;
    bool m_channelListRequested = false;
    bool m_channelsFromConfigOpened = false;
    QStringList m_history;
    qsizetype m_historyIndex = 0;
    QPointer<TNpcDialogController> m_npcDialog;
    /// Cmd/Ctrl+C while chat output has a selection (CipSoft).
    int m_copySelectionListenerId = 0;
};

}
