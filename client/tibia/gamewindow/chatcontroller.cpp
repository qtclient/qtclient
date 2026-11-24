#include "creatures/vipstorage.h"
#include "game/gamesessionbase.h"
#include "chatcontroller.h"

#include "app/clientoptions.h"
#include "config/clientoptions.h"
#include "client/gameclient.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/npcdialogcontroller.h"
#include "mods/registry.h"
#include "input/eventfiltermanager.h"
#include "input/inputciptypes.h"
#include "input/crosshairgameactionhandlers.h"
#include "input/gameactionhandlers.h"
#include "protocol/protocolciptypes.h"
#include "protocol/protocolmessagequeue.h"

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
#include <QJsonObject>
#include <QTextLayout>
#include <QTextBlock>

#include <algorithm>
#include <cmath>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

/// The plain text of an HTML chat line.
QString htmlToPlainText(const QString& html)
{
    QTextDocument document;
    document.setHtml(html);
    return document.toPlainText();
}

using A = input::EGameAction;
using chat::TChatChannel;
using chat::TChatChannelIdentifier;
using chat::TChatEntry;

/// Rule contexts of the chat (CIP: onChatChannelTabClicked ,
/// onChannelClicked).
constexpr int ChatTabContext = 4;
constexpr int ChatEntryContext = 5;

/// CIP EWorldType's Optional PvP .
constexpr int WorldTypeOptionalPvp = 1;

/// CIP 's context menu order.
constexpr A EntryMenuOrder[] = {
    A::OPEN_MESSAGE_TO_CHAT_CHANNEL, A::INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL, A::ADD_NEW_VIP, A::IGNORE_NAME, A::UNIGNORE_NAME,
    A::CONTEXT_MENU_SEPARATOR, A::CHAT_CHANNEL_SELECT_ALL, A::COPY_TEXT_TO_CLIPBOARD, A::COPY_CHAT_ENTRY_TO_CLIPBOARD,
    A::CONTEXT_MENU_SEPARATOR, A::REPORT_STATEMENT, A::REPORT_NAME, A::CONTEXT_MENU_SEPARATOR, A::REPORT_BUG_TYPO,
    A::RECENTLY_TRADED, A::CONTEXT_MENU_SEPARATOR, A::CONTEXT_MENU_SEPARATOR, A::COPY_CREATURE_NAME_TO_CLIPBOARD,
};

/// The game actions TChatController::handleAction takes (CIP),
/// including tab/entry menu actions that runMenuAction also handles with an
/// explicit channel/model (here they target the current / secondary channel).
constexpr A HandledActions[] = {
    A::GET_CHAT_CHANNEL_LIST, A::CLOSE_CURRENT_CHAT_CHANNEL, A::TOGGLE_SERVER_LOG_SUBSCRIPTION_IN_CURRENT_CHAT_CHANNEL,
    A::PRESS_ENTER_IN_CHAT, A::SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT, A::DISPLAY_NEXT_CHAT_CHANNEL_TAB,
    A::DISPLAY_PREVIOUS_CHAT_CHANNEL_TAB, A::SWITCH_CHAT_MODE, A::OPEN_WHITEBLACKLIST_DIALOG, A::OPEN_INVITE_TO_CHAT_CHANNEL_DIALOG,
    A::OPEN_EXCLUDE_FROM_CHAT_CHANNEL_DIALOG, A::SAVE_CHAT_CHANNEL_TO_FILE, A::RECENTLY_TRADED, A::EXIVA_WAS_SUPPRESSED,
    A::SHOW_CHAT_CHANNEL_AS_SECONDARY, A::SELECT_CHAT_CHANNEL_AS_SECONDARY, A::HIDE_SECONDARY_CHAT_CHANNEL,
    A::CHAT_CHANNEL_SELECT_ALL, A::COPY_CHAT_ENTRY_TO_CLIPBOARD,
};

/// CIP 's context menu order.
constexpr A TabMenuOrder[] = {
    A::OPEN_INVITE_TO_CHAT_CHANNEL_DIALOG, A::OPEN_EXCLUDE_FROM_CHAT_CHANNEL_DIALOG, A::CONTEXT_MENU_SEPARATOR,
    A::GET_GUILD_MESSAGE, A::SHOW_CHANNEL_JOIN_LEAVE_MESSAGES, A::HIDE_CHANNEL_JOIN_LEAVE_MESSAGES, A::CONTEXT_MENU_SEPARATOR,
    A::CLOSE_CHAT_CHANNEL, A::CHAT_CHANNEL_SUBSCRIBE_TO_SERVER_LOG, A::CHAT_CHANNEL_UNSUBSCRIBE_SERVER_LOG, A::CONTEXT_MENU_SEPARATOR,
    A::SHOW_CHAT_CHANNEL_AS_SECONDARY, A::HIDE_SECONDARY_CHAT_CHANNEL, A::SELECT_CHAT_CHANNEL_AS_SECONDARY, A::CONTEXT_MENU_SEPARATOR,
    A::CHAT_CHANNEL_MUTE, A::CHAT_CHANNEL_UNMUTE, A::CONTEXT_MENU_SEPARATOR, A::SAVE_CHAT_CHANNEL_TO_FILE, A::CLEAR_CHAT_CHANNEL,
};

int ownLevel()
{
    return tibia::game::hasLocalPlayer() ? tibia::game::localPlayerLevel() : 0;
}

QString ownName()
{
    return QString::fromStdString(tibia::game::characterName());
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
    , m_filter(this, this)
{
}

void TChatChannelController::setChannel(chat::TChatChannel* channel)
{
    if (m_channel == channel) {
        Q_EMIT scrollViewToEnd();
        return;
    }
    if (m_channel) {
        disconnect(m_channel, nullptr, this, nullptr);
        m_channel = nullptr;
        Q_EMIT chatChannelPropertyChanged();
        Q_EMIT chatChannelChanged();
    }
    clearRows();
    if (channel) {
        m_channel = channel;
        onIdRangeChanged(channel->firstEntryId(), channel->afterLastEntryId());
        connect(channel, &TChatChannel::idRangeChanged, this, &TChatChannelController::onIdRangeChanged);
        connect(channel, &TChatChannel::isClosedChanged, this, &TChatChannelController::chatChannelPropertyChanged);
        connect(channel, &TChatChannel::serverLogSubscriptionChanged, this, &TChatChannelController::chatChannelPropertyChanged);
        connect(channel, &TChatChannel::tutorialMarkerChanged, this, &TChatChannelController::onTutorialMarkerChanged);
        connect(channel, &QObject::destroyed, this, &TChatChannelController::resetChannel);
        Q_EMIT chatChannelPropertyChanged();
        Q_EMIT chatChannelChanged();
    }
}

void TChatChannelController::resetChannel()
{
    if (m_channel) {
        disconnect(m_channel, nullptr, this, nullptr);
        m_channel = nullptr;
        Q_EMIT chatChannelPropertyChanged();
        Q_EMIT chatChannelChanged();
    }
    clearRows();
}

const chat::TChatEntry* TChatChannelController::entryAt(const int row) const
{
    if (!m_channel || row < 0 || row >= rowCount())
        return nullptr;
    const uint32_t id = m_rows[static_cast<size_t>(row)].entryId;
    if (id < m_channel->firstEntryId() || id >= m_channel->afterLastEntryId())
        return nullptr;
    return &m_channel->entry(id);
}

QString TChatChannelController::plainTextAt(const int row) const
{
    if (row < 0 || row >= rowCount())
        return {};
    const Row& r = m_rows[static_cast<size_t>(row)];
    if (r.plain.isNull())
        r.plain = htmlToPlainText(r.html);
    return r.plain;
}

QString TChatChannelController::selectedText() const
{
    QStringList lines;
    for (int row = rowCount() - 1; row >= 0; --row) {
        const Row& r = m_rows[static_cast<size_t>(row)];
        if (r.selectionEnd <= r.selectionStart)
            continue;
        lines.append(plainTextAt(row).mid(r.selectionStart, r.selectionEnd - r.selectionStart));
    }
    return lines.join(QLatin1Char('\n'));
}

bool TChatChannelController::hasSelection() const
{
    return std::any_of(m_rows.begin(), m_rows.end(), [](const Row& r) { return r.selectionEnd > r.selectionStart; });
}

int TChatChannelController::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

Qt::ItemFlags TChatChannelController::flags(const QModelIndex& index) const
{
    if (index.row() >= 0 && index.column() >= 0 && index.model())
        return QAbstractListModel::flags(index);
    return Qt::ItemIsEnabled;
}

QVariant TChatChannelController::data(const QModelIndex& index, const int role) const
{
    if (index.row() < 0 || index.row() >= rowCount())
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
        return tutorialMarkerOf(r);
    case DateTimeRole:
        if (!m_channel || r.entryId < m_channel->firstEntryId() || r.entryId >= m_channel->afterLastEntryId())
            return {};
        return m_channel->entry(r.entryId).time;
    case Qt::SizeHintRole:
        if (!r.size.isValid())
            r.size = sizeOf(r.html);
        return r.size;
    default:
        return {};
    }
}

