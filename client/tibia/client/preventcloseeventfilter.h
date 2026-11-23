#pragma once

#include <cip_gen.h>

class QEvent;

namespace tibia {
namespace client {

/// CIP client::TPreventCloseEventFilter: swallows the watched object's close
/// events and reports each one with preventedClose().
class TPreventCloseEventFilter final : public cip::tibia::client::TPreventCloseEventFilter
{
    Q_OBJECT
public:
    explicit TPreventCloseEventFilter(QObject* parent = nullptr);
    ~TPreventCloseEventFilter() override;

    bool eventFilter(QObject* watched, QEvent* event) override;
    /// Off lets the close events through (the client is ready to close).
    void setPreventClose(bool PreventClose) { m_preventClose = PreventClose; }

private:
    bool m_preventClose = true;
};

}
}
