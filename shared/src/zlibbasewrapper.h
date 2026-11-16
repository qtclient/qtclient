#pragma once

#include <zlib.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace tibia::shared {

/// CIP `shared::TZlibBaseWrapper`: owns the z_stream of a raw (headerless)
/// deflate or inflate stream and the last error message.
class TZlibBaseWrapper
{
public:
    TZlibBaseWrapper() = default;
    virtual ~TZlibBaseWrapper() = default;

    std::string getLastErrorMessage() const;

protected:
    /// Stores "`message` (`code`)" as the last error.
    void setLastError(int code, const std::string& message);

    /// The SYNC_FLUSH trailer a streaming deflate ends every message with;
    /// the deflater drops it and the inflater feeds it back.
    static const std::vector<unsigned char>& syncFlushMarker();

    std::shared_ptr<z_stream> m_stream;
    std::mutex m_mutex;
    std::string m_lastErrorMessage;
};

}
