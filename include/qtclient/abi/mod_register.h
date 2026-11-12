#pragma once

/// Convenience include for pack MODULE entry points.

#include "qtclient/abi/host_api_v1.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Pack MODULE entry — implement in pack code; tip resolves by name. */
QTCLIENT_MOD_EXPORT void qtclient_mod_register(const QtClientHostApiV1* api,
                                               const QtClientModLoadCtx* ctx);

#ifdef __cplusplus
}
#endif
