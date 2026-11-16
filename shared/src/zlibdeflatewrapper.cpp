#include "zlibdeflatewrapper.h"

namespace tibia::shared {

namespace {

std::shared_ptr<z_stream>& sharedOneShotStream()
{
    static std::shared_ptr<z_stream> stream;
    return stream;
}

}

TZlibDeflateWrapper::~TZlibDeflateWrapper()
{
    end();
}

int TZlibDeflateWrapper::init(const unsigned char windowBits, const unsigned char memLevel,
                              const unsigned char level, const bool oneShot)
{
    m_oneShot = oneShot;
    if (m_stream) {
        setLastError(Z_STREAM_ERROR, "TZlibDeflateWrapper::init: already initialized");
        return Z_OK;
    }
    int result = Z_OK;
    if (!oneShot) {
        m_stream = std::make_shared<z_stream>();
        result = deflateInit2(m_stream.get(), level, Z_DEFLATED, -int(windowBits), memLevel, Z_DEFAULT_STRATEGY);
    } else {
        std::shared_ptr<z_stream>& shared = sharedOneShotStream();
        if (!shared) {
            shared = std::make_shared<z_stream>();
            result = deflateInit2(shared.get(), level, Z_DEFLATED, -15, 9, Z_DEFAULT_STRATEGY);
            if (result != Z_OK) {
                setLastError(result, "Error during deflateInit2");
                shared.reset();
            }
        }
        if (result == Z_OK)
            m_stream = shared;
    }
    if (result != Z_OK) {
        setLastError(result, "Error during deflateInit2");
        m_stream.reset();
        m_windowBits = 0;
        m_memLevel = 0;
        m_level = 0;
        m_oneShot = false;
        return result;
    }
    m_windowBits = windowBits;
    m_memLevel = memLevel;
    m_level = level;
    m_oneShot = oneShot;
    return Z_OK;
}

// The shared one-shot stream is never ended.
int TZlibDeflateWrapper::end()
{
    if (m_stream) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_oneShot)
            deflateEnd(m_stream.get());
        m_stream.reset();
    }
    return Z_OK;
}

bool TZlibDeflateWrapper::isInitialized() const
{
    return m_stream != nullptr;
}

unsigned char TZlibDeflateWrapper::getWindowBits() const
{
    return m_windowBits;
}

unsigned char TZlibDeflateWrapper::getMemLevel() const
{
    return m_memLevel;
}

unsigned char TZlibDeflateWrapper::getLevel() const
{
    return m_level;
}

bool TZlibDeflateWrapper::isOneShot() const
{
    return m_oneShot;
}

int TZlibDeflateWrapper::deflate(const unsigned char* input, const unsigned inputSize,
                                 std::vector<unsigned char>& output)
{
    output.resize(compressBound(inputSize));
    unsigned outputSize = unsigned(output.size());
    const int result = deflateInto(input, inputSize, output.data(), outputSize);
    if (result != Z_OK) {
        output.clear();
        return result;
    }
    output.resize(outputSize);
    return result;
}

int TZlibDeflateWrapper::deflateInto(const unsigned char* input, const unsigned inputSize, unsigned char* output,
                                     unsigned& outputSize)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    z_stream* stream = m_stream.get();
    if (!stream) {
        setLastError(Z_STREAM_ERROR, "Stream not initialized or already deleted");
        return Z_STREAM_ERROR;
    }
    const bool oneShot = m_oneShot;
    stream->next_in = const_cast<unsigned char*>(input);
    stream->avail_in = inputSize;
    stream->next_out = output;
    stream->avail_out = outputSize;
    stream->total_out = 0;
    const int result = ::deflate(stream, oneShot ? Z_FINISH : Z_SYNC_FLUSH);
    if (unsigned(result) > Z_STREAM_END) {
        setLastError(result, "Error during deflating");
        return result;
    }
    if (!(result == Z_OK || (result == Z_STREAM_END && m_oneShot))) {
        setLastError(result, "Error during deflating");
        return Z_STREAM_END;
    }
    const unsigned trailer = oneShot ? 0 : unsigned(syncFlushMarker().size());
    outputSize = unsigned(stream->total_out) - trailer;
    if (result == Z_STREAM_END && m_oneShot) {
        const int reset = deflateReset(stream);
        if (reset != Z_OK) {
            setLastError(reset, "Error during deflateReset");
            return reset;
        }
    }
    return Z_OK;
}

}