QVariant TChatChannelController::tutorialMarkerOf(const Row& row) const
{
    // The marker sits on the keyword link whose target is the marked keyword.
    const std::shared_ptr<chat::TKeywordGuiTutorialMarkerInfo> marker = m_channel ? m_channel->tutorialMarker() : nullptr;
    if (!marker || marker->entryId != row.entryId)
        return {};
    QTextDocument document;
    document.setTextWidth(m_width);
    document.setDefaultFont(m_font);
    document.setDocumentMargin(0);
    document.setUseDesignMetrics(true);
    document.setHtml(row.html);
    const QList<QTextLayout::FormatRange> formats = document.firstBlock().textFormats();
    for (const QTextLayout::FormatRange& range : formats) {
        const QString target = range.format.stringProperty(QTextFormat::AnchorHref);
        if (target.isEmpty() || target.compare(marker->keyword, Qt::CaseInsensitive) != 0)
            continue;
        auto* info = new TChatTutorialMarkerRenderInfo(marker->markerId, range.start, static_cast<int>(target.size()));
        QQmlEngine::setObjectOwnership(info, QQmlEngine::JavaScriptOwnership);
        return QVariant::fromValue<QObject*>(info);
    }
    return {};
}

QSize TChatChannelController::sizeOf(const QString& html) const
{
    QTextDocument document;
    document.setTextWidth(m_width);
    document.setDefaultFont(m_font);
    document.setDocumentMargin(0);
    document.setUseDesignMetrics(true);
    document.setHtml(html);
    const QSizeF size = document.size();
    return QSize(static_cast<int>(size.width() + 0.5), static_cast<int>(size.height() + 0.5));
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
    return m_channel && m_channel->identifier().canBeClosed();
}

bool TChatChannelController::isSubscribedToServerLog() const
{
    return m_channel && m_channel->isSubscribedToServerLog();
}

bool TChatChannelController::writesToLocalChat() const
{
    // Local Chat, the Server Log and Loot talk on the map, and so does any
    // channel the server closed.
    if (!m_channel)
        return true;
    switch (m_channel->identifier().type) {
    case TChatChannelIdentifier::Local:
    case TChatChannelIdentifier::ServerLog:
    case TChatChannelIdentifier::Loot:
        return true;
    default:
        return m_channel->isClosed();
    }
}

bool TChatChannelController::canBeClosed() const
{
    return m_channel && m_channel->identifier().canBeClosed();
}

QString TChatChannelController::channelName() const
{
    return m_channel ? m_channel->identifier().displayName() : QString();
}

void TChatChannelController::onChatChannelControllerOptionsChanged()
{
    const QJsonObject options = app::ClientOptions::load().options();
    onEntryFormatChanged(options.value(QLatin1String("consoleShowTimestamps")).toBool(true),
                         options.value(QLatin1String("consoleShowTimestampsSeconds")).toBool(false),
                         options.value(QLatin1String("consoleShowLevels")).toBool(true));
}

void TChatChannelController::onEntryFormatChanged(const bool showTimestamp, const bool showTimestampSeconds, const bool showLevel)
{
    if (m_showTimestamps == showTimestamp && m_showTimestampSeconds == showTimestampSeconds && m_showLevels == showLevel)
        return;
    m_showTimestamps = showTimestamp;
    m_showTimestampSeconds = showTimestampSeconds;
    m_showLevels = showLevel;
    if (!m_channel)
        return;
    for (Row& row : m_rows) {
        if (row.entryId >= m_channel->firstEntryId() && row.entryId < m_channel->afterLastEntryId())
            formatRow(row, m_channel->entry(row.entryId));
    }
    if (!m_rows.empty())
        Q_EMIT dataChanged(index(0), index(rowCount() - 1), { ChatTextRole, Qt::SizeHintRole });
}

void TChatChannelController::formatRow(Row& row, const chat::TChatEntry& entry) const
{
    row.selectionStart = 0;
    row.selectionEnd = 0;
    row.selectionAnchor = 0;
    row.html = chat::formatEntry(entry, m_showTimestamps, m_showTimestampSeconds, m_showLevels, &row.plainLength);
    row.plain = QString();
    row.size = QSize();
}

TChatChannelController::Row TChatChannelController::makeRow(const chat::TChatEntry& entry, const uint32_t entryId) const
{
    Row row;
    row.entryId = entryId;
    formatRow(row, entry);
    return row;
}

void TChatChannelController::clearRows()
{
    m_selectionHigh = 0;
    m_selectionLow = 0;
    m_selectionFocus = 0;
    if (rowCount() == 0)
        return;
    beginRemoveRows({}, 0, rowCount() - 1);
    m_rows.clear();
    endRemoveRows();
    Q_EMIT scrollViewToEnd();
}

void TChatChannelController::removeOldestRow()
{
    if (rowCount() <= 0)
        return;
    const bool autoScroll = m_autoScroll;
    const int last = rowCount() - 1;
    beginRemoveRows({}, last, last);
    m_rows.pop_back();
    endRemoveRows();
    if (autoScroll)
        Q_EMIT scrollViewToEnd();
}

void TChatChannelController::onIdRangeChanged(const uint32_t firstId, const uint32_t afterLastId)
{
    if (firstId == afterLastId) {
        clearRows();
        return;
    }
    if (!m_channel)
        return;
    if (rowCount() == 0) {
        // Row 0 is the newest entry: each one goes in on top.
        beginInsertRows({}, 0, static_cast<int>(afterLastId - firstId) - 1);
        for (uint32_t id = firstId; id < afterLastId; ++id)
            m_rows.insert(m_rows.begin(), makeRow(m_channel->entry(id), id));
        endInsertRows();
        Q_EMIT scrollViewToEnd();
        return;
    }
    // The oldest rows whose entries left the channel go.
    const uint32_t oldest = m_rows.back().entryId;
    if (oldest < firstId) {
        for (uint32_t count = firstId - oldest; count > 0; --count)
            removeOldestRow();
    }
    // The entries after the newest row come on top, one by one.
    for (uint32_t id = m_rows.empty() ? firstId : m_rows.front().entryId + 1; id < afterLastId; ++id) {
        const bool autoScroll = m_autoScroll;
        beginInsertRows({}, 0, 0);
        m_rows.insert(m_rows.begin(), makeRow(m_channel->entry(id), id));
        endInsertRows();
        if (autoScroll)
            Q_EMIT scrollViewToEnd();
    }
}

void TChatChannelController::onTutorialMarkerChanged()
{
    if (rowCount() != 0)
        Q_EMIT dataChanged(index(0), index(rowCount() - 1), { TutorialMarkerRole });
}

void TChatChannelController::emitSelectionChanged(const int firstRow, const int lastRow)
{
    Q_EMIT dataChanged(index(firstRow), index(lastRow), { SelectionStartRole, SelectionEndRole });
}

namespace {

/// The direction a selection crosses a row: from the line's start (0) or
/// towards its end (1).
enum class ESelectionSide
{
    FromStart = 0,
    ToEnd = 1,
};

}

void TChatChannelController::Row::clearSelection()
{
    selectionStart = 0;
    selectionEnd = 0;
}

void TChatChannelController::Row::startSelection(const int position)
{
    selectionStart = position;
    selectionEnd = position;
    selectionAnchor = position;
}

