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

// lpl_files — File I/O platform helpers
// Thin C wrappers called by stdlib/files.lpl
// Library: linked into liblplrt.a

#include "../lplrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "cstr.h"

void __lpl_files_read_file(LPLString* result, LPLString* path) {
    char* cpath = _to_cstr(path);
    if (!cpath) { __lpl_string_create(result, "", 0); return; }
    FILE* fp = fopen(cpath, "rb");
    free(cpath);
    if (!fp) { __lpl_string_create(result, "", 0); return; }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)sz + 1);
    if (buf) {
        size_t rd = fread(buf, 1, (size_t)sz, fp);
        buf[rd] = '\0';
        __lpl_string_create(result, buf, (int64_t)rd);
        free(buf);
    } else {
        __lpl_string_create(result, "", 0);
    }
    fclose(fp);
}

#define MAX_LINE_READERS 16

static struct {
    char* path;
    FILE* fp;
    char* buf;
    long  bufsz;
} _readers[MAX_LINE_READERS];
static int _reader_count = 0;

static int _find_reader(const char* path) {
    for (int i = 0; i < _reader_count; i++) {
        if (_readers[i].path && strcmp(_readers[i].path, path) == 0)
            return i;
    }
    return -1;
}

static void _close_reader(int idx) {
    fclose(_readers[idx].fp);
    free(_readers[idx].buf);
    free(_readers[idx].path);
    _readers[idx] = _readers[--_reader_count];
}

void __lpl_files_read_by_line(LPLString* result, LPLString* path) {
    char* cpath = _to_cstr(path);
    if (!cpath) { __lpl_string_create(result, "", 0); return; }

    int idx = _find_reader(cpath);

    // Open a new reader for this path
    if (idx < 0) {
        if (_reader_count >= MAX_LINE_READERS) {
            free(cpath);
            __lpl_string_create(result, "", 0);
            return;
        }
        FILE* fp = fopen(cpath, "r");
        if (!fp) { free(cpath); __lpl_string_create(result, "", 0); return; }
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        rewind(fp);
        char* buf = (char*)malloc((size_t)sz + 1);
        if (!buf) { fclose(fp); free(cpath); __lpl_string_create(result, "", 0); return; }
        idx = _reader_count++;
        _readers[idx].path = cpath;
        _readers[idx].fp = fp;
        _readers[idx].buf = buf;
        _readers[idx].bufsz = sz;
    } else {
        free(cpath);
    }

    // Read next line
    int ci, len = 0;
    while ((ci = fgetc(_readers[idx].fp)) != '\n' && ci != EOF) {
        _readers[idx].buf[len++] = (char)ci;
    }

    // EOF — close and clean up
    if (ci == EOF && len == 0) {
        __lpl_string_create(result, "", 0);
        _close_reader(idx);
        return;
    }

    __lpl_string_create(result, _readers[idx].buf, len);
}

void __lpl_files_write_file(LPLString* path, LPLString* content) {
    char* cpath = _to_cstr(path);
    if (!cpath) return;
    FILE* fp = fopen(cpath, "wb");
    free(cpath);
    if (!fp) return;
    if (content && content->data && content->length > 0) {
        fwrite(content->data, 1, (size_t)content->length, fp);
    }
    fclose(fp);
}

void __lpl_files_append_file(LPLString* path, LPLString* content) {
    char* cpath = _to_cstr(path);
    if (!cpath) return;
    FILE* fp = fopen(cpath, "ab");
    free(cpath);
    if (!fp) return;
    if (content && content->data && content->length > 0) {
        fwrite(content->data, 1, (size_t)content->length, fp);
    }
    fclose(fp);
}

int8_t __lpl_files_file_exists(LPLString* path) {
    char* cpath = _to_cstr(path);
    if (!cpath) return 0;
    int result = (access(cpath, F_OK) == 0) ? 1 : 0;
    free(cpath);
    return (int8_t)result;
}

int64_t __lpl_files_file_size(LPLString* path) {
    char* cpath = _to_cstr(path);
    if (!cpath) return 0;
    struct stat st;
    int64_t result = 0;
    if (stat(cpath, &st) == 0) {
        result = (int64_t)st.st_size;
    }
    free(cpath);
    return result;
}

void __lpl_files_remove_file(LPLString* path) {
    char* cpath = _to_cstr(path);
    if (!cpath) return;
    unlink(cpath);
    free(cpath);
}