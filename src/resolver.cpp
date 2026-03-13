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
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "llvm/Support/JSON.h"

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

void IncludeResolver::setExtraIncludePaths(const std::vector<std::string>& paths) {
    extraIncludePaths = paths;
}

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

    // Resolve any #include directives inside extern blocks
    resolveExternCIncludes(prog);

    return errors.empty();
}

// ============================================================
// C header resolution helpers
// ============================================================

// Strip C type qualifiers and normalize the type string.
static std::string normalizeCType(std::string t) {
    // Remove qualifiers
    const char* qualifiers[] = {
        "const ", "volatile ", "restrict ", "__restrict ", "__restrict__ ",
        "signed ", "__signed__ ", nullptr
    };
    for (int qi = 0; qualifiers[qi]; qi++) {
        size_t pos;
        while ((pos = t.find(qualifiers[qi])) != std::string::npos)
            t.erase(pos, strlen(qualifiers[qi]));
    }
    // Trim leading/trailing spaces
    size_t s = t.find_first_not_of(' ');
    if (s == std::string::npos) return "";
    t = t.substr(s);
    size_t e = t.find_last_not_of(' ');
    if (e != std::string::npos) t = t.substr(0, e + 1);
    // Collapse multiple spaces
    std::string out;
    bool lastSpace = false;
    for (char c : t) {
        if (c == ' ') { if (!lastSpace) out += ' '; lastSpace = true; }
        else { out += c; lastSpace = false; }
    }
    return out;
}

// Convert a C type string (from clang's qualType) to an LPL TypeSpec.
static TypeSpec cTypeStringToTypeSpec(const std::string& rawCtype) {
    TypeSpec ts;

    // Count and strip trailing '*' (pointer depth)
    std::string t = rawCtype;
    while (!t.empty() && (t.back() == '*' || t.back() == ' ')) {
        if (t.back() == '*') ts.pointerDepth++;
        t.pop_back();
    }
    t = normalizeCType(t);

    // Map base type string to TypeSpec kind
    if (t == "void")                                           ts.kind = TypeSpec::Void;
    else if (t == "char" || t == "unsigned char"
          || t == "uint8_t" || t == "int8_t"
          || t == "__uint8_t" || t == "__int8_t")              ts.kind = TypeSpec::Char;
    else if (t == "short" || t == "short int"
          || t == "unsigned short" || t == "unsigned short int"
          || t == "int16_t" || t == "uint16_t"
          || t == "__int16_t" || t == "__uint16_t")             ts.kind = TypeSpec::Short;
    else if (t == "int" || t == "unsigned int" || t == "unsigned"
          || t == "int32_t" || t == "uint32_t"
          || t == "__int32_t" || t == "__uint32_t")             ts.kind = TypeSpec::Int;
    else if (t == "long" || t == "long int"
          || t == "unsigned long" || t == "unsigned long int"
          || t == "long long" || t == "long long int"
          || t == "unsigned long long" || t == "unsigned long long int"
          || t == "int64_t" || t == "uint64_t"
          || t == "__int64_t" || t == "__uint64_t"
          || t == "size_t" || t == "ssize_t"
          || t == "ptrdiff_t" || t == "intptr_t" || t == "uintptr_t"
          || t == "__darwin_size_t" || t == "__darwin_ssize_t"
          || t == "off_t" || t == "__off_t")                   ts.kind = TypeSpec::Long;
    else if (t == "float")                                     ts.kind = TypeSpec::Float;
    else if (t == "double" || t == "long double")              ts.kind = TypeSpec::Double;
    else if (t == "_Bool" || t == "bool")                      ts.kind = TypeSpec::Bool;
    else {
        // Unknown type: treat as opaque pointer if we already have pointer depth,
        // otherwise use void* as a generic opaque handle
        if (ts.pointerDepth > 0) {
            ts.kind = TypeSpec::Void;
        } else {
            // Struct or typedef we don't know: use void* so it can be passed around
            ts.kind = TypeSpec::Void;
            ts.pointerDepth++;
        }
    }

    return ts;
}

// Extract the return type string from a C function qualType like "int (const char *, ...)"
// by finding the last outermost '(' and taking everything before it.
static std::string extractReturnType(const std::string& funcQualType) {
    int depth = 0;
    for (int i = (int)funcQualType.size() - 1; i >= 0; i--) {
        if (funcQualType[i] == ')') depth++;
        else if (funcQualType[i] == '(') {
            depth--;
            if (depth == 0) {
                std::string ret = funcQualType.substr(0, i);
                while (!ret.empty() && ret.back() == ' ') ret.pop_back();
                return ret;
            }
        }
    }
    return funcQualType;
}