void TChatChannelController::Row::selectToCursor(const int position)
{
    selectionStart = std::min(position, selectionAnchor);
    selectionEnd = std::max(position, selectionAnchor);
}

void TChatChannelController::Row::selectFromSide(const int position, const int side)
{
    if (side == static_cast<int>(ESelectionSide::FromStart))
        selectionAnchor = 0;
    else if (side == static_cast<int>(ESelectionSide::ToEnd))
        selectionAnchor = plainLength;
    selectToCursor(position);
}

void TChatChannelController::Row::selectWhole()
{
    selectionAnchor = 0;
    selectionStart = std::min(plainLength, 0);
    selectionEnd = std::max(plainLength, 0);
}

void TChatChannelController::Row::selectAnchorTo(const int side)
{
    if (side == static_cast<int>(ESelectionSide::ToEnd))
        selectToCursor(0);
    else if (side == static_cast<int>(ESelectionSide::FromStart))
        selectToCursor(plainLength);
}

void TChatChannelController::clearSelection()
{
    for (int row = m_selectionLow; row <= m_selectionHigh && row < rowCount(); ++row)
        m_rows[static_cast<size_t>(row)].clearSelection();
    emitSelectionChanged(m_selectionLow, m_selectionHigh);
    m_selectionHigh = 0;
    m_selectionLow = 0;
    m_selectionFocus = 0;
}

void TChatChannelController::startNewSelection(const int entryIndex, const int cursorPosition)
{
    if (m_rows.empty())
        return;
    clearSelection();
    if (entryIndex < 0 || entryIndex >= rowCount())
        return;
    m_rows[static_cast<size_t>(entryIndex)].startSelection(cursorPosition);
    m_selectionHigh = entryIndex;
    m_selectionLow = entryIndex;
    m_selectionFocus = entryIndex;
}

void TChatChannelController::updateSelection(const int entryIndex, const int cursorPosition)
{
    if (entryIndex < 0 || entryIndex >= rowCount() || m_selectionFocus >= rowCount())
        return;
    // Rows [low, high] hold the selection; the focus row is the end that moves.
    // Lower rows are newer.
    int high = m_selectionHigh;
    int low = m_selectionLow;
    const int focus = m_selectionFocus;
    const auto at = [this](const int row) -> Row& { return m_rows[static_cast<size_t>(row)]; };
    if (focus == entryIndex) {
        at(entryIndex).selectToCursor(cursorPosition);
    } else if (entryIndex < focus) {
        for (int row = focus; row >= entryIndex; --row) {
            if (row < m_selectionLow) {
                if (row == entryIndex)
                    at(row).selectFromSide(cursorPosition, static_cast<int>(ESelectionSide::FromStart));
                else
                    at(row).selectWhole();
            } else if (row > m_selectionLow) {
                if (row == entryIndex)
                    at(row).selectFromSide(cursorPosition, static_cast<int>(ESelectionSide::ToEnd));
                else
                    at(row).clearSelection();
            } else if (entryIndex < m_selectionLow) {
                at(row).selectAnchorTo(static_cast<int>(ESelectionSide::FromStart));
            } else {
                at(row).selectToCursor(cursorPosition);
            }
        }
        if (entryIndex < m_selectionLow)
            low = entryIndex;
        if (entryIndex < m_selectionHigh && focus == m_selectionHigh)
            high = m_selectionLow <= entryIndex ? entryIndex : m_selectionLow;
    } else {
        for (int row = focus; row <= entryIndex; ++row) {
            if (m_selectionHigh < row) {
                if (row == entryIndex)
                    at(row).selectFromSide(cursorPosition, static_cast<int>(ESelectionSide::ToEnd));
                else
                    at(row).selectWhole();
            } else if (row < m_selectionHigh) {
                if (row == entryIndex)
                    at(row).selectFromSide(cursorPosition, static_cast<int>(ESelectionSide::FromStart));
                else
                    at(row).clearSelection();
            } else if (m_selectionHigh < entryIndex) {
                at(row).selectAnchorTo(static_cast<int>(ESelectionSide::ToEnd));
            } else {
                at(row).selectToCursor(cursorPosition);
            }
        }
        if (m_selectionHigh < entryIndex)
            high = entryIndex;
        if (m_selectionLow < entryIndex && focus == m_selectionLow)
            low = entryIndex <= m_selectionHigh ? entryIndex : m_selectionHigh;
    }
    m_selectionHigh = high;
    m_selectionLow = low;
    m_selectionFocus = entryIndex;
    emitSelectionChanged(std::min(entryIndex, focus), std::max(entryIndex, focus));
}

void TChatChannelController::selectAll()
{
    if (m_rows.empty())
        return;
    startNewSelection(rowCount() - 1, 0);
    updateSelection(0, m_rows.front().plainLength);
}

void TChatChannelController::selectAllSince(const QDateTime& since)
{
    if (!m_channel || m_channel->entryCount() == 0 || rowCount() <= 0)
        return;
    // Row 0 is the newest; the selection ends before the first older entry.
    int row = 0;
    for (; row < rowCount(); ++row) {
        const uint32_t entryId = m_rows[static_cast<size_t>(row)].entryId;
        if (entryId >= m_channel->firstEntryId() && entryId < m_channel->afterLastEntryId()
            && m_channel->entry(entryId).time < since)
            break;
    }
    const int oldestRow = row - 1;
    if (oldestRow == -1)
        return;
    startNewSelection(oldestRow, 0);
    updateSelection(0, m_rows.front().plainLength);
}

void TChatChannelController::copySelectionToClipboard()
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionCopyTextToClipboard>(selectedText()));
}

void TChatChannelController::selectTextPassage(const int entryIndex, const int passageStart, const int passageEnd)
{
    startNewSelection(entryIndex, passageStart);
    updateSelection(entryIndex, passageEnd);
}

void TChatChannelController::invalidateSizes()
{
    for (Row& r : m_rows)
        r.size = QSize();
    if (!m_rows.empty())
        Q_EMIT dataChanged(index(0), index(rowCount() - 1), { Qt::SizeHintRole });
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
    if (m_channel)
        Q_EMIT subscribeToServerLogRequested(subscribe, m_channel);
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
}

TChatChannelTabController::TChatChannelTabController(QObject* parent)
    : cip::tibia::gamewindow::TChatChannelTabController(parent)
{
    m_currentRowId = 0;
}

void TChatChannelTabController::setChannelStorage(chat::TChatChannelStorage* storage)
{
    if (m_storage) {
        disconnect(m_storage, nullptr, this, nullptr);
        m_storage = nullptr;
    }
    removeAllTabs();
    if (!storage)
        return;
    m_storage = storage;
    const std::vector<TChatChannel*> channels = storage->channels();
    removeAllTabs();
    if (!channels.empty()) {
        beginInsertRows({}, 0, static_cast<int>(channels.size()) - 1);
        for (TChatChannel* channel : channels)
            addRenderInfo(channel);
        endInsertRows();
        setCurrentRow(0);
    }
    connect(storage, &chat::TChatChannelStorage::newChatChannelOpened, this, &TChatChannelTabController::onAddNewChatChannelTab);
    connect(storage, &chat::TChatChannelStorage::chatChannelReopend, this, &TChatChannelTabController::onChatChannelReopend);
    connect(storage, &chat::TChatChannelStorage::chatChannelRemoved, this, &TChatChannelTabController::onChatChannelRemoved);
    connect(storage, &chat::TChatChannelStorage::entryAddedToChatChannel, this, &TChatChannelTabController::onEntryAddedToChatChannel);
}

void TChatChannelTabController::removeAllTabs()
{
    setCurrentRow(-1);
    if (const int count = rowCount(); count != 0) {
        beginRemoveRows({}, 0, count - 1);
        m_rows.clear();
        m_renderInfos.clear();
        endRemoveRows();
    }
}

chat::TChatChannel* TChatChannelTabController::channelAt(const int row) const
{
    return row >= 0 && row < static_cast<int>(m_rows.size()) ? m_rows[static_cast<size_t>(row)] : nullptr;
}

std::vector<chat::TChatChannelIdentifier> TChatChannelTabController::identifiers() const
{
    std::vector<chat::TChatChannelIdentifier> result;
    for (const TChatChannel* channel : m_rows)
        result.push_back(channel->identifier());
    return result;
}

