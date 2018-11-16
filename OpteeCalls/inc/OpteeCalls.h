// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once


#ifdef __cplusplus
extern "C" {
#endif

#define OPTEE_CALL __stdcall

typedef
BOOL
(OPTEE_CALL *OpteeRpcCallbackType)(
    _In_opt_                            void *                 RpcCallbackContext,
    _In_                                uint32_t               RpcType,
    _In_reads_bytes_(rpcInputSize)      const void *           RrpcInputBuffer,
    _In_                                uint32_t               RpcInputSize,
    _Out_writes_bytes_to_(
        rpcOutputSize, 
        *rpcOutputSizeWritten)          void *                 RpcOutputBuffer,
    _In_                                uint32_t               RpcOutputSize,
    _Out_                               uint32_t *             RpcOutputSizeWritten
    );

BOOL
OPTEE_CALL
CallOpteeCommand(
    _In_                                HANDLE                  TreeServiceHandle,
    _In_                                uint32_t                FunctionCode,
    _In_reads_bytes_(commandInputSize)  const void *            CommandInputBuffer,
    _In_                                uint32_t                CommandInputSize,
    _Out_writes_bytes_to_(
        commandOutputSize, 
        *commandOutputSizeWritten)      void *                  CommandOutputBuffer,
    _In_                                uint32_t                CommandOutputSize,
    _Out_                               uint32_t *              CommandOutputSizeWritten,
    _In_opt_                            OpteeRpcCallbackType    RpcCallback,
    _In_opt_                            void *                  RpcCallbackContext
    );

VOID
__cdecl
OpteeLibLog(
    _In_ PCSTR Format,
    ...
    );

#ifdef __cplusplus
}; // extern "C"
#endif