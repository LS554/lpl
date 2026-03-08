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

// lpl_sync — Synchronization platform helpers
// Thin C wrappers called by stdlib/sync.lpl
// Library: linked into liblplrt.a

#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>

int64_t __lpl_mutex_create(void) {
    pthread_mutex_t* mtx = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(mtx, NULL);
    return (int64_t)(uintptr_t)mtx;
}

void __lpl_mutex_destroy(int64_t handle) {
    if (handle) {
        pthread_mutex_t* mtx = (pthread_mutex_t*)(uintptr_t)handle;
        pthread_mutex_destroy(mtx);
        free(mtx);
    }
}

void __lpl_mutex_lock(int64_t handle) {
    if (handle) {
        pthread_mutex_t* mtx = (pthread_mutex_t*)(uintptr_t)handle;
        pthread_mutex_lock(mtx);
    }
}

void __lpl_mutex_unlock(int64_t handle) {
    if (handle) {
        pthread_mutex_t* mtx = (pthread_mutex_t*)(uintptr_t)handle;
        pthread_mutex_unlock(mtx);
    }
}

int8_t __lpl_mutex_tryLock(int64_t handle) {
    if (handle) {
        pthread_mutex_t* mtx = (pthread_mutex_t*)(uintptr_t)handle;
        return pthread_mutex_trylock(mtx) == 0 ? 1 : 0;
    }
    return 0;
}