/**
 * @file
 */
/******************************************************************************
 * Copyright AllSeen Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

#define AJ_MODULE CONSOLE

#include "ajs.h"
#include "ajs_util.h"
#include "ajs_target.h"
#include "aj_crypto.h"
#include "ajs_services.h"
#include "ajs_debugger.h"

/**
 * Controls debug output for this module
 */
#ifndef NDEBUG
uint8_t dbgCONSOLE;
#endif

/*
 * Port number for the console service. This value must match the console port number defined in AllJoyn.js
 */
#define SCRIPT_CONSOLE_PORT 7714

#define ENDSWAP32(v) (((v) >> 24) | (((v) & 0xFF0000) >> 8) | (((v) & 0x00FF00) << 8) | ((v) << 24))

/*
 *  Reply codes for the eval and install methods
 */
#define SCRIPT_OK                   0  /* script compiled and ran succesfully */
#define SCRIPT_SYNTAX_ERROR         1  /* script did not compile */
#define SCRIPT_EVAL_ERROR           2  /* script compiled but did not run */
#define SCRIPT_RESOURCE_ERROR       3  /* insufficient resources */
#define SCRIPT_NEED_RESET_ERROR     4  /* reset required before script can be installed */
#define SCRIPT_INTERNAL_ERROR       5  /* an undiagnosed internal error */
#define SCRIPT_DEBUG_STARTED        6  /* the debugger was started successfully */
#define SCRIPT_DEBUG_STOPPED        7  /* the debugger was not started successfully, or stopped */
#define SCRIPT_DEBUG_START          8  /* start the debugger */
#define SCRIPT_DEBUG_STOP           9  /* stop the debugger */

typedef enum {
    ENGINE_RUNNING, /* A script is installed and the engine is running */
    ENGINE_CLEAN,   /* The engine has been reset, there is no script running */
    ENGINE_DIRTY    /* The engine is in an unknown state */
} EngineState;

static EngineState engineState = ENGINE_RUNNING;

static const size_t MAX_EVAL_LEN = 1024;

static const char* const scriptConsoleIface[] = {
    "org.allseen.scriptConsole",
    "@engine>s",                                   /* Script engine supported e.g. JavaScript, Lua, Python, etc. */
    "@maxEvalLen>u",                               /* Maximum size script the eval method can handle */
    "@maxScriptLen>u",                             /* Maximum size script the install method can handle */
    "?eval script<ay status>y output>s",           /* Evaluate a short script and run it */
    "?install name<s script<ay status>y output>s", /* Install a new script, the script engine must be in a reset state */
    "?reset",                                      /* Reset the script engine */
    "?reboot",                                     /* Reboot the device */
    "!print txt>s",                                /* Send a print string to the controller */
    "!alert txt>s",                                /* Send an alert string to the controller */
    NULL
};

static const char* const scriptDebugIface[] = {
    "org.allseen.scriptDebugger",
    "!notification id>y data>yssyy",                /* Notification to the debug client, id=type of notification */
    "?basicInfo reply>yssy",                        /* Basic info request */
    "?triggerStatus reply>y",                       /* This triggers a notification update */
    "?pause reply>y",                               /* Pause the debugger */
    "?resume reply>y",                              /* Resume the debugger */
    "?stepInto reply>y",                            /* Step into a function */
    "?stepOver reply>y",                            /* Step over a function */
    "?stepOut reply>y",                             /* Step out of a function */
    "?listBreak reply>a(sy)",                       /* List breakpoints */
    "?addBreak request<sy reply>y",                 /* Add a breakpoint */
    "?delBreak request<y reply>y",                  /* Delete a breakpoint */
    "?getVar request<s reply>yyv",                  /* Get a variable */
    "?putVar request<syay reply>y",                 /* Put a variable */
    "?getCallStack reply>a(ssyy)",                  /* Get the call stack */
    "?getLocals reply>a(ysv)",                      /* Get locals */
    "?dumpHeap reply>av",                           /* Dump the heap */
    "!version versionString>s",                     /* Initial version information */
    "?getScript script>ay",                         /* Get the debug targets script (for remote debugging) */
    "?detach reply>y",                              /* Detach the console from the debugger (script will continue to run) */
    "?eval string<s reply>yyv",                     /* Special eval method for use while debugging (regular eval will not work) */
    "?begin quiet<y output>y",                      /* Begin a debug session */
    "?end output>y",                                /* End a debug session */
    NULL
};

