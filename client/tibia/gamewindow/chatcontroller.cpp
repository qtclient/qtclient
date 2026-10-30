#include "chatcontroller.h"

#include "app/clientoptions.h"
#include "client/gameclient.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/npcdialogcontroller.h"
#include "mods/registry.h"
#include "input/inputciptypes.h"
#include "input/crosshairgameactionhandlers.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QLocale>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextDocument>

#include <algorithm>
#include <cmath>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using chat::TChatChannel;
using chat::TChatChannelIdentifier;
using chat::TChatEntry;

/// Rule contexts of the chat (CIP: onChatChannelTabClicked 0x100b9c130,
/// onChannelClicked 0x100bd46c0).
constexpr int ChatTabContext = 4;
constexpr int ChatEntryContext = 5;

/// CIP EWorldType's Optional PvP (0x100ba0020).
constexpr int WorldTypeOptionalPvp = 1;

/// CIP 0x100bd46c0's context menu order.
constexpr A EntryMenuOrder[] = {
    A::OPEN_MESSAGE_TO_CHAT_CHANNEL, A::INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL, A::ADD_NEW_VIP, A::IGNORE_NAME, A::UNIGNORE_NAME,
    A::CONTEXT_MENU_SEPARATOR, A::CHAT_CHANNEL_SELECT_ALL, A::COPY_TEXT_TO_CLIPBOARD, A::COPY_CHAT_ENTRY_TO_CLIPBOARD,
    A::CONTEXT_MENU_SEPARATOR, A::REPORT_STATEMENT, A::REPORT_NAME, A::CONTEXT_MENU_SEPARATOR, A::REPORT_BUG_TYPO,
    A::RECENTLY_TRADED, A::CONTEXT_MENU_SEPARATOR, A::CONTEXT_MENU_SEPARATOR, A::COPY_CREATURE_NAME_TO_CLIPBOARD,
};

/// The game actions TChatController::handleAction takes (CIP 0x100ba1070),
/// including tab/entry menu actions that runMenuAction also handles with an
/// explicit channel/model (here they target the current / secondary channel).
constexpr A HandledActions[] = {
    A::GET_CHAT_CHANNEL_LIST, A::OPEN_CHAT_CHANNEL, A::OPEN_CHAT_CHANNEL_AND_MAKE_CURRENT, A::OPEN_MESSAGE_TO_CHAT_CHANNEL,
    A::CLOSE_CURRENT_CHAT_CHANNEL, A::TOGGLE_SERVER_LOG_SUBSCRIPTION_IN_CURRENT_CHAT_CHANNEL, A::INVITE_PLAYER_TO_CHAT_CHANNEL,
    A::INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL, A::EXCLUDE_PLAYER_FROM_CHAT_CHANNEL, A::PRESS_ENTER_IN_CHAT,
    A::SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT, A::DISPLAY_NEXT_CHAT_CHANNEL_TAB, A::DISPLAY_PREVIOUS_CHAT_CHANNEL_TAB,
    A::ADD_SYSTEM_CHAT_MESSAGE, A::IGNORE_NAME, A::UNIGNORE_NAME, A::SWITCH_CHAT_MODE, A::OPEN_WHITEBLACKLIST_DIALOG,
    A::OPEN_INVITE_TO_CHAT_CHANNEL_DIALOG, A::OPEN_EXCLUDE_FROM_CHAT_CHANNEL_DIALOG, A::SAVE_CHAT_CHANNEL_TO_FILE, A::RECENTLY_TRADED,
    A::GET_GUILD_MESSAGE, A::SHOW_CHANNEL_JOIN_LEAVE_MESSAGES, A::HIDE_CHANNEL_JOIN_LEAVE_MESSAGES, A::EXIVA_WAS_SUPPRESSED,
    A::SEND_CHAT_MESSAGE_WITH_SPEECH_VOLUME, A::SEND_CHAT_MESSAGE_AS_GAMEMASTER_BROADCAST, A::SEND_CHAT_MESSAGE_WITH_CHANNEL_ID,
    A::SEND_CHAT_MESSAGE_WITH_RECIPIENT, A::SEND_CHAT_MESSAGE_TO_NPC, A::CLOSE_CHAT_CHANNEL, A::CLEAR_CHAT_CHANNEL,
    A::CHAT_CHANNEL_SUBSCRIBE_TO_SERVER_LOG, A::CHAT_CHANNEL_UNSUBSCRIBE_SERVER_LOG, A::CHAT_CHANNEL_MUTE, A::CHAT_CHANNEL_UNMUTE,
    A::SHOW_CHAT_CHANNEL_AS_SECONDARY, A::SELECT_CHAT_CHANNEL_AS_SECONDARY, A::HIDE_SECONDARY_CHAT_CHANNEL,
    A::CHAT_CHANNEL_SELECT_ALL, A::COPY_CHAT_ENTRY_TO_CLIPBOARD,
};

/// CIP 0x100b9c130's context menu order.
constexpr A TabMenuOrder[] = {
    A::OPEN_INVITE_TO_CHAT_CHANNEL_DIALOG, A::OPEN_EXCLUDE_FROM_CHAT_CHANNEL_DIALOG, A::CONTEXT_MENU_SEPARATOR,
    A::GET_GUILD_MESSAGE, A::SHOW_CHANNEL_JOIN_LEAVE_MESSAGES, A::HIDE_CHANNEL_JOIN_LEAVE_MESSAGES, A::CONTEXT_MENU_SEPARATOR,
    A::CLOSE_CHAT_CHANNEL, A::CHAT_CHANNEL_SUBSCRIBE_TO_SERVER_LOG, A::CHAT_CHANNEL_UNSUBSCRIBE_SERVER_LOG, A::CONTEXT_MENU_SEPARATOR,
    A::SHOW_CHAT_CHANNEL_AS_SECONDARY, A::HIDE_SECONDARY_CHAT_CHANNEL, A::SELECT_CHAT_CHANNEL_AS_SECONDARY, A::CONTEXT_MENU_SEPARATOR,
    A::CHAT_CHANNEL_MUTE, A::CHAT_CHANNEL_UNMUTE, A::CONTEXT_MENU_SEPARATOR, A::SAVE_CHAT_CHANNEL_TO_FILE, A::CLEAR_CHAT_CHANNEL,
};

input::TGameActionPtr textAction(const A type, const QString& text)
{
    std::shared_ptr<input::TGameAction> action;
    switch (type) {
    case A::COPY_TEXT_TO_CLIPBOARD:
        action = std::make_shared<input::TGameActionCopyTextToClipboard>(text);
        break;
    case A::COPY_TRUSTED_TEXT_TO_CLIPBOARD:
        action = std::make_shared<input::TGameActionCopyTrustedTextToClipboard>(text);
        break;
    case A::COPY_CHAT_ENTRY_TO_CLIPBOARD:
        action = std::make_shared<input::TGameActionCopyChatEntryToClipboard>(text);
        break;
    case A::COPY_CREATURE_NAME_TO_CLIPBOARD:
        action = std::make_shared<input::TGameActionCopyCreatureNameToClipboard>(text);
        break;
    case A::ADD_NEW_VIP:
        action = std::make_shared<input::TGameActionAddNewVip>(text);
        break;
    case A::EXCLUDE_PLAYER_FROM_CHAT_CHANNEL:
        action = std::make_shared<input::TGameActionExcludePlayerFromChatChannel>(text);
        break;
    case A::INVITE_PLAYER_TO_CHAT_CHANNEL:
        action = std::make_shared<input::TGameActionChatChannelModeration>(type, text);
        break;
    case A::OPEN_MESSAGE_TO_CHAT_CHANNEL:
    case A::INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL:
    case A::IGNORE_NAME:
    case A::UNIGNORE_NAME:
    case A::REPORT_NAME:
    case A::RECENTLY_TRADED:
        action = std::make_shared<input::TGameActionCreatureWithName>(type, text);
        break;
    case A::SELECT_CHAT_CHANNEL_AS_SECONDARY:
    case A::OPEN_CHAT_CHANNEL:
    case A::OPEN_CHAT_CHANNEL_AND_MAKE_CURRENT:
    case A::CLOSE_CHAT_CHANNEL:
    case A::CLEAR_CHAT_CHANNEL:
    case A::CHAT_CHANNEL_MUTE:
    case A::CHAT_CHANNEL_UNMUTE:
    case A::SHOW_CHAT_CHANNEL_AS_SECONDARY:
    case A::HIDE_SECONDARY_CHAT_CHANNEL:
    case A::SAVE_CHAT_CHANNEL_TO_FILE:
    case A::CHAT_CHANNEL_SELECT_ALL:
        action = std::make_shared<input::TGameActionChatChannel>(type);
        action->text = text;
        break;
    default:
        action = input::mutableClone(*input::makeAction(type));
        action->text = text;
        break;
    }
    return action;
}

int ownLevel()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player ? static_cast<int>(player->getLevel()) : 0;
}

QString ownName()
{
    return QString::fromStdString(g_game.getCharacterName());
}

