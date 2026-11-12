#include <tibia/network/logincrypt.h>

#include <framework/util/crypt.h>

namespace tibia::network {
namespace {

// modules/gamelib/const.lua OTSERV_RSA — Crystal's login packet key.
constexpr const char* kOtservRsa =
    "1091201329673994292788609605089955415282375029027981291234687579"
    "3726629149257644633073969600111060390723088861007265581882535850"
    "3429057592827629436413108566029093628212635953836686562675849720"
    "6207862794310902180176810615217550567108238764764442605581471797"
    "07119674283982419152118103759076030616683978566631413";

}

void installLoginRsa()
{
    setRsaPublicKey(kOtservRsa, "65537");
}

void setRsaPublicKey(const std::string_view n, const std::string_view e)
{
    g_crypt.rsaSetPublicKey(std::string(n), std::string(e));
}

int rsaBlockSize()
{
    return g_crypt.rsaGetSize();
}

bool rsaEncrypt(uint8_t* msg, const int size)
{
    return g_crypt.rsaEncrypt(msg, size);
}

bool rsaDecrypt(uint8_t* msg, const int size)
{
    return g_crypt.rsaDecrypt(msg, size);
}

std::string crc32Hex(const std::string& data, const bool upperCase)
{
    return g_crypt.crc32(data, upperCase);
}

}