int TChatChannelTabController::rowOf(const chat::TChatChannel* channel) const
{
    const auto it = std::find(m_rows.begin(), m_rows.end(), channel);
    return it == m_rows.end() ? -1 : static_cast<int>(it - m_rows.begin());
}

int TChatChannelTabController::rowOf(const chat::TChatChannelIdentifier& identifier) const
{
    const auto it = std::find_if(m_rows.begin(), m_rows.end(),
                                 [&](const TChatChannel* channel) { return channel->identifier().sameChannel(identifier); });
    return it == m_rows.end() ? -1 : static_cast<int>(it - m_rows.begin());
}

chat::TChatChannel* TChatChannelTabController::currentChannel() const
{
    return m_currentRowId == -1 ? nullptr : channelAt(m_currentRowId);
}

void TChatChannelTabController::setMakeNewTabsCurrent(const bool makeNewTabsCurrent)
{
    m_makeNewTabsCurrent = makeNewTabsCurrent;
}

void TChatChannelTabController::resetTabHighlight(const chat::TChatChannelIdentifier& identifier)
{
    const int row = rowOf(identifier);
    if (row < 0)
        return;
    if (TChatChannelTabRenderInfo* info = renderInfo(channelAt(row)))
        info->resetTabHighlight();
}

int TChatChannelTabController::rowCount(const QModelIndex&) const
{
    return static_cast<int>(m_rows.size());
}

int TChatChannelTabController::currentRowId() const
{
    return m_currentRowId;
}

QVariant TChatChannelTabController::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
        return {};
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
    QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
    roles[ChannelNameRole] = "channelName";
    roles[TabHighlightRole] = "tabHighlight";
    return roles;
}

uint TChatChannelTabController::highlightOf(const int row) const
{
    const TChatChannelTabRenderInfo* info = renderInfo(channelAt(row));
    return info ? info->tabHighlight() : NONE;
}

uint TChatChannelTabController::leftButtonHighlight() const
{
    uint result = NONE;
    for (uint row = 0; row < m_hiddenLeft; ++row) {
        if (static_cast<uint>(rowCount()) <= row)
            return result;
        const uint highlight = highlightOf(static_cast<int>(row));
        if (highlight == FLASH_NEW_ENTRY)
            return FLASH_NEW_ENTRY;
        if (highlight == NEW_ENTRY)
            result = NEW_ENTRY;
    }
    return result;
}

uint TChatChannelTabController::rightButtonHighlight() const
{
    uint result = NONE;
    for (uint row = static_cast<uint>(rowCount()) - m_hiddenRight; row < static_cast<uint>(rowCount()); ++row) {
        const uint highlight = highlightOf(static_cast<int>(row));
        if (highlight == FLASH_NEW_ENTRY)
            return FLASH_NEW_ENTRY;
        if (highlight == NEW_ENTRY)
            result = NEW_ENTRY;
    }
    return result;
}

TChatChannelTabRenderInfo* TChatChannelTabController::renderInfo(const chat::TChatChannel* channel) const
{
    const auto it = m_renderInfos.find(channel);
    return it == m_renderInfos.end() ? nullptr : it->second.get();
}

void TChatChannelTabController::addRenderInfo(chat::TChatChannel* channel)
{
    auto info = std::make_unique<TChatChannelTabRenderInfo>(channel);
    connect(info.get(), &TChatChannelTabRenderInfo::tabHighlightChanged, this, &TChatChannelTabController::onTabHighlightChanged);
    m_renderInfos[channel] = std::move(info);
    m_rows.push_back(channel);
}

void TChatChannelTabController::onTabHighlightChanged(chat::TChatChannel* channel)
{
    const int row = rowOf(channel);
    if (row < 0)
        return;
    Q_EMIT dataChanged(index(row), index(row), { TabHighlightRole });
    if (static_cast<uint>(row) < m_hiddenLeft)
        Q_EMIT leftButtonHighlightChanged();
    if (static_cast<uint>(row) < static_cast<uint>(rowCount()) - m_hiddenRight)
        return;
    Q_EMIT rightButtonHighlightChanged();
}

void TChatChannelTabController::setCurrentRow(const int row)
{
    // A row out of range leaves no tab current.
    if (row < 0 || row >= rowCount()) {
        m_currentRowId = -1;
        Q_EMIT currentRowIdChanged();
        Q_EMIT currentChannelChanged(nullptr);
        return;
    }
    if (TChatChannelTabRenderInfo* previous = renderInfo(channelAt(m_currentRowId));
        previous && previous->tabHighlight() == CURRENT)
        previous->setTabHighlight(TChatChannelTabRenderInfo::NONE);
    m_currentRowId = row;
    TChatChannel* channel = channelAt(row);
    if (TChatChannelTabRenderInfo* info = renderInfo(channel))
        info->setTabHighlight(TChatChannelTabRenderInfo::CURRENT);
    Q_EMIT currentRowIdChanged();
    Q_EMIT currentChannelChanged(channel);
}

void TChatChannelTabController::onMakeChannelWithRowIdCurrent(const int rowId)
{
    setCurrentRow(rowId);
}

void TChatChannelTabController::onMakeNextChannelCurrent()
{
    if (m_currentRowId < 0)
        return;
    if (const int count = rowCount(); count > 0)
        setCurrentRow((m_currentRowId + 1) % count);
}

void TChatChannelTabController::onMakePreviousChannelCurrent()
{
    if (m_currentRowId < 0)
        return;
    if (const int count = rowCount(); count > 0)
        setCurrentRow((m_currentRowId + count - 1) % count);
}

void TChatChannelTabController::onMakeChannelWithChatChannelIdentifierCurrent(const chat::TChatChannelIdentifier& identifier)
{
    if (const int row = rowOf(identifier); row >= 0)
        setCurrentRow(row);
}

void TChatChannelTabController::onChannelTabClicked(const int rowId, const int mouseButtonRaw, const int keyboardModifiersRaw)
{
    if (TChatChannel* channel = channelAt(rowId))
        Q_EMIT tabClicked(channel, mouseButtonRaw, keyboardModifiersRaw);
}

void TChatChannelTabController::setNumberOfHiddenTabs(uint hiddenLeft, uint hiddenRight)
{
    // At most every tab is hidden on either side.
    hiddenLeft = std::min(hiddenLeft, static_cast<uint>(rowCount()));
    hiddenRight = std::min(hiddenRight, static_cast<uint>(rowCount()));
    if (m_hiddenLeft != hiddenLeft) {
        m_hiddenLeft = hiddenLeft;
        Q_EMIT leftButtonHighlightChanged();
    }
    if (m_hiddenRight != hiddenRight) {
        m_hiddenRight = hiddenRight;
        Q_EMIT rightButtonHighlightChanged();
    }
}

void TChatChannelTabController::moveTab(const int sourceIndex, const int targetIndex)
{
    // The dragged tab moves to the target position and becomes current; the
    // channel storage keeps its order.
    const int count = rowCount();
    if (sourceIndex == targetIndex || sourceIndex < 0 || targetIndex < 0 || sourceIndex >= count || targetIndex >= count)
        return;
    const int destination = sourceIndex < targetIndex ? targetIndex + 1 : targetIndex;
    beginMoveRows({}, sourceIndex, sourceIndex, {}, destination);
    const auto rows = m_rows.begin();
    if (sourceIndex < targetIndex)
        std::rotate(rows + sourceIndex, rows + sourceIndex + 1, rows + targetIndex + 1);
    else
        std::rotate(rows + targetIndex, rows + sourceIndex, rows + sourceIndex + 1);
    endMoveRows();
    setCurrentRow(targetIndex);
}

void TChatChannelTabController::onAddNewChatChannelTab(const chat::TChatChannelIdentifier& identifier, const bool forceMakeCurrent)
{
    TChatChannel* channel = m_storage ? m_storage->find(identifier) : nullptr;
    if (!channel)
        return;
    const int row = rowCount();
    beginInsertRows({}, row, row);
    addRenderInfo(channel);
    endInsertRows();
    if ((forceMakeCurrent || channel->makeCurrentOnOpen()) && m_makeNewTabsCurrent)
        setCurrentRow(rowCount() - 1);
}

