#pragma once

#include "game/blessingdialogdata.h"

#include <cip_gen.h>

namespace tibia {
namespace game {

/// One row of the blessings dialog: a blessing with its charges.
class TQmlBlessingWithCharges final : public cip::tibia::game::TQmlBlessingWithCharges
{
    Q_OBJECT
public:
    explicit TQmlBlessingWithCharges(const TBlessingDialogData::TBlessing& Blessing);

    int blessingId() const override;
    /// The blessing's translated name ("dummy_unknown" for an unknown bit).
    QString blessingName() const override;
    /// The blessing's cell of the "blessings-icons" image strip.
    QString blessingImage() const override;
    int blessingCharges() const override;
    int blessingChargesFromStore() const override;

private:
    TBlessingDialogData::TBlessing m_blessing;
};

}
}
