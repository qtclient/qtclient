#pragma once

#include <QString>

#include <stdexcept>

namespace tibia {
namespace shared {

/// CIP `shared::TRPCServiceError`: an RPC service failure.
class TRPCServiceError : public std::runtime_error
{
public:
    explicit TRPCServiceError(const QString& message);
};

}
}