void TChatChannelTabController::onChatChannelReopend(const chat::TChatChannelIdentifier& identifier, const bool forceMakeCurrent)
{
    if (m_storage) {
        const TChatChannel* channel = m_storage->find(identifier);
        if (!(channel && channel->makeCurrentOnOpen()) && !forceMakeCurrent)
            return;
    }
    onMakeChannelWithChatChannelIdentifierCurrent(identifier);
}

void TChatChannelTabController::onChatChannelRemoved(const chat::TChatChannelIdentifier& identifier)
{
    const int row = rowOf(identifier);
    if (row < 0)
        return;
    beginRemoveRows({}, row, row);
    m_renderInfos.erase(m_rows[static_cast<size_t>(row)]);
    m_rows.erase(m_rows.begin() + row);
    endRemoveRows();
    // The current tab's successor (or the new last tab) becomes current; a
    // current tab after it moves up with it.
    int current = m_currentRowId;
    if (current == row) {
        current = row - (rowCount() <= row ? 1 : 0);
    } else {
        if (current <= row)
            return;
        current = current - 1;
    }
    setCurrentRow(current);
}

void TChatChannelTabController::onEntryAddedToChatChannel(const chat::TChatChannelIdentifier& identifier)
{
    // NPC conversations and channels shown in the secondary pane do not flash;
    // the current tab ignores it.
    const int row = rowOf(identifier);
    const TChatChannel* channel = m_storage ? m_storage->find(identifier) : nullptr;
    const bool secondary = channel && channel->isShownAsSecondaryChannel();
    if (identifier.type == chat::TChatChannelIdentifier::Npc || row < 0 || secondary)
        return;
    if (TChatChannelTabRenderInfo* info = renderInfo(channelAt(row)))
        info->setTabHighlight(TChatChannelTabRenderInfo::FLASH_NEW_ENTRY);
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
    Q_EMIT abort();
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
    dialog()->close();
    Q_EMIT privateChannelChosen(recipient);
}

// ---------------------------------------------------------------------------
// TChatController

TChatController::TChatController(QQuickItem* chatItem, client::TGameClient& gameClient, QObject* parent)
    : cip::tibia::gamewindow::TChatController(parent)
    , m_chatItem(chatItem)
    , m_mapper(config::controlSchemeForListModelIndex(
          app::ClientOptions::load().options().value(QLatin1String("controlSchemeIndex")).toInt(0)))
    , m_gameClient(gameClient)
{
    own(this);
    own(&m_primary);
    own(&m_secondary);
    own(&m_tabs);
    connect(&m_primary, &TChatChannelController::publishGameAction, this, &TChatController::publishGameAction);
    connect(&m_secondary, &TChatChannelController::publishGameAction, this, &TChatController::publishGameAction);

    reloadOptions();

    // For 2 s after a session connects, its new channels' tabs do not become
    // current.
    m_makeNewTabsCurrentTimer.setInterval(2000);
    m_makeNewTabsCurrentTimer.setSingleShot(true);
    connect(&m_makeNewTabsCurrentTimer, &QTimer::timeout, this, [this] { m_tabs.setMakeNewTabsCurrent(true); });
    // CIP / : both console buttons flash for 1000 ms.
    m_ignoredFlashTimer.setSingleShot(true);
    m_ignoredFlashTimer.setInterval(1000);
    connect(&m_ignoredFlashTimer, &QTimer::timeout, this, &TChatController::onIgnoredFlashTimeout);
    m_exivaSuppressedTimer.setSingleShot(true);
    m_exivaSuppressedTimer.setInterval(1000);
    connect(&m_exivaSuppressedTimer, &QTimer::timeout, this, &TChatController::onExivaSuppressedFlashTimeout);

    for (TGenericEnterTextDialogController* prompt : { &m_inviteDialog, &m_excludeDialog }) {
        const bool exclude = prompt == &m_excludeDialog;
        prompt->setCaption(qtTrId(exclude ? "chat_exclude_player_caption" : "chat_invite_player_caption"));
        prompt->setDescription(qtTrId("chat_enter_character_name"));
        prompt->setMaxInputLength(29);
        connect(prompt, &TGenericEnterTextDialogController::textEntered, this,
                exclude ? &TChatController::onPlayerNameToExcludeFromChatChannelReceived
                        : &TChatController::onPlayerNameToInviteToChatChannelReceived);
    }
    m_messageDialog.setButtons(okDialogButtons());
    m_messageDialog.setCloseOnButtonClicked(true);

    connect(&m_mode, &chat::TChatModeManager::chatModeChanged, this, &TChatController::chatModeChanged);
    connect(&m_tabs, &TChatChannelTabController::currentChannelChanged, &m_primary, &TChatChannelController::setChannel);
    connect(&m_tabs, &TChatChannelTabController::tabClicked, this, &TChatController::onChatChannelTabClicked);
    for (TChatChannelController* model : { &m_primary, &m_secondary }) {
        connect(model, &TChatChannelController::closeRequested, this, &TChatController::onRequestCloseChannel);
        connect(model, &TChatChannelController::subscribeToServerLogRequested, this, &TChatController::onRequestSubscribeToServerLog);
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
    connect(&m_openDialog, &TOpenChatChannelDialogController::channelChosen, this, &TChatController::onRequestOpenChanel);
    connect(&m_openDialog, &TOpenChatChannelDialogController::privateChannelChosen, this,
            [](const QString& name) { tibia::game::openPrivateChannel(name.toStdString()); });

    connect(&gameClient, &client::TGameClient::worldEntered, this, &TChatController::onWorldEntered);
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, &TChatController::onOptionChanged);

    for (const A type : HandledActions) {
        m_gameActions.on(type, [this](const input::IGameAction& action) { return handleAction(action); });
    }

    // Every handler sees these actions; the chat acts unless the NPC dialog
    // holds the keyboard.
    m_gameActions.on(A::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD, [this](const input::IGameAction&) {
        if (!m_npcDialog || !m_npcDialog->hasFocus())
            copySelectedText();
    });
    m_gameActions.on(A::REQUEST_SELECT_ALL_TEXT, [this](const input::IGameAction&) {
        if (!m_npcDialog || !m_npcDialog->hasFocus())
            selectAllText();
    });

    m_primary.setChannel(m_tabs.currentChannel());
    bindItem();
}

TChatController::~TChatController()
{
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
    if (m_chatProtocolMessageHandler)
        m_chatProtocolMessageHandler->setOptions(values);
    const bool timestamps = values.value(QLatin1String("consoleShowTimestamps")).toBool(true);
    const bool seconds = values.value(QLatin1String("consoleShowTimestampsSeconds")).toBool(true);
    const bool levels = values.value(QLatin1String("consoleShowLevels")).toBool(true);
    m_primary.onEntryFormatChanged(timestamps, seconds, levels);
    m_secondary.onEntryFormatChanged(timestamps, seconds, levels);
}

void TChatController::onOptionChanged(const QString& name)
{
    if (name == QLatin1String("controlSchemeIndex"))
        m_mapper.setControlScheme(config::controlSchemeForListModelIndex(app::ClientOptions::load().options().value(name).toInt(0)));
    else if (name.startsWith(QLatin1String("console")) || name.startsWith(QLatin1String("gameWindowShow")) || name == QLatin1String("openNewPrivateMessagesInNewTab"))
        reloadOptions();
}

void TChatController::onWorldEntered()
{
    if (m_chatChannelStorage)
        m_chatChannelStorage->setOwnName(ownName());
    bindItem();
    Q_EMIT exivaOptionsAvailableChanged();
    if (!m_channelsFromConfigOpened)
        openChannelsFromConfig();
}

void TChatController::openChannelsFromConfig()
{
    m_channelsFromConfigOpened = true;
    const QJsonObject chatOptions = app::ClientOptions::load().root().value(QLatin1String("chatOptions")).toObject();
    // product (game_chat): one join per login-sync tick, under the server's packets-per-second limit.
    for (const QJsonValue& id : chatOptions.value(QLatin1String("openChannels")).toArray()) {
        const auto channelId = static_cast<uint16_t>(id.toInt());
        tibia::mods::enqueueLoginSync([channelId] { tibia::game::joinChannel(channelId); });
    }
    if (chatOptions.value(QLatin1String("lootChannelOpen")).toBool(true) && m_chatChannelStorage)
        m_chatChannelStorage->open({ TChatChannelIdentifier::Loot }, false, false);
}

