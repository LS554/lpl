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

// lpl_system — System utilities platform helpers
// Thin C wrappers called by stdlib/system.lpl
// Library: linked into liblplrt.a

#include "../lplrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

extern int __lpl_get_argc(void);
extern char** __lpl_get_argv(void);

void __lpl_system_exit(int32_t code) {
    exit(code);
}

int32_t __lpl_system_argc(void) {
    return (int32_t)__lpl_get_argc();
}

void __lpl_system_argv(LPLString* result, int32_t index) {
    int argc = __lpl_get_argc();
    char** argv = __lpl_get_argv();
    if (index < 0 || index >= argc || !argv) {
        __lpl_string_create(result, "", 0);
        return;
    }
    int64_t len = (int64_t)strlen(argv[index]);
    __lpl_string_create(result, argv[index], len);
}

void __lpl_system_getenv(LPLString* result, LPLString* name) {
    if (!name || !name->data) {
        __lpl_string_create(result, "", 0);
        return;
    }
    char* cname = (char*)malloc((size_t)name->length + 1);
    memcpy(cname, name->data, (size_t)name->length);
    cname[name->length] = '\0';
    const char* val = getenv(cname);
    free(cname);
    if (val) {
        __lpl_string_create(result, val, (int64_t)strlen(val));
    } else {
        __lpl_string_create(result, "", 0);
    }
}

int64_t __lpl_system_currentTimeMillis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
}

void __lpl_system_sleep(int32_t milliseconds) {
    if (milliseconds > 0) {
        usleep((useconds_t)milliseconds * 1000);
    }
}