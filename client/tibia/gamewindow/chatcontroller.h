#pragma once

#include "input/gameactiondispatch.h"
#include "chat/chatchannel.h"
#include "chat/chatgameactionhandler.h"
#include "chat/chatmodemanager.h"
#include "chat/chatprotocolmessagehandler.h"
#include "gamewindow/chatchannelfiltercontroller.h"
#include "gamewindow/chatchanneltabrenderinfo.h"
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
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

class QQuickItem;

namespace tibia::game {
class IGameSession;
}

class QQmlEngine;

namespace tibia::config {
class TClientOptions;
}

namespace tibia::shared {
class IClockService;
class TTibiaFileSystemHelper;
class TTibiaUrlTools;
}

namespace tibia {
namespace gamewindow {

class TNpcDialogController;

/// One channel's entries for ChatOutput.qml (CIP: TChatChannelController,
/// data , roleNames). Row 0 is the newest entry: the
/// view lays out bottom to top and CIP inserts at 0 (onIdRangeChanged
/// ).
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

    /// CIP: the console options give the entry format and follow its
    /// changes; `Kind` tells the console's primary (0), secondary (1) and
    /// the NPC dialog's (2) channel view apart.
    TChatChannelController(std::shared_ptr<config::TClientOptions> ClientOptions,
                           std::shared_ptr<input::TGameActionMapper> GameActionMapper, int Kind, QObject* parent = nullptr);

    chat::TChatChannel* channel() const { return m_channel; }
    /// CIP: the rows go and the new channel's entries come.
    void setChannel(chat::TChatChannel* channel);
    /// The session's item prices for loot colours (colorizeLootStyle != 0).
    void setItemPriceStorage(std::shared_ptr<analysers::TItemPriceStorage> ItemPriceStorage);
    /// CIP: the channel went away (also the connector without a session).
    void resetChannel();

    /// The entry of a row (row 0 newest), or null.
    const chat::TChatEntry* entryAt(int row) const;
    QString plainTextAt(int row) const;
    /// The selected text, oldest line first.
    QString selectedText() const;
    bool hasSelection() const;
    /// Publishes the selected text for the clipboard.
    void copySelectionToClipboard();

    int rowCount(const QModelIndex& parent = {}) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool canSubscribeToServerLog() const override;
    bool isSubscribedToServerLog() const override;
    bool writesToLocalChat() const override;
    bool canBeClosed() const override;
    QString channelName() const override;

public Q_SLOTS:
    /// CIP: the console's timestamp, seconds and level options.
    void onChatChannelControllerOptionsChanged() override;
    /// CIP: every line is formatted again.
    void onEntryFormatChanged(bool showTimestamp, bool showTimestampSeconds, bool showLevel) override;

public:
    void startNewSelection(int entryIndex, int cursorPosition) override;
    void updateSelection(int entryIndex, int cursorPosition) override;
    void selectAll() override;
    /// Selects the newest rows whose entries are not older than `since`.
    void selectAllSince(const QDateTime& since);
    void selectTextPassage(int entryIndex, int passageStart, int passageEnd) override;
    void setChatWidth(int width) override;
    void setChatFont(const QFont& font) override;
    void setAutoScrollToEnd(bool autoScroll) override;
    void setSubscribeToServerLog(bool subscribe) override;
    void closeChannel() override;
    void onChannelClicked(int entryId, int mouseButtonRaw, int keyboardModifiersRaw) override;

Q_SIGNALS:
    /// CIP requestCloseChannel.
    void closeRequested(chat::TChatChannel* channel);
    /// CIP requestSubscribeToServerLog.
    void subscribeToServerLogRequested(bool subscribe, chat::TChatChannel* channel);
    void entryClicked(int row, int mouseButton, int keyboardModifiers);

private:
    /// One line (CIP's 0x38-byte row): its text, selection and entry id.
    struct Row
    {
        QString html;
        int selectionStart = 0;
        int selectionEnd = 0;
        /// Where the selection in this line is anchored.
        int selectionAnchor = 0;
        /// The line's length as plain text.
        int plainLength = 0;
        uint32_t entryId = 0;
        mutable QSize size;
        mutable QString plain;

