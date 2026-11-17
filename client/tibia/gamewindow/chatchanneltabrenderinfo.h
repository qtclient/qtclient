#pragma once

#include <cip_gen.h>

#include <QTimer>

namespace tibia {
namespace chat {
class TChatChannel;
}
namespace gamewindow {

/// A chat tab's highlight (CIP: TChatChannelTabRenderInfo, one per tab of
/// TChatChannelTabController). A new entry makes it flash for a second, then
/// it stays marked; the current tab is never flashed or marked.
class TChatChannelTabRenderInfo final : public cip::tibia::gamewindow::TChatChannelTabRenderInfo
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

    explicit TChatChannelTabRenderInfo(chat::TChatChannel* channel, QObject* parent = nullptr);

    /// The tab's channel.
    chat::TChatChannel* channel() const { return m_channel; }
    EHighlight tabHighlight() const { return m_tabHighlight; }
    /// A current tab only leaves CURRENT for NONE, other tabs never go to
    /// NONE here; FLASH_NEW_ENTRY (re)starts the flash.
    void setTabHighlight(EHighlight highlight);
    /// Back to NONE unless current, without stopping a running flash.
    void resetTabHighlight();

public Q_SLOTS:
    /// The flash is over: the tab stays marked.
    void onFlashTimerTimeout();

Q_SIGNALS:
    void tabHighlightChanged(tibia::chat::TChatChannel* channel);

private:
    chat::TChatChannel* m_channel;
    EHighlight m_tabHighlight = NONE;
    QTimer m_flashTimer;
};

}
}