#define SHOW_DBG_MSG

static const AJ_InterfaceDescription consoleInterfaces[] = {
    AJ_PropertiesIface,
    scriptConsoleIface,
    scriptDebugIface,
    NULL
};

static const AJ_Object consoleObjects[] = {
    { "/ScriptConsole", consoleInterfaces, AJ_OBJ_FLAG_ANNOUNCED },
    { NULL, NULL }
};

#define GET_PROP_MSGID      AJ_APP_MESSAGE_ID(0, 0, AJ_PROP_GET)
#define SET_PROP_MSGID      AJ_APP_MESSAGE_ID(0, 0, AJ_PROP_SET)

#define SCRIPT_ENGINE_PROP  AJ_APP_PROPERTY_ID(0, 1, 0)
#define MAX_EVAL_LEN_PROP   AJ_APP_PROPERTY_ID(0, 1, 1)
#define MAX_SCRIPT_LEN_PROP AJ_APP_PROPERTY_ID(0, 1, 2)

/*
 * Console messages (org.allseen.scriptConsole)
 */
#define EVAL_MSGID          AJ_APP_MESSAGE_ID(0,  1, 3)
#define INSTALL_MSGID       AJ_APP_MESSAGE_ID(0,  1, 4)
#define RESET_MSGID         AJ_APP_MESSAGE_ID(0,  1, 5)
#define REBOOT_MSGID        AJ_APP_MESSAGE_ID(0,  1, 6)
#define PRINT_SIGNAL_MSGID  AJ_APP_MESSAGE_ID(0,  1, 7)
#define ALERT_SIGNAL_MSGID  AJ_APP_MESSAGE_ID(0,  1, 8)

/*
 * We don't want scripts to fill all available NVRAM.
 */
static uint32_t MaxScriptLen()
{
    return (3 * AJ_NVRAM_GetSizeRemaining()) / 4;
}

/**
 * Active session for this service
 */
static uint32_t consoleSession = 0;
static char consoleBusName[16];
static uint32_t scriptSize = 0;
static uint8_t debuggerStarted = FALSE;
/*
 * If debugQuiet is enabled will not send prints to the console
 * and instead print them out locally
 */
static uint8_t debugQuiet = FALSE;

uint32_t AJS_GetScriptSize(void)
{
    return scriptSize;
}

char* AJS_GetConsoleBusName(void)
{
    return consoleBusName;
}

uint32_t AJS_GetConsoleSession(void)
{
    return consoleSession;
}

static void SignalConsole(duk_context* ctx, uint32_t sigId, int nargs)
{
    AJ_Status status;
    AJ_Message msg;
    size_t len = 0;
    AJ_BusAttachment* bus = AJS_GetBusAttachment();
    int i;

    if (!debugQuiet) {
        /*
         * We need to know the total string length before we start to marshal
         */
        for (i = 0; i < nargs; ++i) {
            size_t sz;
            duk_safe_to_lstring(ctx, i, &sz);
            len += sz;
        }
        status = AJ_MarshalSignal(bus, &msg, sigId, consoleBusName, consoleSession, 0, 0);

        if (status == AJ_OK) {
            status = AJ_DeliverMsgPartial(&msg, len + 1 + sizeof(uint32_t));
        }
        if (status == AJ_OK) {
            status = AJ_MarshalRaw(&msg, &len, 4);
        }
        for (i = 0; (status == AJ_OK) && (i < nargs); ++i) {
            size_t sz;
            const char* str = duk_safe_to_lstring(ctx, i, &sz);
            status = AJ_MarshalRaw(&msg, str, sz);
        }
        /*
         * Marshal final NUL
         */
        if (status == AJ_OK) {
            char nul = 0;
            status = AJ_MarshalRaw(&msg, &nul, 1);
        }
        if (status == AJ_OK) {
            status = AJ_DeliverMsg(&msg);
        }
        if (status != AJ_OK) {
            AJ_ErrPrintf(("Failed to deliver signal error:%s\n", AJ_StatusText(status)));
        }
    }
}

