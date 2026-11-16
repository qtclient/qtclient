#pragma once

// CIP-named sessiondump surfaces owned by the game client / tutorial path.

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

#include "sessiondump/sessiondumpprogressclockservice.h"

namespace tibia {
namespace sessiondump {

class TClientSessiondumpRecorder final : public cip::tibia::sessiondump::TClientSessiondumpRecorder
{
    Q_OBJECT
public:
    using cip::tibia::sessiondump::TClientSessiondumpRecorder::TClientSessiondumpRecorder;
};

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

class TSessiondumpPlayer final : public cip::tibia::sessiondump::TSessiondumpPlayer
{
    Q_OBJECT
public:
    using cip::tibia::sessiondump::TSessiondumpPlayer::TSessiondumpPlayer;
};

}
}

