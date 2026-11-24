#pragma once

#include "game/blessingdialogdata.h"

#include <cip_gen.h>

namespace tibia {
namespace game {

/// One line of the blessings dialog's history.
class TQmlBlessingHistoryEntry final : public cip::tibia::game::TQmlBlessingHistoryEntry
{
    Q_OBJECT
public:
    explicit TQmlBlessingHistoryEntry(TBlessingDialogData::THistoryEntry&& Entry);

    /// "yyyy-MM-dd, HH:mm:ss" (TextHelper.formatDateTime with seconds).
    QString timestamp() const override;
    /// Every colour but 0 and 5 (the lost blessing lines).
    bool blessingWasGained() const override;
    QString description() const override;

private:
    TBlessingDialogData::THistoryEntry m_entry;
};

}
}
