#include "../lplrt.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// Helper: null-terminate an LPLString for libc calls
char* _to_cstr(LPLString* s) {
    if (!s || !s->data) return NULL;
    char* c = (char*)malloc((size_t)s->length + 1);
    memcpy(c, s->data, (size_t)s->length);
    c[s->length] = '\0';
    return c;
}
