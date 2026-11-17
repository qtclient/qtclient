#pragma once

#include <cip_gen.h>

#include <QDateTime>

namespace tibia {
namespace gamewindow {

class TChatChannelController;

/// A filtered view of one chat channel (CIP: TChatChannelFilterController):
/// entries older than the minimum time are hidden; everything else is the
/// channel controller's, to which every property and call is forwarded.
class TChatChannelFilterController final : public cip::tibia::gamewindow::TChatChannelFilterController
{
    Q_OBJECT
public:
    explicit TChatChannelFilterController(TChatChannelController* source, QObject* parent = nullptr);

    TChatChannelController* source() const;

    bool canSubscribeToServerLog() const override;
    bool isSubscribedToServerLog() const override;
    bool writesToLocalChat() const override;
    bool canBeClosed() const override;
    QString channelName() const override;

    void startNewSelection(int EntryIndex, int CursorPosition) override;
    void updateSelection(int EntryIndex, int CursorPosition) override;
    void selectAll() override;
    void selectTextPassage(int EntryIndex, int PassageStart, int PassageEnd) override;
    void setChatWidth(int NewWidth) override;
    void setChatFont(const QFont& NewFont) override;
    void setAutoScrollToEnd(bool AutoScroll) override;
    void setSubscribeToServerLog(bool Subscribe) override;
    void closeChannel() override;
    void onChannelClicked(int EntryId, int MouseButtonRaw, int KeyboardModifiersRaw) override;

protected:
    /// With a minimum time, only entries from then on.
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QDateTime m_minimumTime;
};

}
}
