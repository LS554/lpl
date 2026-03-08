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

// lpl_collections — Collections runtime implementation
// Linked when: include <collections.lph>;
// Library: liblpl_collections.a
//
// Provides runtime backing for generic List<T> and Map<K,V> classes.
// Each supported instantiation has mangled entry points matching the
// compiler's monomorphized names.  The class struct layout is:
//
//   { int64_t data }     — pointer to the internal C data structure
//
// Every method receives void* self (pointer to the class struct).
// Struct-typed parameters (e.g. LPLString) are passed by pointer.
// Struct-typed returns use an sret pointer inserted after self.

#include "../lplrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================
// Internal data structures
// ============================================================

typedef struct {
    int32_t* data;
    int32_t  size;
    int32_t  capacity;
} IntListData;

typedef struct {
    LPLString* data;   // array of LPLString values
    int32_t    size;
    int32_t    capacity;
} StringListData;

typedef struct IntMapEntry {
    char*    key;
    int32_t  value;
    struct IntMapEntry* next;
} IntMapEntry;

typedef struct {
    IntMapEntry** buckets;
    int32_t       bucketCount;
    int32_t       size;
} IntMapData;

typedef struct StrMapEntry {
    char*     key;
    LPLString value;
    struct StrMapEntry* next;
} StrMapEntry;

typedef struct {
    StrMapEntry** buckets;
    int32_t       bucketCount;
    int32_t       size;
} StrMapData;

static uint32_t hash_str(const char* s) {
    uint32_t h = 0;
    for (; *s; s++) h = h * 31 + (uint32_t)*s;
    return h;
}

// Helper: get data pointer from self
static inline int64_t* data_of(void* self) { return (int64_t*)self; }

// ============================================================
// List<int>    — mangled prefix: 9ListIintE
// struct: { i64 data }  where data -> IntListData*
// ============================================================

void _LPL9ListIintE4init(void* self) {
    IntListData* d = (IntListData*)calloc(1, sizeof(IntListData));
    d->capacity = 16;
    d->data = (int32_t*)malloc(16 * sizeof(int32_t));
    *data_of(self) = (int64_t)(uintptr_t)d;
}

void _LPL9ListIintE7destroy(void* self) {
    IntListData* d = (IntListData*)(uintptr_t)*data_of(self);
    if (d) { free(d->data); free(d); *data_of(self) = 0; }
}

void _LPL9ListIintE3add(void* self, int32_t value) {
    IntListData* d = (IntListData*)(uintptr_t)*data_of(self);
    if (!d) return;
    if (d->size >= d->capacity) {
        d->capacity *= 2;
        d->data = (int32_t*)realloc(d->data, (size_t)d->capacity * sizeof(int32_t));
    }
    d->data[d->size++] = value;
}

int32_t _LPL9ListIintE3get(void* self, int32_t index) {
    IntListData* d = (IntListData*)(uintptr_t)*data_of(self);
    if (!d || index < 0 || index >= d->size) return 0;
    return d->data[index];
}

void _LPL9ListIintE3set(void* self, int32_t index, int32_t value) {
    IntListData* d = (IntListData*)(uintptr_t)*data_of(self);
    if (!d || index < 0 || index >= d->size) return;
    d->data[index] = value;
}

void _LPL9ListIintE8removeAt(void* self, int32_t index) {
    IntListData* d = (IntListData*)(uintptr_t)*data_of(self);
    if (!d || index < 0 || index >= d->size) return;
    memmove(&d->data[index], &d->data[index + 1],
            (size_t)(d->size - index - 1) * sizeof(int32_t));
    d->size--;
}

int32_t _LPL9ListIintE4size(void* self) {
    IntListData* d = (IntListData*)(uintptr_t)*data_of(self);
    return d ? d->size : 0;
}

// ============================================================
// List<string>  — mangled prefix: 12ListIstringE
// String params passed as LPLString*.
// String returns via sret pointer (2nd param after self).
// ============================================================

void _LPL12ListIstringE4init(void* self) {
    StringListData* d = (StringListData*)calloc(1, sizeof(StringListData));
    d->capacity = 16;
    d->data = (LPLString*)calloc(16, sizeof(LPLString));
    *data_of(self) = (int64_t)(uintptr_t)d;
}

void _LPL12ListIstringE7destroy(void* self) {
    StringListData* d = (StringListData*)(uintptr_t)*data_of(self);
    if (d) {
        for (int32_t i = 0; i < d->size; i++) {
            __lpl_string_destroy(&d->data[i]);
        }
        free(d->data);
        free(d);
        *data_of(self) = 0;
    }
}