void AJS_AlertHandler(duk_context* ctx, uint8_t alert)
{
    int nargs = duk_get_top(ctx);

    if (consoleSession && !debugQuiet) {
        SignalConsole(ctx, alert ? ALERT_SIGNAL_MSGID : PRINT_SIGNAL_MSGID, nargs);
    } else {
        int i;
        AJ_Printf("%s: ", alert ? "ALERT" : "PRINT");
        for (i = 0; i < nargs; ++i) {
            AJ_Printf("%s", duk_safe_to_string(ctx, i));
        }
        AJ_Printf("\n");
    }
}

static int SafeAlert(duk_context* ctx)
{
    AJS_AlertHandler(ctx, TRUE);
    return 0;
}

static int GetStackSafe(duk_context* ctx)
{
#if DUK_VERSION < 10099
#define duk_is_error(ctx, idx) (1)
#endif
    if (duk_is_object(ctx, -1) && duk_has_prop_string(ctx, -1, "stack") && duk_is_error(ctx, -1)) {
        duk_get_prop_string(ctx, -1, "stack");
        duk_remove(ctx, -2);
    }
    return 1;
}

void AJS_ConsoleSignalError(duk_context* ctx)
{
    duk_safe_call(ctx, GetStackSafe, 1, 1);
    duk_safe_call(ctx, SafeAlert, 0, 0);
}

static AJ_Status EvalReply(duk_context* ctx, AJ_Message* msg, int dukStatus)
{
    AJ_Message reply;
    uint8_t replyStatus;
    const char* replyTxt;

    switch (dukStatus) {
    case DUK_EXEC_SUCCESS:
        replyStatus = SCRIPT_OK;
        break;

    case DUK_RET_EVAL_ERROR:
    case DUK_RET_TYPE_ERROR:
    case DUK_RET_RANGE_ERROR:
        replyStatus = SCRIPT_EVAL_ERROR;
        break;

    case DUK_RET_SYNTAX_ERROR:
        replyStatus = SCRIPT_SYNTAX_ERROR;
        break;

    case DUK_RET_ALLOC_ERROR:
        replyStatus = SCRIPT_RESOURCE_ERROR;
        break;

    default:
        replyStatus = SCRIPT_INTERNAL_ERROR;
    }

    AJ_MarshalReplyMsg(msg, &reply);

    duk_to_string(ctx, -1);
    replyTxt = duk_get_string(ctx, -1);
    AJ_MarshalArgs(&reply, "ys", replyStatus, replyTxt);
    duk_pop(ctx);
    return AJ_DeliverMsg(&reply);
}

static AJ_Status Eval(duk_context* ctx, AJ_Message* msg)
{
    AJ_Message error;
    AJ_Status status;
    duk_int_t retval;
    size_t sz;
    const void* raw;
    uint32_t len;
    uint8_t endswap = (msg->hdr->endianess != AJ_NATIVE_ENDIAN);

    status = AJ_UnmarshalRaw(msg, &raw, sizeof(len), &sz);
    if (status != AJ_OK) {
        goto ErrorReply;
    }
    memcpy(&len, raw, sizeof(len));
    if (endswap) {
        len = ENDSWAP32(len);
    }
    if (len > MAX_EVAL_LEN) {
        retval = DUK_RET_ALLOC_ERROR;
        duk_push_string((ctx), "Eval expression too long");
    } else {
        uint8_t* js = AJ_Malloc(len);
        size_t l = len;
        uint8_t* p = js;
        if (!js) {
            status = AJ_ERR_RESOURCES;
            goto ErrorReply;
        }
        while (l) {
            status = AJ_UnmarshalRaw(msg, &raw, l, &sz);
            if (status != AJ_OK) {
                AJ_Free(js);
                goto ErrorReply;
            }
            memcpy(p, raw, sz);
            p += sz;
            l -= sz;
        }
        /*
         * Strip trailing NULs
         */
        while (len && !js[len - 1]) {
            --len;
        }
        duk_push_string(ctx, "ConsoleInput.js");
        retval = duk_pcompile_lstring_filename(ctx, DUK_COMPILE_EVAL, (const char*)js, len);
        AJ_Free(js);
        if (retval == DUK_EXEC_SUCCESS) {
            AJS_SetWatchdogTimer(AJS_DEFAULT_WATCHDOG_TIMEOUT);
            retval = duk_pcall(ctx, 0);
            AJS_ClearWatchdogTimer();
        }
        /*
         * Eval leaves the engine in an unknown state
         */
        engineState = ENGINE_DIRTY;
    }
    return EvalReply(ctx, msg, retval);

ErrorReply:

    AJ_MarshalStatusMsg(msg, &error, status);
    return AJ_DeliverMsg(&error);
}

