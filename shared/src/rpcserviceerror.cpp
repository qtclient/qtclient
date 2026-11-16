#include "rpcserviceerror.h"

namespace tibia::shared {

TRPCServiceError::TRPCServiceError(const QString& message)
    : std::runtime_error(message.toStdString())
{
}

}