bool isVip(const QString& name)
{
    for (const auto& [id, vip] : g_game.getVips()) {
        if (QString::fromStdString(std::get<0>(vip)).compare(name, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

void own(QObject* object)
{
    QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);
}

}

// ---------------------------------------------------------------------------
// TChatChannelController

TChatChannelController::TChatChannelController(QObject* parent)
    : cip::tibia::gamewindow::TChatChannelController(parent)
    , m_filter(this)
{
    m_filter.setChannelModel(this);
}

void TChatChannelController::setChannel(chat::TChatChannel* channel)
{
    if (m_channel == channel)
        return;
    if (m_channel)
        disconnect(m_channel, nullptr, this, nullptr);
    m_channel = channel;
    if (channel) {
        connect(channel, &TChatChannel::entryAdded, this, &TChatChannelController::onEntryAdded);
        connect(channel, &TChatChannel::oldestEntryRemoved, this, &TChatChannelController::onOldestEntryRemoved);
        connect(channel, &TChatChannel::cleared, this, &TChatChannelController::rebuild);
        connect(channel, &TChatChannel::propertiesChanged, this, &TChatChannelController::chatChannelPropertyChanged);
        connect(channel, &QObject::destroyed, this, [this] {
            m_channel = nullptr;
            rebuild();
            Q_EMIT chatChannelChanged();
            Q_EMIT chatChannelPropertyChanged();
        });
    }
    rebuild();
    m_filter.syncFromChannel(canSubscribeToServerLog(), isSubscribedToServerLog(), writesToLocalChat(), canBeClosed(), channelName());
    Q_EMIT chatChannelChanged();
    Q_EMIT chatChannelPropertyChanged();
    Q_EMIT scrollViewToEnd();
}

const chat::TChatEntry* TChatChannelController::entryAt(const int row) const
{
    if (!m_channel || row < 0 || row >= static_cast<int>(m_channel->entries().size()))
        return nullptr;
    const auto& entries = m_channel->entries();
    return &entries[entries.size() - 1 - static_cast<size_t>(row)];
}

QString TChatChannelController::plainTextAt(const int row) const
{
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
        return {};
    const Row& r = m_rows[static_cast<size_t>(row)];
    if (r.plain.isNull()) {
        QTextDocument document;
        document.setHtml(r.html);
        r.plain = document.toPlainText();
    }
    return r.plain;
}

int TChatChannelController::plainLength(const int row) const
{
    return static_cast<int>(plainTextAt(row).size());
}

QString TChatChannelController::selectedText() const
{
    QStringList lines;
    for (int row = static_cast<int>(m_rows.size()) - 1; row >= 0; --row) {
        const Row& r = m_rows[static_cast<size_t>(row)];
        if (r.selectionStart < 0 || r.selectionEnd <= r.selectionStart)
            continue;
        lines.append(plainTextAt(row).mid(r.selectionStart, r.selectionEnd - r.selectionStart));
    }
    return lines.join(QLatin1Char('\n'));
}

bool TChatChannelController::hasSelection() const
{
    return std::any_of(m_rows.begin(), m_rows.end(), [](const Row& r) { return r.selectionStart >= 0 && r.selectionEnd > r.selectionStart; });
}

int TChatChannelController::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant TChatChannelController::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_rows.size()))
        return {};
    const Row& r = m_rows[static_cast<size_t>(index.row())];
    switch (role) {
    case ChatTextRole:
        return r.html;
    case SelectionStartRole:
        return r.selectionStart;
    case SelectionEndRole:
        return r.selectionEnd;
    case TutorialMarkerRole:
        // CIP ChatOutput treats undefined as "no marker". A sentinel with
        // startIndex=-1 made QML call positionToRectangle(-1) → QTextCursor warnings.
        return {};
    case DateTimeRole:
        if (const TChatEntry* entry = entryAt(index.row()))
            return entry->time;
        return {};
    case Qt::SizeHintRole:
        if (!r.size.isValid()) {
            QTextDocument document;
            document.setDocumentMargin(0);
            document.setDefaultFont(m_font);
            document.setHtml(r.html);
            document.setTextWidth(m_width > 0 ? m_width : -1);
            r.size = QSize(static_cast<int>(std::ceil(document.size().width())), static_cast<int>(std::ceil(document.size().height())));
        }
        return r.size;
    default:
        return {};
    }
}

QHash<int, QByteArray> TChatChannelController::roleNames() const
{
    return {
        { ChatTextRole, "chatText" },
        { SelectionStartRole, "selectionStartIndex" },
        { SelectionEndRole, "selectionEndIndex" },
        { Qt::SizeHintRole, "sizeHint" },
        { TutorialMarkerRole, "tutorialMarker" },
        { DateTimeRole, "dateTime" },
    };
}

bool TChatChannelController::canSubscribeToServerLog() const
{
    return m_channel && m_channel->canSubscribeToServerLog();
}

bool TChatChannelController::isSubscribedToServerLog() const
{
    return m_channel && m_channel->isSubscribedToServerLog();
}

bool TChatChannelController::writesToLocalChat() const
{
    return !m_channel || m_channel->writesToLocalChat();
}

bool TChatChannelController::canBeClosed() const
{
    return m_channel && m_channel->canBeClosed();
}

QString TChatChannelController::channelName() const
{
    return m_channel ? m_channel->name() : QString();
}

void TChatChannelController::onChatChannelControllerOptionsChanged()
{
    rebuild();
}

void TChatChannelController::onEntryFormatChanged(const bool showTimestamp, const bool showTimestampSeconds, const bool showLevel)
{
    if (m_showTimestamps == showTimestamp && m_showTimestampSeconds == showTimestampSeconds && m_showLevels == showLevel)
        return;
    m_showTimestamps = showTimestamp;
    m_showTimestampSeconds = showTimestampSeconds;
    m_showLevels = showLevel;
    rebuild();
}

void TChatChannelController::rebuild()
{
    beginResetModel();
    m_rows.clear();
    m_anchorRow = -1;
    if (m_channel) {
        const auto& entries = m_channel->entries();
        m_rows.reserve(entries.size());
        for (auto it = entries.rbegin(); it != entries.rend(); ++it)
            m_rows.push_back(makeRow(*it));
    }
    endResetModel();
}

TChatChannelController::Row TChatChannelController::makeRow(const chat::TChatEntry& entry) const
{
    Row row;
    row.html = chat::formatEntry(entry, m_showTimestamps, m_showTimestampSeconds, m_showLevels);
    return row;
}

void TChatChannelController::onEntryAdded()
{
    if (!m_channel || m_channel->entries().empty())
        return;
    beginInsertRows({}, 0, 0);
    m_rows.insert(m_rows.begin(), makeRow(m_channel->entries().back()));
    if (m_anchorRow >= 0)
        ++m_anchorRow;
    endInsertRows();
    if (m_autoScroll)
        Q_EMIT scrollViewToEnd();
}

void TChatChannelController::onOldestEntryRemoved()
{
    if (m_rows.empty())
        return;
    const int last = static_cast<int>(m_rows.size()) - 1;
    beginRemoveRows({}, last, last);
    m_rows.pop_back();
    if (m_anchorRow == last)
        m_anchorRow = -1;
    endRemoveRows();
}

void TChatChannelController::clearSelection()
{
    for (size_t row = 0; row < m_rows.size(); ++row) {
        Row& r = m_rows[row];
        if (r.selectionStart == -1 && r.selectionEnd == -1)
            continue;
        r.selectionStart = -1;
        r.selectionEnd = -1;
        const QModelIndex i = index(static_cast<int>(row));
        Q_EMIT dataChanged(i, i, { SelectionStartRole, SelectionEndRole });
    }
}

void TChatChannelController::applySelection(const int anchorRow, const int anchorPos, const int focusRow, const int focusPos)
{
    // Higher rows are older, so the selection runs from the higher row down.
    int firstRow = anchorRow;
    int firstPos = anchorPos;
    int lastRow = focusRow;
    int lastPos = focusPos;
    if (focusRow > anchorRow || (focusRow == anchorRow && focusPos < anchorPos)) {
        std::swap(firstRow, lastRow);
        std::swap(firstPos, lastPos);
    }
    for (int row = 0; row < static_cast<int>(m_rows.size()); ++row) {
        int start = -1;
        int end = -1;
        if (row <= firstRow && row >= lastRow) {
            start = row == firstRow ? firstPos : 0;
            end = row == lastRow ? lastPos : plainLength(row);
            if (end <= start)
                start = end = -1;
        }
        Row& r = m_rows[static_cast<size_t>(row)];
        if (r.selectionStart == start && r.selectionEnd == end)
            continue;
        r.selectionStart = start;
        r.selectionEnd = end;
        const QModelIndex i = index(row);
        Q_EMIT dataChanged(i, i, { SelectionStartRole, SelectionEndRole });
    }
}

void TChatChannelController::startNewSelection(const int entryIndex, const int cursorPosition)
{
    clearSelection();
    m_anchorRow = entryIndex;
    m_anchorPos = cursorPosition;
}

void TChatChannelController::updateSelection(const int entryIndex, const int cursorPosition)
{
    if (m_anchorRow < 0 || m_anchorRow >= static_cast<int>(m_rows.size()) || entryIndex < 0 || entryIndex >= static_cast<int>(m_rows.size()))
        return;
    applySelection(m_anchorRow, m_anchorPos, entryIndex, cursorPosition);
}

void TChatChannelController::selectAll()
{
    if (m_rows.empty())
        return;
    applySelection(static_cast<int>(m_rows.size()) - 1, 0, 0, plainLength(0));
}

void TChatChannelController::selectTextPassage(const int entryIndex, const int passageStart, const int passageEnd)
{
    if (entryIndex < 0 || entryIndex >= static_cast<int>(m_rows.size()))
        return;
    m_anchorRow = entryIndex;
    m_anchorPos = passageStart;
    applySelection(entryIndex, passageStart, entryIndex, passageEnd);
}