void TChatController::onChannelAdded(chat::TChatChannel* channel, bool)
{
    const TChatChannelIdentifier::Type type = channel->identifier().type;
    if (m_channelsFromConfigOpened && (type == TChatChannelIdentifier::ChannelId || type == TChatChannelIdentifier::Loot))
        storeOpenChannels();
}

void TChatController::storeOpenChannels() const
{
    if (!m_chatChannelStorage)
        return;
    QJsonObject chatOptions = app::ClientOptions::load().root().value(QLatin1String("chatOptions")).toObject();
    QJsonArray ids;
    bool loot = false;
    for (const TChatChannelIdentifier& identifier : m_tabs.identifiers()) {
        const TChatChannel* channel = m_chatChannelStorage->find(identifier);
        if (!channel)
            continue;
        if (identifier.type == TChatChannelIdentifier::Loot)
            loot = true;
        else if (identifier.type == TChatChannelIdentifier::ChannelId && !channel->isOwnPrivateChannel() && !identifier.isGuildChannel()
                 && !identifier.isPartyChannel())
            ids.append(identifier.id);
    }
    chatOptions.insert(QLatin1String("openChannels"), ids);
    chatOptions.insert(QLatin1String("lootChannelOpen"), loot);
    app::ClientOptions::storeAll({ { QStringLiteral("chatOptions"), chatOptions } });
}

void TChatController::closeMessageDialog()
{
    m_messageDialog.dialog()->close();
}

void TChatController::onMessageFiltered()
{
    m_ignoredFlash = true;
    Q_EMIT messageIgnoredFlashChanged();
    m_ignoredFlashTimer.start();
}

void TChatController::onExivaSuppressed()
{
    m_exivaSuppressedFlash = true;
    Q_EMIT exivaSuppressedFlashChanged();
    m_exivaSuppressedTimer.start();
}

void TChatController::onIgnoredFlashTimeout()
{
    if (!m_ignoredFlash)
        return;
    m_ignoredFlash = false;
    Q_EMIT messageIgnoredFlashChanged();
}

void TChatController::onExivaSuppressedFlashTimeout()
{
    if (!m_exivaSuppressedFlash)
        return;
    m_exivaSuppressedFlash = false;
    Q_EMIT exivaSuppressedFlashChanged();
}

bool TChatController::exivaOptionsAvailable() const
{
    return m_gameClient.worldType() == WorldTypeOptionalPvp;
}

void TChatController::openWhiteBlacklistDialog()
{
    Q_EMIT publishGameAction(input::makeGameAction(A::OPEN_WHITEBLACKLIST_DIALOG));
}

void TChatController::openExivaOptionsDialog()
{
    Q_EMIT publishGameAction(input::makeGameAction(A::OPEN_EXIVA_OPTIONS_DIALOG));
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
    if (tibia::game::isOnline())
        m_chatChannelStorage->addServerLogEntry(chat::consoleColor(30), qtTrId("chat_append_contents").arg(name, QDir::toNativeSeparators(path)));
}

void TChatController::onPlayerNameToInviteToChatChannelReceived(const QString& playerName)
{
    // The server channel the prompt was opened for; then the prompt's target goes.
    if (m_inviteTarget && m_inviteTarget->identifier().type == TChatChannelIdentifier::ChannelId && m_inviteTarget->isOwnPrivateChannel())
        tibia::game::inviteToOwnChannel(playerName.trimmed().toStdString());
    m_inviteTarget = nullptr;
}

void TChatController::onPlayerNameToExcludeFromChatChannelReceived(const QString& playerName)
{
    if (m_inviteTarget && m_inviteTarget->identifier().type == TChatChannelIdentifier::ChannelId && m_inviteTarget->isOwnPrivateChannel())
        tibia::game::excludeFromOwnChannel(playerName.trimmed().toStdString());
    m_inviteTarget = nullptr;
}

void TChatController::openInviteDialog(chat::TChatChannel* channel, const bool exclude)
{
    if (!channel)
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

void TChatController::onChannelListReceived(const std::vector<TChatChannelIdentifier>& channels)
{
    if (!m_channelListRequested)
        return;
    m_channelListRequested = false;
    m_openDialog.open(channels);
}

void TChatController::requestChannelList()
{
    Q_EMIT publishGameAction(input::makeGameAction(A::GET_CHAT_CHANNEL_LIST));
}

void TChatController::onRequestOpenChanel(const TChatChannelIdentifier& ChannelIdentifier)
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionChatChannel>(
        A::OPEN_CHAT_CHANNEL, std::make_shared<const TChatChannelIdentifier>(ChannelIdentifier)));
}

void TChatController::requestChannelListFromServer()
{
    if (!tibia::game::isOnline())
        return;
    m_channelListRequested = true;
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
    switch (text.at(1).unicode()) {
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
    m_mode.onMessageSent();
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
    if (!m_chatChannelStorage)
        return keep;
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
        if (m_chatChannelStorage->ownChannel() && !text.isEmpty())
            tibia::game::inviteToOwnChannel(text.toStdString());
        return keep;
    case EInput::Exclude:
        if (m_chatChannelStorage->ownChannel() && !text.isEmpty())
            tibia::game::excludeFromOwnChannel(text.toStdString());
        return keep;
    default: break;
    }
    if (text.isEmpty() || !current || !tibia::game::isOnline())
        return keep;

    const TChatChannelIdentifier& identifier = current->identifier();
    switch (input) {
    case EInput::Default:
        // The pane's channel talks on the map (or a spell is cast), unless the
        // line goes to a given channel.
        if (!target && (m_primary.writesToLocalChat() || isSpell))
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
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionSendChatMessage>(text, A::SEND_CHAT_MESSAGE_AS_GAMEMASTER_BROADCAST));
        break;
    case EInput::RedChannel:
        if (identifier.type == TChatChannelIdentifier::ChannelId && !m_primary.writesToLocalChat() && !target)
            Q_EMIT publishGameAction(std::make_shared<input::TGameActionSendChatMessageWithChannelID>(text, identifier.id, true));
        else
            m_chatChannelStorage->addSystemEntry(current, chat::systemRed(), qtTrId("chat_invalid_channel"));
        break;
    case EInput::Private:
        if (identifier.type == TChatChannelIdentifier::MessageTo && identifier.name == recipient)
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
    if (!channel) {
        // The chat's game action handler casts spells by their cast type.
        Q_EMIT publishGameAction(
            std::make_shared<input::TGameActionSendChatMessageWithSpeechVolume>(text, std::clamp(volume, 0, 2), spellCastType));
        return;
    }
    const TChatChannelIdentifier& identifier = channel->identifier();
    switch (identifier.type) {
    case TChatChannelIdentifier::Npc:
        if (m_npcDialog && m_npcDialog->sayToRemoteNpc(text)) {
            if (m_chatChannelStorage)
                m_chatChannelStorage->addOwnNpcChannelEntry(chat::consoleColor(12), ownName(), text, ownLevel());
        } else {
            Q_EMIT publishGameAction(
                std::make_shared<input::TGameActionSendChatMessageToNpc>(text, ownName(), static_cast<uint16_t>(ownLevel())));
        }
        break;
    case TChatChannelIdentifier::MessageTo:
        sendPrivate(identifier.name, text, false);
        break;
    case TChatChannelIdentifier::ChannelId:
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionSendChatMessageWithChannelID>(text, identifier.id, false));
        break;
    default:
        break;
    }
}

void TChatController::sendPrivate(const QString& recipient, const QString& text, const bool gamemaster)
{
    // The handler adds the own line and sends it (1: a gamemaster's red message).
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionSendChatMessageWithRecipient>(
        text, recipient, ownName(), static_cast<uint16_t>(ownLevel()), gamemaster ? 1 : 0));
}

void TChatController::sendKeyword(const QString& keyword)
{
    // The keyword is sent right away, through the game action.
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionSetCurrentChatChannelInputText>(keyword, true));
}

