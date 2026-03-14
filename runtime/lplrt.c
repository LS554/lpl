// Copyright 2026 London Sheard
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This file is part of the LPL runtime library.
// It is exempt from the Apache 2.0 license per the runtime exception
// in the root LICENSE file.

#include "lplrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * String implementation
 * ============================================================ */

void __lpl_string_create(LPLString* out, const char* data, int64_t len) {
    if (len < 0) len = 0;
    out->length = len;
    out->capacity = len + 1;
    out->data = (char*)malloc((size_t)out->capacity);
    if (out->data) {
        if (data && len > 0) {
            memcpy(out->data, data, (size_t)len);
        }
        out->data[len] = '\0';
    } else {
        out->length = 0;
        out->capacity = 0;
    }
}

void __lpl_string_destroy(LPLString* s) {
    if (s && s->data) {
        free(s->data);
        s->data = NULL;
        s->length = 0;
        s->capacity = 0;
    }
}

void __lpl_string_concat(LPLString* result, const LPLString* a, const LPLString* b) {
    int64_t newLen = a->length + b->length;
    result->length = newLen;
    result->capacity = newLen + 1;
    result->data = (char*)malloc((size_t)result->capacity);
    if (result->data) {
        if (a->data && a->length > 0) {
            memcpy(result->data, a->data, (size_t)a->length);
        }
        if (b->data && b->length > 0) {
            memcpy(result->data + a->length, b->data, (size_t)b->length);
        }
        result->data[newLen] = '\0';
    } else {
        result->length = 0;
        result->capacity = 0;
    }
}

int8_t __lpl_string_equal(const LPLString* a, const LPLString* b) {
    if (a->length != b->length) return 0;
    if (a->length == 0) return 1;
    return memcmp(a->data, b->data, (size_t)a->length) == 0 ? 1 : 0;
}

void __lpl_int_to_string(LPLString* out, int32_t val) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d", val);
    __lpl_string_create(out, buf, len);
}

/* ============================================================
 * Exception handling implementation
 * ============================================================ */

static LPLExceptionFrame exception_stack[LPL_MAX_EXCEPTION_HANDLERS];
static int exception_stack_top = -1;
static void* current_exception = NULL;
static const char* current_exception_type = NULL;

/* Type hierarchy: child->parent mapping for exception class inheritance */
#define LPL_MAX_EXCEPTION_TYPES 64
typedef struct {
    const char* child;
    const char* parent;
} LPLTypeRelation;

static LPLTypeRelation type_relations[LPL_MAX_EXCEPTION_TYPES];
static int type_relation_count = 0;

void __lpl_exception_register_type(const char* child, const char* parent) {
    if (type_relation_count < LPL_MAX_EXCEPTION_TYPES) {
        type_relations[type_relation_count].child = child;
        type_relations[type_relation_count].parent = parent;
        type_relation_count++;
    }
}

void* __lpl_try_enter(void) {
    exception_stack_top++;
    if (exception_stack_top >= LPL_MAX_EXCEPTION_HANDLERS) {
        fprintf(stderr, "fatal: exception handler stack overflow\n");
        abort();
    }
    return (void*)&exception_stack[exception_stack_top].env;
}

void __lpl_try_leave(void) {
    if (exception_stack_top >= 0) {
        exception_stack_top--;
    }
}

void __lpl_throw(void* exception, const char* type_name) {
    current_exception = exception;
    current_exception_type = type_name;
    if (exception_stack_top < 0) {
        fprintf(stderr, "fatal: unhandled exception of type '%s'\n",
                type_name ? type_name : "unknown");
        abort();
    }
    jmp_buf* env = &exception_stack[exception_stack_top].env;
    exception_stack_top--;
    longjmp(*env, 1);
}

void* __lpl_exception_current(void) {
    return current_exception;
}

const char* __lpl_exception_type(void) {
    return current_exception_type;
}

int __lpl_exception_is_type(const char* target_type) {
    if (!current_exception_type || !target_type) return 0;
    /* Walk up the type hierarchy */
    const char* cur = current_exception_type;
    while (cur) {
        if (strcmp(cur, target_type) == 0) return 1;
        /* Find parent of cur */
        const char* parent = NULL;
        for (int i = 0; i < type_relation_count; i++) {
            if (strcmp(type_relations[i].child, cur) == 0) {
                parent = type_relations[i].parent;
                break;
            }
        }
        cur = parent;
    }
    return 0;
}

void __lpl_exception_clear(void) {
    current_exception = NULL;
    current_exception_type = NULL;
}

/* ============================================================
 * Program arguments (argc/argv storage)
 * ============================================================ */

static int _lpl_argc = 0;
static char** _lpl_argv = NULL;

void __lpl_system_set_args(int argc, char** argv) {
    _lpl_argc = argc;
    _lpl_argv = argv;
}

int __lpl_get_argc(void) {
    return _lpl_argc;
}

char** __lpl_get_argv(void) {
    return _lpl_argv;
}

/* ============================================================
 * Runtime lifecycle
 * ============================================================ */

void __lpl_runtime_init(void) {
    /* Future: set up allocator, console streams, static initializers */
}

void __lpl_runtime_shutdown(void) {
    /* Future: run global destructors, final cleanup */
    fflush(stdout);
    fflush(stderr);
}