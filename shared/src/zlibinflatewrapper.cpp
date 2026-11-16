#include "zlibinflatewrapper.h"

namespace tibia::shared {

namespace {
constexpr size_t InitialOutputSize = 0xffff;
}

TZlibInflateWrapper::~TZlibInflateWrapper()
{
    end();
}

int TZlibInflateWrapper::init(const unsigned char windowBits, const bool oneShot)
{
    m_oneShot = oneShot;
    if (m_stream) {
        setLastError(Z_STREAM_ERROR, "TZlibInflateWrapper::init: already initialized");
        return Z_OK;
    }
    m_stream = std::make_shared<z_stream>();
    const int result = inflateInit2(m_stream.get(), -int(windowBits));
    if (result != Z_OK)
        setLastError(result, "Error during inflateInit2");
    return result;
}

// CipSoft does not end a one-shot stream.
int TZlibInflateWrapper::end()
{
    if (m_stream) {
        if (!m_oneShot)
            inflateEnd(m_stream.get());
        m_stream.reset();
    }
    return Z_OK;
}

bool TZlibInflateWrapper::isInitialized() const
{
    return m_stream != nullptr;
}

bool TZlibInflateWrapper::isOneShot() const
{
    return m_oneShot;
}

// Runs inflate, doubling `output` while zlib runs out of room (or, for a
// streaming inflater, while input remains).
void TZlibInflateWrapper::inflateGrowing(const int flush, int& result, std::vector<unsigned char>& output)
{
    while (true) {
        result = ::inflate(m_stream.get(), flush);
        if (result != Z_BUF_ERROR && (result != Z_OK || m_oneShot || m_stream->avail_in == 0))
            return;
        const size_t used = output.size();
        output.resize(size_t(unsigned(used * 2)));
        m_stream->next_out = output.data() + unsigned(used);
        m_stream->avail_out = unsigned(used);
    }
}

int TZlibInflateWrapper::inflate(const unsigned char* input, const unsigned inputSize,
                                 std::vector<unsigned char>& output)
{
    const int flush = m_oneShot ? Z_FINISH : Z_SYNC_FLUSH;
    output.resize(InitialOutputSize);
    m_stream->next_in = const_cast<unsigned char*>(input);
    m_stream->avail_in = inputSize;
    m_stream->next_out = output.data();
    m_stream->avail_out = unsigned(output.size());
    m_stream->total_out = 0;
    int result = Z_OK;
    inflateGrowing(flush, result, output);
    if (unsigned(result) > Z_STREAM_END) {
        setLastError(result, "Error during inflating");
        return result;
    }
    bool reset = false;
    if (!m_oneShot) {
        const auto& marker = syncFlushMarker();
        m_stream->next_in = const_cast<unsigned char*>(marker.data());
        m_stream->avail_in = unsigned(marker.size());
        inflateGrowing(flush, result, output);
        if (result != Z_OK) {
            setLastError(result, "Error during inflating");
            return result;
        }
    } else {
        reset = result == Z_STREAM_END;
        if (!reset && result != Z_OK) {
            setLastError(result, "Error during inflating");
            return result;
        }
    }
    output.resize(m_stream->total_out);
    if (!reset)
        return Z_OK;
    result = inflateReset(m_stream.get());
    if (result != Z_OK) {
        setLastError(result, "Error during inflateReset");
        return result;
    }
    return Z_OK;
}

}
