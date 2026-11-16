#include "rsahelper.h"

#include <QRandomGenerator>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace tibia::shared {

namespace {

// Unsigned big integer, little-endian 32-bit limbs: the stand-in for the
// CryptoPP::Integer CipSoft links.
class BigInt
{
public:
    std::vector<uint32_t> d;

    static BigInt fromBytes(const uint8_t* data, int size)
    {
        BigInt out;
        for (int i = 0; i < size; ++i) {
            out = mulSmall(out, 256);
            out = add(out, fromU32(data[i]));
        }
        return out;
    }

    void toBytes(uint8_t* data, int size) const
    {
        std::fill(data, data + size, 0);
        BigInt rest = *this;
        for (int i = size - 1; i >= 0; --i) {
            data[i] = static_cast<uint8_t>(rest.d.empty() ? 0 : (rest.d[0] & 0xff));
            rest = divSmall(rest, 256);
        }
    }

    /// Bytes needed to store the value.
    int byteCount() const
    {
        return (bitSize() + 7) / 8;
    }

    /// Byte `index`, counted from the least significant.
    uint8_t byte(const int index) const
    {
        const size_t limb = static_cast<size_t>(index) / 4;
        if (limb >= d.size())
            return 0;
        return static_cast<uint8_t>(d[limb] >> (8 * (index % 4)));
    }

    bool isOdd() const
    {
        return !d.empty() && (d[0] & 1u);
    }

    static int compare(const BigInt& a, const BigInt& b)
    {
        return cmp(a, b);
    }

    static BigInt one()
    {
        return fromU32(1);
    }

    BigInt minusOne() const
    {
        if (d.empty())
            return {};
        return sub(*this, fromU32(1));
    }

    int bitSize() const
    {
        if (d.empty())
            return 0;
        int bits = static_cast<int>(d.size() - 1) * 32;
        uint32_t top = d.back();
        while (top) {
            ++bits;
            top >>= 1;
        }
        return bits;
    }

    static BigInt modPow(BigInt base, BigInt exp, const BigInt& mod)
    {
        BigInt result = fromU32(1);
        base = modReduce(base, mod);
        for (int bit = exp.bitSize() - 1; bit >= 0; --bit) {
            result = modReduce(mul(result, result), mod);
            if (exp.bit(bit))
                result = modReduce(mul(result, base), mod);
        }
        return result;
    }

private:
    static BigInt fromU32(uint32_t v)
    {
        BigInt out;
        if (v)
            out.d.push_back(v);
        return out;
    }

    bool bit(int index) const
    {
        const size_t limb = static_cast<size_t>(index) / 32;
        if (limb >= d.size())
            return false;
        return (d[limb] >> (index % 32)) & 1u;
    }

    static int cmp(const BigInt& a, const BigInt& b)
    {
        if (a.d.size() != b.d.size())
            return a.d.size() < b.d.size() ? -1 : 1;
        for (size_t i = a.d.size(); i-- > 0;) {
            if (a.d[i] != b.d[i])
                return a.d[i] < b.d[i] ? -1 : 1;
        }
        return 0;
    }

    static BigInt add(const BigInt& a, const BigInt& b)
    {
        BigInt out;
        const size_t n = std::max(a.d.size(), b.d.size());
        out.d.resize(n);
        uint64_t carry = 0;
        for (size_t i = 0; i < n; ++i) {
            carry += i < a.d.size() ? a.d[i] : 0;
            carry += i < b.d.size() ? b.d[i] : 0;
            out.d[i] = static_cast<uint32_t>(carry);
            carry >>= 32;
        }
        if (carry)
            out.d.push_back(static_cast<uint32_t>(carry));
        return out;
    }

    static BigInt sub(const BigInt& a, const BigInt& b)
    {
        BigInt out = a;
        uint32_t borrow = 0;
        for (size_t i = 0; i < b.d.size() || borrow; ++i) {
            uint64_t cur = out.d[i];
            const uint64_t take = (i < b.d.size() ? b.d[i] : 0) + borrow;
            if (cur < take) {
                cur += 1ull << 32;
                borrow = 1;
            } else {
                borrow = 0;
            }
            out.d[i] = static_cast<uint32_t>(cur - take);
        }
        while (!out.d.empty() && out.d.back() == 0)
            out.d.pop_back();
        return out;
    }

