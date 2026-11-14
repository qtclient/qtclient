#pragma once

// Shared classic u8/u16/u32/string reader/writer for structured TR fill/serialize.

#include <QByteArray>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

namespace tibia {
namespace protocol {

struct ClassicReader {
    const uint8_t* p = nullptr;
    size_t n = 0;
    size_t i = 0;

    size_t remaining() const { return i < n ? n - i : 0; }

    bool u8(uint8_t& out)
    {
        if (remaining() < 1)
            return false;
        out = p[i++];
        return true;
    }
    bool u16(uint16_t& out)
    {
        if (remaining() < 2)
            return false;
        out = uint16_t(p[i]) | (uint16_t(p[i + 1]) << 8);
        i += 2;
        return true;
    }
    bool u32(uint32_t& out)
    {
        if (remaining() < 4)
            return false;
        out = uint32_t(p[i]) | (uint32_t(p[i + 1]) << 8) | (uint32_t(p[i + 2]) << 16)
            | (uint32_t(p[i + 3]) << 24);
        i += 4;
        return true;
    }
    bool u64(uint64_t& out)
    {
        uint32_t low = 0, high = 0;
        if (remaining() < 8 || !u32(low) || !u32(high))
            return false;
        out = uint64_t(low) | (uint64_t(high) << 32);
        return true;
    }
    bool s16(int16_t& out)
    {
        uint16_t raw = 0;
        if (!u16(raw))
            return false;
        out = int16_t(raw);
        return true;
    }
    /// A u8 count of decimals, then the value plus 0x7fffffff as a u32.
    bool real(double& out)
    {
        uint8_t decimals = 0;
        uint32_t scaled = 0;
        if (remaining() < 5 || !u8(decimals) || !u32(scaled))
            return false;
        double divisor = 1.0;
        for (uint8_t k = 0; k < decimals; ++k)
            divisor *= 10.0;
        out = double(int64_t(scaled) - int64_t(0x7fffffff)) / divisor;
        return true;
    }
    bool string(std::string& out)
    {
        uint16_t len = 0;
        if (!u16(len) || remaining() < len)
            return false;
        out.assign(reinterpret_cast<const char*>(p + i), len);
        i += len;
        return true;
    }
    bool otDouble(double& out)
    {
        uint8_t precision = 0;
        if (!u8(precision) || remaining() < 4)
            return false;
        uint32_t scaled = 0;
        if (!u32(scaled))
            return false;
        const auto raw = int32_t(scaled - uint32_t(0x7fffffff));
        double factor = 1.0;
        for (uint8_t k = 0; k < precision; ++k)
            factor *= 10.0;
        out = double(raw) / factor;
        return true;
    }
};

struct ClassicWriter {
    QByteArray out;
    void u8(uint8_t v) { out.append(char(v)); }
    void u16(uint16_t v)
    {
        out.append(char(uint8_t(v)));
        out.append(char(uint8_t(v >> 8)));
    }
    void u32(uint32_t v)
    {
        out.append(char(uint8_t(v)));
        out.append(char(uint8_t(v >> 8)));
        out.append(char(uint8_t(v >> 16)));
        out.append(char(uint8_t(v >> 24)));
    }
    void string(const std::string& s)
    {
        const auto len = uint16_t(std::min(s.size(), size_t(0xffff)));
        u16(len);
        out.append(s.data(), int(len));
    }
    void otDouble(double value, uint8_t precision = 3)
    {
        u8(precision);
        double factor = 1.0;
        for (uint8_t k = 0; k < precision; ++k)
            factor *= 10.0;
        const auto scaled = int32_t(value * factor);
        u32(uint32_t(scaled + 0x7fffffff));
    }
};

}
}