void TChatChannelController::invalidateSizes()
{
    for (Row& r : m_rows)
        r.size = QSize();
    if (!m_rows.empty())
        Q_EMIT dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1), { Qt::SizeHintRole });
}

void TChatChannelController::setChatWidth(const int width)
{
    if (m_width == width)
        return;
    m_width = width;
    invalidateSizes();
}

void TChatChannelController::setChatFont(const QFont& font)
{
    if (m_font == font)
        return;
    m_font = font;
    invalidateSizes();
}

void TChatChannelController::setAutoScrollToEnd(const bool autoScroll)
{
    m_autoScroll = autoScroll;
}

void TChatChannelController::setSubscribeToServerLog(const bool subscribe)
{
    if (m_channel && m_channel->canSubscribeToServerLog())
        m_channel->setSubscribedToServerLog(subscribe);
}

void TChatChannelController::closeChannel()
{
    if (m_channel)
        Q_EMIT closeRequested(m_channel);
}

void TChatChannelController::onChannelClicked(const int entryId, const int mouseButtonRaw, const int keyboardModifiersRaw)
{
    Q_EMIT entryClicked(entryId, mouseButtonRaw, keyboardModifiersRaw);
}

// ---------------------------------------------------------------------------
// TChatChannelTabController

namespace {
constexpr int ChannelNameRole = Qt::UserRole + 1;
constexpr int TabHighlightRole = Qt::UserRole + 2;
/// How long a tab flashes before it stays marked (TChatChannelTabRenderInfo
/// 0x100bd7ed0: single-shot timer, 0x3e8 ms).
constexpr int FlashMilliseconds = 1000;
}

TChatChannelTabController::TChatChannelTabController(chat::TChatChannelStorage& storage, QObject* parent)
    : cip::tibia::gamewindow::TChatChannelTabController(parent)
    , m_storage(storage)
{
    m_currentRowId = 0;
    m_rows = storage.channels();
    connect(&storage, &chat::TChatChannelStorage::channelAdded, this, &TChatChannelTabController::onChannelAdded);
    connect(&storage, &chat::TChatChannelStorage::channelRemoved, this, &TChatChannelTabController::onChannelRemoved);
    connect(&storage, &chat::TChatChannelStorage::entryAddedToChannel, this, &TChatChannelTabController::onEntryAdded);
    connect(&storage, &chat::TChatChannelStorage::channelsMoved, this, [this] {
        beginResetModel();
        m_rows = m_storage.channels();
        endResetModel();
    });
    connect(&storage, &chat::TChatChannelStorage::channelsReset, this, [this] {
        beginResetModel();
        m_rows = m_storage.channels();
        m_highlights.clear();
        m_flashInfos.clear();
        m_currentRowId = 0;
        endResetModel();
        Q_EMIT currentRowIdChanged();
        updateButtonHighlights();
        Q_EMIT currentChannelChanged(currentChannel());
    });
}

chat::TChatChannel* TChatChannelTabController::channelAt(const int row) const
{
    return row >= 0 && row < static_cast<int>(m_rows.size()) ? m_rows[static_cast<size_t>(row)] : nullptr;
}

int TChatChannelTabController::rowOf(const chat::TChatChannel* channel) const
{
    const auto it = std::find(m_rows.begin(), m_rows.end(), channel);
    return it == m_rows.end() ? -1 : static_cast<int>(it - m_rows.begin());
}

chat::TChatChannel* TChatChannelTabController::currentChannel() const
{
    return channelAt(m_currentRowId);
}

void TChatChannelTabController::makeCurrent(chat::TChatChannel* channel)
{
    if (const int row = rowOf(channel); row >= 0)
        setCurrentRow(row);
}

int TChatChannelTabController::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant TChatChannelTabController::data(const QModelIndex& index, const int role) const
{
    const TChatChannel* channel = channelAt(index.row());
    if (!channel)
        return {};
    if (role == ChannelNameRole)
        return channel->name();
    if (role == TabHighlightRole)
        return highlightOf(index.row());
    return {};
}

QHash<int, QByteArray> TChatChannelTabController::roleNames() const
{
    return { { ChannelNameRole, "channelName" }, { TabHighlightRole, "tabHighlight" } };
}

uint TChatChannelTabController::highlightOf(const int row) const
{
    if (row == m_currentRowId)
        return CURRENT;
    const auto it = m_highlights.find(channelAt(row));
    return it == m_highlights.end() ? NONE : it->second;
}

void TChatChannelTabController::setCurrentRow(const int row)
{
    TChatChannel* channel = channelAt(row);
    if (!channel)
        return;
    const int previous = m_currentRowId;
    m_currentRowId = row;
    m_highlights.erase(channel);
    if (auto it = m_flashInfos.find(channel); it != m_flashInfos.end()) {
        it->second->stopFlash();
        m_flashInfos.erase(it);
    }
    if (previous != row) {
        if (channelAt(previous))
            Q_EMIT dataChanged(index(previous), index(previous), { TabHighlightRole });
        Q_EMIT currentRowIdChanged();
    }
    Q_EMIT dataChanged(index(row), index(row), { TabHighlightRole });
    updateButtonHighlights();
    Q_EMIT currentChannelChanged(channel);
}

void TChatChannelTabController::onMakeChannelWithRowIdCurrent(const int rowId)
{
    setCurrentRow(rowId);
}

void TChatChannelTabController::onMakeNextChannelCurrent()
{
    const int count = rowCount();
    if (count > 0)
        setCurrentRow((m_currentRowId + 1) % count);
}

void TChatChannelTabController::onMakePreviousChannelCurrent()
{
    const int count = rowCount();
    if (count > 0)
        setCurrentRow((m_currentRowId + count - 1) % count);
}

void TChatChannelTabController::onChannelTabClicked(const int rowId, const int mouseButtonRaw, const int keyboardModifiersRaw)
{
    if (TChatChannel* channel = channelAt(rowId))
        Q_EMIT tabClicked(channel, mouseButtonRaw, keyboardModifiersRaw);
}

void TChatChannelTabController::setNumberOfHiddenTabs(const uint hiddenLeft, const uint hiddenRight)
{
    m_hiddenLeft = hiddenLeft;
    m_hiddenRight = hiddenRight;
    updateButtonHighlights();
}

void TChatChannelTabController::moveTab(const int sourceIndex, const int targetIndex)
{
    TChatChannel* current = currentChannel();
    m_storage.moveChannel(sourceIndex, targetIndex);
    if (const int row = rowOf(current); row >= 0 && row != m_currentRowId) {
        m_currentRowId = row;
        Q_EMIT currentRowIdChanged();
    }
    updateButtonHighlights();
}

void TChatChannelTabController::onChannelAdded(chat::TChatChannel* channel, const bool makeCurrent)
{
    int row = rowOf(channel);
    if (row < 0) {
        row = static_cast<int>(m_rows.size());
        beginInsertRows({}, row, row);
        m_rows.push_back(channel);
        endInsertRows();
    }
    if (makeCurrent)
        setCurrentRow(row);
    else
        updateButtonHighlights();
}

void TChatChannelTabController::onChannelRemoved(chat::TChatChannel* channel)
{
    const int row = rowOf(channel);
    if (row < 0)
        return;
    const bool wasCurrent = row == m_currentRowId;
    m_highlights.erase(channel);
    if (auto it = m_flashInfos.find(channel); it != m_flashInfos.end()) {
        it->second->deleteLater();
        m_flashInfos.erase(it);
    }
    beginRemoveRows({}, row, row);
    m_rows.erase(m_rows.begin() + row);
    endRemoveRows();
    if (wasCurrent || row < m_currentRowId) {
        m_currentRowId = std::clamp(m_currentRowId - 1, 0, std::max(0, rowCount() - 1));
        Q_EMIT currentRowIdChanged();
    }
    setCurrentRow(m_currentRowId);
}

void TChatChannelTabController::onEntryAdded(chat::TChatChannel* channel, const bool fromOthers)
{
    if (!fromOthers || channel == currentChannel() || channel->isMuted())
        return;
    const int row = rowOf(channel);
    if (row < 0)
        return;
    m_highlights[channel] = FLASH_NEW_ENTRY;
    TChatChannelTabRenderInfo*& info = m_flashInfos[channel];
    if (!info) {
        info = new TChatChannelTabRenderInfo(this);
        QObject::connect(info, &TChatChannelTabRenderInfo::flashFinished, this, [this, channel] {
            const auto it = m_highlights.find(channel);
            if (it == m_highlights.end())
                return;
            it->second = NEW_ENTRY;
            if (const int r = rowOf(channel); r >= 0)
                Q_EMIT dataChanged(index(r), index(r), { TabHighlightRole });
            updateButtonHighlights();
        });
    }
    info->startFlash(FlashMilliseconds);
    Q_EMIT dataChanged(index(row), index(row), { TabHighlightRole });
    updateButtonHighlights();
}

void TChatChannelTabController::updateButtonHighlights()
{
    const auto strongest = [this](const int from, const int to) {
        uint result = NONE;
        for (int row = from; row < to; ++row) {
            const uint h = highlightOf(row);
            if (h == FLASH_NEW_ENTRY)
                return h;
            if (h == NEW_ENTRY)
                result = NEW_ENTRY;
        }
        return result;
    };
    const int count = rowCount();
    const uint left = strongest(0, std::min<int>(static_cast<int>(m_hiddenLeft), count));
    const uint right = strongest(std::max(0, count - static_cast<int>(m_hiddenRight)), count);
    if (left != m_leftButtonHighlight) {
        m_leftButtonHighlight = left;
        Q_EMIT leftButtonHighlightChanged();
    }
    if (right != m_rightButtonHighlight) {
        m_rightButtonHighlight = right;
        Q_EMIT rightButtonHighlightChanged();
    }
}

