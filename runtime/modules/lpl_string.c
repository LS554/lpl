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

// lpl_string — String utilities platform helpers
// Thin C wrappers called by stdlib/string.lpl
// Library: linked into liblplrt.a

#include "../lplrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int32_t __lpl_strings_length(LPLString* s) {
    return (int32_t)(s ? s->length : 0);
}

void __lpl_strings_substring(LPLString* result, LPLString* s, int32_t start, int32_t end) {
    if (!s || !s->data || start < 0 || end < start || start >= s->length) {
        __lpl_string_create(result, "", 0);
        return;
    }
    if (end > s->length) end = (int32_t)s->length;
    __lpl_string_create(result, s->data + start, end - start);
}

int32_t __lpl_strings_indexOf(LPLString* s, LPLString* needle) {
    if (!s || !s->data || !needle || !needle->data) return -1;
    char* found = strstr(s->data, needle->data);
    if (!found) return -1;
    return (int32_t)(found - s->data);
}

int8_t __lpl_strings_contains(LPLString* s, LPLString* needle) {
    return __lpl_strings_indexOf(s, needle) >= 0 ? 1 : 0;
}

void __lpl_strings_toUpper(LPLString* result, LPLString* s) {
    if (!s || !s->data) {
        __lpl_string_create(result, "", 0);
        return;
    }
    char* buf = (char*)malloc((size_t)s->length + 1);
    for (int64_t i = 0; i < s->length; i++) {
        buf[i] = (char)toupper((unsigned char)s->data[i]);
    }
    buf[s->length] = '\0';
    __lpl_string_create(result, buf, s->length);
    free(buf);
}

void __lpl_strings_toLower(LPLString* result, LPLString* s) {
    if (!s || !s->data) {
        __lpl_string_create(result, "", 0);
        return;
    }
    char* buf = (char*)malloc((size_t)s->length + 1);
    for (int64_t i = 0; i < s->length; i++) {
        buf[i] = (char)tolower((unsigned char)s->data[i]);
    }
    buf[s->length] = '\0';
    __lpl_string_create(result, buf, s->length);
    free(buf);
}

void __lpl_strings_trim(LPLString* result, LPLString* s) {
    if (!s || !s->data || s->length == 0) {
        __lpl_string_create(result, "", 0);
        return;
    }
    int64_t start = 0, end = s->length;
    while (start < end && isspace((unsigned char)s->data[start])) start++;
    while (end > start && isspace((unsigned char)s->data[end - 1])) end--;
    __lpl_string_create(result, s->data + start, end - start);
}

void __lpl_strings_fromInt(LPLString* result, int32_t value) {
    __lpl_int_to_string(result, value);
}

void __lpl_strings_fromFloat(LPLString* result, double value) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%g", value);
    __lpl_string_create(result, buf, len);
}

void __lpl_strings_fromBool(LPLString* result, int8_t value) {
    if (value) {
        __lpl_string_create(result, "true", 4);
    } else {
        __lpl_string_create(result, "false", 5);
    }
}

int32_t __lpl_strings_toInt(LPLString* s) {
    if (!s || !s->data) return 0;
    return (int32_t)atoi(s->data);
}

double __lpl_strings_toFloat(LPLString* s) {
    if (!s || !s->data) return 0.0;
    return atof(s->data);
}

int8_t __lpl_strings_startsWith(LPLString* s, LPLString* prefix) {
    if (!s || !prefix || prefix->length > s->length) return 0;
    return memcmp(s->data, prefix->data, (size_t)prefix->length) == 0 ? 1 : 0;
}

int8_t __lpl_strings_endsWith(LPLString* s, LPLString* suffix) {
    if (!s || !suffix || suffix->length > s->length) return 0;
    return memcmp(s->data + s->length - suffix->length, suffix->data,
                  (size_t)suffix->length) == 0 ? 1 : 0;
}

void __lpl_strings_replace(LPLString* result, LPLString* s, LPLString* old, LPLString* rep) {
    if (!s || !s->data || !old || !old->data || old->length == 0) {
        if (s) __lpl_string_create(result, s->data, s->length);
        else __lpl_string_create(result, "", 0);
        return;
    }
    int count = 0;
    const char* p = s->data;
    while ((p = strstr(p, old->data)) != NULL) {
        count++;
        p += old->length;
    }
    if (count == 0) {
        __lpl_string_create(result, s->data, s->length);
        return;
    }
    int64_t newLen = s->length + (int64_t)count * (rep->length - old->length);
    char* buf = (char*)malloc((size_t)newLen + 1);
    char* dst = buf;
    p = s->data;
    const char* prev = p;
    while ((p = strstr(prev, old->data)) != NULL) {
        memcpy(dst, prev, (size_t)(p - prev));
        dst += (p - prev);
        if (rep->data && rep->length > 0) {
            memcpy(dst, rep->data, (size_t)rep->length);
            dst += rep->length;
        }
        prev = p + old->length;
    }
    int64_t remaining = s->length - (prev - s->data);
    memcpy(dst, prev, (size_t)remaining);
    dst += remaining;
    *dst = '\0';
    __lpl_string_create(result, buf, newLen);
    free(buf);
}

void __lpl_strings_charAt(LPLString* result, LPLString* s, int32_t index) {
    if (!s || !s->data || index < 0 || index >= s->length) {
        __lpl_string_create(result, "", 0);
        return;
    }
    __lpl_string_create(result, s->data + index, 1);
}