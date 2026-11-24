#include "chatchannelfiltercontroller.h"

#include "gamewindow/chatcontroller.h"

namespace tibia::gamewindow {

TChatChannelFilterController::TChatChannelFilterController(TChatChannelController* source, QObject* parent)
    : cip::tibia::gamewindow::TChatChannelFilterController(parent)
{
    setSourceModel(source);
    connect(source, &TChatChannelController::scrollViewToEnd, this, &TChatChannelFilterController::scrollViewToEnd);
    connect(source, &TChatChannelController::chatChannelPropertyChanged, this,
            &TChatChannelFilterController::chatChannelPropertyChanged);
    connect(source, &TChatChannelController::chatChannelChanged, this, &TChatChannelFilterController::chatChannelChanged);
    connect(source, &TChatChannelController::closeRequested, this, &TChatChannelFilterController::closeRequested);
    connect(source, &TChatChannelController::subscribeToServerLogRequested, this,
            &TChatChannelFilterController::subscribeToServerLogRequested);
    connect(source, &TChatChannelController::publishGameAction, this, &TChatChannelFilterController::publishGameAction);
}

TChatChannelController* TChatChannelFilterController::source() const { return static_cast<TChatChannelController*>(sourceModel()); }

QDateTime TChatChannelFilterController::minimumTime() const { return m_minimumTime; }

void TChatChannelFilterController::setMinimumTime(const QDateTime& time)
{
    if (m_minimumTime == time)
        return;
    beginFilterChange();
    m_minimumTime = time;
    endFilterChange(QSortFilterProxyModel::Direction::Both);
}

void TChatChannelFilterController::clearMinimumTime()
{
    const QDateTime none;
    if (m_minimumTime == none)
        return;
    beginFilterChange();
    m_minimumTime = none;
    endFilterChange(QSortFilterProxyModel::Direction::Both);
}

bool TChatChannelFilterController::canSubscribeToServerLog() const { return source()->canSubscribeToServerLog(); }
bool TChatChannelFilterController::isSubscribedToServerLog() const { return source()->isSubscribedToServerLog(); }
bool TChatChannelFilterController::writesToLocalChat() const { return source()->writesToLocalChat(); }
bool TChatChannelFilterController::canBeClosed() const { return source()->canBeClosed(); }
QString TChatChannelFilterController::channelName() const { return source()->channelName(); }

void TChatChannelFilterController::startNewSelection(const int EntryIndex, const int CursorPosition)
{
    source()->startNewSelection(EntryIndex, CursorPosition);
}

void TChatChannelFilterController::updateSelection(const int EntryIndex, const int CursorPosition)
{
    source()->updateSelection(EntryIndex, CursorPosition);
}

void TChatChannelFilterController::selectAll() { source()->selectAllSince(m_minimumTime); }

void TChatChannelFilterController::selectTextPassage(const int EntryIndex, const int PassageStart, const int PassageEnd)
{
    source()->selectTextPassage(EntryIndex, PassageStart, PassageEnd);
}

void TChatChannelFilterController::setChatWidth(const int NewWidth) { source()->setChatWidth(NewWidth); }
void TChatChannelFilterController::setChatFont(const QFont& NewFont) { source()->setChatFont(NewFont); }
void TChatChannelFilterController::setAutoScrollToEnd(const bool AutoScroll) { source()->setAutoScrollToEnd(AutoScroll); }
void TChatChannelFilterController::setSubscribeToServerLog(const bool Subscribe) { source()->setSubscribeToServerLog(Subscribe); }
void TChatChannelFilterController::closeChannel() { source()->closeChannel(); }

void TChatChannelFilterController::onChannelClicked(const int EntryId, const int MouseButtonRaw, const int KeyboardModifiersRaw)
{
    source()->onChannelClicked(EntryId, MouseButtonRaw, KeyboardModifiersRaw);
}

bool TChatChannelFilterController::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    if (!m_minimumTime.isValid())
        return true;
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    return index.data(TChatChannelController::DateTimeRole).toDateTime() >= m_minimumTime;
}

}
