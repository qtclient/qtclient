#pragma once

// CIP-named client surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::client {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TAntiCheatController final : public cip::tibia::client::TAntiCheatController
{
    Q_OBJECT
public:
    using cip::tibia::client::TAntiCheatController::TAntiCheatController;
};

class TGameApplication final : public ::tibia::CipTyped {};

class TGameApplicationBase final : public ::tibia::CipTyped {};

class TGameClientBase final : public cip::tibia::client::TGameClientBase
{
    Q_OBJECT
public:
    using cip::tibia::client::TGameClientBase::TGameClientBase;
};

class TPreventCloseEventFilter final : public cip::tibia::client::TPreventCloseEventFilter
{
    Q_OBJECT
public:
    using cip::tibia::client::TPreventCloseEventFilter::TPreventCloseEventFilter;
};

} // namespace tibia::client

