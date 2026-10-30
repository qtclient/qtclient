#pragma once

// CIP-named reporting surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::reporting {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TBugReportProtocolMessageHandler final : public cip::tibia::reporting::TBugReportProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::reporting::TBugReportProtocolMessageHandler::TBugReportProtocolMessageHandler;
};

class TRuleViolationProtocolMessageHandler final : public cip::tibia::reporting::TRuleViolationProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::reporting::TRuleViolationProtocolMessageHandler::TRuleViolationProtocolMessageHandler;
};

} // namespace tibia::reporting