static AJ_Status Install(duk_context* ctx, AJ_Message* msg)
{
    AJ_Message reply;
    AJ_Status status;
    const void* raw;
    size_t sz;
    uint8_t replyStatus;
    uint32_t len;
    uint8_t endswap = (msg->hdr->endianess != AJ_NATIVE_ENDIAN);
    AJ_NV_DATASET* ds = NULL;

    /*
     * Scripts can only be installed on a clean engine
     */
    if (engineState != ENGINE_CLEAN) {
        AJ_Message reply;
        AJ_MarshalReplyMsg(msg, &reply);
        AJ_MarshalArgs(&reply, "ys", SCRIPT_NEED_RESET_ERROR, "Reset required");
        return AJ_DeliverMsg(&reply);
    } else {
        const char* scriptName;
        status = AJ_UnmarshalArgs(msg, "s", &scriptName);
        if (status != AJ_OK) {
            goto ErrorReply;
        }
        AJ_InfoPrintf(("Installing script %s\n", scriptName));
        /*
         * Save the script name so it can be passed to the compiler
         */
        sz = strlen(scriptName) + 1;
        ds = AJ_NVRAM_Open(AJS_SCRIPT_NAME_NVRAM_ID, "w", sz);
        if (ds) {
            AJ_NVRAM_Write(scriptName, sz, ds);
            AJ_NVRAM_Close(ds);
            ds = NULL;
        }
    }
    /*
     * Load script and install it
     */
    status = AJ_UnmarshalRaw(msg, &raw, sizeof(len), &sz);
    if (status != AJ_OK) {
        goto ErrorReply;
    }
    memcpy(&len, raw, sizeof(len));
    if (endswap) {
        len = ENDSWAP32(len);
    }
    AJ_MarshalReplyMsg(msg, &reply);
    if (len > MaxScriptLen()) {
        replyStatus = SCRIPT_RESOURCE_ERROR;
        AJ_MarshalArgs(&reply, "ys", replyStatus, "Script too long");
        AJ_ErrPrintf(("Script installation failed - too long\n"));
        status = AJ_DeliverMsg(&reply);
    } else {
        ds = AJ_NVRAM_Open(AJS_SCRIPT_NVRAM_ID, "w", sizeof(len) + len);
        if (AJ_NVRAM_Write(&len, sizeof(len), ds) != sizeof(len)) {
            status = AJ_ERR_RESOURCES;
            goto ErrorReply;
        }
        /* Save the script's length */
        scriptSize = len + 4;
        while (len) {
            status = AJ_UnmarshalRaw(msg, &raw, len, &sz);
            if (status != AJ_OK) {
                goto ErrorReply;
            }
            if (AJ_NVRAM_Write(raw, sz, ds) != sz) {
                status = AJ_ERR_RESOURCES;
                goto ErrorReply;
            }
            len -= sz;
        }
        AJ_NVRAM_Close(ds);

        ds = AJ_NVRAM_Open(AJS_SCRIPT_SIZE_ID, "w", sizeof(uint32_t));
        if (AJ_NVRAM_Write(&scriptSize, sizeof(scriptSize), ds) != sizeof(scriptSize)) {
            status = AJ_ERR_RESOURCES;
            goto ErrorReply;
        }
        AJ_NVRAM_Close(ds);
        ds = NULL;
        /*
         * Let console know the script was installed sucessfully
         */
        replyStatus = SCRIPT_OK;
        AJ_MarshalArgs(&reply, "ys", replyStatus, "Script installed");
        AJ_InfoPrintf(("Script succesfully installed\n"));
        status = AJ_DeliverMsg(&reply);
        if (status == AJ_OK) {
            /*
             * Return a RESTART_APP status code; this will cause the msg loop to exit and reload the
             * script engine and run the script we just installed.
             */
            status = AJ_ERR_RESTART_APP;
        }
    }
    return status;

ErrorReply:

    if (ds) {
        AJ_NVRAM_Close(ds);
    }
    /*
     * We don't want to leave a stale or partial script in NVRAM
     */
    AJ_NVRAM_Delete(AJS_SCRIPT_NVRAM_ID);

    AJ_MarshalStatusMsg(msg, &reply, status);
    return AJ_DeliverMsg(&reply);
}

