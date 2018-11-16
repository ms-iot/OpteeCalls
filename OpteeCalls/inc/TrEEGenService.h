/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License.

Module Name:

    TrEEGenService.h

Abstract:

    Defines TrEE generic service public types

*/

#pragma once

//
// Generic service request input/output parameters format
//

//
// Input buffer
//

typedef enum _GENSVC_INPUT_TYPE
{
    GenSvcInputTypeCommand = 0xCDCDCDCD,
    GenSvcInputTypeRpcResponse

} GENSVC_INPUT_TYPE;

typedef struct _GENSVC_INPUT_BUFFER_HEADER
{
    //
    // Either issuing a new command, or responding to an RPC during a command.
    //
    GENSVC_INPUT_TYPE Type;

    //
    // The Key must be unique for each pending commands, and common for all
    // requests pertaining to that command - i.e., any GenSvcInputTypeRpcResponse
    // requests for a given command must use the same Key as their corresponding
    // GenSvcInputTypeCommand request.
    //
    ULONG Key;

    //
    // OPTEE_MINIMUM_COMMAND_INPUT_SIZE is typically larger than the actual
    // input data sent to OPTEE, so record the actual data size here.
    //
    ULONG InputDataSize;

    //
    // OPTEE_MINIMUM_COMMAND_OUTPUT_SIZE is typically larger than the actual
    // output data size requested by the app, so record the actual data size here.
    //
    ULONG OutputDataSize;

    //
    // Zero or more bytes follow this header.
    // ...
    //

} GENSVC_INPUT_BUFFER_HEADER, *PGENSVC_INPUT_BUFFER_HEADER;


//
// Output buffer
//

typedef enum _GENSVC_OUTPUT_TYPE
{
    GenSvcOutputTypeCommandCompleted = 0xCAAACAAA,
    GenSvcOutputTypeRpcCommand

} GENSVC_OUTPUT_TYPE;


typedef struct _GENSVC_OUTPUT_BUFFER_HEADER
{
    //
    // - GenSvcOutputTypeCommandCompleted: when the entire TA function 
    //      has been completed.
    // - GenSvcOutputTypeRpcCommand: when TA has made an ocall RPC, provides
    //      input data, and awaits response. The untrusted APP will respond 
    //      with an GENSVC_INPUT_BUFFER_HEADER.Type = GenSvcInputTypeRpcResponse.
    //      The above cycle continues until the TA completes the call.
    //
    GENSVC_OUTPUT_TYPE Type;

    //
    // RPC type from TA
    //
    ULONG RpcType;

    //
    // OPTEE_MAXIMUM_RPC_INPUT_SIZE or more bytes follow this header.
    // ...
    //

} GENSVC_OUTPUT_BUFFER_HEADER, *PGENSVC_OUTPUT_BUFFER_HEADER;

#define OPTEE_RPC_INPUT_PAYLOAD_MAX_SIZE    (12 * 1024)
#define OPTEE_RPC_OUTPUT_PAYLOAD_MAX_SIZE   (12 * 1024)

#define OPTEE_MAXIMUM_RPC_INPUT_SIZE        \
    (OPTEE_RPC_INPUT_PAYLOAD_MAX_SIZE - sizeof(GENSVC_OUTPUT_BUFFER_HEADER))
#define OPTEE_MAXIMUM_RPC_OUTPUT_SIZE       \
    (OPTEE_RPC_OUTPUT_PAYLOAD_MAX_SIZE - sizeof(GENSVC_INPUT_BUFFER_HEADER))

#define OPTEE_MINIMUM_COMMAND_INPUT_SIZE    \
    (sizeof(GENSVC_INPUT_BUFFER_HEADER) + OPTEE_MAXIMUM_RPC_OUTPUT_SIZE)
#define OPTEE_MINIMUM_COMMAND_OUTPUT_SIZE   \
    (sizeof(GENSVC_OUTPUT_BUFFER_HEADER) + OPTEE_MAXIMUM_RPC_INPUT_SIZE)