        void clearSelection();
        void startSelection(int position);
        /// From the anchor to `position`.
        void selectToCursor(int position);
        /// From the line's start (0) or end (1) to `position`.
        void selectFromSide(int position, int side);
        void selectWhole();
        /// From the anchor to the line's end (1: its start).
        void selectAnchorTo(int side);
    };

    void onIdRangeChanged(uint32_t firstId, uint32_t afterLastId);
    void onTutorialMarkerChanged();
    void clearRows();
    void removeOldestRow();
    Row makeRow(const chat::TChatEntry& entry, uint32_t entryId) const;
    void formatRow(Row& row, const chat::TChatEntry& entry) const;
    QVariant tutorialMarkerOf(const Row& row) const;
    QSize sizeOf(const QString& html) const;
    void emitSelectionChanged(int firstRow, int lastRow);
    /// Clears the selected rows and forgets the selection.
    void clearSelection();
    void invalidateSizes();

    std::shared_ptr<config::TClientOptions> m_clientOptions;
    std::shared_ptr<input::TGameActionMapper> m_pGameActionMapper;
    int m_kind = 0;
    QPointer<chat::TChatChannel> m_channel;
    std::vector<Row> m_rows;
    TChatChannelFilterController m_filter;
    bool m_showTimestamps = true;
    bool m_showTimestampSeconds = true;
    bool m_showLevels = true;
    std::shared_ptr<analysers::TItemPriceStorage> m_lootPrices;
    /// The selected rows and the one the selection ends in.
    int m_selectionHigh = 0;
    int m_selectionLow = 0;
    int m_selectionFocus = 0;
    int m_width = 0;
    QFont m_font{ QStringLiteral("Verdana"), 8, QFont::Bold };
    bool m_autoScroll = true;
};

/// The channel tabs (CIP: TChatChannelTabController, roles 0x101 channelName,
/// 0x102 tabHighlight; new entries , flash end).
class TChatChannelTabController final : public cip::tibia::gamewindow::TChatChannelTabController
{
    Q_OBJECT
public:
    using EHighlight = TChatChannelTabRenderInfo::EHighlight;
    static constexpr uint NONE = TChatChannelTabRenderInfo::NONE;
    static constexpr uint CURRENT = TChatChannelTabRenderInfo::CURRENT;
    static constexpr uint FLASH_NEW_ENTRY = TChatChannelTabRenderInfo::FLASH_NEW_ENTRY;
    static constexpr uint NEW_ENTRY = TChatChannelTabRenderInfo::NEW_ENTRY;

    explicit TChatChannelTabController(QObject* parent = nullptr);

    /// The session's channels become the tabs (the first one current); null
    /// clears them.
    void setChannelStorage(chat::TChatChannelStorage* storage);

    /// The current tab's channel, null when no tab is current.
    chat::TChatChannel* currentChannel() const;
    /// The channel of tab `row`, null outside the tabs.
    chat::TChatChannel* channelAt(int row) const;
    /// The tabs' channels, in tab order.
    std::vector<chat::TChatChannelIdentifier> identifiers() const;
    /// While the session's first channels arrive (false), new tabs do not
    /// become current.
    void setMakeNewTabsCurrent(bool makeNewTabsCurrent);
    /// The channel went to the secondary pane: its tab is no longer marked
    /// (unless current).
    void resetTabHighlight(const chat::TChatChannelIdentifier& identifier);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int currentRowId() const override;
    /// The strongest highlight among the tabs hidden left (FLASH_NEW_ENTRY
    /// wins, then NEW_ENTRY).
    uint leftButtonHighlight() const override;
    /// The same for the tabs hidden right.
    uint rightButtonHighlight() const override;

public Q_SLOTS:
    void onMakeChannelWithRowIdCurrent(int rowId) override;
    void onMakeNextChannelCurrent() override;
    void onMakePreviousChannelCurrent() override;

public:
    void onChannelTabClicked(int rowId, int mouseButtonRaw, int keyboardModifiersRaw) override;
    void setNumberOfHiddenTabs(uint hiddenLeft, uint hiddenRight) override;
    void moveTab(int sourceIndex, int targetIndex) override;

