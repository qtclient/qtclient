// CIP shared::TRsaHelper: the login message's RSA block (raw modular
// exponentiation with the public key, no padding).

#include "shared/sharedciptypes.h"

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

namespace tibia::shared {

namespace {

// Little-endian limbs. Login RSA is raw modular exponentiation (no padding).
class BigInt
{
public:
    std::vector<uint32_t> d;

    static BigInt fromDec(std::string_view text)
    {
        BigInt out;
        for (const char ch : text) {
            if (ch < '0' || ch > '9')
                continue;
            out = add(mulSmall(out, 10), BigInt::fromU32(static_cast<uint32_t>(ch - '0')));
        }
        return out;
    }

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

// Crystal's login RSA modulus; the exponent is 65537.
constexpr const char* kLoginModulus =
    "1091201329673994292788609605089955415282375029027981291234687579"
    "3726629149257644633073969600111060390723088861007265581882535850"
    "3429057592827629436413108566029093628212635953836686562675849720"
    "6207862794310902180176810615217550567108238764764442605581471797"
    "07119674283982419152118103759076030616683978566631413";

BigInt& modulus()
{
    static BigInt value;
    return value;
}

BigInt& exponent()
{
    static BigInt value;
    return value;
}

}

void TRsaHelper::installLoginKey()
{
    setPublicKey(kLoginModulus, "65537");
}

void TRsaHelper::setPublicKey(const std::string_view modulusDecimal, const std::string_view exponentDecimal)
{
    modulus() = BigInt::fromDec(modulusDecimal);
    exponent() = BigInt::fromDec(exponentDecimal);
}

int TRsaHelper::blockSize()
{
    const int bytes = (modulus().bitSize() + 7) / 8;
    return (bytes / 128) * 128;
}

bool TRsaHelper::encrypt(uint8_t* block, const int size)
{
    if (size <= 0 || size != blockSize())
        return false;
    const BigInt encrypted = BigInt::modPow(BigInt::fromBytes(block, size), exponent(), modulus());
    encrypted.toBytes(block, size);
    return true;
}

}