static AJ_Status Reset(AJ_Message* msg)
{
    AJ_Status status;
    AJ_Message reply;

    AJ_MarshalReplyMsg(msg, &reply);
    status = AJ_DeliverMsg(&reply);
    if (status == AJ_OK) {
        engineState = ENGINE_CLEAN;
        /*
         * The script engine must be restarted after a reset
         */
        status = AJ_ERR_RESTART_APP;
    }
    return status;
}

AJS_DebuggerState* dbgState = NULL;

static AJ_Status StartDebugger(duk_context* ctx, AJ_Message* msg)
{
    AJ_Status status = AJ_OK;
    AJ_Message reply;
    uint8_t quiet;
    status = AJ_UnmarshalArgs(msg, "y", &quiet);
    if (quiet) {
        debugQuiet = TRUE;
    }
    AJ_InfoPrintf(("StartStopDebugger(): Got method to start debugging\n"));
    if (!debuggerStarted) {
        AJ_MarshalReplyMsg(msg, &reply);
        AJ_MarshalArgs(&reply, "y", SCRIPT_DEBUG_STARTED);
        AJ_DeliverMsg(&reply);
    }
    AJS_DisableWatchdogTimer();
    dbgState = AJS_InitDebugger(ctx);
    /* Start the debugger */
    duk_debugger_attach(ctx,
                        AJS_DebuggerRead,
                        AJS_DebuggerWrite,
                        AJS_DebuggerPeek,
                        AJS_DebuggerReadFlush,
                        AJS_DebuggerWriteFlush,
                        AJS_DebuggerDetached,
                        (void*)dbgState);
    return status;
}

static AJ_Status StopDebugger(duk_context* ctx, AJ_Message* msg)
{
    AJ_Message reply;
    AJ_InfoPrintf(("StartStopDebugger(): Got method to stop debugging\n"));
    if (debuggerStarted) {
        /* Stop the debugger */
        duk_debugger_detach(ctx);
    }
    AJS_EnableWatchdogTimer();
    AJ_MarshalReplyMsg(msg, &reply);
    AJ_MarshalArgs(&reply, "y", SCRIPT_DEBUG_STOPPED);
    return AJ_DeliverMsg(&reply);
}

static AJ_Status PropGetHandler(AJ_Message* replyMsg, uint32_t propId, void* context)
{
    switch (propId) {
    case SCRIPT_ENGINE_PROP:
        return AJ_MarshalArgs(replyMsg, "s", "JavaScript");

    case MAX_EVAL_LEN_PROP:
        return AJ_MarshalArgs(replyMsg, "u", MAX_EVAL_LEN);

    case MAX_SCRIPT_LEN_PROP:
        return AJ_MarshalArgs(replyMsg, "u", (uint32_t)MaxScriptLen());

    default:
        return AJ_ERR_UNEXPECTED;
    }
}

static AJ_Status PropSetHandler(AJ_Message* replyMsg, uint32_t propId, void* context)
{
    return AJ_ERR_UNEXPECTED;
}

