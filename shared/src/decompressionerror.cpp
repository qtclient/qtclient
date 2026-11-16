#include "decompressionerror.h"

namespace tibia::shared {

TDecompressionError::TDecompressionError(const std::string& message)
    : std::runtime_error(message)
{
}

TDecompressionError::~TDecompressionError() = default;

}