// ---------------------------------------------------------------------------
// TOpenChatChannelDialogController

TOpenChatChannelDialogController::TOpenChatChannelDialogController(QObject* parent)
    : TDialogController(QStringLiteral("OpenChatChannelDialog.qml"), parent)
{}

void TOpenChatChannelDialogController::open(const std::vector<chat::TChatChannelIdentifier>& channels)
{
    m_channels = channels;
    dialog()->close();
    dialog()->show();
}

QVariantMap TOpenChatChannelDialogController::initialDialogProperties() const
{
    QVariantMap properties = TDialogController::initialDialogProperties();
    QStringList names;
    for (const TChatChannelIdentifier& channel : m_channels)
        names.append(channel.displayName());
    properties.insert(QStringLiteral("channelsModel"), names);
    return properties;
}

void TOpenChatChannelDialogController::onCancelClicked()
{
    dialog()->close();
}

void TOpenChatChannelDialogController::onControllerFinished()
{
    dialog()->close();
}

void TOpenChatChannelDialogController::onOpenChannelFromList(const int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(m_channels.size()))
        return;
    const TChatChannelIdentifier channel = m_channels[static_cast<size_t>(rowIndex)];
    dialog()->close();
    Q_EMIT channelChosen(channel);
}

void TOpenChatChannelDialogController::onOpenPrivateChannelTo(const QString& recipient)
{
    const QString name = recipient.trimmed();
    dialog()->close();
    if (!name.isEmpty())
        Q_EMIT privateChannelChosen(name);
}

// ---------------------------------------------------------------------------
// TChatController

TChatController::TChatController(QQuickItem* chatItem, client::TGameClient& gameClient, QObject* parent)
    : cip::tibia::gamewindow::TChatController(parent)
    , m_chatItem(chatItem)
    , m_gameActionHandler(this)
    , m_chatStorage(this)
    , m_tabs(m_storage)
    , m_mapper(app::ClientOptions::load().options().value(QLatin1String("controlSchemeIndex")).toInt(0))
    , m_whiteBlacklistDialog(m_blacklist)
    , m_gameClient(gameClient)
{
    own(this);
    own(&m_primary);
    own(&m_secondary);
    own(&m_tabs);
    (void)m_gameActionHandler;
    (void)m_chatStorage;

    if (client::gamecore::isRunning()) {
        m_protocol = std::make_unique<chat::TChatProtocolMessageHandler>(m_storage, m_blacklist);
        connect(m_protocol.get(), &chat::TChatProtocolMessageHandler::channelListReceived, this, &TChatController::onChannelListReceived);
        connect(m_protocol.get(), &chat::TChatProtocolMessageHandler::messageFiltered, this, &TChatController::onMessageFiltered);
    }
    m_blacklist.setVipQuery(isVip);
    reloadOptions();

    // CIP 0x100ba31c0 / 0x100ba3200: both console buttons flash for 1000 ms.
    m_ignoredFlashTimer.setSingleShot(true);
    m_ignoredFlashTimer.setInterval(1000);
    connect(&m_ignoredFlashTimer, &QTimer::timeout, this, [this] {
        m_ignoredFlash = false;
        Q_EMIT messageIgnoredFlashChanged();
    });
    m_exivaSuppressedTimer.setSingleShot(true);
    m_exivaSuppressedTimer.setInterval(1000);
    connect(&m_exivaSuppressedTimer, &QTimer::timeout, this, [this] {
        m_exivaSuppressedFlash = false;
        Q_EMIT exivaSuppressedFlashChanged();
    });

    for (TGenericEnterTextDialogController* prompt : { &m_inviteDialog, &m_excludeDialog }) {
        const bool exclude = prompt == &m_excludeDialog;
        prompt->setCaption(qtTrId(exclude ? "chat_exclude_player_caption" : "chat_invite_player_caption"));
        prompt->setDescription(qtTrId("chat_enter_character_name"));
        prompt->setMaxInputLength(29);
        connect(prompt, &TGenericEnterTextDialogController::textEntered, this, [this, exclude](const QString& text) {
            const QString name = text.trimmed();
            if (name.isEmpty() || !m_inviteTarget || !m_inviteTarget->isOwnChannel())
                return;
            if (exclude)
                g_game.excludeFromOwnChannel(name.toStdString());
            else
                g_game.inviteToOwnChannel(name.toStdString());
        });
    }
    m_messageDialog.setButtons(
        { new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK, QKeySequence(QStringLiteral("Return"))) });
    m_messageDialog.setCloseOnButtonClicked(true);

    connect(&m_mode, &chat::TChatModeManager::chatModeChanged, this, &TChatController::chatModeChanged);
    connect(&m_storage, &chat::TChatChannelStorage::channelAdded, this, &TChatController::onChannelAdded);
    connect(&m_tabs, &TChatChannelTabController::currentChannelChanged, &m_primary, &TChatChannelController::setChannel);
    connect(&m_tabs, &TChatChannelTabController::tabClicked, this, &TChatController::showTabMenu);
    connect(&m_storage, &chat::TChatChannelStorage::channelRemoved, this, [this](TChatChannel* channel) {
        if (m_secondary.channel() == channel)
            setSecondary(nullptr);
    });
    for (TChatChannelController* model : { &m_primary, &m_secondary }) {
        connect(model, &TChatChannelController::closeRequested, this, &TChatController::closeChannel);
        connect(model, &TChatChannelController::entryClicked, this,
                [this, model](const int row, const int button, const int modifiers) { showEntryMenu(model, row, button, modifiers); });
    }
    connect(&m_primary, &TChatChannelController::scrollViewToEnd, this, [this] {
        if (m_chatItem)
            QMetaObject::invokeMethod(m_chatItem, "onTriggerScrollChatListToEnd");
    });
    connect(&m_secondary, &TChatChannelController::scrollViewToEnd, this, [this] {
        if (m_chatItem)
            QMetaObject::invokeMethod(m_chatItem, "onTriggerScrollSecondaryChatListToEnd");
    });
    connect(&m_openDialog, &TOpenChatChannelDialogController::channelChosen, this, [this](const TChatChannelIdentifier& channel) {
        switch (channel.type) {
        case TChatChannelIdentifier::ChannelId:
            if (m_protocol)
                m_protocol->openInForeground(channel.id);
            g_game.joinChannel(channel.id);
            break;
        case TChatChannelIdentifier::OwnPrivateChannelToOpen:
            if (m_protocol)
                m_protocol->openInForeground(chat::TChatProtocolMessageHandler::OwnChannelRequest);
            g_game.openOwnChannel();
            break;
        default:
            m_storage.open(channel, true);
            break;
        }
    });
    connect(&m_openDialog, &TOpenChatChannelDialogController::privateChannelChosen, this,
            [](const QString& name) { g_game.openPrivateChannel(name.toStdString()); });

    connect(&gameClient, &client::TGameClient::mainWindowInitialized, this, &TChatController::onWorldEntered);
    connect(&gameClient, &client::TGameClient::worldLeft, this, &TChatController::onWorldLeft);
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, &TChatController::onOptionChanged);

    for (const A type : HandledActions) {
        input::setActionHandler(type, [this](const input::TGameAction& action) { return handleAction(action); });
    }

    // CipSoft: Ctrl/Cmd+C copies chat-output selection when present (TextEdits
    // are not focusable — clipboard handler alone would only see chat input).
    m_copySelectionListenerId = input::addActionListener(
        A::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD, [this](const input::TGameAction&) {
            QString text = m_primary.selectedText();
            if (text.isEmpty())
                text = m_secondary.selectedText();
            if (text.isEmpty())
                return false;
            if (QClipboard* clip = QGuiApplication::clipboard())
                clip->setText(text);
            return true;
        });

    m_primary.setChannel(m_tabs.currentChannel());
    bindItem();
}

TChatController::~TChatController()
{
    if (m_copySelectionListenerId)
        input::removeActionListener(m_copySelectionListenerId);
    for (const A type : HandledActions) {
        input::setActionHandler(type, nullptr);
    }
}

void TChatController::setChatItem(QQuickItem* chatItem)
{
    if (chatItem == m_chatItem)
        return;
    QString text;
    int volume = 0;
    QString volumeState;
    if (m_chatItem) {
        text = m_chatItem->property("currentChatInput").toString();
        volume = m_chatItem->property("currentSpeechVolume").toInt();
        volumeState = m_chatItem->property("currentSpeechVolumeButtonState").toString();
    }
    m_chatItem = chatItem;
    bindItem();
    if (!m_chatItem)
        return;
    m_chatItem->setProperty("currentChatInput", text);
    m_chatItem->setProperty("currentSpeechVolumeButtonState", volumeState);
    Q_UNUSED(volume);
}

void TChatController::bindItem()
{
    if (!m_chatItem)
        return;
    m_chatItem->setProperty("chatController", QVariant::fromValue<QObject*>(this));
    m_chatItem->setProperty("chatChannelModel", QVariant::fromValue<QObject*>(&m_primary));
    m_chatItem->setProperty("secondaryChatChannelModel", QVariant::fromValue<QObject*>(m_secondary.channel() ? &m_secondary : nullptr));
    m_chatItem->setProperty("chatChannelTabModel", QVariant::fromValue<QObject*>(&m_tabs));
}

