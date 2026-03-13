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

#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include "ast.h"

// Resolves include directives in a Program.
// Parses .lph header files, merges their declarations, and
// tracks which standard library modules are used for linking.
class IncludeResolver {
public:
    // searchPaths: directories to search for system includes (e.g. stdlib/)
    // sourceDir: directory of the source file being compiled (for local includes)
    IncludeResolver(const std::vector<std::string>& searchPaths,
                    const std::string& sourceDir);

    // Set extra header search paths (from -I flags) used when resolving
    // #include directives inside extern "C" blocks.
    void setExtraIncludePaths(const std::vector<std::string>& paths);

    // Process all IncludeDecl nodes in the program.
    // Replaces them with the declarations from the included files.
    // Also resolves any #include directives inside extern "C"/"C++" blocks by
    // invoking clang to extract function signatures.
    // Returns false on error.
    bool resolve(Program& prog);

    bool hasErrors() const { return !errors.empty(); }
    const std::vector<std::string>& getErrors() const { return errors; }

    // Which stdlib modules were included (e.g. "console", "math")
    // Used by the linker to decide which runtime libs to link.
    const std::unordered_set<std::string>& usedModules() const { return modules; }

private:
    std::vector<std::string> searchPaths;
    std::string sourceDir;
    std::vector<std::string> extraIncludePaths; // from -I flags
    std::vector<std::string> errors;
    std::unordered_set<std::string> included; // already-included paths (dedup)
    std::unordered_set<std::string> modules;  // stdlib module names

    // Find a file given the include path and whether it's a system include
    std::string findFile(const std::string& path, bool isSystem);

    // Parse a .lph file and return its declarations
    std::vector<DeclPtr> parseHeader(const std::string& resolvedPath,
                                     const std::string& originalPath);

    // Resolve #include directives inside all extern blocks in the program.
    // Calls clang to extract C function signatures and adds them to the block.
    void resolveExternCIncludes(Program& prog);

    // Parse a C header file using clang -ast-dump=json and return extern
    // function declarations that can be imported into the LPL symbol table.
    std::vector<ExternFuncDecl> parseCHeader(const std::string& path, bool isSystem);
};