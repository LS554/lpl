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

// lpl_math — Math platform helpers
// Only random() needs C (for seeding state). Everything else is called
// directly from stdlib/math.lpl via libc extern "C" declarations.
// Library: linked into liblplrt.a

#include <stdlib.h>
#include <stdint.h>
#include <time.h>

static int _lpl_math_seeded = 0;

double __lpl_math_random(void) {
    if (!_lpl_math_seeded) {
        srand((unsigned)time(NULL));
        _lpl_math_seeded = 1;
    }
    return (double)rand() / (double)RAND_MAX;
}