void TChatController::reloadOptions()
{
    const app::ClientOptions options = app::ClientOptions::load();
    const QJsonObject values = options.options();
    if (m_protocol)
        m_protocol->setOptions(values);
    const bool timestamps = values.value(QLatin1String("consoleShowTimestamps")).toBool(true);
    const bool seconds = values.value(QLatin1String("consoleShowTimestampsSeconds")).toBool(true);
    const bool levels = values.value(QLatin1String("consoleShowLevels")).toBool(true);
    m_primary.onEntryFormatChanged(timestamps, seconds, levels);
    m_secondary.onEntryFormatChanged(timestamps, seconds, levels);
}

void TChatController::onOptionChanged(const QString& name)
{
    if (name == QLatin1String("controlSchemeIndex"))
        m_mapper.setControlScheme(app::ClientOptions::load().options().value(name).toInt(0));
    else if (name.startsWith(QLatin1String("console")) || name.startsWith(QLatin1String("gameWindowShow")) || name == QLatin1String("openNewPrivateMessagesInNewTab"))
        reloadOptions();
}

void TChatController::onWorldEntered()
{
    m_storage.setOwnName(ownName());
    bindItem();
    Q_EMIT exivaOptionsAvailableChanged();
    if (!m_channelsFromConfigOpened)
        openChannelsFromConfig();
}

void TChatController::onWorldLeft()
{
    m_openDialog.dialog()->close();
    m_inviteDialog.dialog()->close();
    m_excludeDialog.dialog()->close();
    m_messageDialog.dialog()->close();
    setSecondary(nullptr);
    m_storage.reset();
    m_mode.onGameSessionDisconnected();
    m_channelsFromConfigOpened = false;
    m_channelListRequested = false;
}

void TChatController::openChannelsFromConfig()
{
    m_channelsFromConfigOpened = true;
    const QJsonObject chatOptions = app::ClientOptions::load().root().value(QLatin1String("chatOptions")).toObject();
    // product (game_chat): one join per login-sync tick, under the server's packets-per-second limit.
    for (const QJsonValue& id : chatOptions.value(QLatin1String("openChannels")).toArray()) {
        const auto channelId = static_cast<uint16_t>(id.toInt());
        tibia::mods::enqueueLoginSync([channelId] { g_game.joinChannel(channelId); });
    }
    if (chatOptions.value(QLatin1String("lootChannelOpen")).toBool(true))
        m_storage.open({ TChatChannelIdentifier::Loot }, false);
}

void TChatController::onChannelAdded(chat::TChatChannel* channel, bool)
{
    const TChatChannelIdentifier::Type type = channel->identifier().type;
    if (m_channelsFromConfigOpened && (type == TChatChannelIdentifier::ChannelId || type == TChatChannelIdentifier::Loot))
        storeOpenChannels();
}

void TChatController::storeOpenChannels() const
{
    QJsonObject chatOptions = app::ClientOptions::load().root().value(QLatin1String("chatOptions")).toObject();
    QJsonArray ids;
    bool loot = false;
    for (const TChatChannel* channel : m_storage.channels()) {
        const TChatChannelIdentifier& identifier = channel->identifier();
        if (identifier.type == TChatChannelIdentifier::Loot)
            loot = true;
        else if (identifier.type == TChatChannelIdentifier::ChannelId && !channel->isOwnChannel() && !identifier.isGuildChannel()
                 && !identifier.isPartyChannel())
            ids.append(identifier.id);
    }
    chatOptions.insert(QLatin1String("openChannels"), ids);
    chatOptions.insert(QLatin1String("lootChannelOpen"), loot);
    app::ClientOptions::storeAll({ { QStringLiteral("chatOptions"), chatOptions } });
}

void TChatController::onMessageFiltered()
{
    m_ignoredFlash = true;
    Q_EMIT messageIgnoredFlashChanged();
    m_ignoredFlashTimer.start();
}

void TChatController::onExivaSuppressed()
{
    if (!m_exivaSuppressedFlash) {
        m_exivaSuppressedFlash = true;
        Q_EMIT exivaSuppressedFlashChanged();
    }
    m_exivaSuppressedTimer.start();
}

bool TChatController::exivaOptionsAvailable() const
{
    return m_gameClient.worldType() == WorldTypeOptionalPvp;
}

void TChatController::openWhiteBlacklistDialog()
{
    input::publish(input::makeAction(A::OPEN_WHITEBLACKLIST_DIALOG));
}

void TChatController::openExivaOptionsDialog()
{
    input::publish(input::makeAction(A::OPEN_EXIVA_OPTIONS_DIALOG));
}

void TChatController::addLogMessage(const QString& text)
{
    m_storage.addServerLogEntry(chat::systemWhite(), text);
}

void TChatController::saveChannel(chat::TChatChannel* channel)
{
    if (!channel)
        return;
    const TChatChannelController* model = m_primary.channel() == channel ? &m_primary : nullptr;
    if (!model && m_secondary.channel() == channel)
        model = &m_secondary;
    TChatChannelController hidden;
    if (!model) {
        const QJsonObject values = app::ClientOptions::load().options();
        hidden.onEntryFormatChanged(values.value(QLatin1String("consoleShowTimestamps")).toBool(true),
                                    values.value(QLatin1String("consoleShowTimestampsSeconds")).toBool(true),
                                    values.value(QLatin1String("consoleShowLevels")).toBool(true));
        hidden.setChannel(channel);
        model = &hidden;
    }
    const QString name = channel->name();
    const QString stamp = QLocale(QLocale::English, QLocale::UnitedStates).toString(QDateTime::currentDateTime(), QStringLiteral("ddd MMM dd hh:mm:ss yyyy"));
    QStringList lines;
    for (int row = model->rowCount() - 1; row >= 0; --row)
        lines.append(model->plainTextAt(row));
    const QString header = qtTrId("chat_save_contents").arg(name, stamp) + QLatin1Char('\n');
    const QString content = QStringLiteral("%1\n%2").arg(header, lines.join(QLatin1Char('\n')) + QLatin1Char('\n'));

    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/log");
    QDir().mkpath(directory);
    const QString path = QDir(directory).filePath(QStringLiteral("%1.txt").arg(name));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    file.write(content.toUtf8());
    file.close();
    if (g_game.isOnline())
        m_storage.addServerLogEntry(chat::consoleColor(30), qtTrId("chat_append_contents").arg(name, QDir::toNativeSeparators(path)));
}

void TChatController::openInviteDialog(chat::TChatChannel* channel, const bool exclude)
{
    if (!channel || !channel->isOwnChannel())
        return;
    m_inviteTarget = channel;
    TGenericEnterTextDialogController& prompt = exclude ? m_excludeDialog : m_inviteDialog;
    prompt.setText(QString());
    prompt.dialog()->create();
    prompt.dialog()->show();
}

void TChatController::showRecentlyTraded(const QString& name)
{
    m_messageDialog.showMessage(qtTrId("recently_traded_titel"), qtTrId("recently_traded_text").arg(name));
}

void TChatController::onChannelListReceived(const std::vector<std::pair<uint16_t, QString>>& channels)
{
    if (!m_channelListRequested)
        return;
    m_channelListRequested = false;
    std::vector<TChatChannelIdentifier> list;
    list.reserve(channels.size() + 2);
    for (const auto& [id, name] : channels) {
        if (id == 0xFFFF)
            list.push_back({ TChatChannelIdentifier::OwnPrivateChannelToOpen, id, name });
        else
            list.push_back(TChatChannelIdentifier::channel(id, name));
    }
    list.push_back({ TChatChannelIdentifier::Npc });
    list.push_back({ TChatChannelIdentifier::Loot });
    m_openDialog.open(list);
}

void TChatController::requestChannelList()
{
    if (!g_game.isOnline())
        return;
    m_channelListRequested = true;
    g_game.requestChannels();
}

TChatController::EInput TChatController::parseInput(QString& text, QString& recipient)
{
    text = text.trimmed();
    if (text.isEmpty())
        return EInput::Empty;
    const QChar first = text.front();
    if (first == QLatin1Char('*') || first == QLatin1Char('@')) {
        const qsizetype end = text.indexOf(first, 1);
        if (end <= 1)
            return EInput::Default;
        recipient = text.mid(1, end - 1);
        text = text.mid(end + 1).trimmed();
        return first == QLatin1Char('*') ? EInput::Private : EInput::RedPrivate;
    }
    if (first != QLatin1Char('#'))
        return EInput::Default;
    static const QRegularExpression whitespace(QStringLiteral("\\s"));
    const qsizetype space = text.indexOf(whitespace);
    if (space != 2)
        return EInput::Default;
    EInput input = EInput::Default;
    switch (text.at(1).toLower().unicode()) {
    case 'b': input = EInput::Broadcast; break;
    case 'c': input = EInput::RedChannel; break;
    case 'i': input = EInput::Invite; break;
    case 's': input = EInput::Say; break;
    case 'w': input = EInput::Whisper; break;
    case 'x': input = EInput::Exclude; break;
    case 'y': input = EInput::Yell; break;
    default: return EInput::Default;
    }
    text = text.mid(space).trimmed();
    return input;
}

QString TChatController::sendChatMessage(const QString& chatMessage, const int chatVolume)
{
    return sendChatMessage(chatMessage, chatVolume, false, 0);
}