void TChatController::setInputText(const QString& text, const bool sendDirectly)
{
    if (!m_chatItem)
        return;
    m_chatItem->setProperty("currentChatInput", text);
    if (sendDirectly)
        pressEnter();
}

/// The chat mode learns that a line was sent without the chat input.
void TChatController::onChatMessageSendDirectly()
{
    m_mode.onMessageSent();
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
    if (chatInputText.isEmpty())
        return;
    // A full history drops its oldest line first; a repeat of the newest line is not stored again.
    if (m_history.size() >= maxHistory)
        m_history.removeFirst();
    if (m_history.isEmpty() || m_history.constLast() != chatInputText)
        m_history.append(chatInputText);
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

void TChatController::onRequestCloseChannel(chat::TChatChannel* channel)
{
    if (!channel)
        return;
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionChatChannel>(
        A::CLOSE_CHAT_CHANNEL, std::make_shared<const TChatChannelIdentifier>(channel->identifier())));
}

void TChatController::onRequestSubscribeToServerLog(const bool subscribe, chat::TChatChannel* channel)
{
    if (!channel)
        return;
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionChatChannel>(
        subscribe ? A::CHAT_CHANNEL_SUBSCRIBE_TO_SERVER_LOG : A::CHAT_CHANNEL_UNSUBSCRIBE_SERVER_LOG,
        std::make_shared<const TChatChannelIdentifier>(channel->identifier())));
}

void TChatController::closeChannel(chat::TChatChannel* channel)
{
    if (!channel || !channel->canBeClosed())
        return;
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionChatChannel>(
        A::CLOSE_CHAT_CHANNEL, std::make_shared<const TChatChannelIdentifier>(channel->identifier())));
}

void TChatController::setSecondary(chat::TChatChannel* channel)
{
    const bool wasShown = m_secondary.channel() != nullptr;
    if (TChatChannel* previous = m_secondary.channel())
        previous->setShownAsSecondaryChannel(false);
    if (channel) {
        channel->setShownAsSecondaryChannel(true);
        m_tabs.resetTabHighlight(channel->identifier());
    }
    m_secondary.setChannel(channel);
    if (wasShown != (channel != nullptr) && m_chatItem)
        m_chatItem->setProperty("secondaryChatChannelModel", QVariant::fromValue<QObject*>(channel ? &m_secondary : nullptr));
}

void TChatController::onSecondaryChannelClicked(const int mouseButtonRaw, const int keyboardModifiersRaw)
{
    onChatChannelTabClicked(m_secondary.channel(), mouseButtonRaw, keyboardModifiersRaw);
}

void TChatController::onChatChannelRemoved(TChatChannel* channel)
{
    // The secondary pane hides when its channel goes.
    if (m_secondary.channel() == channel)
        setSecondary(nullptr);
    const TChatChannelIdentifier::Type type = channel->identifier().type;
    if (m_channelsFromConfigOpened && (type == TChatChannelIdentifier::ChannelId || type == TChatChannelIdentifier::Loot))
        storeOpenChannels();
}

void TChatController::onSelectTabAsSecondaryChannel(const int rowId)
{
    if (TChatChannel* channel = m_tabs.channelAt(rowId))
        setSecondary(channel);
}

bool TChatController::handleAction(const input::IGameAction& action)
{
    const bool noDialog = !TDialogStackController::isAnyDialogStacked();
    const auto* channelAction = input::gameActionAs<input::TGameActionChatChannel>(&action);
    const auto* visibleAction = input::gameActionAs<input::TGameActionVisibleChatChannel>(&action);
    // CIP TGameActionVisibleChatChannel: the pane the action is for.
    const auto visibleChannel = [&]() -> TChatChannel* {
        return visibleAction && visibleAction->isSecondaryChannel() ? m_secondary.channel() : m_primary.channel();
    };
    // CIP TGameActionChatChannel: the channel the action names.
    const auto namedChannel = [&]() -> TChatChannel* {
        return channelAction && m_chatChannelStorage ? m_chatChannelStorage->find(channelAction->chatChannelIdentifier()) : nullptr;
    };
    switch (action.gameActionType()) {
    case A::GET_CHAT_CHANNEL_LIST:
        // chat::TChatGameActionHandler asks the server; its answer opens the dialog.
        requestChannelListFromServer();
        return true;
    case A::CLOSE_CURRENT_CHAT_CHANNEL:
        closeChannel(m_tabs.currentChannel());
        return true;
    case A::TOGGLE_SERVER_LOG_SUBSCRIPTION_IN_CURRENT_CHAT_CHANNEL:
        m_primary.setSubscribeToServerLog(!m_primary.isSubscribedToServerLog());
        return true;
    case A::PRESS_ENTER_IN_CHAT:
        // product: while the NPC dialog is open, Enter drives that input (CIP
        // focuses the NPC pane; we never hand Enter to HudChat).
        if (m_npcDialog && m_npcDialog->isShown()) {
            if (!m_npcDialog->hasFocus())
                m_npcDialog->takeFocus();
            else
                m_npcDialog->pressEnter();
        } else if (const input::TEventFilterManager* eventFilters = input::TEventFilterManager::instance();
                   noDialog || !eventFilters || !eventFilters->isBlocked()) {
            pressEnter();
        }
        return true;
    case A::SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT: {
        const auto* input = input::gameActionAs<input::TGameActionSetCurrentChatChannelInputText>(&action);
        if (!input)
            return true;
        if (m_npcDialog && m_npcDialog->isShown()) {
            m_npcDialog->setInputText(input->text(), input->sendAutomatically(), input->isSpell(), input->spellCastOptionalTargetType());
            return true;
        }
        // CIP : sent text goes out at say volume with the hotkey's
        // spell flags, and the input is cleared.
        if (input->sendAutomatically() && m_chatItem) {
            sendChatMessage(input->text(), 0, input->isSpell(), input->spellCastOptionalTargetType());
            m_chatItem->setProperty("currentChatInput", QString());
        } else {
            setInputText(input->text(), input->sendAutomatically());
        }
        return true;
    }
    case A::DISPLAY_NEXT_CHAT_CHANNEL_TAB:
        m_tabs.onMakeNextChannelCurrent();
        return true;
    case A::DISPLAY_PREVIOUS_CHAT_CHANNEL_TAB:
        m_tabs.onMakePreviousChannelCurrent();
        return true;
    case A::SWITCH_CHAT_MODE: {
        // CIP Enter-from-chat-off → ChatModeTemporaryOn. With an NPC dialog open,
        // focus that input instead of opening HudChat (bubble stays closed).
        const auto* mode = input::gameActionAs<input::TGameActionSwitchChatMode>(&action);
        m_mode.handleGameAction(action);
        if (mode && mode->chatMode() == chat::TChatModeManager::CHAT_TEMPORARY_ON && m_npcDialog && m_npcDialog->isShown())
            m_npcDialog->takeFocus();
        return true;
    }
    case A::OPEN_INVITE_TO_CHAT_CHANNEL_DIALOG:
    case A::OPEN_EXCLUDE_FROM_CHAT_CHANNEL_DIALOG:
        if (visibleAction)
            openInviteDialog(visibleChannel(), action.gameActionType() == A::OPEN_EXCLUDE_FROM_CHAT_CHANNEL_DIALOG);
        return true;
    case A::SAVE_CHAT_CHANNEL_TO_FILE:
        if (visibleAction)
            saveChannel(visibleChannel());
        return true;
    case A::RECENTLY_TRADED:
        if (const auto* traded = input::gameActionAs<input::TGameActionRecentlyTraded>(&action))
            showRecentlyTraded(traded->name());
        return true;
    case A::EXIVA_WAS_SUPPRESSED:
        onExivaSuppressed();
        return true;
    case A::SHOW_CHAT_CHANNEL_AS_SECONDARY:
    case A::SELECT_CHAT_CHANNEL_AS_SECONDARY:
        if (TChatChannel* channel = namedChannel())
            setSecondary(channel);
        return true;
    case A::HIDE_SECONDARY_CHAT_CHANNEL:
        setSecondary(nullptr);
        return true;
    case A::CHAT_CHANNEL_SELECT_ALL:
        if (visibleAction) {
            if (visibleAction->isSecondaryChannel())
                m_secondary.selectAll();
            else
                m_primary.selectAll();
        }
        return true;
    case A::COPY_CHAT_ENTRY_TO_CLIPBOARD:
        if (const auto* copy = input::gameActionAs<input::TGameActionCopyTextToClipboard>(&action); copy && !copy->text().isEmpty())
            QGuiApplication::clipboard()->setText(copy->text());
        return true;
    default:
        return false;
    }
}