// add(string value) — value is LPLString*
void _LPL12ListIstringE3add(void* self, LPLString* value) {
    StringListData* d = (StringListData*)(uintptr_t)*data_of(self);
    if (!d || !value) return;
    if (d->size >= d->capacity) {
        d->capacity *= 2;
        d->data = (LPLString*)realloc(d->data, (size_t)d->capacity * sizeof(LPLString));
    }
    // Copy the string
    __lpl_string_create(&d->data[d->size], value->data, value->length);
    d->size++;
}

// get(int index) -> string — sret: void(self, LPLString* result, int32_t index)
void _LPL12ListIstringE3get(void* self, LPLString* result, int32_t index) {
    StringListData* d = (StringListData*)(uintptr_t)*data_of(self);
    if (!d || index < 0 || index >= d->size) {
        memset(result, 0, sizeof(LPLString));
        return;
    }
    // Return a copy
    __lpl_string_create(result, d->data[index].data, d->data[index].length);
}

// set(int index, string value) — value is LPLString*
void _LPL12ListIstringE3set(void* self, int32_t index, LPLString* value) {
    StringListData* d = (StringListData*)(uintptr_t)*data_of(self);
    if (!d || !value || index < 0 || index >= d->size) return;
    __lpl_string_destroy(&d->data[index]);
    __lpl_string_create(&d->data[index], value->data, value->length);
}

void _LPL12ListIstringE8removeAt(void* self, int32_t index) {
    StringListData* d = (StringListData*)(uintptr_t)*data_of(self);
    if (!d || index < 0 || index >= d->size) return;
    __lpl_string_destroy(&d->data[index]);
    memmove(&d->data[index], &d->data[index + 1],
            (size_t)(d->size - index - 1) * sizeof(LPLString));
    d->size--;
}

int32_t _LPL12ListIstringE4size(void* self) {
    StringListData* d = (StringListData*)(uintptr_t)*data_of(self);
    return d ? d->size : 0;
}

// ============================================================
// Map<string, int>  — mangled prefix: 15MapIstringCintE
// ============================================================

void _LPL15MapIstringCintE4init(void* self) {
    IntMapData* d = (IntMapData*)calloc(1, sizeof(IntMapData));
    d->bucketCount = 64;
    d->buckets = (IntMapEntry**)calloc(64, sizeof(IntMapEntry*));
    *data_of(self) = (int64_t)(uintptr_t)d;
}

void _LPL15MapIstringCintE7destroy(void* self) {
    IntMapData* d = (IntMapData*)(uintptr_t)*data_of(self);
    if (!d) return;
    for (int32_t i = 0; i < d->bucketCount; i++) {
        IntMapEntry* e = d->buckets[i];
        while (e) {
            IntMapEntry* next = e->next;
            free(e->key);
            free(e);
            e = next;
        }
    }
    free(d->buckets);
    free(d);
    *data_of(self) = 0;
}

// put(string key, int value)
void _LPL15MapIstringCintE3put(void* self, LPLString* key, int32_t value) {
    IntMapData* d = (IntMapData*)(uintptr_t)*data_of(self);
    if (!d || !key || !key->data) return;
    uint32_t idx = hash_str(key->data) % (uint32_t)d->bucketCount;
    IntMapEntry* e = d->buckets[idx];
    while (e) {
        if (strcmp(e->key, key->data) == 0) { e->value = value; return; }
        e = e->next;
    }
    e = (IntMapEntry*)malloc(sizeof(IntMapEntry));
    e->key = (char*)malloc((size_t)key->length + 1);
    memcpy(e->key, key->data, (size_t)key->length);
    e->key[key->length] = '\0';
    e->value = value;
    e->next = d->buckets[idx];
    d->buckets[idx] = e;
    d->size++;
}

// get(string key) -> int
int32_t _LPL15MapIstringCintE3get(void* self, LPLString* key) {
    IntMapData* d = (IntMapData*)(uintptr_t)*data_of(self);
    if (!d || !key || !key->data) return 0;
    uint32_t idx = hash_str(key->data) % (uint32_t)d->bucketCount;
    IntMapEntry* e = d->buckets[idx];
    while (e) {
        if (strcmp(e->key, key->data) == 0) return e->value;
        e = e->next;
    }
    return 0;
}

// containsKey(string key) -> bool
int8_t _LPL15MapIstringCintE11containsKey(void* self, LPLString* key) {
    IntMapData* d = (IntMapData*)(uintptr_t)*data_of(self);
    if (!d || !key || !key->data) return 0;
    uint32_t idx = hash_str(key->data) % (uint32_t)d->bucketCount;
    IntMapEntry* e = d->buckets[idx];
    while (e) {
        if (strcmp(e->key, key->data) == 0) return 1;
        e = e->next;
    }
    return 0;
}