void TChatController::setNpcDialog(TNpcDialogController* dialog)
{
    if (m_npcDialog == dialog)
        return;
    if (m_npcDialog)
        disconnect(m_npcDialog, &TNpcDialogController::shownChanged, this, nullptr);
    m_npcDialog = dialog;
    if (dialog) {
        // Re-emit chatModeChanged so HudChat re-syncs open/closed (bubble stays
        // closed while the NPC dialog owns Enter / temporary chat-on).
        connect(dialog, &TNpcDialogController::shownChanged, this, [this](bool) {
            Q_EMIT chatModeChanged();
        });
    }
    Q_EMIT chatModeChanged();
}

bool TChatController::npcDialogShown() const
{
    return m_npcDialog && m_npcDialog->isShown();
}

QString TChatController::sendChatMessageTo(chat::TChatChannel* channel, const QString& chatMessage, const bool isSpell, const int spellCastType)
{
    return sendChatMessage(chatMessage, 0, isSpell, spellCastType, channel);
}

QString TChatController::sendChatMessage(const QString& chatMessage, const int chatVolume, const bool isSpell, const int spellCastType,
                                         TChatChannel* target)
{
    QString keep;
    addEntryToChatInputHistory(chatMessage);
    if (const QString& password = m_gameClient.accountPassword(); !password.isEmpty() && chatMessage.contains(password)) {
        m_messageDialog.showMessage(qtTrId("chat_account_warning_caption"), qtTrId("chat_account_warning_description"));
        return keep;
    }
    QString text = chatMessage;
    QString recipient;
    const EInput input = parseInput(text, recipient);
    // CIP's volumes: 0 whisper, 1 say, 2 yell.
    int volume = chatVolume < 0 ? 0 : (chatVolume > 0 ? 2 : 1);
    TChatChannel* current = target ? target : m_tabs.currentChannel();

    switch (input) {
    case EInput::Say: volume = 1; break;
    case EInput::Yell: volume = 2; break;
    case EInput::Whisper: volume = 0; break;
    case EInput::Private: keep = QStringLiteral("*%1* ").arg(recipient); break;
    case EInput::RedPrivate: keep = QStringLiteral("@%1@ ").arg(recipient); break;
    case EInput::Invite:
        if (m_storage.ownChannel() && !text.isEmpty())
            g_game.inviteToOwnChannel(text.toStdString());
        return keep;
    case EInput::Exclude:
        if (m_storage.ownChannel() && !text.isEmpty())
            g_game.excludeFromOwnChannel(text.toStdString());
        return keep;
    default: break;
    }
    if (text.isEmpty() || !current || !g_game.isOnline())
        return keep;

    const TChatChannelIdentifier& identifier = current->identifier();
    switch (input) {
    case EInput::Default:
        if (current->writesToLocalChat() || isSpell)
            sayInChannel(nullptr, text, volume, spellCastType);
        else
            sayInChannel(current, text, volume);
        break;
    case EInput::Say:
    case EInput::Yell:
    case EInput::Whisper:
        sayInChannel(nullptr, text, volume, spellCastType);
        break;
    case EInput::Broadcast:
        g_game.talkChannel(Otc::MessageGamemasterBroadcast, 0, text.toStdString());
        break;
    case EInput::RedChannel:
        if (identifier.type == TChatChannelIdentifier::ChannelId && !current->writesToLocalChat())
            g_game.talkChannel(Otc::MessageGamemasterChannel, identifier.id, text.toStdString());
        else
            m_storage.addSystemEntry(current, chat::systemRed(), qtTrId("chat_invalid_channel"));
        break;
    case EInput::Private:
        if (identifier.type == TChatChannelIdentifier::MessageTo && identifier.name.compare(recipient, Qt::CaseInsensitive) == 0)
            keep.clear();
        sendPrivate(recipient, text, false);
        break;
    case EInput::RedPrivate:
        sendPrivate(recipient, text, true);
        break;
    default:
        break;
    }
    return keep;
}

void TChatController::sayInChannel(chat::TChatChannel* channel, const QString& text, const int volume, const int spellCastType)
{
    const std::string message = text.toStdString();
    if (!channel) {
        static constexpr Otc::MessageMode Modes[] = { Otc::MessageWhisper, Otc::MessageSay, Otc::MessageYell };
        // CIP FUN_100993990: crosshair spells by cast type.
        input::say(Modes[std::clamp(volume, 0, 2)], text, spellCastType);
        return;
    }
    const TChatChannelIdentifier& identifier = channel->identifier();
    switch (identifier.type) {
    case TChatChannelIdentifier::Npc:
        if (!m_npcDialog || !m_npcDialog->sayToRemoteNpc(text))
            g_game.talkChannel(Otc::MessageNpcTo, 0, message);
        m_storage.addNpcEntry(chat::consoleColor(12), text, ownName());
        break;
    case TChatChannelIdentifier::MessageTo:
        sendPrivate(identifier.name, text, false);
        break;
    case TChatChannelIdentifier::ChannelId:
        g_game.talkChannel(Otc::MessageChannel, identifier.id, message);
        break;
    default:
        break;
    }
}

void TChatController::sendPrivate(const QString& recipient, const QString& text, const bool gamemaster)
{
    g_game.talkPrivate(gamemaster ? Otc::MessageGamemasterPrivateTo : Otc::MessagePrivateTo, recipient.toStdString(), text.toStdString());
    m_storage.addPrivateEntry(recipient, chat::consoleColor(5), text, ownName(), ownLevel(), 0);
}

void TChatController::sendKeyword(const QString& keyword)
{
    setInputText(keyword, true);
}

void TChatController::setInputText(const QString& text, const bool sendDirectly)
{
    if (!m_chatItem)
        return;
    m_chatItem->setProperty("currentChatInput", text);
    if (sendDirectly)
        onChatMessageSendDirectly();
}

void TChatController::onChatMessageSendDirectly()
{
    pressEnter();
}

namespace {

/// Docked CIP Chat or floated HudChat — prefer the one that currently has focus.
QQuickItem* chatItemForFocus(QQuickItem* bound)
{
    QQuickWindow* window = bound ? bound->window() : nullptr;
    QQuickItem* focus = window ? window->activeFocusItem() : nullptr;
    for (QQuickItem* item = focus; item; item = item->parentItem()) {
        const QString name = item->objectName();
        if (name == QLatin1String("hudChat") || name == QLatin1String("chat"))
            return item;
    }
    return bound;
}

}

void TChatController::pressEnter()
{
    QQuickItem* chatItem = chatItemForFocus(m_chatItem);
    if (!chatItem)
        return;
    const QString text = chatItem->property("currentChatInput").toString();
    const int volume = chatItem->property("currentSpeechVolume").toInt();
    const QString rest = sendChatMessage(text, volume);
    chatItem->setProperty("currentChatInput", rest);
    chatItem->setProperty("currentSpeechVolumeButtonState", QString());
    m_mode.onMessageSent();
}

QString TChatController::getOlderChatInputHistoryEntry()
{
    if (m_history.isEmpty())
        return {};
    m_historyIndex = std::max<qsizetype>(0, m_historyIndex - 1);
    return m_history.at(m_historyIndex);
}

QString TChatController::getNewerChatInputHistoryEntry()
{
    if (m_historyIndex + 1 >= m_history.size()) {
        m_historyIndex = m_history.size();
        return {};
    }
    return m_history.at(++m_historyIndex);
}

void TChatController::addEntryToChatInputHistory(const QString& chatInputText)
{
    static const qsizetype maxHistory = chat::chatSetting(QStringLiteral("maxChatInputHistoryLength"), 1000, 1);
    if (!chatInputText.isEmpty() && (m_history.isEmpty() || m_history.constLast() != chatInputText)) {
        if (m_history.size() >= maxHistory)
            m_history.removeFirst();
        m_history.append(chatInputText);
    }
    m_historyIndex = m_history.size();
}

void TChatController::onChatModeButtonClicked()
{
    m_mode.onChatModeButtonClicked();
}

void TChatController::onChatInputClicked()
{
    m_mode.onChatInputClicked();
}

void TChatController::closeChannel(chat::TChatChannel* channel)
{
    if (!channel || !channel->canBeClosed())
        return;
    const TChatChannelIdentifier identifier = channel->identifier();
    if (identifier.type == TChatChannelIdentifier::ChannelId && !channel->isClosedByServer())
        g_game.leaveChannel(identifier.id);
    else if (identifier.type == TChatChannelIdentifier::Npc)
        g_game.closeNpcChannel();
    m_storage.close(channel);
    if (identifier.type == TChatChannelIdentifier::ChannelId || identifier.type == TChatChannelIdentifier::Loot)
        storeOpenChannels();
}

void TChatController::setSecondary(chat::TChatChannel* channel)
{
    const bool wasShown = m_secondary.channel() != nullptr;
    m_secondary.setChannel(channel);
    if (wasShown != (channel != nullptr) && m_chatItem)
        m_chatItem->setProperty("secondaryChatChannelModel", QVariant::fromValue<QObject*>(channel ? &m_secondary : nullptr));
}

void TChatController::onSecondaryChannelClicked(const int mouseButtonRaw, const int keyboardModifiersRaw)
{
    showTabMenu(m_secondary.channel(), mouseButtonRaw, keyboardModifiersRaw);
}

void TChatController::onSelectTabAsSecondaryChannel(const int rowId)
{
    if (TChatChannel* channel = m_tabs.channelAt(rowId))
        setSecondary(channel);
}

