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

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <climits>
#include <libgen.h>
#include <mach-o/dyld.h>

#include "lexer.h"
#include "parser.h"
#include "resolver.h"
#include "sema.h"
#include "codegen.h"

#include "llvm/Support/TargetSelect.h"
#include "llvm/TargetParser/Host.h"

static std::string getCompilerDir() {
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        char real[PATH_MAX];
        if (realpath(path, real)) {
            return std::string(dirname(real));
        }
    }
    return ".";
}

static std::string getStdlibDir() {
    std::string compilerDir = getCompilerDir();
    // Look for stdlib/ relative to compiler binary:
    // build/src/lplc -> build/src/../stdlib or project/stdlib
    std::string candidates[] = {
        compilerDir + "/../stdlib",
        compilerDir + "/../../stdlib",
        compilerDir + "/stdlib",
    };
    for (auto& c : candidates) {
        char resolved[PATH_MAX];
        if (realpath(c.c_str(), resolved)) {
            return std::string(resolved);
        }
    }
    return compilerDir + "/../stdlib";
}

static std::string getSourceDir(const std::string& sourceFile) {
    char path[PATH_MAX];
    strncpy(path, sourceFile.c_str(), sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
    return std::string(dirname(path));
}

static void printUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " <source files> [options]\n"
              << "\nOptions:\n"
              << "  -o <name>           Output file name\n"
              << "  -target <triple>    LLVM target triple\n"
              << "  -O0,-O1,-O2,-O3    Optimization level\n"
              << "  -Os                 Optimize for size\n"
              << "  -Oz                 Optimize aggressively for size\n"
              << "  -g                  Emit debug information\n"
              << "  -emit-llvm          Output LLVM IR (.ll)\n"
              << "  -emit-obj, -c       Output object file only\n"
              << "  -S                  Output assembly\n"
              << "  --dump-tokens       Dump lexer tokens\n"
              << "  --dump-ast          Dump AST (placeholder)\n"
              << "  -h, --help          Show this help\n";
}

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "error: cannot open file '" << path << "'\n";
        return "";
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::vector<std::string> sourceFiles;
    std::string outputName = "a.out";
    std::string targetTriple = llvm::sys::getDefaultTargetTriple();
    bool emitLLVM = false;
    bool emitObj = false;
    bool emitAsm = false;
    bool dumpTokens = false;
    bool dumpAST = false;
    int optLevel = 0;

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            outputName = argv[++i];
        } else if (arg == "-target" && i + 1 < argc) {
            targetTriple = argv[++i];
        } else if (arg == "-O0") { optLevel = 0; }
        else if (arg == "-O1") { optLevel = 1; }
        else if (arg == "-O2") { optLevel = 2; }
        else if (arg == "-O3") { optLevel = 3; }
        else if (arg == "-Os") { optLevel = 4; }
        else if (arg == "-Oz") { optLevel = 5; }
        else if (arg == "-g") { /* debug info - TODO */ }
        else if (arg == "-emit-llvm") { emitLLVM = true; }
        else if (arg == "-emit-obj" || arg == "-c") { emitObj = true; }
        else if (arg == "-S") { emitAsm = true; }
        else if (arg == "--dump-tokens") { dumpTokens = true; }
        else if (arg == "--dump-ast") { dumpAST = true; }
        else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg[0] == '-') {
            std::cerr << "error: unknown option '" << arg << "'\n";
            return 1;
        } else {
            sourceFiles.push_back(arg);
        }
    }

    if (sourceFiles.empty()) {
        std::cerr << "error: no input files\n";
        return 1;
    }

    // Read and concatenate all source files
    std::string allSource;
    std::string primaryFile = sourceFiles[0];
    for (auto& file : sourceFiles) {
        std::string src = readFile(file);
        if (src.empty() && file == primaryFile) return 1;
        allSource += src + "\n";
    }

    // Phase 1: Lex
    Lexer lexer(allSource, primaryFile);
    auto tokens = lexer.tokenize();

    if (dumpTokens) {
        for (auto& tok : tokens) {
            std::cout << tok.loc.line << ":" << tok.loc.col << "  "
                      << tokenTypeName(tok.type);
            if (!tok.value.empty()) std::cout << "  '" << tok.value << "'";
            std::cout << "\n";
        }
        return 0;
    }

    // Phase 2: Parse
    Parser parser(tokens);
    Program program = parser.parse();

    if (parser.hasErrors()) {
        for (auto& err : parser.getErrors()) {
            std::cerr << err << "\n";
        }
        return 1;
    }

    // Phase 2.5: Resolve includes
    std::string stdlibDir = getStdlibDir();
    std::string srcDir = getSourceDir(primaryFile);
    IncludeResolver resolver({stdlibDir}, srcDir);
    if (!resolver.resolve(program)) {
        for (auto& err : resolver.getErrors()) {
            std::cerr << err << "\n";
        }
        return 1;
    }

    if (dumpAST) {
        std::cout << "AST: " << program.declarations.size() << " top-level declarations\n";
        for (auto& decl : program.declarations) {
            if (auto cls = std::dynamic_pointer_cast<ClassDecl>(decl)) {
                std::cout << "  class " << cls->qualifiedName();
                if (!cls->parentClass.empty()) std::cout << " extends " << cls->parentClass;
                std::cout << " { "
                          << cls->fields.size() << " fields, "
                          << cls->methods.size() << " methods, "
                          << cls->constructors.size() << " constructors }\n";
            } else if (auto fn = std::dynamic_pointer_cast<FunctionDecl>(decl)) {
                std::cout << "  function " << fn->returnType.toString()
                          << " " << fn->qualifiedName() << "(...)\n";
            } else if (auto ext = std::dynamic_pointer_cast<ExternBlockDecl>(decl)) {
                std::cout << "  extern \"" << ext->convention << "\" { "
                          << ext->functions.size() << " functions }\n";
            } else if (auto ifc = std::dynamic_pointer_cast<InterfaceDecl>(decl)) {
                std::cout << "  interface " << ifc->name << " { "
                          << ifc->methods.size() << " methods }\n";
            }
        }
        return 0;
    }

    // Phase 3: Semantic analysis
    Sema sema;
    if (!sema.analyze(program)) {
        for (auto& err : sema.getErrors()) {
            std::cerr << err << "\n";
        }
        return 1;
    }

    // Phase 4: Code generation
    // Initialize native target only
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    CodeGen codegen(primaryFile, targetTriple, sema, optLevel);
    if (!codegen.generate(program)) {
        std::cerr << "error: code generation failed\n";
        return 1;
    }

    // Run LLVM optimization passes on the generated IR
    codegen.runOptimizationPasses();

    // Phase 5: Emit output
    if (emitLLVM) {
        std::string outFile = outputName;
        if (outFile == "a.out") outFile = primaryFile + ".ll";
        codegen.emitLLVMIR(outFile);
        std::cout << "wrote " << outFile << "\n";
        return 0;
    }

    if (emitAsm) {
        std::string outFile = outputName;
        if (outFile == "a.out") outFile = primaryFile + ".s";
        codegen.emitAssembly(outFile);
        std::cout << "wrote " << outFile << "\n";
        return 0;
    }

    // Emit object file
    std::string objFile = outputName + ".o";
    if (emitObj) {
        if (outputName != "a.out") objFile = outputName;
        codegen.emitObjectFile(objFile);
        std::cout << "wrote " << objFile << "\n";
        return 0;
    }

    // Full pipeline: emit object then link
    codegen.emitObjectFile(objFile);

    // Link with system compiler/linker
    std::string compilerDir = getCompilerDir();
    std::string runtimeDir = compilerDir + "/../runtime";
    // Build link command — only link runtime modules that were actually included
    std::string linkCmd = "cc " + objFile + " -o " + outputName;
    linkCmd += " -L" + runtimeDir;
    linkCmd += " -L" + compilerDir;
    linkCmd += " -L.";
    auto& usedMods = resolver.usedModules();
    // Link per-module stdlib libraries first (compiled from .lpl)
    for (auto& mod : usedMods) {
        linkCmd += " -llpl_" + mod;
    }
    // Core runtime + platform helpers last (resolves symbols used by modules)
    linkCmd += " -llplrt";
    linkCmd += " -lc";
    // Strip dead code and symbols for smaller binaries
    linkCmd += " -Wl,-dead_strip";
    if (optLevel > 0) {
        linkCmd += " -Wl,-x"; // strip local symbols
    }
    std::cout << "linking: " << linkCmd << "\n";
    int linkResult = system(linkCmd.c_str());

    // Clean up temp object file
    std::remove(objFile.c_str());

    if (linkResult != 0) {
        std::cerr << "error: linking failed\n";
        return 1;
    }

    std::cout << "wrote " << outputName << "\n";
    return 0;
}