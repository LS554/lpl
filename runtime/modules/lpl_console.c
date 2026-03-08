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

// lpl_console — Console I/O platform helpers
// Thin C wrappers called by stdlib/console.lpl
// Library: linked into liblplrt.a

#include "../lplrt.h"
#include <stdio.h>

void __lpl_console_print(LPLString* message) {
    if (message && message->data) {
        fwrite(message->data, 1, (size_t)message->length, stdout);
    }
}

void __lpl_console_println(LPLString* message) {
    if (message && message->data) {
        fwrite(message->data, 1, (size_t)message->length, stdout);
    }
    fputc('\n', stdout);
}

void __lpl_console_print_int(int32_t value) {
    printf("%d", value);
}

void __lpl_console_print_float(double value) {
    printf("%g", value);
}