bool TChatController::handleAction(const input::TGameAction& action)
{
    const bool noDialog = !TDialogStackController::instance() || TDialogStackController::instance()->isEmpty();
    switch (action.type) {
    case A::GET_CHAT_CHANNEL_LIST:
        requestChannelList();
        return true;
    case A::OPEN_CHAT_CHANNEL:
    case A::OPEN_CHAT_CHANNEL_AND_MAKE_CURRENT: {
        // CIP FUN_100992360: server channels are joined, the client's own opened.
        const bool makeCurrent = action.type == A::OPEN_CHAT_CHANNEL_AND_MAKE_CURRENT;
        const TChatChannelIdentifier identifier{ TChatChannelIdentifier::Type(action.chatChannelType),
                                                 static_cast<uint16_t>(action.chatChannelId), action.text };
        switch (identifier.type) {
        case TChatChannelIdentifier::ChannelId:
            if (makeCurrent && m_protocol)
                m_protocol->openInForeground(identifier.id);
            g_game.joinChannel(identifier.id);
            break;
        case TChatChannelIdentifier::MessageTo:
            if (!identifier.name.isEmpty())
                g_game.openPrivateChannel(identifier.name.toStdString());
            break;
        case TChatChannelIdentifier::OwnPrivateChannelToOpen:
            if (makeCurrent && m_protocol)
                m_protocol->openInForeground(chat::TChatProtocolMessageHandler::OwnChannelRequest);
            g_game.openOwnChannel();
            break;
        default:
            m_storage.open(identifier, makeCurrent);
            break;
        }
        return true;
    }
    case A::OPEN_MESSAGE_TO_CHAT_CHANNEL:
        if (!action.text.isEmpty())
            g_game.openPrivateChannel(action.text.toStdString());
        return true;
    case A::CLOSE_CURRENT_CHAT_CHANNEL:
        closeChannel(m_tabs.currentChannel());
        return true;
    case A::TOGGLE_SERVER_LOG_SUBSCRIPTION_IN_CURRENT_CHAT_CHANNEL:
        m_primary.setSubscribeToServerLog(!m_primary.isSubscribedToServerLog());
        return true;
    case A::INVITE_PLAYER_TO_CHAT_CHANNEL:
    case A::INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL:
        if (!action.text.isEmpty())
            g_game.inviteToOwnChannel(action.text.toStdString());
        return true;
    case A::EXCLUDE_PLAYER_FROM_CHAT_CHANNEL:
        if (!action.text.isEmpty())
            g_game.excludeFromOwnChannel(action.text.toStdString());
        return true;
    case A::PRESS_ENTER_IN_CHAT:
        // product: while the NPC dialog is open, Enter drives that input (CIP
        // focuses the NPC pane; we never hand Enter to HudChat).
        if (m_npcDialog && m_npcDialog->isShown()) {
            if (!m_npcDialog->hasFocus())
                m_npcDialog->takeFocus();
            else
                m_npcDialog->pressEnter();
        } else if (noDialog || !TDialogStackController::instance()->hotkeysBlocked()) {
            pressEnter();
        }
        return true;
    case A::SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT:
        if (m_npcDialog && m_npcDialog->isShown()) {
            m_npcDialog->setInputText(action.text, action.enable, action.isSpell, action.spellCastType);
            return true;
        }
        // CIP FUN_100ba2330: sent text goes out at say volume with the hotkey's
        // spell flags, and the input is cleared.
        if (action.enable && m_chatItem) {
            sendChatMessage(action.text, 0, action.isSpell, action.spellCastType);
            m_chatItem->setProperty("currentChatInput", QString());
        } else {
            setInputText(action.text, action.enable);
        }
        return true;
    case A::DISPLAY_NEXT_CHAT_CHANNEL_TAB:
        m_tabs.onMakeNextChannelCurrent();
        return true;
    case A::DISPLAY_PREVIOUS_CHAT_CHANNEL_TAB:
        m_tabs.onMakePreviousChannelCurrent();
        return true;
    case A::ADD_SYSTEM_CHAT_MESSAGE:
        m_storage.addSystemEntry(m_tabs.currentChannel(), action.color.isValid() ? action.color : chat::systemWhite(), action.text);
        return true;
    case A::IGNORE_NAME:
        m_blacklist.ignore(action.text);
        addLogMessage(qtTrId("ignoring_player_log_message").arg(action.text));
        return true;
    case A::UNIGNORE_NAME:
        m_blacklist.unignore(action.text);
        addLogMessage(qtTrId("unignoring_player_log_message").arg(action.text));
        return true;
    case A::SEND_CHAT_MESSAGE_WITH_SPEECH_VOLUME:
        sendChatMessage(action.text, action.parameter, action.isSpell, action.spellCastType);
        return true;
    case A::SEND_CHAT_MESSAGE_AS_GAMEMASTER_BROADCAST:
        if (!action.text.isEmpty())
            g_game.talkChannel(Otc::MessageGamemasterBroadcast, 0, action.text.toStdString());
        return true;
    case A::SEND_CHAT_MESSAGE_WITH_CHANNEL_ID:
        if (!action.text.isEmpty())
            g_game.talkChannel(Otc::MessageChannel, static_cast<uint16_t>(action.chatChannelId), action.text.toStdString());
        return true;
    case A::SEND_CHAT_MESSAGE_WITH_RECIPIENT:
        // CIP: recipient in `text`, message body in `statement` (hotkey private message).
        if (!action.text.isEmpty() && !action.statement.isEmpty())
            sendPrivate(action.text, action.statement, false);
        return true;
    case A::SEND_CHAT_MESSAGE_TO_NPC:
        if (!action.text.isEmpty())
            g_game.talkChannel(Otc::MessageNpcTo, 0, action.text.toStdString());
        return true;
    case A::SWITCH_CHAT_MODE:
        // CIP Enter-from-chat-off → ChatModeTemporaryOn. With an NPC dialog open,
        // focus that input instead of opening HudChat (bubble stays closed).
        if (action.parameter == chat::TChatModeManager::CHAT_TEMPORARY_ON && m_npcDialog
            && m_npcDialog->isShown()) {
            m_mode.switchChatMode(chat::TChatModeManager::CHAT_TEMPORARY_ON);
            m_npcDialog->takeFocus();
            return true;
        }
        m_mode.switchChatMode(action.parameter);
        return true;
    case A::OPEN_WHITEBLACKLIST_DIALOG:
        m_whiteBlacklistDialog.open();
        return true;
    case A::OPEN_INVITE_TO_CHAT_CHANNEL_DIALOG:
    case A::OPEN_EXCLUDE_FROM_CHAT_CHANNEL_DIALOG:
        openInviteDialog(m_tabs.currentChannel(), action.type == A::OPEN_EXCLUDE_FROM_CHAT_CHANNEL_DIALOG);
        return true;
    case A::SAVE_CHAT_CHANNEL_TO_FILE:
        saveChannel(action.parameter == 1 ? m_secondary.channel() : m_primary.channel());
        return true;
    case A::RECENTLY_TRADED:
        showRecentlyTraded(action.text);
        return true;
    case A::GET_GUILD_MESSAGE:
        g_game.requestGuildMessage();
        return true;
    case A::SHOW_CHANNEL_JOIN_LEAVE_MESSAGES:
    case A::HIDE_CHANNEL_JOIN_LEAVE_MESSAGES:
        app::ClientOptions::store(QStringLiteral("consoleShowJoinLeaveMessages"), action.type == A::SHOW_CHANNEL_JOIN_LEAVE_MESSAGES);
        return true;
    case A::EXIVA_WAS_SUPPRESSED:
        onExivaSuppressed();
        return true;
    case A::CLOSE_CHAT_CHANNEL:
        closeChannel(m_tabs.currentChannel());
        return true;
    case A::CLEAR_CHAT_CHANNEL:
        if (TChatChannel* channel = m_tabs.currentChannel())
            channel->clear();
        return true;
    case A::CHAT_CHANNEL_SUBSCRIBE_TO_SERVER_LOG:
    case A::CHAT_CHANNEL_UNSUBSCRIBE_SERVER_LOG:
        if (TChatChannel* channel = m_tabs.currentChannel())
            channel->setSubscribedToServerLog(action.type == A::CHAT_CHANNEL_SUBSCRIBE_TO_SERVER_LOG);
        return true;
    case A::CHAT_CHANNEL_MUTE:
    case A::CHAT_CHANNEL_UNMUTE:
        if (TChatChannel* channel = m_tabs.currentChannel()) {
            channel->setMuted(action.type == A::CHAT_CHANNEL_MUTE);
            Q_EMIT channel->propertiesChanged();
        }
        return true;
    case A::SHOW_CHAT_CHANNEL_AS_SECONDARY:
    case A::SELECT_CHAT_CHANNEL_AS_SECONDARY:
        if (!action.text.isEmpty()) {
            for (TChatChannel* each : m_storage.channels()) {
                if (each->name().compare(action.text, Qt::CaseInsensitive) == 0) {
                    setSecondary(each);
                    return true;
                }
            }
        }
        setSecondary(m_tabs.currentChannel());
        return true;
    case A::HIDE_SECONDARY_CHAT_CHANNEL:
        setSecondary(nullptr);
        return true;
    case A::CHAT_CHANNEL_SELECT_ALL:
        m_primary.selectAll();
        return true;
    case A::COPY_CHAT_ENTRY_TO_CLIPBOARD:
        if (!action.text.isEmpty())
            QGuiApplication::clipboard()->setText(action.text);
        return true;
    default:
        return false;
    }
}

