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
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifdef __linux__
#include <unistd.h>
#endif

#include "lexer.h"
#include "parser.h"
#include "resolver.h"
#include "sema.h"
#include "codegen.h"

#include "llvm/Support/TargetSelect.h"
#include "llvm/TargetParser/Host.h"

static std::string getCompilerDir() {
    char path[PATH_MAX];
#ifdef __APPLE__
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        char real[PATH_MAX];
        if (realpath(path, real)) {
            return std::string(dirname(real));
        }
    }
#elif defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        return std::string(dirname(path));
    }
#endif
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
    std::cerr << "Usage: " << prog << " <source.lpl> [file.c ...] [options]\n"
              << "\nOptions:\n"
              << "  -o <name>           Output file name\n"
              << "  -target <triple>    LLVM target triple\n"
              << "  -O0,-O1,-O2,-O3     Optimization level\n"
              << "  -Os                 Optimize for size\n"
              << "  -Oz                 Optimize aggressively for size\n"
              << "  -g                  Emit debug information\n"
              << "  -emit-llvm          Output LLVM IR (.ll)\n"
              << "  -emit-obj, -c       Output object file only\n"
              << "  -S                  Output assembly\n"
              << "  --dump-tokens       Dump lexer tokens\n"
              << "  --dump-ast          Dump AST (placeholder)\n"
              << "  --check             Check for errors (no codegen), output JSON diagnostics\n"
              << "  -I<path>            Add header search path for C #include resolution\n"
              << "  -L<path>            Add library search path\n"
              << "  -l<name>            Link against library\n"
              << "  -static             Force static linking\n"
              << "  -shared             Produce a shared library\n"
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

    std::vector<std::string> lplFiles;    // .lpl source files
    std::vector<std::string> cFiles;      // .c source files to compile
    std::string outputName = "a.out";
    std::string targetTriple = llvm::sys::getDefaultTargetTriple();
    bool emitLLVM = false;
    bool emitObj = false;
    bool emitAsm = false;
    bool dumpTokens = false;
    bool dumpAST = false;
    bool checkOnly = false;
    bool staticLink = false;
    bool sharedLib = false;
    int optLevel = 0;
    std::vector<std::string> extraLibPaths;  // from -L flags
    std::vector<std::string> extraLibs;      // from -l flags
    std::vector<std::string> extraIncPaths;  // from -I flags

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
        else if (arg == "--check") { checkOnly = true; }
        else if (arg == "-static") { staticLink = true; }
        else if (arg == "-shared") { sharedLib = true; }
        else if (arg.size() > 2 && arg.substr(0, 2) == "-L") {
            extraLibPaths.push_back(arg.substr(2));
        } else if (arg == "-L" && i + 1 < argc) {
            extraLibPaths.push_back(argv[++i]);
        } else if (arg.size() > 2 && arg.substr(0, 2) == "-l") {
            extraLibs.push_back(arg.substr(2));
        } else if (arg == "-l" && i + 1 < argc) {
            extraLibs.push_back(argv[++i]);
        } else if (arg.size() > 2 && arg.substr(0, 2) == "-I") {
            extraIncPaths.push_back(arg.substr(2));
        } else if (arg == "-I" && i + 1 < argc) {
            extraIncPaths.push_back(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg[0] == '-') {
            std::cerr << "error: unknown option '" << arg << "'\n";
            return 1;
        } else if (arg.size() >= 2 && arg.substr(arg.size() - 2) == ".c") {
            cFiles.push_back(arg);
        } else {
            lplFiles.push_back(arg);
        }
    }

    // For backwards compat: treat all non-.c inputs as LPL files
    std::vector<std::string>& sourceFiles = lplFiles;

    if (sourceFiles.empty() && cFiles.empty()) {
        std::cerr << "error: no input files\n";
        return 1;
    }
    if (sourceFiles.empty()) {
        std::cerr << "error: no .lpl source file provided\n";
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

    // Collect all diagnostics for --check mode
    std::vector<std::string> allErrors;

    if (parser.hasErrors()) {
        for (auto& err : parser.getErrors()) {
            allErrors.push_back(err);
        }
        if (!checkOnly) {
            for (auto& err : allErrors) std::cerr << err << "\n";
            return 1;
        }
    }

    // Phase 2.5: Resolve includes
    std::string stdlibDir = getStdlibDir();
    std::string srcDir = getSourceDir(primaryFile);
    IncludeResolver resolver({stdlibDir}, srcDir);
    resolver.setExtraIncludePaths(extraIncPaths);
    if (!resolver.resolve(program)) {
        for (auto& err : resolver.getErrors()) {
            allErrors.push_back(err);
        }
        if (!checkOnly) {
            for (auto& err : resolver.getErrors()) std::cerr << err << "\n";
            return 1;
        }
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
            allErrors.push_back(err);
        }
        if (!checkOnly) {
            for (auto& err : sema.getErrors()) std::cerr << err << "\n";
            return 1;
        }
    }

    // --check mode: output JSON diagnostics and exit
    if (checkOnly) {
        std::cout << "[";
        for (size_t i = 0; i < allErrors.size(); i++) {
            const auto& err = allErrors[i];
            // Parse "file:line:col: error: message" or "file:line: error: message"
            std::string file, message;
            int line = 1, col = 1;
            size_t pos1 = err.find(':');
            if (pos1 != std::string::npos) {
                file = err.substr(0, pos1);
                size_t pos2 = err.find(':', pos1 + 1);
                if (pos2 != std::string::npos) {
                    std::string after1 = err.substr(pos1 + 1, pos2 - pos1 - 1);
                    try { line = std::stoi(after1); } catch (...) {}
                    // Check if next part is a number (col) or "error"
                    size_t pos3 = err.find(':', pos2 + 1);
                    if (pos3 != std::string::npos) {
                        std::string after2 = err.substr(pos2 + 1, pos3 - pos2 - 1);
                        // Trim leading space
                        size_t ns = after2.find_first_not_of(' ');
                        if (ns != std::string::npos) after2 = after2.substr(ns);
                        bool isCol = !after2.empty() && std::isdigit(after2[0]);
                        if (isCol) {
                            try { col = std::stoi(after2); } catch (...) {}
                            // Message after "error: "
                            size_t msgStart = err.find("error: ", pos3);
                            if (msgStart != std::string::npos) {
                                message = err.substr(msgStart + 7);
                            } else {
                                message = err.substr(pos3 + 1);
                                size_t ms = message.find_first_not_of(' ');
                                if (ms != std::string::npos) message = message.substr(ms);
                            }
                        } else {
                            // No column — after2 starts with "error" etc.
                            size_t msgStart = err.find("error: ", pos2);
                            if (msgStart != std::string::npos) {
                                message = err.substr(msgStart + 7);
                            } else {
                                message = err.substr(pos2 + 1);
                            }
                        }
                    }
                }
            }
            if (message.empty()) message = err;
            // Escape JSON strings
            auto jsonEscape = [](const std::string& s) -> std::string {
                std::string r;
                for (char c : s) {
                    if (c == '\"') r += "\\\"";
                    else if (c == '\\') r += "\\\\";
                    else if (c == '\n') r += "\\n";
                    else r += c;
                }
                return r;
            };
            if (i > 0) std::cout << ",";
            std::cout << "{\"file\":\"" << jsonEscape(file)
                      << "\",\"line\":" << line
                      << ",\"col\":" << col
                      << ",\"message\":\"" << jsonEscape(message) << "\"}";
        }
        std::cout << "]\n";
        return allErrors.empty() ? 0 : 1;
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

    // Full pipeline: emit LPL object then link

    // Compile any .c source files via clang first
    std::vector<std::string> cObjFiles;
    for (auto& cFile : cFiles) {
        std::string cObj = cFile + ".o";
        std::string cCmd = "clang -c " + cFile + " -o " + cObj;
        for (auto& p : extraIncPaths) {
            cCmd += " -I" + p;
        }
        std::cout << "compiling: " << cCmd << "\n";
        int cResult = system(cCmd.c_str());
        if (cResult != 0) {
            std::cerr << "error: failed to compile C source '" << cFile << "'\n";
            return 1;
        }
        cObjFiles.push_back(cObj);
    }

    codegen.emitObjectFile(objFile);

    // Link with system compiler/linker
    std::string compilerDir = getCompilerDir();
    std::string runtimeDir = compilerDir + "/../runtime";

    // GPL compliance: warn when statically linking known GPL libraries.
    // Dynamic linking (the default) naturally avoids GPL derivative issues.
    if (staticLink && !extraLibs.empty()) {
        static const std::vector<std::string> knownGPL = {
            "readline", "gmp", "bfd", "gnutls", "ffmpeg",
            "avcodec", "avformat", "avutil", "swscale",
            "x264", "x265", "fftw3",
        };
        for (auto& lib : extraLibs) {
            for (auto& gpl : knownGPL) {
                if (lib == gpl) {
                    std::cerr << "warning: statically linking GPL library '-l" << lib
                              << "' — the resulting binary is a GPL derivative\n";
                }
            }
        }
    }

    // Start building link command
    std::string linkCmd = "cc";
    if (sharedLib) linkCmd += " -shared";
    if (staticLink) linkCmd += " -static";

    // LPL object file
    linkCmd += " " + objFile;

    // Extra C object files
    for (auto& co : cObjFiles) {
        linkCmd += " " + co;
    }

    linkCmd += " -o " + outputName;

    // Runtime library search paths
    linkCmd += " -L" + runtimeDir;
    linkCmd += " -L" + compilerDir;
    linkCmd += " -L.";

    // User-supplied library search paths (-L flags)
    for (auto& lp : extraLibPaths) {
        linkCmd += " -L" + lp;
    }

    auto& usedMods = resolver.usedModules();
    // Link per-module stdlib libraries first (compiled from .lpl)
    for (auto& mod : usedMods) {
        linkCmd += " -llpl_" + mod;
    }
    // Core runtime + platform helpers last (resolves symbols used by modules)
    linkCmd += " -llplrt";
    linkCmd += " -lc";

    // User-supplied library links (-l flags)
    for (auto& lib : extraLibs) {
        linkCmd += " -l" + lib;
    }

    // Link C++ standard library if any extern "C++" blocks were used
    if (codegen.requiresCppLink()) {
        linkCmd += " -lc++";
    }

    // Strip dead code and symbols for smaller binaries (not when building shared libs)
    if (!sharedLib && !staticLink) {
        linkCmd += " -Wl,-dead_strip";
        if (optLevel > 0) {
            linkCmd += " -Wl,-x"; // strip local symbols
        }
    }

    std::cout << "linking: " << linkCmd << "\n";
    int linkResult = system(linkCmd.c_str());

    // Clean up temp object files
    std::remove(objFile.c_str());
    for (auto& co : cObjFiles) {
        std::remove(co.c_str());
    }

    if (linkResult != 0) {
        std::cerr << "error: linking failed\n";
        return 1;
    }

    std::cout << "wrote " << outputName << "\n";
    return 0;
}