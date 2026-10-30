#pragma once

#include "gamewindow/gamewindowciptypes.h"

#include <QObject>
#include <QPointer>

class QQuickItem;

namespace tibia::gamewindow {

/// CIP's TTargetSelectionController (slots 0x100bdb500 start / 0x100bdb1f0
/// finish) owning TTargetSelectionEventFilter (0x100bdb630).
class TTargetSelectionController final : public QObject
{
    Q_OBJECT
public:
    explicit TTargetSelectionController(QQuickItem* root, QObject* parent = nullptr);
    ~TTargetSelectionController() override;

    TTargetSelectionEventFilter* eventFilterObject() const { return m_filter; }

private:
    TTargetSelectionEventFilter* m_filter = nullptr;
};

}