void TChatController::showEntryMenu(TChatChannelController* model, const int row, const int button, const int modifiers)
{
    const TChatEntry* entry = model->entryAt(row);
    if (!entry)
        return;
    // CIP: the provider 0x100b03710 and TGenericChatGameActionProvider 0x100e74c40.
    input::TGameActionList candidates{ input::makeAction(A::CONTEXT_MENU), input::makeAction(A::CHAT_CHANNEL_SELECT_ALL),
                                       textAction(A::COPY_CHAT_ENTRY_TO_CLIPBOARD, model->plainTextAt(row)) };
    if (!entry->name.isEmpty())
        candidates.push_back(textAction(A::COPY_CREATURE_NAME_TO_CLIPBOARD, entry->name));
    if (model->hasSelection())
        candidates.push_back(textAction(A::COPY_TEXT_TO_CLIPBOARD, model->selectedText()));
    if (entry->kind == TChatEntry::System || entry->kind == TChatEntry::Speaker) {
        auto typo = input::mutableClone(*input::makeAction(A::REPORT_BUG_TYPO));
        typo->text = entry->name;
        typo->statement = entry->text;
        candidates.push_back(std::move(typo));
    }
    if (entry->kind == TChatEntry::SpeakerWithLevel && entry->name.compare(m_storage.ownName(), Qt::CaseInsensitive) != 0) {
        candidates.push_back(textAction(A::OPEN_MESSAGE_TO_CHAT_CHANNEL, entry->name));
        if (m_storage.ownChannel())
            candidates.push_back(textAction(A::INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL, entry->name));
        candidates.push_back(textAction(A::REPORT_NAME, entry->name));
        candidates.push_back(textAction(m_blacklist.isIgnored(entry->name) ? A::UNIGNORE_NAME : A::IGNORE_NAME, entry->name));
        if (!isVip(entry->name))
            candidates.push_back(textAction(A::ADD_NEW_VIP, entry->name));
        auto statement = input::mutableClone(*input::makeAction(A::REPORT_STATEMENT));
        statement->text = entry->name;
        statement->statement = entry->text;
        statement->parameter = static_cast<int>(entry->statementId);
        candidates.push_back(std::move(statement));
        if (entry->traded)
            candidates.push_back(textAction(A::RECENTLY_TRADED, entry->name));
    }
    showMenu(candidates, EntryMenuOrder, ChatEntryContext, button, modifiers, model, nullptr);
}

void TChatController::showTabMenu(chat::TChatChannel* channel, const int button, const int modifiers)
{
    // CIP: the provider 0x100b02320.
    input::TGameActionList candidates{ input::makeAction(A::CONTEXT_MENU) };
    if (!channel) {
        auto* menu = TContextMenuController::instance();
        if (!menu || m_mapper.map(candidates, ChatTabContext, static_cast<Qt::MouseButton>(button), modifiers) == nullptr)
            return;
        menu->beginCustomMenu();
        for (TChatChannel* each : m_storage.channels()) {
            QString shortcut;
            const QString label = TContextMenuController::labelOf(*textAction(A::SELECT_CHAT_CHANNEL_AS_SECONDARY, each->name()), shortcut);
            connect(menu->addCustomEntry(label, shortcut), &TContextMenuEntryItem::triggered, this,
                    [this, target = QPointer<TChatChannel>(each)] { setSecondary(target); });
        }
        menu->showCustomMenu();
        return;
    }
    candidates.push_back(input::makeAction(A::CLEAR_CHAT_CHANNEL));
    candidates.push_back(input::makeAction(A::SAVE_CHAT_CHANNEL_TO_FILE));
    if (channel->canBeClosed())
        candidates.push_back(input::makeAction(A::CLOSE_CHAT_CHANNEL));
    if (channel->canSubscribeToServerLog())
        candidates.push_back(input::makeAction(channel->isSubscribedToServerLog() ? A::CHAT_CHANNEL_UNSUBSCRIBE_SERVER_LOG
                                                                                  : A::CHAT_CHANNEL_SUBSCRIBE_TO_SERVER_LOG));
    const TChatChannelIdentifier& identifier = channel->identifier();
    // CIP 0x100b02320: invite/exclude only when the channel object is own
    // (vslot 0xc0). GET_GUILD_MESSAGE is separate: ChannelId + FUN_10098c6c0.
    if (channel->isOwnChannel()) {
        candidates.push_back(input::makeAction(A::OPEN_INVITE_TO_CHAT_CHANNEL_DIALOG));
        candidates.push_back(input::makeAction(A::OPEN_EXCLUDE_FROM_CHAT_CHANNEL_DIALOG));
    }
    if (identifier.isGuildChannel())
        candidates.push_back(input::makeAction(A::GET_GUILD_MESSAGE));
    if (identifier.isGuildChannel() || identifier.isPartyChannel()) {
        const bool shown = app::ClientOptions::load().options().value(QLatin1String("consoleShowJoinLeaveMessages")).toBool(true);
        candidates.push_back(input::makeAction(shown ? A::HIDE_CHANNEL_JOIN_LEAVE_MESSAGES : A::SHOW_CHANNEL_JOIN_LEAVE_MESSAGES));
    }
    candidates.push_back(input::makeAction(m_secondary.channel() == channel ? A::HIDE_SECONDARY_CHAT_CHANNEL : A::SHOW_CHAT_CHANNEL_AS_SECONDARY));
    if (channel->identifier().type != TChatChannelIdentifier::ServerLog)
        candidates.push_back(input::makeAction(channel->isMuted() ? A::CHAT_CHANNEL_UNMUTE : A::CHAT_CHANNEL_MUTE));
    showMenu(candidates, TabMenuOrder, ChatTabContext, button, modifiers, nullptr, channel);
}

void TChatController::showMenu(const input::TGameActionList& candidates, const std::span<const input::EGameAction> order, const int context,
                               const int button, const int modifiers, TChatChannelController* model, chat::TChatChannel* channel)
{
    // CIP chat::TChatGameActionProvider builds the candidate set for tab/entry menus.
    (void)chat::TChatGameActionProvider();
    const input::TGameActionPtr picked = m_mapper.map(candidates, context, static_cast<Qt::MouseButton>(button), modifiers);
    if (!picked)
        return;
    if (picked->type != A::CONTEXT_MENU) {
        runMenuAction(*picked, model, channel);
        return;
    }
    auto* menu = TContextMenuController::instance();
    const input::TGameActionList entries = input::contextMenuActions(candidates, order);
    if (!menu || entries.empty())
        return;
    menu->beginCustomMenu();
    for (const input::TGameActionPtr& action : entries) {
        if (action->type == A::CONTEXT_MENU_SEPARATOR) {
            menu->addCustomSeparator();
            continue;
        }
        QString shortcut;
        const QString label = TContextMenuController::labelOf(*action, shortcut);
        connect(menu->addCustomEntry(label, shortcut), &TContextMenuEntryItem::triggered, this,
                [this, action, target = QPointer<TChatChannelController>(model), tab = QPointer<TChatChannel>(channel)] {
                    runMenuAction(*action, target, tab);
                });
    }
    menu->showCustomMenu();
}

void TChatController::runMenuAction(const input::TGameAction& action, TChatChannelController* model, chat::TChatChannel* channel)
{
    switch (action.type) {
    case A::CHAT_CHANNEL_SELECT_ALL:
        if (model)
            model->selectAll();
        return;
    case A::COPY_CHAT_ENTRY_TO_CLIPBOARD:
    case A::COPY_CREATURE_NAME_TO_CLIPBOARD:
    case A::COPY_TEXT_TO_CLIPBOARD:
        QGuiApplication::clipboard()->setText(action.text);
        return;
    case A::ADD_NEW_VIP:
        g_game.addVip(action.text.toStdString());
        return;
    case A::CLOSE_CHAT_CHANNEL:
        closeChannel(channel);
        return;
    case A::CLEAR_CHAT_CHANNEL:
        if (channel)
            channel->clear();
        return;
    case A::CHAT_CHANNEL_SUBSCRIBE_TO_SERVER_LOG:
    case A::CHAT_CHANNEL_UNSUBSCRIBE_SERVER_LOG:
        if (channel)
            channel->setSubscribedToServerLog(action.type == A::CHAT_CHANNEL_SUBSCRIBE_TO_SERVER_LOG);
        return;
    case A::CHAT_CHANNEL_MUTE:
    case A::CHAT_CHANNEL_UNMUTE:
        if (channel) {
            channel->setMuted(action.type == A::CHAT_CHANNEL_MUTE);
            Q_EMIT channel->propertiesChanged();
        }
        return;
    case A::SHOW_CHAT_CHANNEL_AS_SECONDARY:
    case A::SELECT_CHAT_CHANNEL_AS_SECONDARY:
        setSecondary(channel);
        return;
    case A::HIDE_SECONDARY_CHAT_CHANNEL:
        setSecondary(nullptr);
        return;
    case A::SAVE_CHAT_CHANNEL_TO_FILE:
        saveChannel(model ? model->channel() : channel);
        return;
    case A::OPEN_INVITE_TO_CHAT_CHANNEL_DIALOG:
    case A::OPEN_EXCLUDE_FROM_CHAT_CHANNEL_DIALOG:
        openInviteDialog(channel, action.type == A::OPEN_EXCLUDE_FROM_CHAT_CHANNEL_DIALOG);
        return;
    default:
        input::publish(input::mutableClone(action));
        return;
    }
}

}
