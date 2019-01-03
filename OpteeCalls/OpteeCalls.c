/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "stdafx.h"
#include <windows.h>

#include <winioctl.h>
#include <trustedrt.h>

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <malloc.h>

#include "inc\OpteeCalls.h"
#include "inc\TrEEGenService.h"

VOID
__cdecl
OpteeLibLog(
    _In_ PCSTR Format,
    ...
    )
{
    va_list arglist;
    char outputBuffer[256];

    va_start (arglist, Format);
    _vsnprintf_s (outputBuffer, ARRAYSIZE(outputBuffer), _TRUNCATE, Format, arglist);
    va_end (arglist);

    printf("Thread %#x: %s", GetCurrentThreadId(), outputBuffer);
    fflush(stdout);

    /*
    DbgPrintEx(DPFLTR_VERIFIER_ID, DPFLTR_ERROR_LEVEL,
                "Thread %u: %S", ThreadIndex, outputBuffer);
                */
}

static
void
CloseHandleIfNotNull(
    _In_ HANDLE Handle
    )
{
    BOOL success;

    if (Handle != NULL)
    {
        success = CloseHandle(Handle);
        assert(success);
    }
}

_Use_decl_annotations_
BOOL
OPTEE_CALL
CallOpteeCommand(
    HANDLE                  TreeServiceHandle,
    uint32_t                FunctionCode,
    void *                  CommandInputBuffer,
    uint32_t                CommandInputSize,
    void *                  CommandOutputBuffer,
    uint32_t                CommandOutputSize,
    uint32_t *              CommandOutputSizeWritten,
    OpteeRpcCallbackType    RpcCallback,
    void *                  RpcCallbackContext
    )
{
    TR_SERVICE_REQUEST request;
    TR_SERVICE_REQUEST_RESPONSE response;
    BOOL success = TRUE;
    ULONG responseBytesWritten;
    ULONG outputSizeRounded;
    ULONG inputSizeRounded;
    uint8_t *inputHeader = NULL, *outputHeader = NULL;
    OVERLAPPED overlapped;
    uint32_t requestKey = GetCurrentThreadId();

    *CommandOutputSizeWritten = 0;
    memset(&overlapped, 0, sizeof(overlapped));

    // Add header to input buffer, and make sure the input buffer is large enough
    // to fit the largest RPC output data.
    assert((CommandInputBuffer != NULL) || (CommandInputSize == 0));
    inputSizeRounded = max(
        sizeof(GENSVC_INPUT_BUFFER_HEADER) + CommandInputSize,
        OPTEE_MINIMUM_COMMAND_INPUT_SIZE);

    inputHeader = malloc(inputSizeRounded);
    assert(inputHeader != NULL);
    memcpy(((PGENSVC_INPUT_BUFFER_HEADER)inputHeader) + 1, CommandInputBuffer, CommandInputSize);

    ((PGENSVC_INPUT_BUFFER_HEADER)inputHeader)->Type = GenSvcInputTypeCommand;
    ((PGENSVC_INPUT_BUFFER_HEADER)inputHeader)->Key = requestKey;
    ((PGENSVC_INPUT_BUFFER_HEADER)inputHeader)->InputDataSize = CommandInputSize;
    ((PGENSVC_INPUT_BUFFER_HEADER)inputHeader)->OutputDataSize = CommandOutputSize;

    // Add header to output buffer, and make sure the input buffer is large enough
    // to fit the largest RPC input data.
    assert((CommandOutputBuffer != NULL) || (CommandOutputSize == 0));
    outputSizeRounded = max(
        sizeof(GENSVC_OUTPUT_BUFFER_HEADER) + CommandOutputSize,
        OPTEE_MINIMUM_COMMAND_OUTPUT_SIZE);

    outputHeader = malloc(outputSizeRounded);
    assert(outputHeader != NULL);

    // Get ready to send requests to the OPTEE driver.
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (overlapped.hEvent == NULL)
    {
        success = FALSE;
        goto Done;
    }

    for (;;)
    {
        request.FunctionCode = FunctionCode;
        request.InputBuffer = inputHeader;
        request.InputBufferSize = inputSizeRounded;
        request.OutputBuffer = outputHeader;
        request.OutputBufferSize = outputSizeRounded;

        success = ResetEvent(overlapped.hEvent);
        assert(success);

        // OpteeLibLog("Calling DeviceIoControl\n");

        success = DeviceIoControl(
            TreeServiceHandle,
            IOCTL_TR_EXECUTE_FUNCTION,
            &request,
            sizeof(request),
            &response,
            sizeof(response),
            &responseBytesWritten,
            &overlapped);

        if (!success && (GetLastError() == ERROR_IO_PENDING))
        {
            // OpteeLibLog("Waiting for completion...\n", responseBytesWritten);

            success = GetOverlappedResult(
                TreeServiceHandle, 
                &overlapped,
                &responseBytesWritten,
                TRUE);
        }

        if (!success)
        {
            OpteeLibLog("Command failed, error %u\n", GetLastError());
            break;
        }

        // OpteeLibLog("DeviceIoControl returned %u bytes\n", responseBytesWritten);

        // Check if the response structure has been filled.
        if (responseBytesWritten != sizeof(response))
        {
            OpteeLibLog(
                "DeviceIoControl returned size %u, expected %u\n",
                responseBytesWritten,
                sizeof(response));

            success = FALSE;
            break;
        }

        // Validate the BytesWritten field of the response.
        responseBytesWritten = (ULONG)response.BytesWritten;

        if (responseBytesWritten < sizeof (GENSVC_OUTPUT_BUFFER_HEADER))
        {
            OpteeLibLog(
                "DeviceIoControl returned size %u, expected at least %u\n",
                responseBytesWritten,
                sizeof(GENSVC_OUTPUT_BUFFER_HEADER));

            success = FALSE;
            break;
        }

        // Check the response type.
        switch(((PGENSVC_OUTPUT_BUFFER_HEADER)outputHeader)->Type)
        {
            case GenSvcOutputTypeCommandCompleted:
            {
                // responseBytesWritten can be larger than commandOutputSize, because
                // OPTEE_MINIMUM_COMMAND_OUTPUT_SIZE is a relatively-large value.
                *CommandOutputSizeWritten = responseBytesWritten - sizeof(GENSVC_OUTPUT_BUFFER_HEADER);

                if (*CommandOutputSizeWritten > CommandOutputSize)
                {
                    *CommandOutputSizeWritten = CommandOutputSize;
                }

                if ((*CommandOutputSizeWritten > 0) &&
                    (*CommandOutputSizeWritten <= CommandOutputSize))
                {
                    memcpy(
                        CommandOutputBuffer,
                        ((PGENSVC_OUTPUT_BUFFER_HEADER)outputHeader) + 1,
                        *CommandOutputSizeWritten);
                }
                goto Done;
            }

            case GenSvcOutputTypeRpcCommand:
            {
                if (RpcCallback == NULL) {
                    OpteeLibLog("ERROR: RPC request, with no callback");
                    success = FALSE;
                    goto Done;
                }

                success = RpcCallback(
                    RpcCallbackContext,
                    ((PGENSVC_OUTPUT_BUFFER_HEADER)outputHeader)->RpcType,                     // RPC type
                    (PGENSVC_OUTPUT_BUFFER_HEADER)outputHeader + 1,                            // RPC input buffer.
                    responseBytesWritten - sizeof((PGENSVC_OUTPUT_BUFFER_HEADER)outputHeader), // RPC input buffer size.
                    (PGENSVC_INPUT_BUFFER_HEADER)inputHeader + 1,                              // RPC output buffer.
                    OPTEE_MAXIMUM_RPC_OUTPUT_SIZE,                                             // RPC output buffer size.
                    (uint32_t *)&((PGENSVC_INPUT_BUFFER_HEADER)inputHeader)->InputDataSize);   // Actual RPC output size.

                /*
                OpteeLibLog(
                    "%s: callback for RPC type %u returned success = %u, %#x output bytes\n",
                    __FUNCTION__,
                    ((PTESTSVC_OUTPUT_BUFFER_HEADER)outputHeader)->RpcType,
                    (uint32_t)success,
                    ((PTESTSVC_INPUT_BUFFER_HEADER)inputHeader)->InputDataSize);
                    */

                if (!success)
                {
                    OpteeLibLog(
                        "ERROR: callback failed for RPC type %#x\n",
                        ((PGENSVC_OUTPUT_BUFFER_HEADER)outputHeader)->RpcType);

                    goto Done;
                }

                ((PGENSVC_INPUT_BUFFER_HEADER)inputHeader)->Type = GenSvcInputTypeRpcResponse;
                ((PGENSVC_INPUT_BUFFER_HEADER)inputHeader)->Key = requestKey;
                ((PGENSVC_INPUT_BUFFER_HEADER)inputHeader)->OutputDataSize = OPTEE_MAXIMUM_RPC_OUTPUT_SIZE;

                // Go to the top of the loop, and send the RPC output data.
                break;
            }

            default:
            {
                OpteeLibLog(
                    "Error: outputHeader->Type has unknown value %u",
                    ((PGENSVC_OUTPUT_BUFFER_HEADER)outputHeader)->Type);

                success = FALSE;
                goto Done;
            }
        }
    }

Done:

    if (inputHeader != NULL)
    {
        free(inputHeader);
    }

    if (outputHeader != NULL)
    {
        free(outputHeader);
    }

    CloseHandleIfNotNull(overlapped.hEvent);
    return (success != FALSE);
}