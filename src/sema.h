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
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include "ast.h"

struct ClassInfo {
    std::string name;
    std::string parent;
    std::vector<FieldDecl> fields;
    std::vector<MethodDecl> methods;
    std::vector<ConstructorDecl> constructors;
    bool hasDestructor = false;
    bool isAbstract = false;
    std::vector<std::string> typeParams; // non-empty for generic template
};

struct FuncInfo {
    TypeSpec returnType;
    std::vector<Param> params;
    bool isExtern = false;
    bool isVariadic = false;
    bool isSquib = false;
    bool squibUsed = false;
};

class Sema {
public:
    bool analyze(Program& prog);
    bool hasErrors() const { return !errors.empty(); }
    const std::vector<std::string>& getErrors() const { return errors; }

    // Lookup helpers used by codegen
    const ClassInfo* lookupClass(const std::string& name) const;
    const FuncInfo* lookupFunction(const std::string& name) const;
    int getFieldIndex(const std::string& className, const std::string& fieldName) const;

private:
    std::vector<std::string> errors;
    std::unordered_map<std::string, ClassInfo> classes;
    std::unordered_map<std::string, FuncInfo> functions;

    // Generic templates (keyed by unqualified name, e.g. "Box")
    std::unordered_map<std::string, std::shared_ptr<ClassDecl>> genericClassTemplates;
    std::unordered_map<std::string, std::shared_ptr<FunctionDecl>> genericFuncTemplates;

    // Track which instantiations have been created (e.g. "Box<int>")
    std::unordered_set<std::string> instantiatedGenerics;

    // Scope stack for local variables
    struct VarInfo {
        TypeSpec type;
        bool isConst = false;
        bool isSquib = false;
        bool squibUsed = false; // true after first use of a squib variable
    };
    struct Scope {
        std::unordered_map<std::string, VarInfo> vars;
    };
    std::vector<Scope> scopes;
    std::string currentClassName;
    std::string currentNamespace_;

    // Capture tracking for [=] and [&] lambdas
    struct CaptureTracker {
        LambdaExpr* lambda = nullptr;
        bool captureByRef = false;
        int lambdaScopeDepth = -1;  // scope index where lambda's own scope starts
    };
    CaptureTracker* captureTracker_ = nullptr;

    void error(const SourceLoc& loc, const std::string& msg);

    // Name resolution helpers
    std::string resolveClassName(const std::string& name) const;
    bool isNamespacePrefix(const std::string& prefix) const;
    static std::string flattenDottedExpr(Expr& expr);

    // Passes
    void registerBuiltins();
    void registerDeclarations(Program& prog);
    void analyzeDecl(Decl& decl);
    void analyzeClass(ClassDecl& cls);
    void analyzeFunction(FunctionDecl& fn);
    void analyzeMethod(const std::string& className, MethodDecl& method);
    void analyzeConstructor(const std::string& className, ConstructorDecl& ctor);

    void analyzeStmt(Stmt& stmt);
    void analyzeBlock(BlockStmt& block);
    TypeSpec analyzeExpr(Expr& expr);

    void pushScope();
    void popScope();
    void declareVar(const std::string& name, const TypeSpec& type, const SourceLoc& loc, bool isConst = false, bool isSquib = false);
    TypeSpec* lookupVar(const std::string& name);
    VarInfo* lookupVarInfo(const std::string& name);
    bool isVarConst(const std::string& name);
    bool isTypeValid(const TypeSpec& type);
    bool typesCompatible(const TypeSpec& target, const TypeSpec& source);

    // Generic template instantiation
    void ensureGenericClassInstantiated(TypeSpec& type);
    void ensureGenericFuncInstantiated(const std::string& name, const std::vector<TypeSpec>& typeArgs);
    static TypeSpec substituteType(const TypeSpec& type,
                                   const std::vector<std::string>& typeParams,
                                   const std::vector<TypeSpec>& typeArgs);
    // Reference to the program being analyzed (for injecting instantiated decls)
    Program* currentProgram_ = nullptr;
};