// remove(string key)
void _LPL15MapIstringCintE6remove(void* self, LPLString* key) {
    IntMapData* d = (IntMapData*)(uintptr_t)*data_of(self);
    if (!d || !key || !key->data) return;
    uint32_t idx = hash_str(key->data) % (uint32_t)d->bucketCount;
    IntMapEntry** pp = &d->buckets[idx];
    while (*pp) {
        if (strcmp((*pp)->key, key->data) == 0) {
            IntMapEntry* e = *pp;
            *pp = e->next;
            free(e->key); free(e);
            d->size--;
            return;
        }
        pp = &(*pp)->next;
    }
}

int32_t _LPL15MapIstringCintE4size(void* self) {
    IntMapData* d = (IntMapData*)(uintptr_t)*data_of(self);
    return d ? d->size : 0;
}

// ============================================================
// Map<string, string> — mangled prefix: 18MapIstringCstringE
// ============================================================

void _LPL18MapIstringCstringE4init(void* self) {
    StrMapData* d = (StrMapData*)calloc(1, sizeof(StrMapData));
    d->bucketCount = 64;
    d->buckets = (StrMapEntry**)calloc(64, sizeof(StrMapEntry*));
    *data_of(self) = (int64_t)(uintptr_t)d;
}

void _LPL18MapIstringCstringE7destroy(void* self) {
    StrMapData* d = (StrMapData*)(uintptr_t)*data_of(self);
    if (!d) return;
    for (int32_t i = 0; i < d->bucketCount; i++) {
        StrMapEntry* e = d->buckets[i];
        while (e) {
            StrMapEntry* next = e->next;
            free(e->key);
            __lpl_string_destroy(&e->value);
            free(e);
            e = next;
        }
    }
    free(d->buckets);
    free(d);
    *data_of(self) = 0;
}

// put(string key, string value)
void _LPL18MapIstringCstringE3put(void* self, LPLString* key, LPLString* value) {
    StrMapData* d = (StrMapData*)(uintptr_t)*data_of(self);
    if (!d || !key || !key->data || !value) return;
    uint32_t idx = hash_str(key->data) % (uint32_t)d->bucketCount;
    StrMapEntry* e = d->buckets[idx];
    while (e) {
        if (strcmp(e->key, key->data) == 0) {
            __lpl_string_destroy(&e->value);
            __lpl_string_create(&e->value, value->data, value->length);
            return;
        }
        e = e->next;
    }
    e = (StrMapEntry*)malloc(sizeof(StrMapEntry));
    e->key = (char*)malloc((size_t)key->length + 1);
    memcpy(e->key, key->data, (size_t)key->length);
    e->key[key->length] = '\0';
    __lpl_string_create(&e->value, value->data, value->length);
    e->next = d->buckets[idx];
    d->buckets[idx] = e;
    d->size++;
}

// get(string key) -> string — sret: void(self, LPLString* result, LPLString* key)
void _LPL18MapIstringCstringE3get(void* self, LPLString* result, LPLString* key) {
    StrMapData* d = (StrMapData*)(uintptr_t)*data_of(self);
    if (!d || !key || !key->data) {
        memset(result, 0, sizeof(LPLString));
        return;
    }
    uint32_t idx = hash_str(key->data) % (uint32_t)d->bucketCount;
    StrMapEntry* e = d->buckets[idx];
    while (e) {
        if (strcmp(e->key, key->data) == 0) {
            __lpl_string_create(result, e->value.data, e->value.length);
            return;
        }
        e = e->next;
    }
    memset(result, 0, sizeof(LPLString));
}

// containsKey(string key) -> bool
int8_t _LPL18MapIstringCstringE11containsKey(void* self, LPLString* key) {
    StrMapData* d = (StrMapData*)(uintptr_t)*data_of(self);
    if (!d || !key || !key->data) return 0;
    uint32_t idx = hash_str(key->data) % (uint32_t)d->bucketCount;
    StrMapEntry* e = d->buckets[idx];
    while (e) {
        if (strcmp(e->key, key->data) == 0) return 1;
        e = e->next;
    }
    return 0;
}

// remove(string key)
void _LPL18MapIstringCstringE6remove(void* self, LPLString* key) {
    StrMapData* d = (StrMapData*)(uintptr_t)*data_of(self);
    if (!d || !key || !key->data) return;
    uint32_t idx = hash_str(key->data) % (uint32_t)d->bucketCount;
    StrMapEntry** pp = &d->buckets[idx];
    while (*pp) {
        if (strcmp((*pp)->key, key->data) == 0) {
            StrMapEntry* e = *pp;
            *pp = e->next;
            free(e->key);
            __lpl_string_destroy(&e->value);
            free(e);
            d->size--;
            return;
        }
        pp = &(*pp)->next;
    }
}

int32_t _LPL18MapIstringCstringE4size(void* self) {
    StrMapData* d = (StrMapData*)(uintptr_t)*data_of(self);
    return d ? d->size : 0;
}