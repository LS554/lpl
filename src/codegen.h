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

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "ast.h"
#include "sema.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DIBuilder.h"

class CodeGen {
public:
    CodeGen(const std::string& moduleName, const std::string& targetTriple, Sema& sema);

    bool generate(Program& prog);
    llvm::Module& getModule() { return *module; }
    bool emitObjectFile(const std::string& filename);
    bool emitLLVMIR(const std::string& filename);
    bool emitAssembly(const std::string& filename);

private:
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    llvm::IRBuilder<> builder;
    Sema& sema;
    std::string targetTriple;

    // Type mappings
    std::unordered_map<std::string, llvm::StructType*> classTypes;
    llvm::StructType* stringType = nullptr;

    // Function mappings
    std::unordered_map<std::string, llvm::Function*> functionMap;

    // User main parameter types (captured from AST for entry point generation)
    std::vector<Param> userMainParams;

    // Current function state
    llvm::Function* currentFunction = nullptr;
    std::string currentClassName;
    std::unordered_map<std::string, llvm::AllocaInst*> namedValues;
    llvm::Value* sretPtr_ = nullptr;

    // Loop break/continue targets
    llvm::BasicBlock* breakTarget = nullptr;
    llvm::BasicBlock* continueTarget = nullptr;

    // RAII cleanup tracking
    struct CleanupEntry {
        llvm::AllocaInst* alloca_;
        std::string typeName;   // class name for destructor lookup
        bool isOwner;           // needs free after destroy
        bool isOwnerArray = false; // owner array: free (ptr - 8)
        bool isMoved = false;
    };
    std::vector<std::vector<CleanupEntry>> cleanupStack;

    // Defer tracking — parallel to cleanupStack, one list per scope
    std::vector<std::vector<Stmt*>> deferStack;

    // Lambda counter for unique names
    int lambdaCounter = 0;

    // Type helpers
    llvm::Type* getLLVMType(const TypeSpec& ts);
    llvm::Type* getClassLLVMType(const std::string& name);
    llvm::StructType* getOrCreateClassType(const std::string& name);
    llvm::StructType* getStringType();
    llvm::FunctionType* getMethodFuncType(const TypeSpec& retType,
                                          const std::vector<Param>& params,
                                          llvm::Type* selfType);

    // Name mangling
    std::string mangleName(const std::string& name);
    std::string mangleMethod(const std::string& className, const std::string& method);
    std::string mangleConstructor(const std::string& className);
    std::string mangleDestructor(const std::string& className);

    // Declaration generation
    void generateDecl(Decl& decl);
    void generateClass(ClassDecl& cls);
    void generateClassTypes(Program& prog);
    void generateFunction(FunctionDecl& fn);
    void generateExternBlock(ExternBlockDecl& ext);
    void generateConstructor(const std::string& className, ConstructorDecl& ctor);
    void generateDestructor(const std::string& className, ClassDecl& cls);
    void generateMethod(const std::string& className, MethodDecl& method);

    // Statement generation
    void generateStmt(Stmt& stmt);
    void generateBlock(BlockStmt& block);
    void generateVarDecl(VarDeclStmt& stmt);
    void generateIf(IfStmt& stmt);
    void generateWhile(WhileStmt& stmt);
    void generateFor(ForStmt& stmt);
    void generateReturn(ReturnStmt& stmt);
    void generateDelete(DeleteStmt& stmt);
    void generateDefer(DeferStmt& stmt);
    void generateDoWhile(DoWhileStmt& stmt);
    void generateSwitch(SwitchStmt& stmt);
    void generateForEach(ForEachStmt& stmt);
    void generateTry(TryStmt& stmt);
    void generateThrow(ThrowStmt& stmt);

    // Expression generation
    llvm::Value* generateExpr(Expr& expr);
    llvm::Value* generateBinary(BinaryExpr& expr);
    llvm::Value* generateCall(CallExpr& expr);
    llvm::Value* generateMethodCall(MethodCallExpr& expr);
    llvm::Value* generateMemberAccess(MemberAccessExpr& expr, bool wantAddress = false);
    llvm::Value* generateNew(NewExpr& expr);
    llvm::Value* generateNewArray(NewArrayExpr& expr);
    llvm::Value* generateStringLit(const std::string& val);
    llvm::Value* generateCharPtrToString(llvm::Value* charPtr);
    llvm::Value* generateStringConcat(llvm::Value* left, llvm::Value* right);
    llvm::Value* generateCast(CastExpr& expr);
    llvm::Value* generateTernary(TernaryExpr& expr);
    llvm::Value* generateIndex(IndexExpr& expr, bool wantAddress = false);

    // RAII helpers
    void pushCleanupScope();
    void popCleanupScope();
    void emitCleanups();
    void emitCleanupsForScope();

    // Utility
    llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* fn, llvm::Type* type,
                                              const std::string& name);
    llvm::Function* getOrDeclareRuntimeFunc(const std::string& name,
                                            llvm::FunctionType* type);
    void declareRuntimeFunctions();
    void generateEntryPoint();
    void generateExceptionTypeRegistration(Program& prog);
};