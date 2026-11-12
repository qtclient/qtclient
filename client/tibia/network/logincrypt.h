#pragma once

/// Tip login-wire crypto façade. Call sites prefer this over engine
/// `framework/util/crypt.h` `g_crypt` (OTC hub peel).
#include <cstdint>
#include <string>
#include <string_view>

namespace tibia {
namespace network {

/// Crystal / OTServ login RSA public key (N) + e=65537.
void installLoginRsa();

void setRsaPublicKey(std::string_view n, std::string_view e);
int rsaBlockSize();
bool rsaEncrypt(uint8_t* msg, int size);
bool rsaDecrypt(uint8_t* msg, int size);

/// Hex CRC-32 (zlib), lower-case unless `upperCase`.
std::string crc32Hex(const std::string& data, bool upperCase = false);

}
}
