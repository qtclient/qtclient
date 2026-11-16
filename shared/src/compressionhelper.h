#pragma once

#include "zlibinflatewrapper.h"

#include <QByteArray>

namespace tibia::shared {

/// CIP `shared::TCompressionHelper`: raw-inflates messages (window 2^15).
class TCompressionHelper
{
public:
    TCompressionHelper() = default;
    virtual ~TCompressionHelper() = default;

    /// The inflated bytes. On an inflate error CipSoft returns the whole
    /// working buffer (at least 64 KiB) unshrunk.
    virtual QByteArray decompress(const QByteArray& data);
    virtual void init(bool oneShot);
    virtual bool isInitialized() const;

private:
    TZlibInflateWrapper m_inflater;
};

/// Unpacks bundle-packed data; data that is not packed comes back unchanged.
/// Throws TDecompressionError when packed data cannot be unpacked.
QByteArray decompressBundle(const QByteArray& data);

}
