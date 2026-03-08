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

#include "resolver.h"
#include "lexer.h"
#include "parser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

// Platform-specific path separator
#ifdef _WIN32
static const char kPathSep = '\\';
#else
static const char kPathSep = '/';
#endif

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

// Extract the module name from a header filename: "console.lph" -> "console"
static std::string moduleNameFromPath(const std::string& path) {
    auto pos = path.rfind(kPathSep);
    std::string filename = (pos != std::string::npos) ? path.substr(pos + 1) : path;
    auto dot = filename.rfind('.');
    if (dot != std::string::npos) filename = filename.substr(0, dot);
    return filename;
}

IncludeResolver::IncludeResolver(const std::vector<std::string>& searchPaths,
                                 const std::string& sourceDir)
    : searchPaths(searchPaths), sourceDir(sourceDir) {}

std::string IncludeResolver::findFile(const std::string& path, bool isSystem) {
    if (!isSystem) {
        // Local include: search relative to source file directory
        std::string fullPath = sourceDir + kPathSep + path;
        if (fileExists(fullPath)) return fullPath;
    }

    // System include: search in system search paths
    for (auto& dir : searchPaths) {
        std::string fullPath = dir + kPathSep + path;
        if (fileExists(fullPath)) return fullPath;
    }

    return "";
}

std::vector<DeclPtr> IncludeResolver::parseHeader(const std::string& resolvedPath,
                                                   const std::string& originalPath) {
    std::string source = readFile(resolvedPath);
    if (source.empty()) {
        errors.push_back("error: could not read header file '" + resolvedPath + "'");
        return {};
    }

    Lexer lexer(source, originalPath);
    auto tokens = lexer.tokenize();

    Parser parser(tokens);
    Program headerProg = parser.parse();

    if (parser.hasErrors()) {
        for (auto& err : parser.getErrors()) {
            errors.push_back(err);
        }
        return {};
    }

    // Recursively resolve includes within this header
    std::vector<DeclPtr> result;
    for (auto& decl : headerProg.declarations) {
        if (auto inc = std::dynamic_pointer_cast<IncludeDecl>(decl)) {
            std::string resolved = findFile(inc->path, inc->isSystem);
            if (resolved.empty()) {
                errors.push_back(inc->loc.file + ":" + std::to_string(inc->loc.line)
                                 + ": error: cannot find header '" + inc->path + "'");
                continue;
            }

            if (included.count(resolved)) continue; // already included
            included.insert(resolved);

            if (inc->isSystem) {
                modules.insert(moduleNameFromPath(inc->path));
            }

            auto nested = parseHeader(resolved, inc->path);
            result.insert(result.end(), nested.begin(), nested.end());
        } else {
            // Mark class declarations from headers as extern
            if (auto cls = std::dynamic_pointer_cast<ClassDecl>(decl)) {
                cls->isExtern = true;
            }
            result.push_back(decl);
        }
    }

    return result;
}

bool IncludeResolver::resolve(Program& prog) {
    std::vector<DeclPtr> resolved;

    for (auto& decl : prog.declarations) {
        if (auto inc = std::dynamic_pointer_cast<IncludeDecl>(decl)) {
            std::string path = findFile(inc->path, inc->isSystem);
            if (path.empty()) {
                errors.push_back(inc->loc.file + ":" + std::to_string(inc->loc.line)
                                 + ": error: cannot find header '" + inc->path + "'");
                continue;
            }

            if (included.count(path)) continue; // dedup — harmless double include
            included.insert(path);

            if (inc->isSystem) {
                modules.insert(moduleNameFromPath(inc->path));
            }

            auto decls = parseHeader(path, inc->path);
            resolved.insert(resolved.end(), decls.begin(), decls.end());
        } else {
            resolved.push_back(decl);
        }
    }

    prog.declarations = std::move(resolved);
    return errors.empty();
}