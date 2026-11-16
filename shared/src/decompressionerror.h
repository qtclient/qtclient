#pragma once

#include <stdexcept>
#include <string>

namespace tibia::shared {

/// CIP `shared::TDecompressionError`: thrown when packed data cannot be unpacked.
class TDecompressionError : public std::runtime_error
{
public:
    explicit TDecompressionError(const std::string& message);
    ~TDecompressionError() override;
};

}
