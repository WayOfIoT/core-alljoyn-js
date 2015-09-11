/**
 * @file
 */
/******************************************************************************
 *    Copyright (c) Open Connectivity Foundation (OCF) and AllJoyn Open
 *    Source Project (AJOSP) Contributors and others.
 *
 *    SPDX-License-Identifier: Apache-2.0
 *
 *    All rights reserved. This program and the accompanying materials are
 *    made available under the terms of the Apache License, Version 2.0
 *    which accompanies this distribution, and is available at
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Copyright (c) Open Connectivity Foundation and Contributors to AllSeen
 *    Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for
 *    any purpose with or without fee is hereby granted, provided that the
 *    above copyright notice and this permission notice appear in all
 *    copies.
 *
 *     THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 *     WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 *     WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 *     AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 *     DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 *     PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 *     TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 *     PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

#include "ajs.h"

static const char* const sortedWords[] = {
    "(?:)",
    "(function(require,exports,module){",
    "+Infinity",
    "-Infinity",
    "/ControlPanel",
    "/ControlPanel/CONTAINER1",
    "/ControlPanel/CONTAINER1/PROPERTY2",
    "1000",
    "AllJoyn",
    "Arguments",
    "Array",
    "Boolean",
    "Buffer",
    "CHECK_BOX",
    "DATE_PICKER",
    "Date",
    "DecEnv",
    "DoubleError",
    "Duktape",
    "EDIT_TEXT",
    "Emergency",
    "Error",
    "EvalError",
    "ExternalS",
    "Flash rate:",
    "Function",
    "HORIZONTAL",
    "Infinity",
    "Info",
    "Invalid Date",
    "JSON",
    "KEYPAD",
    "LN10",
    "LOG10E",
    "LOG2E",
    "Logger",
    "MAX_VALUE",
    "METHOD",
    "MIN_VALUE",
    "Math",
    "NEGATIVE_INFINITY",
    "NUMBER_PICKER",
    "NUMERIC_VIEW",
    "Null",
    "Number",
    "ObjEnv",
    "Object",
    "POSITIVE_INFINITY",
    "PROPERTY",
    "Pointer",
    "Proxy",
    "RADIO_BUTTON",
    "ROTARY_KNOB",
    "RangeError",
    "ReferenceError",
    "RegExp",
    "SIGNAL",
    "SLIDER",
    "SPINNER",
    "SQRT1_2",
    "SQRT2",
    "SWITCH",
    "String",
    "SyntaxError",
    "TEXT_VIEW",
    "TIME_PICKER",
    "Thread",
    "ThrowTypeError",
    "TypeError",
    "URIError",
    "Undefined",
    "VERTICAL",
    "Warning",
    "[...]",
    "__proto__",
    "acos",
    "actionWidget",
    "addMatch",
    "advertiseName",
    "alert",
    "analogIn",
    "analogOut",
    "anon",
    "apply",
    "arguments",
    "asin",
    "atan",
    "atan2",
    "base64",
    "baseWidget",
    "bind",
    "blinky",
    "boolean",
    "break",
    "buffer",
    "call",
    "callTimeout",
    "callee",
    "caller",
    "case",
    "catch",
    "ceil",
    "charAt",
    "charCodeAt",
    "choices",
    "class",
    "clearInterval",
    "clearTimeout",
    "clog",
    "color",
    "compact",
    "compile",
    "concat",
    "config",
    "configurable",
    "const",
    "constructor",
    "containerWidget",
    "continue",
    "controlPanel",
    "cpsObjects",
    "create",
    "current",
    "debug",
    "debugger",
    "decodeURI",
    "decodeURIComponent",
    "default",
    "defineProperties",
    "defineProperty",
    "delete",
    "deleteProperty",
    "dialogWidget",
    "digitalIn",
    "digitalOut",
    "duk_hthread_builtins.c",
    "else",
    "enabled",
    "encodeURI",
    "encodeURIComponent",
    "enum",
    "enumerable",
    "enumerate",
    "errCreate",
    "errThrow",
    "error",
    "error in error handling",
    "escape",
    "eval",
    "every",
    "exec",
    "export",
    "extends",
    "fallingEdge",
    "false",
    "fatal",
    "fileName",
    "filter",
    "finally",
    "findService",
    "findServiceByName",
    "floor",
    "forEach",
    "freeze",
    "fromCharCode",
    "function",
    "functions",
    "getAllProps",
    "getDate",
    "getDay",
    "getFullYear",
    "getHours",
    "getMilliseconds",
    "getMinutes",
    "getMonth",
    "getOwnPropertyDescriptor",
    "getOwnPropertyNames",
    "getProp",
    "getPrototypeOf",
    "getSeconds",
    "getTime",
    "getTimezoneOffset",
    "getUTCDate",
    "getUTCDay",
    "getUTCFullYear",
    "getUTCHours",
    "getUTCMilliseconds",
    "getUTCMinutes",
    "getUTCMonth",
    "getUTCSeconds",
    "getYear",
    "global",
    "hasOwnProperty",
    "i2cMaster",
    "i2cSlave",
    "ignoreCase",
    "implements",
    "import",
    "increment",
    "index",
    "indexOf",
    "info",
    "input",
    "instanceof",
    "interface",
    "interfaceDefinition",
    "isArray",
    "isExtensible",
    "isFinite",
    "isFrozen",
    "isNaN",
    "isPrototypeOf",
    "isSealed",
    "join",
    "keys",
    "label",
    "labelWidget",
    "languages",
    "lastIndex",
    "lastIndexOf",
    "ledslider.js",
    "length",
    "level",
    "lineNumber",
    "linkTimeout",
    "ll p hRHSBO p2 a4 x86 linux gcc",
    "load",
    "localeCompare",
    "match",
    "message",
    "method",
    "milliseconds",
    "modLoaded",
    "modSearch",
    "multiline",
    "name",
    "notification",
    "null",
    "number",
    "object",
    "objectDefinition",
    "onAttach",
    "onValueChanged",
    "openDrain",
    "ownKeys",
    "package",
    "parse",
    "parseFloat",
    "parseInt",
    "path",
    "peerProto",
    "pointer",
    "preventExtensions",
    "print",
    "private",
    "propertyIsEnumerable",
    "propertyWidget",
    "protected",
    "prototype",
    "public",
    "pullDown",
    "pullUp",
    "push",
    "random",
    "range",
    "rate",
    "reduce",
    "reduceRight",
    "replace",
    "require",
    "resetInterval",
    "resetTimeout",
    "resume",
    "return",
    "reverse",
    "risingEdge",
    "round",
    "seal",
    "search",
    "session",
    "setDate",
    "setFullYear",
    "setHours",
    "setInterval",
    "setMilliseconds",
    "setMinutes",
    "setMonth",
    "setProp",
    "setPrototypeOf",
    "setSeconds",
    "setTime",
    "setTimeout",
    "setUTCDate",
    "setUTCFullYear",
    "setUTCHours",
    "setUTCMilliseconds",
    "setUTCMinutes",
    "setUTCMonth",
    "setUTCSeconds",
    "setYear",
    "shift",
    "signal",
    "slice",
    "some",
    "sort",
    "source",
    "splice",
    "split",
    "sqrt",
    "stack",
    "static",
    "store",
    "string",
    "string-stash",
    "stringify",
    "substr",
    "substring",
    "super",
    "switch",
    "system",
    "test",
    "this",
    "throw",
    "timerFuncs",
    "timerState",
    "toDateString",
    "toExponential",
    "toFixed",
    "toGMTString",
    "toISOString",
    "toJSON",
    "toLocaleDateString",
    "toLocaleLowerCase",
    "toLocaleString",
    "toLocaleTimeString",
    "toLocaleUpperCase",
    "toLogString",
    "toLowerCase",
    "toPrecision",
    "toString",
    "toTimeString",
    "toUTCString",
    "toUpperCase",
    "toggle",
    "trace",
    "translations",
    "trim",
    "true",
    "typeof",
    "uart",
    "undefined",
    "unescape",
    "units",
    "unshift",
    "value",
    "valueOf",
    "version",
    "void",
    "warn",
    "while",
    "widgets",
    "with",
    "writable",
    "writeable",
    "yield",
    "{\"_func\":true}",
    "{\"_inf\":true}",
    "{\"_nan\":true}",
    "{\"_ninf\":true}",
    "{\"_undef\":true}",
    "{_func:true}",
    "\377Args",
    "\377Bytecode",
    "\377Callee",
    "\377Finalizer",
    "\377Formals",
    "\377Handler",
    "\377Lexenv",
    "\377Map",
    "\377Next",
    "\377Pc2line",
    "\377Regbase",
    "\377Source",
    "\377Target",
    "\377This",
    "\377Thread",
    "\377Tracedata",
    "\377Value",
    "\377Varenv",
    "\377Varmap",
    "\377ctx",
    "\377label",
    "\377range",
    "\377trigs",
    "\377wbuf"
};

const void* AJS_ExternalStringCheck(void* userData, const void* ptr, duk_size_t len)
{
    const char* str = ptr;
    size_t min;
    size_t max;

    if (len <= 3) {
        return NULL;
    }
    /*
     * Binary search over the sorted words
     */
    min = 0;
    max = ArraySize(sortedWords) - 1;
    do {
        size_t pos = min + (max - min) / 2;
        const char* word = sortedWords[pos];
        int cmp = strncmp(str, word, len);
        //printf("cmp=%d %d..%d pos:%d %s %.*s\n", cmp, min, max, pos, word, len, str);
        if (cmp == 0) {
            /*
             * Check for exact match
             */
            if (word[len] == 0) {
                //printf("Matched %s\n", word);
                return word;
            }
            /*
             * str is shorter than word
             */
            cmp = -1;
        }
        if (cmp < 0) {
            max = pos;
        } else {
            min = pos + 1;
        }
    } while (max != min);
    //printf("no match %.*s\n", len, str);
    return NULL;
}

void AJS_ExternalStringFree(void* userData, const void* extStr)
{
    /* No-op */
}