    void onAddNewChatChannelTab(const chat::TChatChannelIdentifier& identifier, bool forceMakeCurrent);
    void onChatChannelReopend(const chat::TChatChannelIdentifier& identifier, bool forceMakeCurrent);
    void onChatChannelRemoved(const chat::TChatChannelIdentifier& identifier);
    void onEntryAddedToChatChannel(const chat::TChatChannelIdentifier& identifier);
    void onMakeChannelWithChatChannelIdentifierCurrent(const chat::TChatChannelIdentifier& identifier);

Q_SIGNALS:
    void currentChannelChanged(chat::TChatChannel* channel);
    void tabClicked(chat::TChatChannel* channel, int mouseButton, int keyboardModifiers);

private:
    /// A tab's highlight changed: its row repaints, and a scroll button
    /// whose hidden tabs include it.
    void onTabHighlightChanged(chat::TChatChannel* channel);
    /// The tab's render info, made when the tab is added.
    void addRenderInfo(chat::TChatChannel* channel);
    TChatChannelTabRenderInfo* renderInfo(const chat::TChatChannel* channel) const;
    void setCurrentRow(int row);
    /// Every tab goes (none current).
    void removeAllTabs();
    int rowOf(const chat::TChatChannel* channel) const;
    int rowOf(const chat::TChatChannelIdentifier& identifier) const;
    uint highlightOf(int row) const;

    std::vector<chat::TChatChannel*> m_rows;
    QPointer<chat::TChatChannelStorage> m_storage;
    std::unordered_map<const chat::TChatChannel*, std::unique_ptr<TChatChannelTabRenderInfo>> m_renderInfos;
    uint m_hiddenLeft = 0;
    uint m_hiddenRight = 0;
    bool m_makeNewTabsCurrent = false;
};

/// The channel list (CIP: TOpenChatChannelDialogController,
/// OpenChatChannelDialog.qml): the server's channels, then the NPCs and Loot
/// channels .
class TOpenChatChannelDialogController final : public TDialogController<cip::tibia::gamewindow::TOpenChatChannelDialogController>
{
    Q_OBJECT
public:
    /// CIP: the dialog is made in `engine` under `dialogParentItem`.
    TOpenChatChannelDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent = nullptr);
    /// CIP v27: the dialog's item goes.
    void destroyDialog() { dialog()->destroy(); }

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

/// The chat pane (CIP: TChatController; sendChatMessage , prefix
/// parsing , handleGameAction , openChannelsFromConfig
/// , tab click). Owns the channel storage, the
/// console side of the protocol, the ignore list and the chat mode.
class TChatController final : public cip::tibia::gamewindow::TChatController
{
    Q_OBJECT
public:
    /// CIP: the input history keeps `MaxChatInputHistoryLength` lines; the
    /// dialogs are made in `Engine` under `DialogParentItem`; `ChatItem` gets
    /// the controller and its models; the channel views read the client
    /// options; channels are saved through the file system helper with the
    /// clock's time; the chat mode is the client's manager.
    TChatController(uint MaxChatInputHistoryLength, QQuickItem* DialogParentItem, QQuickItem* ChatItem, QQmlEngine* Engine,
                    std::shared_ptr<config::TClientOptions> ClientOptions, std::shared_ptr<input::TGameActionMapper> GameActionMapper,
                    std::shared_ptr<shared::TTibiaFileSystemHelper> TibiaFileSystemHelper,
                    std::shared_ptr<shared::IClockService> ClockService, std::shared_ptr<shared::TTibiaUrlTools> TibiaUrlTools,
                    std::shared_ptr<chat::TChatModeManager> ChatModeManager, QObject* parent = nullptr);
    ~TChatController() override;