std::vector<ExternFuncDecl> IncludeResolver::parseCHeader(
        const std::string& path, bool isSystem) {
    std::vector<ExternFuncDecl> result;

    // Create a temp C file containing just the include directive
    char tmpFile[] = "/tmp/lpl_cinc_XXXXXX.c";
    int fd = mkstemps(tmpFile, 2);
    if (fd < 0) {
        errors.push_back("error: could not create temporary file for C header processing");
        return result;
    }

    std::string content = isSystem
        ? std::string("#include <") + path + ">\n"
        : std::string("#include \"") + path + "\"\n";
    if (write(fd, content.c_str(), content.size()) < 0) {
        close(fd);
        std::remove(tmpFile);
        errors.push_back("error: could not write temporary file for C header '" + path + "'");
        return result;
    }
    close(fd);

    // Build the clang command to dump the AST as JSON
    // Use -Xclang prefix for Apple clang compatibility
    std::string cmd = "clang -x c -fsyntax-only -Xclang -ast-dump=json";
    for (auto& p : extraIncludePaths) {
        cmd += " -I" + p;
    }
    // For local includes, add the source directory to the search path
    if (!isSystem && !sourceDir.empty()) {
        cmd += " -I" + sourceDir;
    }
    cmd += " " + std::string(tmpFile) + " 2>/dev/null";

    // Capture clang's JSON AST output
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::remove(tmpFile);
        errors.push_back("error: could not run clang to process C header '" + path + "'\n"
                         "       (is clang installed and on PATH?)");
        return result;
    }

    std::ostringstream jsonOut;
    char buf[8192];
    while (fgets(buf, sizeof(buf), pipe)) {
        jsonOut << buf;
    }
    int exitCode = pclose(pipe);
    std::remove(tmpFile);

    if (exitCode != 0) {
        errors.push_back(std::string(isSystem ? "<" : "\"") + path
                         + (isSystem ? ">" : "\"")
                         + ": error: C header not found or could not be processed\n"
                         "         (check header name and add search paths with -I)");
        return result;
    }

    // Parse the JSON AST
    auto jsonVal = llvm::json::parse(jsonOut.str());
    if (!jsonVal) {
        // Not a fatal error — header may have had warnings but still produced output
        errors.push_back("error: could not parse clang AST output for header '" + path + "'");
        return result;
    }

    auto* root = jsonVal->getAsObject();
    if (!root) return result;

    auto* inner = root->getArray("inner");
    if (!inner) return result;

    // Track already-seen names to deduplicate (the AST may have multiple
    // declarations of the same function from re-included headers)
    std::unordered_set<std::string> seen;

    for (auto& item : *inner) {
        auto* decl = item.getAsObject();
        if (!decl) continue;

        auto kind = decl->getString("kind");
        if (!kind || *kind != "FunctionDecl") continue;

        // Skip static functions — they're not linkable externally
        auto storageClass = decl->getString("storageClass");
        if (storageClass && *storageClass == "static") continue;

        // Skip compiler builtins (loc.file starts with '<')
        if (auto* loc = decl->getObject("loc")) {
            auto file = loc->getString("file");
            if (file && !file->empty() && (*file)[0] == '<') continue;
        }

        auto name = decl->getString("name");
        if (!name || name->empty()) continue;

        std::string nameStr = name->str();

        // Skip internal/implementation symbols (leading underscore)
        if (!nameStr.empty() && nameStr[0] == '_') continue;

        // Deduplicate by name
        if (seen.count(nameStr)) continue;
        seen.insert(nameStr);

        // Get function type to extract return type
        auto* typeObj = decl->getObject("type");
        if (!typeObj) continue;
        auto qualType = typeObj->getString("qualType");
        if (!qualType) continue;

        ExternFuncDecl efd;
        efd.name = nameStr;
        efd.linkName = ""; // use declared name as link name

        // Extract and convert return type
        std::string retTypeStr = extractReturnType(qualType->str());
        efd.returnType = cTypeStringToTypeSpec(retTypeStr);

        // Check for variadic in the function type string
        if (qualType->contains("...")) {
            efd.isVariadic = true;
        }

        // Extract parameter types from inner ParmVarDecl nodes
        auto* innerDecls = decl->getArray("inner");
        if (innerDecls) {
            for (auto& paramItem : *innerDecls) {
                auto* paramDecl = paramItem.getAsObject();
                if (!paramDecl) continue;
                auto paramKind = paramDecl->getString("kind");
                if (!paramKind || *paramKind != "ParmVarDecl") continue;

                Param p;
                auto paramName = paramDecl->getString("name");
                p.name = paramName ? paramName->str() : "";

                auto* pTypeObj = paramDecl->getObject("type");
                if (pTypeObj) {
                    auto pQualType = pTypeObj->getString("qualType");
                    if (pQualType) {
                        p.type = cTypeStringToTypeSpec(pQualType->str());
                    }
                }
                efd.params.push_back(std::move(p));
            }
        }

        result.push_back(std::move(efd));
    }

    return result;
}

void IncludeResolver::resolveExternCIncludes(Program& prog) {
    for (auto& decl : prog.declarations) {
        auto ext = std::dynamic_pointer_cast<ExternBlockDecl>(decl);
        if (!ext) continue;
        if (ext->cincludes.empty()) continue;

        // Only process "C" and "C++" convention blocks
        if (ext->convention != "C" && ext->convention != "C++") continue;

        for (auto& inc : ext->cincludes) {
            auto funcs = parseCHeader(inc.path, inc.isSystem);
            if (funcs.empty() && !errors.empty()) {
                // parseCHeader already pushed an error — stop processing this include
                continue;
            }
            // Merge parsed C functions into the block's function list
            ext->functions.insert(ext->functions.end(), funcs.begin(), funcs.end());
        }
    }
}