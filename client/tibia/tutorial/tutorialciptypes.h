#pragma once

// CIP-named tutorial surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::tutorial {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TSessiondumpHintBase final : public ::tibia::CipTyped {};

class TSessiondumpHintClearOldestMessage final : public ::tibia::CipTyped {};

class TSessiondumpHintCondition final : public ::tibia::CipTyped {};

class TSessiondumpHintConditionShowTutorialHint final : public ::tibia::CipTyped {};

class TSessiondumpHintPublishGameAction final : public ::tibia::CipTyped {};

class TSessiondumpHintSkipTo final : public ::tibia::CipTyped {};

class TSessiondumpHintSwitchChannel final : public ::tibia::CipTyped {};

class TSessiondumpHintTutorialProgress final : public ::tibia::CipTyped {};

class TSessiondumpHints final : public ::tibia::CipTyped {};

class TTutorialProgressUploader final : public cip::tibia::tutorial::TTutorialProgressUploader
{
    Q_OBJECT
public:
    using cip::tibia::tutorial::TTutorialProgressUploader::TTutorialProgressUploader;
};

class TTutorialProtocolMessageHandler final : public cip::tibia::tutorial::TTutorialProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::tutorial::TTutorialProtocolMessageHandler::TTutorialProtocolMessageHandler;
};

class TTutorialSessiondumpGameActionHandler final : public cip::tibia::tutorial::TTutorialSessiondumpGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::tutorial::TTutorialSessiondumpGameActionHandler::TTutorialSessiondumpGameActionHandler;
};

} // namespace tibia::tutorial
