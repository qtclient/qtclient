#pragma once

// CIP-named news surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia {
namespace news {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TEventsStorage final : public cip::tibia::news::TEventsStorage
{
    Q_OBJECT
public:
    using cip::tibia::news::TEventsStorage::TEventsStorage;
};

class TNewsDiskIO final : public cip::tibia::news::TNewsDiskIO
{
    Q_OBJECT
public:
    using cip::tibia::news::TNewsDiskIO::TNewsDiskIO;
};

class TNewsDiskIOAsync final : public ::tibia::CipTyped {};

class TNewsDiskIOSynchronous final : public ::tibia::CipTyped {};

class TNewsProtocolMessageHandler final : public cip::tibia::news::TNewsProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::news::TNewsProtocolMessageHandler::TNewsProtocolMessageHandler;
public:
    /// Tip Q→H wire: ShowGameNews from `serverMessageEnqueued`.
    static void onServerMessageEnqueued(int opcode, const QByteArray& classicFrame, bool applyClassic);
};

class TNewsViewsUploader final : public cip::tibia::news::TNewsViewsUploader
{
    Q_OBJECT
public:
    using cip::tibia::news::TNewsViewsUploader::TNewsViewsUploader;
};

}
}

