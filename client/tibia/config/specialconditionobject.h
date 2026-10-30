#pragma once

#include "config/specialconditionsoptions.h"

#include <cip_gen.h>

namespace tibia::config {

/// One row of the HUD page's special conditions table (CIP: built by
/// THudQMLOptionsPage FUN_100a1f560).
class TSpecialConditionObject final : public cip::tibia::config::TSpecialConditionObject
{
    Q_OBJECT
public:
    TSpecialConditionObject(const SpecialCondition& condition, int listIndex, bool showInHUD, bool showInBar, QObject* parent = nullptr);

    /// CIP visitor table 0x10264e818 (FUN_101025ad0, FUN_101025d60,
    /// FUN_101025dc0, FUN_101025df0, FUN_101025cc0).
    static QString name(const SpecialCondition& condition);
    /// CIP visitor table 0x10264e868 (FUN_101025ea0, FUN_101026130,
    /// FUN_1010262a0, FUN_1010262d0, FUN_101026090).
    static QString tooltip(const SpecialCondition& condition);
    /// CIP FUN_101026610 / FUN_101026650 / FUN_100925ff0.
    static QString icon(const SpecialCondition& condition);
};

}
