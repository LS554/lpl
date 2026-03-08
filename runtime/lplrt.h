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

#ifndef LPLRT_H
#define LPLRT_H

#include <stdint.h>
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * String type
 * ============================================================ */

typedef struct {
    char*    data;
    int64_t  length;
    int64_t  capacity;
} LPLString;

/* Create a string from a C string literal (copies data) */
void __lpl_string_create(LPLString* out, const char* data, int64_t len);

/* Destroy a string (frees heap buffer) */
void __lpl_string_destroy(LPLString* s);

/* Concatenate two strings into result */
void __lpl_string_concat(LPLString* result, const LPLString* a, const LPLString* b);

/* Convert int to string */
void __lpl_int_to_string(LPLString* out, int32_t val);

/* ============================================================
 * Exception handling
 * ============================================================ */

#define LPL_MAX_EXCEPTION_HANDLERS 256

typedef struct {
    jmp_buf env;
} LPLExceptionFrame;

/* Push a new exception handler frame; returns pointer to jmp_buf for setjmp */
void* __lpl_try_enter(void);

/* Pop the current handler frame (called at end of successful try) */
void __lpl_try_leave(void);

/* Throw an exception: stores object + type name, longjmps to nearest handler */
void __lpl_throw(void* exception, const char* type_name);

/* Get the currently thrown exception object */
void* __lpl_exception_current(void);

/* Get the type name of the currently thrown exception */
const char* __lpl_exception_type(void);

/* Check if thrown exception is of a given type (supports inheritance via type list) */
int __lpl_exception_is_type(const char* target_type);

/* Clear the current exception after handling */
void __lpl_exception_clear(void);

/* Register a parent type relationship for exception hierarchy */
void __lpl_exception_register_type(const char* child, const char* parent);

/* ============================================================
 * Runtime lifecycle
 * ============================================================ */

void __lpl_runtime_init(void);
void __lpl_runtime_shutdown(void);

/* Store argc/argv for System.argc()/System.argv() */
void __lpl_system_set_args(int argc, char** argv);
int __lpl_get_argc(void);
char** __lpl_get_argv(void);

#ifdef __cplusplus
}
#endif

#endif /* LPLRT_H */