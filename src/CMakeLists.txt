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

set(LPLC_SOURCES
    main.cpp
    lexer.cpp
    parser.cpp
    resolver.cpp
    sema.cpp
    codegen.cpp
)

add_executable(lplc ${LPLC_SOURCES})

llvm_map_components_to_libnames(LLVM_LIBS
    core
    support
    irreader
    passes
    native
    nativecodegen
    orcjit
)

target_link_libraries(lplc PRIVATE ${LLVM_LIBS})
target_include_directories(lplc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
