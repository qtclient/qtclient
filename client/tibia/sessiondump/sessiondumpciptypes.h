#pragma once

// CIP-named sessiondump surfaces owned by the game client / tutorial path.

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::sessiondump {

class TClientSessiondumpRecorder final : public cip::tibia::sessiondump::TClientSessiondumpRecorder
{
    Q_OBJECT
public:
    using cip::tibia::sessiondump::TClientSessiondumpRecorder::TClientSessiondumpRecorder;
};

class TLightweightSessiondumpMessageProcessor final : public ::tibia::CipTyped {};

class TSessiondumpAsyncLoader final : public cip::tibia::sessiondump::TSessiondumpAsyncLoader
{
    Q_OBJECT
public:
    using cip::tibia::sessiondump::TSessiondumpAsyncLoader::TSessiondumpAsyncLoader;
};

class TSessiondumpCatalogLoader final : public cip::tibia::sessiondump::TSessiondumpCatalogLoader
{
    Q_OBJECT
public:
    using cip::tibia::sessiondump::TSessiondumpCatalogLoader::TSessiondumpCatalogLoader;
};

class TSessiondumpFactory final : public ::tibia::CipTyped {};

class TSessiondumpMessageProcessor final : public ::tibia::CipTyped {};

class TSessiondumpMessageReader final : public ::tibia::CipTyped {};

class TSessiondumpMetadataReader final : public ::tibia::CipTyped {};

class TSessiondumpPacketProcessor final : public ::tibia::CipTyped {};

class TSessiondumpPacketReader final : public ::tibia::CipTyped {};

class TSessiondumpPlayer final : public cip::tibia::sessiondump::TSessiondumpPlayer
{
    Q_OBJECT
public:
    using cip::tibia::sessiondump::TSessiondumpPlayer::TSessiondumpPlayer;
};

class TSessiondumpProcessor final : public ::tibia::CipTyped {};

class TSessiondumpProgressClockService final : public ::tibia::CipTyped {};

class TSessiondumpReader final : public ::tibia::CipTyped {};

class TSessiondumpReadersFactory final : public ::tibia::CipTyped {};

} // namespace tibia::sessiondump