    /// Bind the active Chat / HudChat item (classic dock vs floated HUD).
    void setChatItem(QQuickItem* chatItem);
    QQuickItem* chatItem() const { return m_chatItem; }

    bool messageIgnoredFlash() const override { return m_ignoredFlash; }
    /// CIP : the console's exiva options button is enabled on an
    /// Optional PvP world.
    bool exivaOptionsAvailable() const override;
    int chatMode() const override { return m_pChatModeManager->chatMode(); }
    /// The client's chat mode manager the chat was given.
    chat::TChatModeManager& chatModeManager() { return *m_pChatModeManager; }

    /// The current session's channels, or null.
    chat::TChatChannelStorage* storage() const { return m_chatChannelStorage.get(); }
    /// CIP connector: the open channels are stored and the tabs, the input
    /// and the dialogs are cleared; then the tabs show `GameSession`'s
    /// channels, and its login opens the channels from the options.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    /// CIP (+0x58): the NPC dialog. While it has focus it takes
    /// PRESS_ENTER_IN_CHAT, while it is shown SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT.
    /// Host: during its external conversation (TExternalNpcConversation) the
    /// NPCs channel talks to that NPC.
    void setNpcDialog(TNpcDialogController* dialog);
    /// CIP as the NPC dialog calls it : the line goes to
    /// `channel` rather than the current tab.
    QString sendChatMessageTo(chat::TChatChannel* channel, const QString& chatMessage, bool isSpell = false, int spellCastType = 0);
    /// CIP : the chat mode's message-sent step.
    void onMessageSent() { m_pChatModeManager->onMessageSent(); }

public Q_SLOTS:
    void openChannelsFromConfig() override;
    void onMessageFiltered() override;
    /// CIP (EXIVA_WAS_SUPPRESSED): the exiva button flashes for a second.
    void onExivaSuppressed() override;
    /// CIP : OPEN_WHITEBLACKLIST_DIALOG.
    void openWhiteBlacklistDialog() override;
    /// CIP : OPEN_EXIVA_OPTIONS_DIALOG.
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
    /// The channel dialog's choice: OPEN_CHAT_CHANNEL with its identifier.
    void onRequestOpenChanel(const tibia::chat::TChatChannelIdentifier& ChannelIdentifier);

private:
    /// GET_CHAT_CHANNEL_LIST: asks the server; its answer opens the channel dialog.
    void requestChannelListFromServer();
    /// The console buttons stop flashing 1000 ms after the last trigger.
    void onIgnoredFlashTimeout();
    void onExivaSuppressedFlashTimeout();
    void onChatChannelRemoved(chat::TChatChannel* channel);
    /// What a typed line asks for ('s return value).
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
    void bindItem();
    void onChannelAdded(chat::TChatChannel* channel, bool makeCurrent);
    void onChannelListReceived(const std::vector<chat::TChatChannelIdentifier>& channels);
    void storeOpenChannels() const;

    bool handleAction(const input::IGameAction& action);
    /// CIP with its spell arguments: a spell always goes to the
    /// local chat, with its ESpellCastOptionalTargetType. A line holding the
    /// account password is not sent; the account data warning shows instead.
    QString sendChatMessage(const QString& chatMessage, int chatVolume, bool isSpell, int spellCastType,
                            chat::TChatChannel* target = nullptr);
    void pressEnter();
    void setInputText(const QString& text, bool sendDirectly);
    void closeChannel(chat::TChatChannel* channel);
    /// CIP: a channel controller's close request becomes CLOSE_CHAT_CHANNEL.
    void onRequestCloseChannel(chat::TChatChannel* channel);
    /// CIP: CHAT_CHANNEL_SUBSCRIBE_TO_SERVER_LOG or CHAT_CHANNEL_UNSUBSCRIBE_SERVER_LOG.
    void onRequestSubscribeToServerLog(bool subscribe, chat::TChatChannel* channel);
    void setSecondary(chat::TChatChannel* channel);
    void sayInChannel(chat::TChatChannel* channel, const QString& text, int volume, int spellCastType = 0);
    void sendPrivate(const QString& recipient, const QString& text, bool gamemaster);

