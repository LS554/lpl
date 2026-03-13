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

// lpl_memory — Memory allocation and manipulation helpers
// Thin C wrappers called by stdlib/memory.lpl
// Library: linked into liblplrt.a

#include <stdlib.h>
#include <string.h>

void* __lpl_memory_alloc(int64_t count, int64_t size) {
    if (count <= 0 || size <= 0) return NULL;
    return calloc((size_t)count, (size_t)size);
}

void* __lpl_memory_realloc(void* ptr, int64_t size) {
    if (size <= 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, (size_t)size);
}

void __lpl_memory_copy(void* dest, void* src, int64_t size) {
    if (dest && src && size > 0) {
        memcpy(dest, src, (size_t)size);
    }
}

void __lpl_memory_set(void* dest, int32_t value, int64_t size) {
    if (dest && size > 0) {
        memset(dest, value, (size_t)size);
    }
}

void __lpl_memory_zero(void* dest, int64_t size) {
    if (dest && size > 0) {
        memset(dest, 0, (size_t)size);
    }
}

int32_t __lpl_memory_compare(void* a, void* b, int64_t size) {
    if (!a || !b || size <= 0) return 0;
    return (int32_t)memcmp(a, b, (size_t)size);
}