AJ_Status AJS_ConsoleMsgHandler(duk_context* ctx, AJ_Message* msg)
{
    AJ_Status status;

    if (msg->msgId == AJ_METHOD_ACCEPT_SESSION) {
        uint32_t sessionId;
        uint16_t port;
        char* joiner;
        status = AJ_UnmarshalArgs(msg, "qus", &port, &sessionId, &joiner);
        if (status != AJ_OK) {
            return status;
        }
        if (port == SCRIPT_CONSOLE_PORT) {
            /*
             * Only allow one controller at a time
             */
            if (consoleSession) {
                status = AJ_BusReplyAcceptSession(msg, FALSE);
            } else {
                status = AJ_BusReplyAcceptSession(msg, TRUE);
                if (status == AJ_OK) {
                    size_t sz = strlen(joiner) + 1;
                    AJ_InfoPrintf(("Accepted session session_id=%u joiner=%s\n", sessionId, joiner));
                    if (sz > sizeof(consoleBusName)) {
                        return AJ_ERR_RESOURCES;
                    }
                    consoleSession = sessionId;
                    memcpy(consoleBusName, joiner, sz);
                }
            }
        } else {
            /*
             * Not for us, reset the args so they can be unmarshaled again.
             */
            status = AJ_ResetArgs(msg);
            if (status == AJ_OK) {
                status = AJ_ERR_NO_MATCH;
            }
        }
        return status;
    }
    /*
     * If there is no console attached then this message is not for us
     */
    status = AJ_ERR_NO_MATCH;
    if (!consoleSession) {
        return status;
    }
    switch (msg->msgId) {
    case AJ_SIGNAL_SESSION_LOST_WITH_REASON:
        {
            uint32_t sessionId;

            status = AJ_UnmarshalArgs(msg, "u", &sessionId);
            if (status != AJ_OK) {
                return status;
            }
            if (sessionId == consoleSession) {
                consoleSession = 0;
                status = AJ_OK;
            } else {
                /*
                 * Not our session, reset the args so they can be unmarshaled again.
                 */
                status = AJ_ResetArgs(msg);
                if (status == AJ_OK) {
                    status = AJ_ERR_NO_MATCH;
                }
            }
        }
        break;

    case GET_PROP_MSGID:
        status = AJ_BusPropGet(msg, PropGetHandler, NULL);
        break;

    case SET_PROP_MSGID:
        status = AJ_BusPropSet(msg, PropSetHandler, NULL);
        break;

    case INSTALL_MSGID:
        status = Install(ctx, msg);
        break;

    case RESET_MSGID:
        status = Reset(msg);
        break;

    case REBOOT_MSGID:
        AJ_Reboot();
        break;

    case EVAL_MSGID:
        status = Eval(ctx, msg);
        break;

    case DBG_BEGIN_MSGID:
        status = StartDebugger(ctx, msg);
        break;

    case DBG_END_MSGID:
        status = StopDebugger(ctx, msg);
        break;

    case DBG_GETSCRIPT_MSGID:
        {
            AJ_Message reply;
            const uint8_t* script;
            AJ_NV_DATASET* ds = NULL;
            AJ_NV_DATASET* dsize = NULL;
            uint32_t sz = AJS_GetScriptSize();

            /* If the script was previously installed on another boot the size will be zero */
            if (!sz) {
                dsize = AJ_NVRAM_Open(AJS_SCRIPT_SIZE_ID, "r", 0);
                if (dsize) {
                    AJ_NVRAM_Read(&sz, sizeof(sz), dsize);
                    AJ_NVRAM_Close(dsize);
                }
            }
            ds = AJ_NVRAM_Open(AJS_SCRIPT_NVRAM_ID, "r", 0);
            if (ds) {
                script = AJ_NVRAM_Peek(ds);
                status = AJ_MarshalReplyMsg(msg, &reply);
                if (status == AJ_OK) {
                    status = AJ_DeliverMsgPartial(&reply, sz + sizeof(uint32_t));
                }
                if (status == AJ_OK) {
                    status = AJ_MarshalRaw(&reply, &sz, sizeof(uint32_t));
                }
                if (status == AJ_OK) {
                    status = AJ_MarshalRaw(&reply, script, sz);
                }
                if (status == AJ_OK) {
                    status = AJ_DeliverMsg(&reply);
                }
                status = AJ_OK;

                AJ_NVRAM_Close(ds);
            } else {
                AJ_ErrPrintf(("Error opening script NVRAM entry\n"));
                AJ_MarshalStatusMsg(msg, &reply, AJ_ERR_BUSY);
                AJ_DeliverMsg(&reply);
                status = AJ_OK;
            }
        }
        break;

    /*
     * Pause can be handled when the debugger is running (as well as in AJS_DebuggerRead())
     */
    case DBG_PAUSE_MSGID:
        {
            uint16_t len = 3;
            uint32_t dbgMsg = BUILD_DBG_MSG(DBG_TYPE_REQ, (PAUSE_REQ + 0x80), DBG_TYPE_EOM, 0);

            /* Copy the message to the static buffer */
            if (AJ_IO_BUF_SPACE(dbgState->read) >= len) {
                memcpy(dbgState->read->writePtr, &dbgMsg, len);
                dbgState->read->writePtr += len;
            } else {
                AJ_ErrPrintf(("No space to write debug message\n"));
            }

            /* Save away the last message for the method reply */
            memcpy(&dbgState->lastMsg, msg, sizeof(AJ_Message));
            dbgState->lastMsgType = PAUSE_REQ;
            status = AJ_OK;

        }
        break;

    /*
     * Breakpoints can be created while the target is running (as well as in AJS_DebuggerRead()).
     */
    case DBG_ADDBREAK_MSGID:
        {
            char* file;
            uint8_t line;
            uint8_t* tmp;
            uint8_t msgLen;
            status = AJ_UnmarshalArgs(msg, "sy", &file, &line);

            if (status == AJ_OK) {
                msgLen = strlen(file) + 5;
                tmp = AJ_Malloc(msgLen);
                memset(tmp, DBG_TYPE_REQ, 1);
                memset(tmp + 1, (ADD_BREAK_REQ + 0x80), 1);
                memset(tmp + 2, (strlen(file) + 0x60), 1);
                memcpy(tmp + 3, file, strlen(file));
                memset(tmp + 3 + strlen(file), (line + 0x80), 1);
                memset(tmp + 3 + strlen(file) + 1, DBG_TYPE_EOM, 1);

                /* Copy the message into the read buffer */
                if (AJ_IO_BUF_SPACE(dbgState->read) >= msgLen) {
                    memcpy(dbgState->read->writePtr, tmp, msgLen);
                    dbgState->read->writePtr += msgLen;
                } else {
                    AJ_ErrPrintf(("No space to write debug message\n"));
                }

                /* Save away this message for the method reply later */
                memcpy(&dbgState->lastMsg, msg, sizeof(AJ_Message));
                dbgState->lastMsgType = ADD_BREAK_REQ;

                AJ_Free(tmp);
                status = AJ_OK;
            }
        }
        break;

    /*
     * If a debug command is issued and picked up here (other than pause) it means the
     * debugger has been resumed (running). Commands in this state have no effect but a reply
     * must be sent back to the console because it is expecting one.
     */
    case DBG_BASIC_MSGID:
    case DBG_TRIGGER_MSGID:
    case DBG_RESUME_MSGID:
    case DBG_STEPIN_MSGID:
    case DBG_STEPOVER_MSGID:
    case DBG_STEPOUT_MSGID:
    case DBG_LISTBREAK_MSGID:
    case DBG_DELBREAK_MSGID:
    case DBG_GETVAR_MSGID:
    case DBG_PUTVAR_MSGID:
    case DBG_GETCALL_MSGID:
    case DBG_GETLOCALS_MSGID:
    case DBG_DUMPHEAP_MSGID:
    case DBG_VERSION_MSGID:
    case DBG_DETACH_MSGID:
        {
            AJ_Message reply;
            AJ_MarshalStatusMsg(msg, &reply, AJ_ERR_BUSY);
            AJ_DeliverMsg(&reply);
            status = AJ_OK;
        }
        break;

    default:
        status = AJ_ERR_NO_MATCH;
        break;
    }
    return status;
}

AJ_Status AJS_ConsoleInit(AJ_BusAttachment* aj)
{
    AJ_Status status;

    AJ_RegisterObjectList(consoleObjects, AJ_APP_ID_FLAG);
    status = AJ_BusBindSessionPort(aj, SCRIPT_CONSOLE_PORT, NULL, AJ_FLAG_NO_REPLY_EXPECTED);
    if (status != AJ_OK) {
        AJ_RegisterObjects(NULL, NULL);
    }
    return status;
}

void AJS_ConsoleTerminate()
{
    consoleSession = 0;
    engineState = ENGINE_DIRTY;
    AJ_RegisterObjects(NULL, NULL);
}
