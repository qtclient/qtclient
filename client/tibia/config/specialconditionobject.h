#pragma once

#include "config/specialconditionsoptions.h"

#include <cip_gen.h>

namespace tibia {
namespace config {

/// One row of the HUD page's special conditions table (CIP: built by
/// THudQMLOptionsPage).
class TSpecialConditionObject final : public cip::tibia::config::TSpecialConditionObject
{
    Q_OBJECT
public:
    TSpecialConditionObject(const SpecialCondition& condition, int listIndex, bool showInHUD, bool showInBar, QObject* parent = nullptr);

    /// CIP visitor table (, ,
    /// , ,).
    static QString name(const SpecialCondition& condition);
    /// CIP visitor table (, ,
    /// , ,).
    static QString tooltip(const SpecialCondition& condition);
    /// CIP /.
    static QString icon(const SpecialCondition& condition);
};

}
}