void TChatController::selectAllText()
{
    if (!m_chatItem || !m_chatItem->property("keepChatInputFocus").toBool())
        return;
    if (m_chatItem->property("currentChatInput").toString().isEmpty()) {
        m_primary.selectAll();
        return;
    }
    m_primary.startNewSelection(0, 0);
    QMetaObject::invokeMethod(m_chatItem, "selectAllChatInput");
}

void TChatController::copySelectedText()
{
    if (!m_chatItem)
        return;
    const QString inputSelection = m_chatItem->property("currentChatInputSelectedText").toString();
    if (!inputSelection.isEmpty()) {
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionCopyTextToClipboard>(inputSelection));
        return;
    }
    if (m_primary.hasSelection())
        m_primary.copySelectionToClipboard();
    else if (m_secondary.hasSelection())
        m_secondary.copySelectionToClipboard();
}

void TChatController::showEntryMenu(TChatChannelController* model, const int row, const int button, const int modifiers)
{
    const TChatEntry* entry = model->entryAt(row);
    if (!entry)
        return;
    if (!m_actionProvider)
        return;
    const input::TGameActionList candidates = m_actionProvider->entryGameActions(
        model->plainTextAt(row), *entry, model->hasSelection() ? model->selectedText() : QString(), ownName(), model == &m_secondary);
    showMenu(candidates, EntryMenuOrder, ChatEntryContext, button, modifiers, model, nullptr);
}

void TChatController::onChatChannelTabClicked(chat::TChatChannel* channel, const int button, const int modifiers)
{
    if (!m_actionProvider)
        return;
    const input::TGameActionList candidates = m_actionProvider->gameActions(channel ? &channel->identifier() : nullptr);
    if (!channel) {
        // The empty tab bar: every channel to pick as the secondary one.
        auto* menu = TContextMenuController::instance();
        if (!menu || m_mapper.map(candidates, ChatTabContext, static_cast<Qt::MouseButton>(button), modifiers) == nullptr)
            return;
        menu->beginCustomMenu();
        for (const input::IGameActionPtr& candidate : candidates) {
            const auto* select = input::gameActionAs<input::TGameActionChatChannel>(candidate);
            if (!select || candidate->gameActionType() != A::SELECT_CHAT_CHANNEL_AS_SECONDARY)
                continue;
            QString shortcut;
            const QString label = TContextMenuController::labelOf(*candidate, shortcut);
            connect(menu->addCustomEntry(label, shortcut), &TContextMenuEntryItem::triggered, this,
                    [this, target = QPointer<TChatChannel>(m_chatChannelStorage->find(select->chatChannelIdentifier()))] { setSecondary(target); });
        }
        menu->showCustomMenu();
        return;
    }
    showMenu(candidates, TabMenuOrder, ChatTabContext, button, modifiers, nullptr, channel);
}

void TChatController::showMenu(const input::TGameActionList& candidates, const std::span<const input::EGameAction> order, const int context,
                               const int button, const int modifiers, TChatChannelController* model, chat::TChatChannel* channel)
{
    const input::IGameActionPtr picked = m_mapper.map(candidates, context, static_cast<Qt::MouseButton>(button), modifiers);
    if (!picked)
        return;
    if (picked->gameActionType() != A::CONTEXT_MENU) {
        runMenuAction(picked, model, channel);
        return;
    }
    auto* menu = TContextMenuController::instance();
    const input::TGameActionList entries = input::contextMenuActions(candidates, order);
    if (!menu || entries.empty())
        return;
    menu->beginCustomMenu();
    for (const input::IGameActionPtr& action : entries) {
        if (action->gameActionType() == A::CONTEXT_MENU_SEPARATOR) {
            menu->addCustomSeparator();
            continue;
        }
        QString shortcut;
        const QString label = TContextMenuController::labelOf(*action, shortcut);
        connect(menu->addCustomEntry(label, shortcut), &TContextMenuEntryItem::triggered, this,
                [this, action, target = QPointer<TChatChannelController>(model), tab = QPointer<TChatChannel>(channel)] {
                    runMenuAction(action, target, tab);
                });
    }
    menu->showCustomMenu();
}

void TChatController::runMenuAction(const input::IGameActionPtr& action, TChatChannelController* model, chat::TChatChannel* channel)
{
    switch (action->gameActionType()) {
    case A::CHAT_CHANNEL_SELECT_ALL:
        if (model)
            model->selectAll();
        return;
    case A::COPY_CHAT_ENTRY_TO_CLIPBOARD:
    case A::COPY_CREATURE_NAME_TO_CLIPBOARD:
    case A::COPY_TEXT_TO_CLIPBOARD:
        if (const auto* copy = input::gameActionAs<input::TGameActionCopyTextToClipboard>(action))
            QGuiApplication::clipboard()->setText(copy->text());
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
        openInviteDialog(channel, action->gameActionType() == A::OPEN_EXCLUDE_FROM_CHAT_CHANNEL_DIALOG);
        return;
    default:
        // ADD_NEW_VIP goes to the VIP list's game action handler (AddBuddy).
        Q_EMIT publishGameAction(action);
        return;
    }
}

}

void tibia::gamewindow::TChatController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    using chat::TChatChannelStorage;
    using chat::TChatProtocolMessageHandler;
    // The previous session's channels are written to the options, its tabs,
    // input and dialogs go.
    setSecondary(nullptr);
    storeOpenChannels();
    m_openDialog.dialog()->close();
    m_inviteDialog.dialog()->close();
    m_excludeDialog.dialog()->close();
    closeMessageDialog();
    m_history.clear();
    m_historyIndex = 0;
    if (m_chatItem)
        m_chatItem->setProperty("currentChatInput", QString());
    for (const QMetaObject::Connection& connection : m_gameSessionConnections)
        disconnect(connection);
    m_gameSessionConnections.clear();
    m_tabs.setChannelStorage(nullptr);
    m_tabs.setMakeNewTabsCurrent(false);
    m_mode.onGameSessionDisconnected();
    m_channelListRequested = false;
    m_actionProvider.reset();
    m_chatProtocolMessageHandler.reset();
    m_chatChannelStorage.reset();
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (session && session->chatChannelStorage() && session->chatProtocolMessageHandler()) {
        m_chatChannelStorage = session->chatChannelStorage();
        m_tabs.setChannelStorage(m_chatChannelStorage.get());
        m_primary.setChannel(m_tabs.currentChannel());
        m_actionProvider = session->chatGameActionProvider();
        m_gameSessionConnections.push_back(connect(m_chatChannelStorage.get(), &TChatChannelStorage::channelRemoved, this,
                                                   &TChatController::onChatChannelRemoved));
        m_gameSessionConnections.push_back(
            connect(m_chatChannelStorage.get(), &TChatChannelStorage::channelAdded, this, &TChatController::onChannelAdded));
        m_chatProtocolMessageHandler = session->chatProtocolMessageHandler();
        m_gameSessionConnections.push_back(connect(m_chatProtocolMessageHandler.get(), &TChatProtocolMessageHandler::currentlyAvailableChannels,
                                                   this, &TChatController::onChannelListReceived));
        m_gameSessionConnections.push_back(
            connect(m_gameSession.get(), &game::IGameSession::gameLoginSuccessful, this, &TChatController::openChannelsFromConfig));
        m_makeNewTabsCurrentTimer.start();
        m_gameSessionConnections.push_back(
            connect(m_gameSession.get(), &game::IGameSession::worldTypeSet, this, &TChatController::exivaOptionsAvailableChanged));
    } else {
        m_channelsFromConfigOpened = false;
    }
    if (!m_gameSession) {
        m_primary.resetChannel();
        m_secondary.resetChannel();
    }
}