    void showEntryMenu(TChatChannelController* model, int row, int button, int modifiers);
    /// REQUEST_SELECT_ALL_TEXT: with the chat input holding the keyboard, an
    /// empty input selects the primary channel, otherwise the input's text.
    void selectAllText();
    /// REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD: the input's selection first,
    /// then the primary channel's, then the secondary one's.
    void copySelectedText();
    void onChatChannelTabClicked(chat::TChatChannel* channel, int button, int modifiers);
    void showMenu(const input::TGameActionList& candidates, std::span<const input::EGameAction> order, int context, int button,
                  int modifiers, TChatChannelController* model, chat::TChatChannel* channel);
    void runMenuAction(const input::IGameActionPtr& action, TChatChannelController* model, chat::TChatChannel* channel);
    /// CIP : the channel's lines as its pane shows them, appended to
    /// <app data>/log/<channel>.txt.
    void saveChannel(chat::TChatChannel* channel);
    /// CIP / : the name prompt for the channel.
    void openInviteDialog(chat::TChatChannel* channel, bool exclude);
    /// CIP: the prompt's name is invited to / excluded from its channel.
    void onPlayerNameToInviteToChatChannelReceived(const QString& playerName);
    void onPlayerNameToExcludeFromChatChannelReceived(const QString& playerName);
    /// CIP .
    void showRecentlyTraded(const QString& name);

    QPointer<QQuickItem> m_chatItem;
    /// CIP +0x60 .. +0xb0, as given.
    std::shared_ptr<config::TClientOptions> m_clientOptions;
    std::shared_ptr<input::TGameActionMapper> m_pGameActionMapper;
    std::shared_ptr<shared::TTibiaFileSystemHelper> m_pTibiaFileSystemHelper;
    std::shared_ptr<shared::IClockService> m_pClockService;
    std::shared_ptr<shared::TTibiaUrlTools> m_pTibiaUrlTools;
    std::shared_ptr<chat::TChatModeManager> m_pChatModeManager;
    /// The current session's channels' actions (made by the connector).
    std::shared_ptr<chat::TChatGameActionProvider> m_actionProvider;
    /// CIP +0x4b0: the current session.
    std::shared_ptr<game::IGameSession> m_gameSession;
    /// CIP +0x4c0: the session's chat protocol message handler.
    std::shared_ptr<chat::TChatProtocolMessageHandler> m_chatProtocolMessageHandler;
    /// CIP +0x4d0: the session's channels.
    std::shared_ptr<chat::TChatChannelStorage> m_chatChannelStorage;
    /// CIP +0x4e0: the connections to the session's objects.
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
    TChatChannelController m_primary;
    TChatChannelController m_secondary;
    TChatChannelTabController m_tabs;
    TOpenChatChannelDialogController m_openDialog;
    TGenericEnterTextDialogController m_inviteDialog;
    TGenericEnterTextDialogController m_excludeDialog;
    /// CIP +0x438: "recently traded" and the account data warning.
    TMessageDialogController m_messageDialog;
    QPointer<chat::TChatChannel> m_inviteTarget;
    /// CIP +0x500: started on connect; new tabs become current once it ran.
    QTimer m_makeNewTabsCurrentTimer;
    QTimer m_ignoredFlashTimer;
    bool m_ignoredFlash = false;
    QTimer m_exivaSuppressedTimer;
    bool m_channelListRequested = false;
    bool m_channelsFromConfigOpened = false;
    /// CIP: the chat input history, at most m_maxChatInputHistoryLength lines
    /// (the client's maxChatInputHistoryLength setting, given at creation).
    qsizetype m_maxChatInputHistoryLength = 0;
    QStringList m_history;
    qsizetype m_historyIndex = 0;
    QPointer<TNpcDialogController> m_npcDialog;
    /// Cmd/Ctrl+C while chat output has a selection (CipSoft).

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }
    /// The "recently traded" / account warning message closes.
    void closeMessageDialog();

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
