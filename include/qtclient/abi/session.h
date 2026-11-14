#pragma once

/// Session peels exposed on the host-pushed pack ABI (C only).

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Host OS string: "windows" | "mac" | "linux" | other. Buf must be >= 32. */
typedef void (*QtClientHostOsFn)(char* buf, size_t buf_len);

typedef void (*QtClientEnableFeatureFn)(int feature);

#ifdef __cplusplus
}
#endif
