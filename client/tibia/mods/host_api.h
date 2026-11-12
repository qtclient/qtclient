#pragma once

/// Tip-internal: filled host API table for runtime pack MODULE load.
#include "qtclient/abi/host_api_v1.h"

#ifdef __cplusplus
extern "C" {
#endif

const QtClientHostApiV1* qtclient_host_api_v1(void);

#ifdef __cplusplus
}
#endif
