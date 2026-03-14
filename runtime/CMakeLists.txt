# Copyright 2026 London Sheard
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.16)

# Core runtime + all platform C helpers (always linked)
add_library(lplrt STATIC
    lplrt.c
    modules/cstr.c
    modules/lpl_console.c
    modules/lpl_math.c
    modules/lpl_string.c
    modules/lpl_files.c
    modules/lpl_system.c
    modules/lpl_sync.c
    modules/lpl_memory.c
)
target_include_directories(lplrt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lplrt PRIVATE m)
# Size optimization + per-function sections for dead stripping
target_compile_options(lplrt PRIVATE -Os -ffunction-sections -fdata-sections)

# Collections stays as C-only (deep generic specialization in codegen)
add_library(lpl_collections STATIC modules/lpl_collections.c)
target_include_directories(lpl_collections PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(lpl_collections PRIVATE -Os -ffunction-sections -fdata-sections)

# --- Bootstrap: compile .lpl stdlib sources with lplc ---
set(STDLIB_DIR ${CMAKE_SOURCE_DIR}/stdlib)
set(STDLIB_OUT ${CMAKE_CURRENT_BINARY_DIR})

set(STDLIB_MODULES console math string files system sync memory)
set(STDLIB_LIBS)

foreach(mod ${STDLIB_MODULES})
    # Compile .lpl -> .o using lplc
    add_custom_command(
        OUTPUT ${STDLIB_OUT}/${mod}_lpl.o
        COMMAND $<TARGET_FILE:lplc> ${STDLIB_DIR}/${mod}.lpl -Os -c -o ${STDLIB_OUT}/${mod}_lpl.o
        DEPENDS lplc ${STDLIB_DIR}/${mod}.lpl
        COMMENT "Compiling stdlib/${mod}.lpl"
    )
    # Archive .o -> .a
    add_custom_command(
        OUTPUT ${STDLIB_OUT}/liblpl_${mod}.a
        COMMAND ${CMAKE_AR} rcs ${STDLIB_OUT}/liblpl_${mod}.a ${STDLIB_OUT}/${mod}_lpl.o
        DEPENDS ${STDLIB_OUT}/${mod}_lpl.o
        COMMENT "Creating liblpl_${mod}.a"
    )
    list(APPEND STDLIB_LIBS ${STDLIB_OUT}/liblpl_${mod}.a)
endforeach()

add_custom_target(stdlib ALL DEPENDS ${STDLIB_LIBS})

# Install targets
install(TARGETS lplrt lpl_collections DESTINATION lib)
install(FILES lplrt.h DESTINATION include)