    static BigInt mulSmall(const BigInt& a, uint32_t m)
    {
        if (m == 0 || a.d.empty())
            return {};
        BigInt out;
        out.d.resize(a.d.size());
        uint64_t carry = 0;
        for (size_t i = 0; i < a.d.size(); ++i) {
            carry += static_cast<uint64_t>(a.d[i]) * m;
            out.d[i] = static_cast<uint32_t>(carry);
            carry >>= 32;
        }
        if (carry)
            out.d.push_back(static_cast<uint32_t>(carry));
        return out;
    }

    static BigInt divSmall(const BigInt& a, uint32_t m)
    {
        BigInt out;
        out.d.resize(a.d.size());
        uint64_t rem = 0;
        for (size_t i = a.d.size(); i-- > 0;) {
            rem = (rem << 32) | a.d[i];
            out.d[i] = static_cast<uint32_t>(rem / m);
            rem %= m;
        }
        while (!out.d.empty() && out.d.back() == 0)
            out.d.pop_back();
        return out;
    }

    static BigInt mul(const BigInt& a, const BigInt& b)
    {
        if (a.d.empty() || b.d.empty())
            return {};
        BigInt out;
        out.d.assign(a.d.size() + b.d.size(), 0);
        for (size_t i = 0; i < a.d.size(); ++i) {
            uint64_t carry = 0;
            for (size_t j = 0; j < b.d.size() || carry; ++j) {
                const uint64_t cur = out.d[i + j] + carry
                    + static_cast<uint64_t>(a.d[i]) * (j < b.d.size() ? b.d[j] : 0);
                out.d[i + j] = static_cast<uint32_t>(cur);
                carry = cur >> 32;
            }
        }
        while (!out.d.empty() && out.d.back() == 0)
            out.d.pop_back();
        return out;
    }

    static BigInt modReduce(BigInt value, const BigInt& mod)
    {
        if (cmp(value, mod) < 0)
            return value;
        const int shift = value.bitSize() - mod.bitSize();
        for (int bit = shift; bit >= 0; --bit) {
            BigInt shifted = mod << bit;
            if (cmp(value, shifted) >= 0)
                value = sub(value, shifted);
        }
        return value;
    }

    BigInt operator<<(int bits) const
    {
        if (bits == 0 || d.empty())
            return *this;
        BigInt out;
        const int limbShift = bits / 32;
        const int bitShift = bits % 32;
        out.d.assign(d.size() + static_cast<size_t>(limbShift) + 1, 0);
        for (size_t i = 0; i < d.size(); ++i) {
            const uint64_t cur = static_cast<uint64_t>(d[i]) << bitShift;
            out.d[i + limbShift] |= static_cast<uint32_t>(cur);
            if (bitShift)
                out.d[i + limbShift + 1] |= static_cast<uint32_t>(cur >> 32);
        }
        while (!out.d.empty() && out.d.back() == 0)
            out.d.pop_back();
        return out;
    }
};

}

struct TRsaHelper::Key
{
    BigInt modulus;
    BigInt exponent;
};

TRsaHelper::TRsaHelper()
    : m_key(std::make_unique<Key>())
{
}

TRsaHelper::~TRsaHelper() = default;

// CryptoPP's RSAFunction::Validate for a public key: n > 1 and odd, e > 1, odd and below n.
void TRsaHelper::setPublicKey(const QByteArray& modulus)
{
    m_key->modulus = BigInt::fromBytes(reinterpret_cast<const uint8_t*>(modulus.constData()), int(modulus.size()));
    m_key->exponent = BigInt::fromBytes(reinterpret_cast<const uint8_t*>("\x01\x00\x01"), 3);
    const BigInt& n = m_key->modulus;
    const BigInt& e = m_key->exponent;
    const bool valid = BigInt::compare(n, BigInt::one()) > 0 && n.isOdd() && BigInt::compare(e, BigInt::one()) > 0
        && e.isOdd() && BigInt::compare(e, n) < 0;
    if (!valid)
        throw std::runtime_error("Rsa public key validation failed");
}

// Pads `data` with random bytes to the key's block size, then returns
// data^e mod n as a big-endian block of that size.
QByteArray TRsaHelper::encrypt(const QByteArray& data)
{
    const int blockSize = m_key->modulus.minusOne().byteCount();
    QByteArray padded = data;
    while (padded.size() < blockSize)
        padded.append(char(QRandomGenerator::system()->generate() & 0xff));
    const BigInt message =
        BigInt::fromBytes(reinterpret_cast<const uint8_t*>(padded.constData()), int(padded.size()));
    const BigInt cipher = BigInt::modPow(message, m_key->exponent, m_key->modulus);
    QByteArray result;
    result.resize(blockSize);
    for (int i = 0; i < blockSize; ++i)
        result[blockSize - 1 - i] = char(cipher.byte(i));
    return result;
}

}
