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

#include "codegen.h"

#include "llvm/IR/Verifier.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"

#include <iostream>

// ============================================================
// Constructor and initialization
// ============================================================

CodeGen::CodeGen(const std::string& moduleName, const std::string& triple,
                 Sema& sema, int optLevel)
    : module(std::make_unique<llvm::Module>(moduleName, context)),
      builder(context), sema(sema), targetTriple(triple), optLevel(optLevel) {
    module->setTargetTriple(llvm::Triple(targetTriple));
}

// ============================================================
// Type helpers
// ============================================================

llvm::StructType* CodeGen::getStringType() {
    if (!stringType) {
        // struct LPLString { i8*, i64, i64 }
        stringType = llvm::StructType::create(context, "LPLString");
        stringType->setBody({
            llvm::PointerType::get(context, 0), // data
            llvm::Type::getInt64Ty(context),     // length
            llvm::Type::getInt64Ty(context),     // capacity
        });
    }
    return stringType;
}

llvm::StructType* CodeGen::getOrCreateClassType(const std::string& name) {
    auto it = classTypes.find(name);
    if (it != classTypes.end()) return it->second;

    auto st = llvm::StructType::create(context, name);
    classTypes[name] = st;
    return st;
}

llvm::Type* CodeGen::getLLVMType(const TypeSpec& ts) {
    if (ts.pointerDepth || ts.isReference) {
        return llvm::PointerType::get(context, 0);
    }
    if (ts.isArray) {
        return llvm::PointerType::get(context, 0); // arrays are fat pointers for now
    }

    switch (ts.kind) {
        case TypeSpec::Void:   return llvm::Type::getVoidTy(context);
        case TypeSpec::Bool:   return llvm::Type::getInt1Ty(context);
        case TypeSpec::Byte:   return llvm::Type::getInt8Ty(context);
        case TypeSpec::Char:   return llvm::Type::getInt32Ty(context);
        case TypeSpec::Short:  return llvm::Type::getInt16Ty(context);
        case TypeSpec::Int:    return llvm::Type::getInt32Ty(context);
        case TypeSpec::Long:   return llvm::Type::getInt64Ty(context);
        case TypeSpec::Float:  return llvm::Type::getFloatTy(context);
        case TypeSpec::Double: return llvm::Type::getDoubleTy(context);
        case TypeSpec::String: return getStringType();
        case TypeSpec::ClassName:
            return getOrCreateClassType(ts.className);
        case TypeSpec::Callable:
            // Fat pointer: { fn_ptr, env_ptr }
            return llvm::StructType::get(context, {
                llvm::PointerType::get(context, 0),
                llvm::PointerType::get(context, 0)
            });
        case TypeSpec::Auto:
            break; // should be resolved by sema
    }
    return llvm::Type::getVoidTy(context);
}

llvm::Type* CodeGen::getClassLLVMType(const std::string& name) {
    return getOrCreateClassType(name);
}

llvm::FunctionType* CodeGen::getMethodFuncType(const TypeSpec& retType,
                                                const std::vector<Param>& params,
                                                llvm::Type* selfType) {
    std::vector<llvm::Type*> paramTypes;
    if (selfType) {
        paramTypes.push_back(llvm::PointerType::get(context, 0)); // self ptr
    }
    for (auto& p : params) {
        llvm::Type* pt = getLLVMType(p.type);
        // Pass structs by pointer
        if (pt->isStructTy()) {
            paramTypes.push_back(llvm::PointerType::get(context, 0));
        } else {
            paramTypes.push_back(pt);
        }
    }

    llvm::Type* ret = getLLVMType(retType);
    if (ret->isStructTy()) {
        // Return structs via sret parameter
        paramTypes.insert(paramTypes.begin() + (selfType ? 1 : 0),
                          llvm::PointerType::get(context, 0));
        return llvm::FunctionType::get(llvm::Type::getVoidTy(context), paramTypes, false);
    }
    return llvm::FunctionType::get(ret, paramTypes, false);
}

// ============================================================
// Name mangling
// ============================================================

// Encodes each segment of a dotted name with length prefix.
// "Std.IO.Console" → "3Std2IO7Console"
// "Console" → "7Console" (backward compatible)
static std::string mangleQualifiedName(const std::string& name) {
    // First, sanitize generic type characters for linker compatibility
    std::string sanitized;
    for (char c : name) {
        switch (c) {
            case '<': sanitized += 'I'; break;  // Itanium-style 'I' for template open
            case '>': sanitized += 'E'; break;  // Itanium-style 'E' for template close
            case ',': sanitized += 'C'; break;
            case ' ': break;                    // skip spaces
            default:  sanitized += c; break;
        }
    }
    std::string result;
    size_t start = 0;
    while (start < sanitized.size()) {
        size_t dot = sanitized.find('.', start);
        std::string part;
        if (dot == std::string::npos) {
            part = sanitized.substr(start);
            start = sanitized.size();
        } else {
            part = sanitized.substr(start, dot - start);
            start = dot + 1;
        }
        result += std::to_string(part.size()) + part;
    }
    return result;
}

std::string CodeGen::mangleName(const std::string& name) {
    return "_LPL" + mangleQualifiedName(name);
}

std::string CodeGen::mangleMethod(const std::string& className, const std::string& method) {
    return "_LPL" + mangleQualifiedName(className)
         + std::to_string(method.size()) + method;
}

std::string CodeGen::mangleConstructor(const std::string& className) {
    return "_LPL" + mangleQualifiedName(className) + "4init";
}

std::string CodeGen::mangleDestructor(const std::string& className) {
    return "_LPL" + mangleQualifiedName(className) + "7destroy";
}

// ============================================================
// Runtime function declarations
// ============================================================

void CodeGen::declareRuntimeFunctions() {
    auto ptr = llvm::PointerType::get(context, 0);
    auto i64 = llvm::Type::getInt64Ty(context);
    auto i32 = llvm::Type::getInt32Ty(context);
    auto voidTy = llvm::Type::getVoidTy(context);

    // malloc
    auto mallocTy = llvm::FunctionType::get(ptr, {i64}, false);
    getOrDeclareRuntimeFunc("malloc", mallocTy);

    // free
    auto freeTy = llvm::FunctionType::get(voidTy, {ptr}, false);
    getOrDeclareRuntimeFunc("free", freeTy);

    // __lpl_runtime_init / __lpl_runtime_shutdown
    auto initTy = llvm::FunctionType::get(voidTy, {}, false);
    getOrDeclareRuntimeFunc("__lpl_runtime_init", initTy);
    getOrDeclareRuntimeFunc("__lpl_runtime_shutdown", initTy);

    // __lpl_system_set_args(int argc, char** argv)
    auto setArgsTy = llvm::FunctionType::get(voidTy, {i32, ptr}, false);
    getOrDeclareRuntimeFunc("__lpl_system_set_args", setArgsTy);

    // __lpl_string_create(data: i8*, len: i64) -> LPLString (via sret)
    auto strCreateTy = llvm::FunctionType::get(voidTy, {ptr, ptr, i64}, false);
    getOrDeclareRuntimeFunc("__lpl_string_create", strCreateTy);

    // __lpl_string_concat(result: LPLString*, a: LPLString*, b: LPLString*)
    auto strConcatTy = llvm::FunctionType::get(voidTy, {ptr, ptr, ptr}, false);
    getOrDeclareRuntimeFunc("__lpl_string_concat", strConcatTy);

    // __lpl_string_destroy(s: LPLString*)
    auto strDestroyTy = llvm::FunctionType::get(voidTy, {ptr}, false);
    getOrDeclareRuntimeFunc("__lpl_string_destroy", strDestroyTy);

    // __lpl_int_to_string(result: LPLString*, val: i32)
    auto i2sTy = llvm::FunctionType::get(voidTy, {ptr, i32}, false);
    getOrDeclareRuntimeFunc("__lpl_int_to_string", i2sTy);

    // Exception handling runtime functions
    // __lpl_try_enter() -> void* (pointer to jmp_buf)
    auto tryEnterTy = llvm::FunctionType::get(ptr, {}, false);
    getOrDeclareRuntimeFunc("__lpl_try_enter", tryEnterTy);

    // __lpl_try_leave()
    auto tryLeaveTy = llvm::FunctionType::get(voidTy, {}, false);
    getOrDeclareRuntimeFunc("__lpl_try_leave", tryLeaveTy);

    // __lpl_throw(exception: void*, type_name: char*)
    auto throwTy = llvm::FunctionType::get(voidTy, {ptr, ptr}, false);
    getOrDeclareRuntimeFunc("__lpl_throw", throwTy);

    // __lpl_exception_current() -> void*
    auto excCurTy = llvm::FunctionType::get(ptr, {}, false);
    getOrDeclareRuntimeFunc("__lpl_exception_current", excCurTy);

    // __lpl_exception_type() -> char*
    auto excTypeTy = llvm::FunctionType::get(ptr, {}, false);
    getOrDeclareRuntimeFunc("__lpl_exception_type", excTypeTy);

    // __lpl_exception_is_type(target_type: char*) -> i32
    auto excIsTypeTy = llvm::FunctionType::get(i32, {ptr}, false);
    getOrDeclareRuntimeFunc("__lpl_exception_is_type", excIsTypeTy);

    // __lpl_exception_clear()
    auto excClearTy = llvm::FunctionType::get(voidTy, {}, false);
    getOrDeclareRuntimeFunc("__lpl_exception_clear", excClearTy);

    // __lpl_exception_register_type(child: char*, parent: char*)
    auto excRegTy = llvm::FunctionType::get(voidTy, {ptr, ptr}, false);
    getOrDeclareRuntimeFunc("__lpl_exception_register_type", excRegTy);

    // setjmp(jmp_buf*) -> i32  (from libc)
    auto setjmpTy = llvm::FunctionType::get(i32, {ptr}, false);
    getOrDeclareRuntimeFunc("setjmp", setjmpTy);
}

llvm::Function* CodeGen::getOrDeclareRuntimeFunc(const std::string& name,
                                                  llvm::FunctionType* type) {
    auto it = functionMap.find(name);
    if (it != functionMap.end()) return it->second;

    auto fn = llvm::Function::Create(type, llvm::Function::ExternalLinkage, name, *module);
    functionMap[name] = fn;
    return fn;
}

// ============================================================
// Utility
// ============================================================

llvm::AllocaInst* CodeGen::createEntryBlockAlloca(llvm::Function* fn,
                                                   llvm::Type* type,
                                                   const std::string& name) {
    llvm::IRBuilder<> tmpBuilder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
    return tmpBuilder.CreateAlloca(type, nullptr, name);
}

// ============================================================
// RAII cleanup
// ============================================================

void CodeGen::pushCleanupScope() {
    cleanupStack.push_back({});
    deferStack.push_back({});
}

void CodeGen::popCleanupScope() {
    // Emit deferred statements in reverse order for this scope
    if (!deferStack.empty()) {
        auto& defers = deferStack.back();
        for (int i = (int)defers.size() - 1; i >= 0; i--) {
            if (!builder.GetInsertBlock()->getTerminator()) {
                generateStmt(*defers[i]);
            }
        }
        deferStack.pop_back();
    }
    emitCleanupsForScope();
    cleanupStack.pop_back();
}

void CodeGen::emitCleanupsForScope() {
    if (cleanupStack.empty()) return;
    auto& scope = cleanupStack.back();
    // Destroy in reverse order
    for (int i = (int)scope.size() - 1; i >= 0; i--) {
        auto& entry = scope[i];
        if (entry.isMoved) continue;

        if (entry.isOwnerArray) {
            // Owner array: data pointer is stored in alloca
            // The real allocation starts at (dataPtr - 8)
            auto dataPtr = builder.CreateLoad(llvm::PointerType::get(context, 0),
                                              entry.alloca_);
            auto i64Ty = llvm::Type::getInt64Ty(context);
            auto minusOne = llvm::ConstantInt::getSigned(i64Ty, -1);
            auto rawPtr = builder.CreateGEP(i64Ty, dataPtr, {minusOne}, "arr.raw");
            auto freeFn = functionMap["free"];
            builder.CreateCall(freeFn, {rawPtr});
        } else if (entry.isOwner) {
            // Owner pointer: alloca holds a pointer to the heap object
            auto loaded = builder.CreateLoad(llvm::PointerType::get(context, 0),
                                             entry.alloca_);
            // Call destructor on the heap object
            std::string dtorName = mangleDestructor(entry.typeName);
            auto it = functionMap.find(dtorName);
            if (it != functionMap.end()) {
                builder.CreateCall(it->second, {loaded});
            }
            // Free the heap object
            auto freeFn = functionMap["free"];
            builder.CreateCall(freeFn, {loaded});
        } else {
            // Stack variable: alloca IS the object
            std::string dtorName = mangleDestructor(entry.typeName);
            auto it = functionMap.find(dtorName);
            if (it != functionMap.end()) {
                builder.CreateCall(it->second, {entry.alloca_});
            }
        }
    }
}

void CodeGen::emitCleanups() {
    // Emit all deferred statements from all scopes (innermost first)
    for (int i = (int)deferStack.size() - 1; i >= 0; i--) {
        auto& defers = deferStack[i];
        for (int j = (int)defers.size() - 1; j >= 0; j--) {
            generateStmt(*defers[j]);
        }
    }
    // Then run RAII cleanups
    for (int i = (int)cleanupStack.size() - 1; i >= 0; i--) {
        auto& scope = cleanupStack[i];
        for (int j = (int)scope.size() - 1; j >= 0; j--) {
            auto& entry = scope[j];
            if (entry.isMoved) continue;

            if (entry.isOwnerArray) {
                auto dataPtr = builder.CreateLoad(llvm::PointerType::get(context, 0),
                                                  entry.alloca_);
                auto i64Ty = llvm::Type::getInt64Ty(context);
                auto minusOne = llvm::ConstantInt::getSigned(i64Ty, -1);
                auto rawPtr = builder.CreateGEP(i64Ty, dataPtr, {minusOne}, "arr.raw");
                auto freeFn = functionMap["free"];
                builder.CreateCall(freeFn, {rawPtr});
            } else if (entry.isOwner) {
                auto loaded = builder.CreateLoad(llvm::PointerType::get(context, 0),
                                                 entry.alloca_);
                std::string dtorName = mangleDestructor(entry.typeName);
                auto it = functionMap.find(dtorName);
                if (it != functionMap.end()) {
                    builder.CreateCall(it->second, {loaded});
                }
                auto freeFn = functionMap["free"];
                builder.CreateCall(freeFn, {loaded});
            } else {
                std::string dtorName = mangleDestructor(entry.typeName);
                auto it = functionMap.find(dtorName);
                if (it != functionMap.end()) {
                    builder.CreateCall(it->second, {entry.alloca_});
                }
            }
        }
    }
}

// ============================================================
// Top-level generation
// ============================================================

bool CodeGen::generate(Program& prog) {
    declareRuntimeFunctions();

    // First pass: create all class struct types
    generateClassTypes(prog);

    // Second pass: declare all functions (so they can be referenced)
    for (auto& decl : prog.declarations) {
        if (auto cls = std::dynamic_pointer_cast<ClassDecl>(decl)) {
            if (!cls->typeParams.empty()) continue; // skip generic templates
            // Declare constructors
            for (auto& ctor : cls->constructors) {
                std::vector<llvm::Type*> paramTypes;
                paramTypes.push_back(llvm::PointerType::get(context, 0)); // self
                for (auto& p : ctor.params) {
                    auto pt = getLLVMType(p.type);
                    if (pt->isStructTy()) paramTypes.push_back(llvm::PointerType::get(context, 0));
                    else paramTypes.push_back(pt);
                }
                auto ft = llvm::FunctionType::get(llvm::Type::getVoidTy(context), paramTypes, false);
                auto fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                                  mangleConstructor(cls->qualifiedName()), *module);
                functionMap[mangleConstructor(cls->qualifiedName())] = fn;
            }

            // Declare destructor
            {
                auto ft = llvm::FunctionType::get(
                    llvm::Type::getVoidTy(context),
                    {llvm::PointerType::get(context, 0)}, false);
                auto fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                                  mangleDestructor(cls->qualifiedName()), *module);
                functionMap[mangleDestructor(cls->qualifiedName())] = fn;
            }

            // Declare methods
            for (auto& method : cls->methods) {
                llvm::Type* selfType = method.isStatic ? nullptr
                    : llvm::PointerType::get(context, 0);
                auto ft = getMethodFuncType(method.returnType, method.params, selfType);
                auto fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                                  mangleMethod(cls->qualifiedName(), method.name), *module);
                functionMap[mangleMethod(cls->qualifiedName(), method.name)] = fn;
            }
        } else if (auto fn = std::dynamic_pointer_cast<FunctionDecl>(decl)) {
            if (!fn->typeParams.empty()) continue; // skip generic templates
            // Capture user main's AST params for entry point generation
            if (fn->name == "main") {
                userMainParams = fn->params;
            }
            std::vector<llvm::Type*> paramTypes;
            for (auto& p : fn->params) {
                auto pt = getLLVMType(p.type);
                if (pt->isStructTy()) paramTypes.push_back(llvm::PointerType::get(context, 0));
                else paramTypes.push_back(pt);
            }
            auto retTy = getLLVMType(fn->returnType);
            auto ft = llvm::FunctionType::get(retTy, paramTypes, false);
            auto f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                             mangleName(fn->qualifiedName()), *module);
            functionMap[mangleName(fn->qualifiedName())] = f;
        } else if (auto ext = std::dynamic_pointer_cast<ExternBlockDecl>(decl)) {
            generateExternBlock(*ext);
        }
    }

    // Third pass: generate function bodies
    for (auto& decl : prog.declarations) {
        generateDecl(*decl);
    }

    // Generate main entry point
    generateEntryPoint();

    // Register exception type hierarchy (class inheritance for catch matching)
    generateExceptionTypeRegistration(prog);

    // Verify module
    std::string err;
    llvm::raw_string_ostream errStream(err);
    if (llvm::verifyModule(*module, &errStream)) {
        std::cerr << "LLVM module verification failed:\n" << err << "\n";
        return false;
    }

    return true;
}

void CodeGen::generateClassTypes(Program& prog) {
    // Create all class types first (forward declarations)
    for (auto& decl : prog.declarations) {
        if (auto cls = std::dynamic_pointer_cast<ClassDecl>(decl)) {
            if (!cls->typeParams.empty()) continue; // skip generic templates
            getOrCreateClassType(cls->qualifiedName());
        }
    }

    // Set bodies
    for (auto& decl : prog.declarations) {
        if (auto cls = std::dynamic_pointer_cast<ClassDecl>(decl)) {
            if (!cls->typeParams.empty()) continue; // skip generic templates
            auto st = classTypes[cls->qualifiedName()];
            std::vector<llvm::Type*> fields;

            // If has parent, include parent fields first
            if (!cls->parentClass.empty()) {
                auto parentSt = getOrCreateClassType(cls->parentClass);
                // Flatten parent into this struct
                for (unsigned i = 0; i < parentSt->getNumElements(); i++) {
                    fields.push_back(parentSt->getElementType(i));
                }
            }

            for (auto& field : cls->fields) {
                auto ft = getLLVMType(field.type);
                fields.push_back(ft);
            }

            if (!st->isOpaque()) continue; // already set
            st->setBody(fields);
        }
    }
}

void CodeGen::generateDecl(Decl& decl) {
    if (auto cls = dynamic_cast<ClassDecl*>(&decl)) {
        if (!cls->typeParams.empty()) return; // skip generic templates
        generateClass(*cls);
    } else if (auto fn = dynamic_cast<FunctionDecl*>(&decl)) {
        if (!fn->typeParams.empty()) return; // skip generic templates
        generateFunction(*fn);
    }
    // extern blocks already handled in declaration pass
}

// ============================================================
// Extern block generation
// ============================================================

void CodeGen::generateExternBlock(ExternBlockDecl& ext) {
    for (auto& ef : ext.functions) {
        // Determine the linker symbol name:
        // - If an explicit link name is provided (as "symbol"), use it
        // - Otherwise, use the LPL-side name directly
        std::string symbolName = ef.linkName.empty() ? ef.name : ef.linkName;

        // Reuse existing function if already declared (e.g., runtime builtins)
        if (auto existing = module->getFunction(symbolName)) {
            functionMap[ef.name] = existing;
            continue;
        }
        std::vector<llvm::Type*> paramTypes;

        // Match the method calling convention: struct returns use sret,
        // struct params are passed by pointer.
        auto retTy = getLLVMType(ef.returnType);
        bool hasSret = retTy->isStructTy();
        if (hasSret) {
            paramTypes.push_back(llvm::PointerType::get(context, 0)); // sret
        }

        for (auto& p : ef.params) {
            auto pt = getLLVMType(p.type);
            if (pt->isStructTy()) {
                paramTypes.push_back(llvm::PointerType::get(context, 0));
            } else {
                paramTypes.push_back(pt);
            }
        }

        auto funcRetTy = hasSret ? llvm::Type::getVoidTy(context) : retTy;
        auto ft = llvm::FunctionType::get(funcRetTy, paramTypes, ef.isVariadic);
        auto fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                          symbolName, *module);
        // Map by LPL-side name so calls resolve using the callable name
        functionMap[ef.name] = fn;
    }

    // Track if C++ standard library linking is needed
    if (ext.convention == "C++") {
        needsCppLink = true;
    }
}

// ============================================================
// Class generation
// ============================================================

void CodeGen::generateClass(ClassDecl& cls) {
    // Extern classes (from .lph headers) only have declarations, no bodies to generate.
    // Their implementations are provided by the runtime libraries.
    if (cls.isExtern) return;

    currentClassName = cls.qualifiedName();

    // Generate constructors
    for (auto& ctor : cls.constructors) {
        generateConstructor(cls.qualifiedName(), ctor);
    }

    // Generate destructor
    generateDestructor(cls.qualifiedName(), cls);

    // Generate methods
    for (auto& method : cls.methods) {
        generateMethod(cls.qualifiedName(), method);
    }

    currentClassName = "";
}

void CodeGen::generateConstructor(const std::string& className, ConstructorDecl& ctor) {
    std::string mangledName = mangleConstructor(className);
    auto fn = functionMap[mangledName];
    if (!fn) return;

    auto bb = llvm::BasicBlock::Create(context, "entry", fn);
    builder.SetInsertPoint(bb);

    currentFunction = fn;
    namedValues.clear();

    // First arg is 'self'
    auto argIt = fn->arg_begin();
    llvm::Value* self = &*argIt;
    self->setName("self");
    auto selfAlloca = createEntryBlockAlloca(fn, llvm::PointerType::get(context, 0), "self.addr");
    builder.CreateStore(self, selfAlloca);
    namedValues["this"] = selfAlloca;
    argIt++;

    // Other params
    for (size_t i = 0; i < ctor.params.size(); i++, argIt++) {
        auto& p = ctor.params[i];
        argIt->setName(p.name);
        auto pt = getLLVMType(p.type);
        if (pt->isStructTy()) {
            // Struct passed by pointer — dereference and store as struct value
            auto alloca = createEntryBlockAlloca(fn, pt, p.name);
            auto loaded = builder.CreateLoad(pt, &*argIt, p.name + ".val");
            builder.CreateStore(loaded, alloca);
            namedValues[p.name] = alloca;
        } else {
            auto alloca = createEntryBlockAlloca(fn, pt, p.name);
            builder.CreateStore(&*argIt, alloca);
            namedValues[p.name] = alloca;
        }
    }

    currentClassName = className;
    pushCleanupScope();

    // Generate body
    if (ctor.body) {
        for (auto& stmt : ctor.body->stmts) {
            generateStmt(*stmt);
        }
    }

    popCleanupScope();
    currentClassName = "";

    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateRetVoid();
    }
}

void CodeGen::generateDestructor(const std::string& className, ClassDecl& cls) {
    std::string mangledName = mangleDestructor(className);
    auto fn = functionMap[mangledName];
    if (!fn) return;

    auto bb = llvm::BasicBlock::Create(context, "entry", fn);
    builder.SetInsertPoint(bb);

    currentFunction = fn;
    namedValues.clear();

    auto self = &*fn->arg_begin();
    self->setName("self");
    auto selfAlloca = createEntryBlockAlloca(fn, llvm::PointerType::get(context, 0), "self.addr");
    builder.CreateStore(self, selfAlloca);
    namedValues["this"] = selfAlloca;

    currentClassName = className;

    // User-defined destructor body
    if (cls.destructor && cls.destructor->body) {
        for (auto& stmt : cls.destructor->body->stmts) {
            generateStmt(*stmt);
        }
    }

    // Destroy fields in reverse order
    auto ci = sema.lookupClass(className);
    if (ci) {
        auto selfPtr = builder.CreateLoad(llvm::PointerType::get(context, 0), selfAlloca);
        auto classType = getOrCreateClassType(className);

        int parentFieldCount = 0;
        if (!ci->parent.empty()) {
            auto parentCi = sema.lookupClass(ci->parent);
            if (parentCi) parentFieldCount = (int)parentCi->fields.size();
        }

        for (int i = (int)ci->fields.size() - 1; i >= 0; i--) {
            auto& field = ci->fields[i];
            int fieldIdx = parentFieldCount + i;

            if (field.type.kind == TypeSpec::String) {
                auto fieldPtr = builder.CreateStructGEP(classType, selfPtr, fieldIdx);
                auto destroyFn = functionMap["__lpl_string_destroy"];
                if (destroyFn) builder.CreateCall(destroyFn, {fieldPtr});
            } else if (field.type.kind == TypeSpec::ClassName && !field.type.pointerDepth) {
                // Nested class object: call its destructor
                auto dtorName = mangleDestructor(field.type.className);
                auto it = functionMap.find(dtorName);
                if (it != functionMap.end()) {
                    auto fieldPtr = builder.CreateStructGEP(classType, selfPtr, fieldIdx);
                    builder.CreateCall(it->second, {fieldPtr});
                }
            }
        }

        // Call parent destructor
        if (!ci->parent.empty()) {
            auto parentDtor = mangleDestructor(ci->parent);
            auto it = functionMap.find(parentDtor);
            if (it != functionMap.end()) {
                auto selfPtr2 = builder.CreateLoad(llvm::PointerType::get(context, 0), selfAlloca);
                builder.CreateCall(it->second, {selfPtr2});
            }
        }
    }

    currentClassName = "";
    builder.CreateRetVoid();
}

void CodeGen::generateMethod(const std::string& className, MethodDecl& method) {
    // Abstract methods have no body — skip code generation
    if (method.isAbstract) return;

    std::string mangledName = mangleMethod(className, method.name);
    auto fn = functionMap[mangledName];
    if (!fn) return;

    auto bb = llvm::BasicBlock::Create(context, "entry", fn);
    builder.SetInsertPoint(bb);

    currentFunction = fn;
    namedValues.clear();

    auto argIt = fn->arg_begin();

    if (!method.isStatic) {
        llvm::Value* self = &*argIt;
        self->setName("self");
        auto selfAlloca = createEntryBlockAlloca(fn, llvm::PointerType::get(context, 0), "self.addr");
        builder.CreateStore(self, selfAlloca);
        namedValues["this"] = selfAlloca;
        argIt++;
    }

    // Handle sret for struct return
    sretPtr_ = nullptr;
    auto retLLVMTy = getLLVMType(method.returnType);
    if (retLLVMTy->isStructTy()) {
        sretPtr_ = &*argIt;
        sretPtr_->setName("sret");
        argIt++;
    }

    for (size_t i = 0; i < method.params.size(); i++, argIt++) {
        auto& p = method.params[i];
        argIt->setName(p.name);
        auto pt = getLLVMType(p.type);
        if (pt->isStructTy()) {
            auto alloca = createEntryBlockAlloca(fn, pt, p.name);
            auto loaded = builder.CreateLoad(pt, &*argIt, p.name + ".val");
            builder.CreateStore(loaded, alloca);
            namedValues[p.name] = alloca;
        } else {
            auto alloca = createEntryBlockAlloca(fn, pt, p.name);
            builder.CreateStore(&*argIt, alloca);
            namedValues[p.name] = alloca;
        }
    }

    currentClassName = className;
    pushCleanupScope();

    if (method.body) {
        for (auto& stmt : method.body->stmts) {
            generateStmt(*stmt);
        }
    }

    popCleanupScope();
    currentClassName = "";

    if (!builder.GetInsertBlock()->getTerminator()) {
        if (sretPtr_ || method.returnType.isVoid()) {
            builder.CreateRetVoid();
        } else {
            builder.CreateRet(llvm::Constant::getNullValue(getLLVMType(method.returnType)));
        }
    }

    sretPtr_ = nullptr;
}

// ============================================================
// Function generation
// ============================================================

void CodeGen::generateFunction(FunctionDecl& fn) {
    std::string mangledName = mangleName(fn.qualifiedName());
    auto llvmFn = functionMap[mangledName];
    if (!llvmFn) return;

    auto bb = llvm::BasicBlock::Create(context, "entry", llvmFn);
    builder.SetInsertPoint(bb);

    currentFunction = llvmFn;
    namedValues.clear();

    auto argIt = llvmFn->arg_begin();
    for (size_t i = 0; i < fn.params.size(); i++, argIt++) {
        auto& p = fn.params[i];
        argIt->setName(p.name);
        auto pt = getLLVMType(p.type);
        if (pt->isStructTy()) {
            auto alloca = createEntryBlockAlloca(llvmFn, pt, p.name);
            auto loaded = builder.CreateLoad(pt, &*argIt, p.name + ".val");
            builder.CreateStore(loaded, alloca);
            namedValues[p.name] = alloca;
        } else {
            auto alloca = createEntryBlockAlloca(llvmFn, pt, p.name);
            builder.CreateStore(&*argIt, alloca);
            namedValues[p.name] = alloca;
        }
    }

    pushCleanupScope();

    if (fn.body) {
        for (auto& stmt : fn.body->stmts) {
            generateStmt(*stmt);
        }
    }

    popCleanupScope();

    if (!builder.GetInsertBlock()->getTerminator()) {
        if (fn.returnType.isVoid()) {
            builder.CreateRetVoid();
        } else {
            builder.CreateRet(llvm::Constant::getNullValue(getLLVMType(fn.returnType)));
        }
    }
}

// ============================================================
// Entry point generation
// ============================================================

void CodeGen::generateEntryPoint() {
    // Only generate entry point if source has a user main()
    auto userMainIt = functionMap.find(mangleName("main"));
    if (userMainIt == functionMap.end() || !userMainIt->second) return;

    // Create C main(int argc, char** argv) that calls runtime init, user main, runtime shutdown
    auto i32Ty = llvm::Type::getInt32Ty(context);
    auto ptr = llvm::PointerType::get(context, 0);
    auto mainTy = llvm::FunctionType::get(i32Ty, {i32Ty, ptr}, false);
    auto mainFn = llvm::Function::Create(mainTy, llvm::Function::ExternalLinkage, "main", *module);

    auto argIt = mainFn->arg_begin();
    llvm::Value* cArgc = &*argIt; cArgc->setName("argc");
    ++argIt;
    llvm::Value* cArgv = &*argIt; cArgv->setName("argv");

    auto bb = llvm::BasicBlock::Create(context, "entry", mainFn);
    builder.SetInsertPoint(bb);

    // Call runtime init
    auto initFn = functionMap["__lpl_runtime_init"];
    if (initFn) builder.CreateCall(initFn, {});

    // Store argc/argv in runtime so System.argc()/System.argv() work
    auto setArgsFn = functionMap["__lpl_system_set_args"];
    if (setArgsFn) builder.CreateCall(setArgsFn, {cArgc, cArgv});

    // Exception type hierarchy registration will be injected here via separate pass

    // Call user's main() — pass argc/argv if user's main accepts them
    auto userMain = functionMap[mangleName("main")];
    if (userMain) {
        auto userMainTy = userMain->getFunctionType();
        if (userMainTy->getNumParams() == 2 && userMainParams.size() == 2) {
            // Determine which style: char** argv or string[] args
            auto& secondParam = userMainParams[1].type;
            if (secondParam.kind == TypeSpec::String && secondParam.isArray) {
                // int main(int argc, string[] args) — build string array from C argv
                auto i64Ty = llvm::Type::getInt64Ty(context);
                auto i8Ty = llvm::Type::getInt8Ty(context);
                auto strTy = getStringType();
                auto dataLayout = module->getDataLayout();
                uint64_t elemSize = dataLayout.getTypeAllocSize(strTy);

                // argc as i64 for sizing
                auto countI64 = builder.CreateSExt(cArgc, i64Ty, "argc.ext");

                // Allocate LPL array: 8-byte header + argc * sizeof(LPLString)
                auto elemSizeVal = llvm::ConstantInt::get(i64Ty, elemSize);
                auto dataBytes = builder.CreateMul(countI64, elemSizeVal, "data.bytes");
                auto headerSize = llvm::ConstantInt::get(i64Ty, 8);
                auto totalBytes = builder.CreateAdd(dataBytes, headerSize, "total.bytes");
                auto mallocFn = functionMap["malloc"];
                auto raw = builder.CreateCall(mallocFn, {totalBytes}, "args.raw");
                builder.CreateMemSet(raw, builder.getInt8(0), totalBytes, llvm::MaybeAlign(8));
                builder.CreateStore(countI64, raw);
                auto dataPtr = builder.CreateGEP(i64Ty, raw,
                    {llvm::ConstantInt::get(i64Ty, 1)}, "args.data");

                // Loop: convert each argv[i] to string and store in array
                auto loopBB = llvm::BasicBlock::Create(context, "args.loop", mainFn);
                auto bodyBB = llvm::BasicBlock::Create(context, "args.body", mainFn);
                auto doneBB = llvm::BasicBlock::Create(context, "args.done", mainFn);

                auto idxAlloca = builder.CreateAlloca(i32Ty, nullptr, "args.idx");
                builder.CreateStore(llvm::ConstantInt::get(i32Ty, 0), idxAlloca);
                builder.CreateBr(loopBB);

                builder.SetInsertPoint(loopBB);
                auto idx = builder.CreateLoad(i32Ty, idxAlloca, "i");
                auto cond = builder.CreateICmpSLT(idx, cArgc, "args.cmp");
                builder.CreateCondBr(cond, bodyBB, doneBB);

                builder.SetInsertPoint(bodyBB);
                // Load argv[i] (char*)
                auto argvI = builder.CreateGEP(ptr, cArgv, {idx}, "argv.i.ptr");
                auto cStr = builder.CreateLoad(ptr, argvI, "argv.i");
                // strlen(cStr)
                auto strlenFn = module->getOrInsertFunction("strlen",
                    llvm::FunctionType::get(i64Ty, {ptr}, false));
                auto len = builder.CreateCall(strlenFn, {cStr}, "len");
                // Create LPLString at args.data[i]
                auto elemPtr = builder.CreateGEP(strTy, dataPtr, {idx}, "args.elem");
                auto createFn = functionMap["__lpl_string_create"];
                builder.CreateCall(createFn, {elemPtr, cStr, len});
                // i++
                auto nextIdx = builder.CreateAdd(idx, llvm::ConstantInt::get(i32Ty, 1), "i.next");
                builder.CreateStore(nextIdx, idxAlloca);
                builder.CreateBr(loopBB);

                builder.SetInsertPoint(doneBB);
                auto callResult = builder.CreateCall(userMain, {cArgc, dataPtr});
                if (userMainTy->getReturnType()->isIntegerTy()) {
                    auto shutdownFn = functionMap["__lpl_runtime_shutdown"];
                    if (shutdownFn) builder.CreateCall(shutdownFn, {});
                    builder.CreateRet(callResult);
                    return;
                }
            } else {
                // int main(int argc, char** argv)
                auto callResult = builder.CreateCall(userMain, {cArgc, cArgv});
                if (userMainTy->getReturnType()->isIntegerTy()) {
                    auto shutdownFn = functionMap["__lpl_runtime_shutdown"];
                    if (shutdownFn) builder.CreateCall(shutdownFn, {});
                    builder.CreateRet(callResult);
                    return;
                }
            }
        } else if (userMainTy->getNumParams() == 0) {
            auto callResult = builder.CreateCall(userMain, {});
            if (userMainTy->getReturnType()->isIntegerTy()) {
                auto shutdownFn = functionMap["__lpl_runtime_shutdown"];
                if (shutdownFn) builder.CreateCall(shutdownFn, {});
                builder.CreateRet(callResult);
                return;
            }
        }
    }

    // Call runtime shutdown
    auto shutdownFn = functionMap["__lpl_runtime_shutdown"];
    if (shutdownFn) builder.CreateCall(shutdownFn, {});

    builder.CreateRet(llvm::ConstantInt::get(i32Ty, 0));
}

void CodeGen::generateExceptionTypeRegistration(Program& prog) {
    // Find the main function and insert registration calls after runtime_init
    auto mainFn = module->getFunction("main");
    if (!mainFn) return;

    auto regFn = functionMap["__lpl_exception_register_type"];
    if (!regFn) return;

    // Collect class inheritance pairs
    std::vector<std::pair<std::string, std::string>> pairs;
    for (auto& decl : prog.declarations) {
        if (auto cls = std::dynamic_pointer_cast<ClassDecl>(decl)) {
            if (!cls->parentClass.empty()) {
                pairs.emplace_back(cls->qualifiedName(), cls->parentClass);
            }
        }
    }
    if (pairs.empty()) return;

    // Insert registration calls at the beginning of main, after the runtime_init call
    auto& entryBB = mainFn->getEntryBlock();
    // Find the instruction after __lpl_runtime_init call
    llvm::Instruction* insertPt = nullptr;
    for (auto& inst : entryBB) {
        if (auto call = llvm::dyn_cast<llvm::CallInst>(&inst)) {
            if (call->getCalledFunction() &&
                call->getCalledFunction()->getName() == "__lpl_runtime_init") {
                insertPt = call->getNextNode();
                break;
            }
        }
    }
    if (!insertPt) return;

    llvm::IRBuilder<> tmpBuilder(insertPt);
    for (auto& [child, parent] : pairs) {
        auto childStr = tmpBuilder.CreateGlobalString(child, ".exc.child." + child);
        auto parentStr = tmpBuilder.CreateGlobalString(parent, ".exc.parent." + parent);
        tmpBuilder.CreateCall(regFn, {childStr, parentStr});
    }
}

// ============================================================
// Statement generation
// ============================================================

void CodeGen::generateStmt(Stmt& stmt) {
    switch (stmt.kind) {
        case Stmt::Block:
            generateBlock(static_cast<BlockStmt&>(stmt));
            break;
        case Stmt::ExprS: {
            auto& s = static_cast<ExprStmt&>(stmt);
            generateExpr(*s.expr);
            break;
        }
        case Stmt::VarDecl:
            generateVarDecl(static_cast<VarDeclStmt&>(stmt));
            break;
        case Stmt::Return:
            generateReturn(static_cast<ReturnStmt&>(stmt));
            break;
        case Stmt::If:
            generateIf(static_cast<IfStmt&>(stmt));
            break;
        case Stmt::While:
            generateWhile(static_cast<WhileStmt&>(stmt));
            break;
        case Stmt::For:
            generateFor(static_cast<ForStmt&>(stmt));
            break;
        case Stmt::Delete:
            generateDelete(static_cast<DeleteStmt&>(stmt));
            break;
        case Stmt::Break:
            if (breakTarget) builder.CreateBr(breakTarget);
            break;
        case Stmt::Continue:
            if (continueTarget) builder.CreateBr(continueTarget);
            break;
        case Stmt::Defer:
            generateDefer(static_cast<DeferStmt&>(stmt));
            break;
        case Stmt::DoWhile:
            generateDoWhile(static_cast<DoWhileStmt&>(stmt));
            break;
        case Stmt::Switch:
            generateSwitch(static_cast<SwitchStmt&>(stmt));
            break;
        case Stmt::ForEach:
            generateForEach(static_cast<ForEachStmt&>(stmt));
            break;
        case Stmt::Try:
            generateTry(static_cast<TryStmt&>(stmt));
            break;
        case Stmt::Throw:
            generateThrow(static_cast<ThrowStmt&>(stmt));
            break;
        case Stmt::Fallthrough:
            // Handled in generateSwitch; nothing to emit here
            break;
    }
}

void CodeGen::generateBlock(BlockStmt& block) {
    pushCleanupScope();
    for (auto& stmt : block.stmts) {
        generateStmt(*stmt);
        if (builder.GetInsertBlock()->getTerminator()) break;
    }
    popCleanupScope();
}

void CodeGen::generateVarDecl(VarDeclStmt& stmt) {
    auto type = getLLVMType(stmt.type);
    llvm::AllocaInst* alloca;

    if (stmt.type.pointerDepth || stmt.type.isReference) {
        alloca = createEntryBlockAlloca(currentFunction,
                                        llvm::PointerType::get(context, 0), stmt.name);
    } else if (type->isStructTy()) {
        alloca = createEntryBlockAlloca(currentFunction, type, stmt.name);
    } else {
        alloca = createEntryBlockAlloca(currentFunction, type, stmt.name);
    }

    namedValues[stmt.name] = alloca;

    if (stmt.init) {
        // Special case: stack construction  e.g., Person p = Person("Alex", 32);
        if (stmt.init->kind == Expr::Call && !stmt.type.pointerDepth) {
            auto& call = static_cast<CallExpr&>(*stmt.init);
            if (sema.lookupClass(call.callee)) {
                // Stack construction: call constructor with alloca as self
                auto ctorName = mangleConstructor(call.callee);
                auto ctorFn = functionMap.find(ctorName);
                if (ctorFn != functionMap.end()) {
                    std::vector<llvm::Value*> args;
                    args.push_back(alloca); // self
                    for (auto& arg : call.args) {
                        auto val = generateExpr(*arg);
                        // If it's a struct, pass pointer
                        if (val && val->getType()->isStructTy()) {
                            auto tmp = createEntryBlockAlloca(currentFunction, val->getType(), "tmp");
                            builder.CreateStore(val, tmp);
                            args.push_back(tmp);
                        } else {
                            args.push_back(val);
                        }
                    }
                    builder.CreateCall(ctorFn->second, args);
                }

                // Register for RAII cleanup
                if (!cleanupStack.empty() && stmt.type.kind == TypeSpec::ClassName) {
                    cleanupStack.back().push_back({alloca, stmt.type.className, false});
                }
                return;
            }
        }

        // Special case: owner pointer from new
        if (stmt.type.isOwner && stmt.init->kind == Expr::New) {
            auto val = generateExpr(*stmt.init);
            builder.CreateStore(val, alloca);

            // Register for RAII cleanup (owner: destroy + free)
            if (!cleanupStack.empty()) {
                cleanupStack.back().push_back({alloca, stmt.type.className, true});
            }
            return;
        }

        // Special case: owner array from new Type[n]
        if (stmt.type.isOwner && stmt.type.isArray && stmt.init->kind == Expr::NewArray) {
            auto val = generateExpr(*stmt.init);
            builder.CreateStore(val, alloca);

            if (!cleanupStack.empty()) {
                CleanupEntry entry;
                entry.alloca_ = alloca;
                entry.isOwner = false;
                entry.isOwnerArray = true;
                cleanupStack.back().push_back(entry);
            }
            return;
        }

        auto val = generateExpr(*stmt.init);
        if (val) {
            if (val->getType()->isStructTy() && type->isStructTy()) {
                // Struct copy via store
                builder.CreateStore(val, alloca);
            } else {
                builder.CreateStore(val, alloca);
            }
        }
    } else {
        // Zero-initialize
        if (type->isStructTy()) {
            auto zero = llvm::Constant::getNullValue(type);
            builder.CreateStore(zero, alloca);
        }
    }

    // Register for RAII cleanup if it's a class type on the stack
    if (!cleanupStack.empty() && stmt.type.kind == TypeSpec::ClassName
        && !stmt.type.pointerDepth && !stmt.type.isReference) {
        cleanupStack.back().push_back({alloca, stmt.type.className, false});
    }
}

void CodeGen::generateIf(IfStmt& stmt) {
    auto cond = generateExpr(*stmt.condition);
    if (!cond) return;

    // Convert to i1 if needed
    if (cond->getType() != llvm::Type::getInt1Ty(context)) {
        cond = builder.CreateICmpNE(cond,
                                    llvm::Constant::getNullValue(cond->getType()), "ifcond");
    }

    auto thenBB = llvm::BasicBlock::Create(context, "then", currentFunction);
    auto elseBB = llvm::BasicBlock::Create(context, "else", currentFunction);
    auto mergeBB = llvm::BasicBlock::Create(context, "ifmerge", currentFunction);

    builder.CreateCondBr(cond, thenBB, stmt.elseBranch ? elseBB : mergeBB);

    // Then
    builder.SetInsertPoint(thenBB);
    generateStmt(*stmt.thenBranch);
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(mergeBB);

    // Else
    if (stmt.elseBranch) {
        builder.SetInsertPoint(elseBB);
        generateStmt(*stmt.elseBranch);
        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(mergeBB);
    } else {
        elseBB->eraseFromParent();
    }

    builder.SetInsertPoint(mergeBB);
}

void CodeGen::generateWhile(WhileStmt& stmt) {
    auto condBB = llvm::BasicBlock::Create(context, "while.cond", currentFunction);
    auto bodyBB = llvm::BasicBlock::Create(context, "while.body", currentFunction);
    auto exitBB = llvm::BasicBlock::Create(context, "while.exit", currentFunction);

    auto savedBreak = breakTarget;
    auto savedContinue = continueTarget;
    breakTarget = exitBB;
    continueTarget = condBB;

    builder.CreateBr(condBB);
    builder.SetInsertPoint(condBB);

    auto cond = generateExpr(*stmt.condition);
    if (cond->getType() != llvm::Type::getInt1Ty(context)) {
        cond = builder.CreateICmpNE(cond,
                                    llvm::Constant::getNullValue(cond->getType()), "whilecond");
    }
    builder.CreateCondBr(cond, bodyBB, exitBB);

    builder.SetInsertPoint(bodyBB);
    generateStmt(*stmt.body);
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(condBB);

    builder.SetInsertPoint(exitBB);
    breakTarget = savedBreak;
    continueTarget = savedContinue;
}

void CodeGen::generateFor(ForStmt& stmt) {
    pushCleanupScope();

    if (stmt.init) generateStmt(*stmt.init);

    auto condBB = llvm::BasicBlock::Create(context, "for.cond", currentFunction);
    auto bodyBB = llvm::BasicBlock::Create(context, "for.body", currentFunction);
    auto incrBB = llvm::BasicBlock::Create(context, "for.incr", currentFunction);
    auto exitBB = llvm::BasicBlock::Create(context, "for.exit", currentFunction);

    auto savedBreak = breakTarget;
    auto savedContinue = continueTarget;
    breakTarget = exitBB;
    continueTarget = incrBB;

    builder.CreateBr(condBB);
    builder.SetInsertPoint(condBB);

    if (stmt.condition) {
        auto cond = generateExpr(*stmt.condition);
        if (cond->getType() != llvm::Type::getInt1Ty(context)) {
            cond = builder.CreateICmpNE(cond,
                                        llvm::Constant::getNullValue(cond->getType()), "forcond");
        }
        builder.CreateCondBr(cond, bodyBB, exitBB);
    } else {
        builder.CreateBr(bodyBB);
    }

    builder.SetInsertPoint(bodyBB);
    generateStmt(*stmt.body);
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(incrBB);

    builder.SetInsertPoint(incrBB);
    if (stmt.increment) generateExpr(*stmt.increment);
    builder.CreateBr(condBB);

    builder.SetInsertPoint(exitBB);
    breakTarget = savedBreak;
    continueTarget = savedContinue;
    popCleanupScope();
}

void CodeGen::generateReturn(ReturnStmt& stmt) {
    if (stmt.value) {
        auto val = generateExpr(*stmt.value);
        emitCleanups();
        if (sretPtr_ && val) {
            builder.CreateStore(val, sretPtr_);
            builder.CreateRetVoid();
        } else if (val && val->getType() != llvm::Type::getVoidTy(context)) {
            builder.CreateRet(val);
        } else {
            builder.CreateRetVoid();
        }
    } else {
        emitCleanups();
        builder.CreateRetVoid();
    }
}

void CodeGen::generateDelete(DeleteStmt& stmt) {
    auto val = generateExpr(*stmt.expr);
    if (!val) return;

    // Try to call destructor
    if (stmt.expr->resolvedType.kind == TypeSpec::ClassName) {
        auto dtorName = mangleDestructor(stmt.expr->resolvedType.className);
        auto it = functionMap.find(dtorName);
        if (it != functionMap.end()) {
            builder.CreateCall(it->second, {val});
        }
    }

    auto freeFn = functionMap["free"];
    if (freeFn) {
        builder.CreateCall(freeFn, {val});
    }
}

void CodeGen::generateDefer(DeferStmt& stmt) {
    if (!deferStack.empty()) {
        deferStack.back().push_back(stmt.body.get());
    }
}

void CodeGen::generateDoWhile(DoWhileStmt& stmt) {
    auto bodyBB = llvm::BasicBlock::Create(context, "dowhile.body", currentFunction);
    auto condBB = llvm::BasicBlock::Create(context, "dowhile.cond", currentFunction);
    auto exitBB = llvm::BasicBlock::Create(context, "dowhile.exit", currentFunction);

    auto savedBreak = breakTarget;
    auto savedContinue = continueTarget;
    breakTarget = exitBB;
    continueTarget = condBB;

    builder.CreateBr(bodyBB);
    builder.SetInsertPoint(bodyBB);
    generateStmt(*stmt.body);
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(condBB);

    builder.SetInsertPoint(condBB);
    auto cond = generateExpr(*stmt.condition);
    if (cond->getType() != llvm::Type::getInt1Ty(context)) {
        cond = builder.CreateICmpNE(cond,
                                    llvm::Constant::getNullValue(cond->getType()), "dowhilecond");
    }
    builder.CreateCondBr(cond, bodyBB, exitBB);

    builder.SetInsertPoint(exitBB);
    breakTarget = savedBreak;
    continueTarget = savedContinue;
}

void CodeGen::generateSwitch(SwitchStmt& stmt) {
    auto exprVal = generateExpr(*stmt.expr);
    if (!exprVal) return;

    auto exitBB = llvm::BasicBlock::Create(context, "switch.exit", currentFunction);
    auto savedBreak = breakTarget;
    breakTarget = exitBB;

    // Create all case basic blocks upfront
    std::vector<llvm::BasicBlock*> caseBBs;
    llvm::BasicBlock* defaultBB = exitBB;
    for (size_t i = 0; i < stmt.cases.size(); i++) {
        if (!stmt.cases[i].value) {
            auto bb = llvm::BasicBlock::Create(context, "switch.default", currentFunction);
            caseBBs.push_back(bb);
            defaultBB = bb;
        } else {
            caseBBs.push_back(llvm::BasicBlock::Create(context, "switch.case", currentFunction));
        }
    }

    auto switchInst = builder.CreateSwitch(exprVal, defaultBB, stmt.cases.size());

    for (size_t i = 0; i < stmt.cases.size(); i++) {
        auto& c = stmt.cases[i];
        auto caseBB = caseBBs[i];

        if (c.value) {
            auto caseVal = generateExpr(*c.value);
            if (auto ci = llvm::dyn_cast<llvm::ConstantInt>(caseVal)) {
                switchInst->addCase(ci, caseBB);
            }
        }

        builder.SetInsertPoint(caseBB);

        // Determine next block for fallthrough
        llvm::BasicBlock* nextBB = (i + 1 < caseBBs.size()) ? caseBBs[i + 1] : exitBB;

        // Empty case body: fall through to next case naturally
        if (c.body.empty()) {
            builder.CreateBr(nextBB);
            continue;
        }

        // Check if the last statement is an explicit fallthrough
        bool hasFallthrough = c.body.back()->kind == Stmt::Fallthrough;

        for (auto& s : c.body) {
            if (s->kind == Stmt::Fallthrough) continue; // skip the fallthrough marker
            generateStmt(*s);
            if (builder.GetInsertBlock()->getTerminator()) break;
        }

        if (!builder.GetInsertBlock()->getTerminator()) {
            if (hasFallthrough) {
                builder.CreateBr(nextBB); // explicit fallthrough to next case
            } else {
                builder.CreateBr(exitBB); // implicit break
            }
        }
    }

    builder.SetInsertPoint(exitBB);
    breakTarget = savedBreak;
}

void CodeGen::generateForEach(ForEachStmt& stmt) {
    pushCleanupScope();

    auto varType = getLLVMType(stmt.varType);
    auto alloca = createEntryBlockAlloca(currentFunction, varType, stmt.varName);
    namedValues[stmt.varName] = alloca;

    auto startVal = generateExpr(*stmt.rangeStart);
    builder.CreateStore(startVal, alloca);

    auto condBB = llvm::BasicBlock::Create(context, "foreach.cond", currentFunction);
    auto bodyBB = llvm::BasicBlock::Create(context, "foreach.body", currentFunction);
    auto incrBB = llvm::BasicBlock::Create(context, "foreach.incr", currentFunction);
    auto exitBB = llvm::BasicBlock::Create(context, "foreach.exit", currentFunction);

    auto savedBreak = breakTarget;
    auto savedContinue = continueTarget;
    breakTarget = exitBB;
    continueTarget = incrBB;

    builder.CreateBr(condBB);
    builder.SetInsertPoint(condBB);

    auto endVal = generateExpr(*stmt.rangeEnd);
    auto curVal = builder.CreateLoad(varType, alloca, stmt.varName + ".cur");
    llvm::Value* cond;
    if (stmt.isInclusive) {
        cond = builder.CreateICmpSLE(curVal, endVal, "foreach.cond");
    } else {
        cond = builder.CreateICmpSLT(curVal, endVal, "foreach.cond");
    }
    builder.CreateCondBr(cond, bodyBB, exitBB);

    builder.SetInsertPoint(bodyBB);
    generateStmt(*stmt.body);
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(incrBB);

    builder.SetInsertPoint(incrBB);
    auto cur = builder.CreateLoad(varType, alloca, "foreach.val");
    auto next = builder.CreateAdd(cur, llvm::ConstantInt::get(varType, 1), "foreach.next");
    builder.CreateStore(next, alloca);
    builder.CreateBr(condBB);

    builder.SetInsertPoint(exitBB);
    breakTarget = savedBreak;
    continueTarget = savedContinue;
    popCleanupScope();
}

void CodeGen::generateThrow(ThrowStmt& stmt) {
    auto val = generateExpr(*stmt.expr);
    if (!val) return;

    // Determine the exception type name
    std::string typeName = "Exception";
    if (stmt.expr->resolvedType.kind == TypeSpec::ClassName) {
        typeName = stmt.expr->resolvedType.className;
    }

    // Create global string constant for type name
    auto typeNameStr = builder.CreateGlobalString(typeName, ".exc.type." + typeName);

    // Call __lpl_throw(exception_ptr, type_name_str)
    auto throwFn = functionMap["__lpl_throw"];
    builder.CreateCall(throwFn, {val, typeNameStr});
    builder.CreateUnreachable();
}

void CodeGen::generateTry(TryStmt& stmt) {
    // 1. Call __lpl_try_enter() to get a pointer to the jmp_buf
    auto tryEnterFn = functionMap["__lpl_try_enter"];
    auto jmpBufPtr = builder.CreateCall(tryEnterFn, {}, "jmpbuf");

    // 2. Call setjmp(jmpBufPtr)
    auto setjmpFn = functionMap["setjmp"];
    auto setjmpResult = builder.CreateCall(setjmpFn, {jmpBufPtr}, "setjmp.res");

    // 3. Branch: 0 = try body, non-0 = exception was thrown
    auto isZero = builder.CreateICmpEQ(setjmpResult,
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), "is.try");

    auto tryBodyBB = llvm::BasicBlock::Create(context, "try.body", currentFunction);
    auto catchDispatchBB = llvm::BasicBlock::Create(context, "catch.dispatch", currentFunction);
    auto tryExitBB = llvm::BasicBlock::Create(context, "try.exit", currentFunction);

    builder.CreateCondBr(isZero, tryBodyBB, catchDispatchBB);

    // 4. Try body
    builder.SetInsertPoint(tryBodyBB);
    if (stmt.body) {
        for (auto& s : stmt.body->stmts) {
            generateStmt(*s);
            if (builder.GetInsertBlock()->getTerminator()) break;
        }
    }
    // If try body completed normally, pop the handler and jump to exit
    if (!builder.GetInsertBlock()->getTerminator()) {
        auto tryLeaveFn = functionMap["__lpl_try_leave"];
        builder.CreateCall(tryLeaveFn, {});
        builder.CreateBr(tryExitBB);
    }

    // 5. Catch dispatch: check exception type against each catch clause
    builder.SetInsertPoint(catchDispatchBB);

    auto excIsTypeFn = functionMap["__lpl_exception_is_type"];
    auto excCurrentFn = functionMap["__lpl_exception_current"];
    auto excClearFn = functionMap["__lpl_exception_clear"];

    llvm::BasicBlock* lastUnmatchedBB = nullptr;

    for (size_t i = 0; i < stmt.catches.size(); i++) {
        auto& cc = stmt.catches[i];

        std::string typeName = "Exception";
        if (cc.exceptionType.kind == TypeSpec::ClassName) {
            typeName = cc.exceptionType.className;
        }

        // Check if exception matches this catch type
        auto typeStr = builder.CreateGlobalString(typeName, ".catch.type." + typeName);
        auto matchResult = builder.CreateCall(excIsTypeFn, {typeStr}, "match");
        auto isMatch = builder.CreateICmpNE(matchResult,
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), "is.match");

        auto catchBodyBB = llvm::BasicBlock::Create(context, "catch.body." + std::to_string(i), currentFunction);
        auto nextCatchBB = llvm::BasicBlock::Create(context, "catch.next." + std::to_string(i), currentFunction);

        builder.CreateCondBr(isMatch, catchBodyBB, nextCatchBB);

        // Catch body
        builder.SetInsertPoint(catchBodyBB);
        pushCleanupScope();

        // Bind exception variable
        auto excPtr = builder.CreateCall(excCurrentFn, {}, "exc.ptr");
        auto excAlloca = createEntryBlockAlloca(currentFunction,
            llvm::PointerType::get(context, 0), cc.varName);
        builder.CreateStore(excPtr, excAlloca);
        namedValues[cc.varName] = excAlloca;

        if (cc.body) {
            for (auto& s : cc.body->stmts) {
                generateStmt(*s);
                if (builder.GetInsertBlock()->getTerminator()) break;
            }
        }

        popCleanupScope();

        if (!builder.GetInsertBlock()->getTerminator()) {
            builder.CreateCall(excClearFn, {});
            builder.CreateBr(tryExitBB);
        }

        // Move to next catch check
        builder.SetInsertPoint(nextCatchBB);
        lastUnmatchedBB = nextCatchBB;
    }

    // If no catch matched, re-throw the exception
    if (lastUnmatchedBB) {
        // We're already at the last unmatched BB
        auto excPtr = builder.CreateCall(excCurrentFn, {}, "rethrow.ptr");
        auto excType = builder.CreateCall(functionMap["__lpl_exception_type"], {}, "rethrow.type");
        auto throwFn = functionMap["__lpl_throw"];
        builder.CreateCall(throwFn, {excPtr, excType});
        builder.CreateUnreachable();
    }

    // 6. Exit block (with optional finally)
    builder.SetInsertPoint(tryExitBB);

    if (stmt.finallyBody) {
        for (auto& s : stmt.finallyBody->stmts) {
            generateStmt(*s);
            if (builder.GetInsertBlock()->getTerminator()) break;
        }
    }
}

// ============================================================
// Expression generation
// ============================================================

llvm::Value* CodeGen::generateExpr(Expr& expr) {
    switch (expr.kind) {
        case Expr::IntLit: {
            auto& e = static_cast<IntLitExpr&>(expr);
            return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), e.value);
        }

        case Expr::FloatLit: {
            auto& e = static_cast<FloatLitExpr&>(expr);
            return llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), e.value);
        }

        case Expr::StringLit: {
            auto& e = static_cast<StringLitExpr&>(expr);
            return generateStringLit(e.value);
        }

        case Expr::BoolLit: {
            auto& e = static_cast<BoolLitExpr&>(expr);
            return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), e.value ? 1 : 0);
        }

        case Expr::CharLit: {
            auto& e = static_cast<CharLitExpr&>(expr);
            return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), (uint32_t)e.value);
        }

        case Expr::NullLit:
            return llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));

        case Expr::This: {
            auto it = namedValues.find("this");
            if (it != namedValues.end()) {
                return builder.CreateLoad(llvm::PointerType::get(context, 0), it->second, "this");
            }
            return nullptr;
        }

        case Expr::Super: {
            // super uses the same `this` pointer — parent methods are called on same object
            auto it = namedValues.find("this");
            if (it != namedValues.end()) {
                return builder.CreateLoad(llvm::PointerType::get(context, 0), it->second, "this");
            }
            return nullptr;
        }

        case Expr::Ident: {
            auto& e = static_cast<IdentExpr&>(expr);
            auto it = namedValues.find(e.name);
            if (it != namedValues.end()) {
                auto allocaType = it->second->getAllocatedType();
                return builder.CreateLoad(allocaType, it->second, e.name);
            }
            return nullptr;
        }

        case Expr::Binary:
            return generateBinary(static_cast<BinaryExpr&>(expr));

        case Expr::Unary: {
            auto& e = static_cast<UnaryExpr&>(expr);
            auto val = generateExpr(*e.operand);
            if (!val) return nullptr;
            if (e.op == TokenType::Minus) {
                if (val->getType()->isFloatingPointTy())
                    return builder.CreateFNeg(val, "fneg");
                return builder.CreateNeg(val, "neg");
            }
            if (e.op == TokenType::Bang) {
                if (val->getType() != llvm::Type::getInt1Ty(context)) {
                    val = builder.CreateICmpNE(val,
                                               llvm::Constant::getNullValue(val->getType()), "tobool");
                }
                return builder.CreateNot(val, "not");
            }
            return val;
        }

        case Expr::Assign: {
            auto& e = static_cast<AssignExpr&>(expr);
            auto val = generateExpr(*e.value);
            if (!val) return nullptr;

            // Get address of target
            if (e.target->kind == Expr::Ident) {
                auto& ident = static_cast<IdentExpr&>(*e.target);
                auto it = namedValues.find(ident.name);
                if (it != namedValues.end()) {
                    builder.CreateStore(val, it->second);
                    return val;
                }
            } else if (e.target->kind == Expr::MemberAccess) {
                auto addr = generateMemberAccess(
                    static_cast<MemberAccessExpr&>(*e.target), true);
                if (addr) {
                    builder.CreateStore(val, addr);
                    return val;
                }
            } else if (e.target->kind == Expr::Index) {
                auto addr = generateIndex(static_cast<IndexExpr&>(*e.target), true);
                if (addr) {
                    builder.CreateStore(val, addr);
                    return val;
                }
            }
            return val;
        }

        case Expr::Call:
            return generateCall(static_cast<CallExpr&>(expr));

        case Expr::MethodCall:
            return generateMethodCall(static_cast<MethodCallExpr&>(expr));

        case Expr::MemberAccess:
            return generateMemberAccess(static_cast<MemberAccessExpr&>(expr));

        case Expr::New:
            return generateNew(static_cast<NewExpr&>(expr));

        case Expr::NewArray:
            return generateNewArray(static_cast<NewArrayExpr&>(expr));

        case Expr::Move: {
            auto& e = static_cast<MoveExpr&>(expr);
            auto val = generateExpr(*e.operand);
            // Mark the source as moved in cleanup tracking
            if (e.operand->kind == Expr::Ident) {
                auto& ident = static_cast<IdentExpr&>(*e.operand);
                for (auto& scope : cleanupStack) {
                    for (auto& entry : scope) {
                        if (namedValues.count(ident.name) &&
                            namedValues[ident.name] == entry.alloca_) {
                            entry.isMoved = true;
                        }
                    }
                }
            }
            return val;
        }

        case Expr::AddressOf: {
            auto& e = static_cast<AddressOfExpr&>(expr);
            if (e.operand->kind == Expr::Ident) {
                auto& ident = static_cast<IdentExpr&>(*e.operand);
                auto it = namedValues.find(ident.name);
                if (it != namedValues.end()) return it->second;
            }
            return generateExpr(*e.operand);
        }

        case Expr::Deref: {
            auto& e = static_cast<DerefExpr&>(expr);
            auto val = generateExpr(*e.operand);
            if (!val) return nullptr;
            // Load through pointer - need to know pointee type
            auto& rt = e.operand->resolvedType;
            if (rt.kind == TypeSpec::ClassName) {
                auto ct = getOrCreateClassType(rt.className);
                return builder.CreateLoad(ct, val, "deref");
            }
            return val;
        }

        case Expr::Index:
            return generateIndex(static_cast<IndexExpr&>(expr));

        case Expr::Cast:
            return generateCast(static_cast<CastExpr&>(expr));

        case Expr::BoundaryConvert:
            return generateBoundaryConvert(static_cast<BoundaryConvertExpr&>(expr));

        case Expr::Ternary:
            return generateTernary(static_cast<TernaryExpr&>(expr));

        case Expr::Lambda: {
            auto& e = static_cast<LambdaExpr&>(expr);

            // Generate a unique function for this lambda
            std::string lambdaName = "__lambda_" + std::to_string(lambdaCounter++);

            // Create function type: (ptr %env, [ptr %sret,] params...) -> R
            std::vector<llvm::Type*> paramTypes;
            // env pointer is always first param
            paramTypes.push_back(llvm::PointerType::get(context, 0));

            for (auto& p : e.params) {
                auto pt = getLLVMType(p.type);
                if (pt->isStructTy()) paramTypes.push_back(llvm::PointerType::get(context, 0));
                else paramTypes.push_back(pt);
            }
            auto retTy = getLLVMType(e.returnType);
            bool lambdaSret = retTy->isStructTy();
            llvm::Type* llvmRetTy = retTy;
            if (lambdaSret) {
                // Insert sret pointer after env
                paramTypes.insert(paramTypes.begin() + 1, llvm::PointerType::get(context, 0));
                llvmRetTy = llvm::Type::getVoidTy(context);
            }
            auto funcTy = llvm::FunctionType::get(llvmRetTy, paramTypes, false);
            auto lambdaFn = llvm::Function::Create(funcTy, llvm::Function::InternalLinkage,
                                                     lambdaName, *module);

            // Build the closure environment struct if there are captures
            llvm::Value* envPtr = llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));
            llvm::StructType* envStructTy = nullptr;

            if (!e.resolvedCaptures.empty()) {
                // Create env struct type: { captured_type_0, captured_type_1, ... }
                std::vector<llvm::Type*> envFields;
                for (auto& cap : e.resolvedCaptures) {
                    if (cap.byRef) {
                        envFields.push_back(llvm::PointerType::get(context, 0)); // pointer to var
                    } else {
                        envFields.push_back(getLLVMType(cap.type));
                    }
                }
                envStructTy = llvm::StructType::get(context, envFields);

                // Malloc the env struct
                auto mallocFn = module->getOrInsertFunction("malloc",
                    llvm::FunctionType::get(llvm::PointerType::get(context, 0),
                        {llvm::Type::getInt64Ty(context)}, false));
                auto envSize = module->getDataLayout().getTypeAllocSize(envStructTy);
                auto sizeVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), envSize);
                envPtr = builder.CreateCall(mallocFn, {sizeVal}, "env.alloc");

                // Populate the env struct with captured values
                for (size_t i = 0; i < e.resolvedCaptures.size(); i++) {
                    auto& cap = e.resolvedCaptures[i];
                    auto fieldPtr = builder.CreateStructGEP(envStructTy, envPtr, i,
                        "env." + cap.name);
                    auto it = namedValues.find(cap.name);
                    if (it != namedValues.end()) {
                        if (cap.byRef) {
                            // Store pointer to the original variable
                            builder.CreateStore(it->second, fieldPtr);
                        } else {
                            // Copy the value
                            auto val = builder.CreateLoad(getLLVMType(cap.type), it->second,
                                cap.name + ".cap");
                            builder.CreateStore(val, fieldPtr);
                        }
                    }
                }
            }

            // Save current state
            auto savedFn = currentFunction;
            auto savedValues = namedValues;
            auto savedClass = currentClassName;
            auto savedBlock = builder.GetInsertBlock();
            auto savedPoint = builder.GetInsertPoint();
            auto savedSret = sretPtr_;

            // Generate function body
            auto bb = llvm::BasicBlock::Create(context, "entry", lambdaFn);
            builder.SetInsertPoint(bb);
            currentFunction = lambdaFn;
            namedValues.clear();
            currentClassName = "";
            sretPtr_ = nullptr;

            auto argIt = lambdaFn->arg_begin();

            // First arg is always env pointer
            llvm::Value* lambdaEnvPtr = &*argIt;
            lambdaEnvPtr->setName("env");
            argIt++;

            if (lambdaSret) {
                sretPtr_ = &*argIt;
                sretPtr_->setName("sret");
                argIt++;
            }

            // Unpack captured variables from env into local allocas
            if (!e.resolvedCaptures.empty() && envStructTy) {
                for (size_t i = 0; i < e.resolvedCaptures.size(); i++) {
                    auto& cap = e.resolvedCaptures[i];
                    auto capTy = getLLVMType(cap.type);
                    auto fieldPtr = builder.CreateStructGEP(envStructTy, lambdaEnvPtr, i,
                        "env." + cap.name);
                    if (cap.byRef) {
                        // Load the pointer to the original, then use it directly
                        auto origPtr = builder.CreateLoad(
                            llvm::PointerType::get(context, 0), fieldPtr,
                            cap.name + ".ref");
                        namedValues[cap.name] = llvm::cast<llvm::AllocaInst>(
                            createEntryBlockAlloca(lambdaFn, capTy, cap.name));
                        // For by-ref, we want to alias the original. Store pointer as the alloca content.
                        // Actually, use the original pointer directly as the "alloca"
                        // We need a different approach: store the ptr and use it
                        // Simplification: treat by-ref capture as a pointer load
                        namedValues[cap.name] = static_cast<llvm::AllocaInst*>(nullptr);
                        // Create a local alloca that holds the pointer to original
                        auto ptrAlloca = createEntryBlockAlloca(lambdaFn,
                            llvm::PointerType::get(context, 0), cap.name + ".ptr");
                        builder.CreateStore(origPtr, ptrAlloca);
                        // For by-ref, we store the loaded current value into a regular alloca
                        // and any writes would go through the pointer. For simplicity, just
                        // load the value at lambda entry (snapshot behavior for by-ref on primitives).
                        auto alloca = createEntryBlockAlloca(lambdaFn, capTy, cap.name);
                        auto val = builder.CreateLoad(capTy, origPtr, cap.name + ".deref");
                        builder.CreateStore(val, alloca);
                        namedValues[cap.name] = alloca;
                    } else {
                        // By-value: load from env struct into local alloca
                        auto alloca = createEntryBlockAlloca(lambdaFn, capTy, cap.name);
                        auto val = builder.CreateLoad(capTy, fieldPtr, cap.name + ".val");
                        builder.CreateStore(val, alloca);
                        namedValues[cap.name] = alloca;
                    }
                }
            }

            for (size_t i = 0; i < e.params.size(); i++, argIt++) {
                auto& p = e.params[i];
                argIt->setName(p.name);
                auto pt = getLLVMType(p.type);
                auto alloca = createEntryBlockAlloca(lambdaFn, pt, p.name);
                if (pt->isStructTy()) {
                    auto loaded = builder.CreateLoad(pt, &*argIt, p.name + ".val");
                    builder.CreateStore(loaded, alloca);
                } else {
                    builder.CreateStore(&*argIt, alloca);
                }
                namedValues[p.name] = alloca;
            }

            pushCleanupScope();
            if (e.body) {
                for (auto& stmt : e.body->stmts) {
                    generateStmt(*stmt);
                    if (builder.GetInsertBlock()->getTerminator()) break;
                }
            }
            popCleanupScope();

            if (!builder.GetInsertBlock()->getTerminator()) {
                if (e.returnType.isVoid() || lambdaSret) {
                    builder.CreateRetVoid();
                } else {
                    builder.CreateRet(llvm::Constant::getNullValue(retTy));
                }
            }

            // Restore state
            currentFunction = savedFn;
            namedValues = savedValues;
            currentClassName = savedClass;
            sretPtr_ = savedSret;
            builder.SetInsertPoint(savedBlock, savedPoint);

            // Return callable struct { fn_ptr, env_ptr }
            auto callableTy = llvm::StructType::get(context, {
                llvm::PointerType::get(context, 0),
                llvm::PointerType::get(context, 0)
            });
            llvm::Value* callableVal = llvm::UndefValue::get(callableTy);
            callableVal = builder.CreateInsertValue(callableVal, lambdaFn, 0, "callable.fn");
            callableVal = builder.CreateInsertValue(callableVal, envPtr, 1, "callable.env");
            return callableVal;
        }

        default:
            return nullptr;
    }
}

llvm::Value* CodeGen::generateBinary(BinaryExpr& expr) {
    // Check for operator overloading on class types
    if (expr.left->resolvedType.kind == TypeSpec::ClassName
        && !expr.left->resolvedType.pointerDepth) {
        std::string opName;
        switch (expr.op) {
            case TokenType::Plus:    opName = "operator+"; break;
            case TokenType::Minus:   opName = "operator-"; break;
            case TokenType::Star:    opName = "operator*"; break;
            case TokenType::Slash:   opName = "operator/"; break;
            case TokenType::Percent: opName = "operator%"; break;
            case TokenType::EqEq:    opName = "operator=="; break;
            case TokenType::BangEq:  opName = "operator!="; break;
            case TokenType::Lt:      opName = "operator<"; break;
            case TokenType::Gt:      opName = "operator>"; break;
            case TokenType::LtEq:    opName = "operator<="; break;
            case TokenType::GtEq:    opName = "operator>="; break;
            default: break;
        }

        if (!opName.empty()) {
            std::string className = expr.left->resolvedType.className;
            auto mangledName = mangleMethod(className, opName);
            auto it = functionMap.find(mangledName);
            // Search parent hierarchy
            if (it == functionMap.end()) {
                std::string cur = className;
                while (it == functionMap.end()) {
                    auto ci = sema.lookupClass(cur);
                    if (!ci || ci->parent.empty()) break;
                    cur = ci->parent;
                    mangledName = mangleMethod(cur, opName);
                    it = functionMap.find(mangledName);
                }
            }

            if (it != functionMap.end()) {
                // Get address of left operand (self)
                llvm::Value* selfPtr = nullptr;
                if (expr.left->kind == Expr::Ident) {
                    auto& ident = static_cast<IdentExpr&>(*expr.left);
                    auto nv = namedValues.find(ident.name);
                    if (nv != namedValues.end()) selfPtr = nv->second;
                } else if (expr.left->kind == Expr::This) {
                    auto nv = namedValues.find("this");
                    if (nv != namedValues.end()) {
                        selfPtr = builder.CreateLoad(llvm::PointerType::get(context, 0),
                                                     nv->second, "this");
                    }
                }
                if (!selfPtr) {
                    // Temporary — store in alloca
                    auto leftVal = generateExpr(*expr.left);
                    if (leftVal) {
                        selfPtr = createEntryBlockAlloca(currentFunction, leftVal->getType(), "op.self");
                        builder.CreateStore(leftVal, selfPtr);
                    }
                }

                auto rightVal = generateExpr(*expr.right);

                std::vector<llvm::Value*> args;
                args.push_back(selfPtr); // self

                auto retLLVMTy = getLLVMType(expr.resolvedType);
                bool hasSret = retLLVMTy && retLLVMTy->isStructTy();
                llvm::AllocaInst* sretBuf = nullptr;
                if (hasSret) {
                    sretBuf = createEntryBlockAlloca(currentFunction, retLLVMTy, "op.sret");
                    args.push_back(sretBuf);
                }

                // Pass right operand
                if (rightVal && rightVal->getType()->isStructTy()) {
                    auto tmp = createEntryBlockAlloca(currentFunction, rightVal->getType(), "op.rhs");
                    builder.CreateStore(rightVal, tmp);
                    args.push_back(tmp);
                } else {
                    args.push_back(rightVal);
                }

                if (hasSret) {
                    builder.CreateCall(it->second, args);
                    return builder.CreateLoad(retLLVMTy, sretBuf, "op.result");
                }
                auto retTy = it->second->getReturnType();
                if (retTy->isVoidTy()) {
                    builder.CreateCall(it->second, args);
                    return nullptr;
                }
                return builder.CreateCall(it->second, args, "op.call");
            }
        }
    }

    auto left = generateExpr(*expr.left);
    auto right = generateExpr(*expr.right);
    if (!left || !right) return nullptr;

    // String concatenation
    if (expr.op == TokenType::Plus &&
        (expr.left->resolvedType.kind == TypeSpec::String ||
         expr.right->resolvedType.kind == TypeSpec::String)) {
        // Ensure both are strings (TODO: convert non-strings)
        return generateStringConcat(left, right);
    }

    // Floating point operations
    if (left->getType()->isFloatingPointTy() && right->getType()->isFloatingPointTy()) {
        switch (expr.op) {
            case TokenType::Plus:  return builder.CreateFAdd(left, right, "fadd");
            case TokenType::Minus: return builder.CreateFSub(left, right, "fsub");
            case TokenType::Star:  return builder.CreateFMul(left, right, "fmul");
            case TokenType::Slash: return builder.CreateFDiv(left, right, "fdiv");
            case TokenType::EqEq:  return builder.CreateFCmpOEQ(left, right, "feq");
            case TokenType::BangEq:return builder.CreateFCmpONE(left, right, "fne");
            case TokenType::Lt:    return builder.CreateFCmpOLT(left, right, "flt");
            case TokenType::Gt:    return builder.CreateFCmpOGT(left, right, "fgt");
            case TokenType::LtEq:  return builder.CreateFCmpOLE(left, right, "fle");
            case TokenType::GtEq:  return builder.CreateFCmpOGE(left, right, "fge");
            default: break;
        }
    }

    // Integer operations
    if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
        // Widen to matching sizes
        if (left->getType() != right->getType()) {
            auto leftBits = left->getType()->getIntegerBitWidth();
            auto rightBits = right->getType()->getIntegerBitWidth();
            if (leftBits < rightBits) {
                left = builder.CreateSExt(left, right->getType(), "sext");
            } else {
                right = builder.CreateSExt(right, left->getType(), "sext");
            }
        }

        switch (expr.op) {
            case TokenType::Plus:    return builder.CreateAdd(left, right, "add");
            case TokenType::Minus:   return builder.CreateSub(left, right, "sub");
            case TokenType::Star:    return builder.CreateMul(left, right, "mul");
            case TokenType::Slash:   return builder.CreateSDiv(left, right, "div");
            case TokenType::Percent: return builder.CreateSRem(left, right, "rem");
            case TokenType::EqEq:    return builder.CreateICmpEQ(left, right, "eq");
            case TokenType::BangEq:  return builder.CreateICmpNE(left, right, "ne");
            case TokenType::Lt:      return builder.CreateICmpSLT(left, right, "lt");
            case TokenType::Gt:      return builder.CreateICmpSGT(left, right, "gt");
            case TokenType::LtEq:    return builder.CreateICmpSLE(left, right, "le");
            case TokenType::GtEq:    return builder.CreateICmpSGE(left, right, "ge");
            case TokenType::AmpAmp: {
                auto l = builder.CreateICmpNE(left, llvm::Constant::getNullValue(left->getType()));
                auto r = builder.CreateICmpNE(right, llvm::Constant::getNullValue(right->getType()));
                return builder.CreateAnd(l, r, "and");
            }
            case TokenType::PipePipe: {
                auto l = builder.CreateICmpNE(left, llvm::Constant::getNullValue(left->getType()));
                auto r = builder.CreateICmpNE(right, llvm::Constant::getNullValue(right->getType()));
                return builder.CreateOr(l, r, "or");
            }
            case TokenType::Ampersand: return builder.CreateAnd(left, right, "bitand");
            case TokenType::Pipe:      return builder.CreateOr(left, right, "bitor");
            case TokenType::Caret:     return builder.CreateXor(left, right, "bitxor");
            case TokenType::LShift:    return builder.CreateShl(left, right, "shl");
            case TokenType::RShift:    return builder.CreateAShr(left, right, "shr");
            default: break;
        }
    }

    // Pointer comparison
    if (left->getType()->isPointerTy() && right->getType()->isPointerTy()) {
        switch (expr.op) {
            case TokenType::EqEq:   return builder.CreateICmpEQ(left, right, "ptreq");
            case TokenType::BangEq: return builder.CreateICmpNE(left, right, "ptrne");
            default: break;
        }
    }

    return nullptr;
}

llvm::Value* CodeGen::generateCall(CallExpr& expr) {
    // Check if it's a constructor call (class name as function)
    auto ci = sema.lookupClass(expr.callee);
    if (ci) {
        // Stack construction: allocate and call constructor, return the struct value
        auto classType = getOrCreateClassType(expr.callee);
        auto alloca = createEntryBlockAlloca(currentFunction, classType, "tmp.obj");

        auto ctorName = mangleConstructor(expr.callee);
        auto ctorIt = functionMap.find(ctorName);
        if (ctorIt != functionMap.end()) {
            std::vector<llvm::Value*> args;
            args.push_back(alloca);
            for (auto& arg : expr.args) {
                auto val = generateExpr(*arg);
                if (val && val->getType()->isStructTy()) {
                    auto tmp = createEntryBlockAlloca(currentFunction, val->getType(), "arg.tmp");
                    builder.CreateStore(val, tmp);
                    args.push_back(tmp);
                } else {
                    args.push_back(val);
                }
            }
            builder.CreateCall(ctorIt->second, args);
        }

        return builder.CreateLoad(classType, alloca, "obj");
    }

    // Regular function call
    std::string mangledName = mangleName(expr.callee);
    auto it = functionMap.find(mangledName);
    if (it == functionMap.end()) {
        // Try unmangled (extern C functions)
        it = functionMap.find(expr.callee);
    }
    if (it == functionMap.end()) {
        // Try as callable variable (lambda / function pointer)
        auto namedIt = namedValues.find(expr.callee);
        if (namedIt != namedValues.end()) {
            // Load the callable struct { fn_ptr, env_ptr }
            auto callableTy = llvm::StructType::get(context, {
                llvm::PointerType::get(context, 0),
                llvm::PointerType::get(context, 0)
            });
            auto callableVal = builder.CreateLoad(callableTy, namedIt->second, "callable");
            auto fnPtr = builder.CreateExtractValue(callableVal, 0, "fn.ptr");
            auto envPtr = builder.CreateExtractValue(callableVal, 1, "env.ptr");

            // Build argument list: env_ptr first, then actual args
            std::vector<llvm::Type*> argTypes;
            std::vector<llvm::Value*> argVals;

            // env pointer is always first
            argTypes.push_back(llvm::PointerType::get(context, 0));
            argVals.push_back(envPtr);

            auto retTy = getLLVMType(expr.resolvedType);

            // Handle sret for struct returns
            bool isSret = retTy->isStructTy();
            llvm::AllocaInst* sretAlloca = nullptr;
            if (isSret) {
                sretAlloca = createEntryBlockAlloca(currentFunction, retTy, "sret.tmp");
                argTypes.push_back(llvm::PointerType::get(context, 0));
                argVals.push_back(sretAlloca);
            }

            for (auto& arg : expr.args) {
                auto val = generateExpr(*arg);
                if (val && val->getType()->isStructTy()) {
                    auto tmp = createEntryBlockAlloca(currentFunction, val->getType(), "arg.tmp");
                    builder.CreateStore(val, tmp);
                    argTypes.push_back(llvm::PointerType::get(context, 0));
                    argVals.push_back(tmp);
                } else if (val) {
                    argTypes.push_back(val->getType());
                    argVals.push_back(val);
                }
            }

            llvm::Type* callRetTy = isSret ? llvm::Type::getVoidTy(context) : retTy;
            auto funcTy = llvm::FunctionType::get(callRetTy, argTypes, false);

            if (isSret) {
                builder.CreateCall(funcTy, fnPtr, argVals);
                return builder.CreateLoad(retTy, sretAlloca, "sret.val");
            }
            if (retTy->isVoidTy()) {
                builder.CreateCall(funcTy, fnPtr, argVals);
                return nullptr;
            }
            return builder.CreateCall(funcTy, fnPtr, argVals, "lambda.call");
        }
        return nullptr;
    }

    // Look up function info for default params
    auto fi = sema.lookupFunction(expr.callee);

    // Check if this call requires sret (extern functions with struct returns)
    auto exprRetTy = getLLVMType(expr.resolvedType);
    bool callHasSret = exprRetTy && exprRetTy->isStructTy() &&
                       it->second->getReturnType()->isVoidTy();
    llvm::AllocaInst* sretBuf = nullptr;

    std::vector<llvm::Value*> args;
    if (callHasSret) {
        sretBuf = createEntryBlockAlloca(currentFunction, exprRetTy, "sret.buf");
        args.push_back(sretBuf);
    }

    for (size_t i = 0; i < expr.args.size(); i++) {
        auto val = generateExpr(*expr.args[i]);
        if (val && val->getType()->isStructTy()) {
            auto tmp = createEntryBlockAlloca(currentFunction, val->getType(), "arg.tmp");
            builder.CreateStore(val, tmp);
            args.push_back(tmp);
        } else {
            args.push_back(val);
        }
    }
    // Fill in default parameter values
    if (fi) {
        for (size_t i = expr.args.size(); i < fi->params.size(); i++) {
            if (fi->params[i].defaultValue) {
                auto val = generateExpr(*fi->params[i].defaultValue);
                args.push_back(val);
            }
        }
    }

    if (callHasSret) {
        builder.CreateCall(it->second, args);
        return builder.CreateLoad(exprRetTy, sretBuf, "sret.val");
    }
    auto retType = it->second->getReturnType();
    if (retType->isVoidTy()) {
        builder.CreateCall(it->second, args);
        return nullptr;
    }
    return builder.CreateCall(it->second, args, "call");
}

llvm::Value* CodeGen::generateMethodCall(MethodCallExpr& expr) {
    // Handle super calls: super(args) or super.method(args)
    if (expr.object->kind == Expr::Super) {
        std::string parentClass = expr.object->resolvedType.className;
        if (parentClass.empty()) return nullptr;

        // Get 'this' pointer
        auto nv = namedValues.find("this");
        if (nv == namedValues.end()) return nullptr;
        auto thisPtr = builder.CreateLoad(llvm::PointerType::get(context, 0),
                                          nv->second, "this");

        if (expr.method == "<init>") {
            // super(args) → call parent constructor
            auto ctorName = mangleConstructor(parentClass);
            auto it = functionMap.find(ctorName);
            if (it == functionMap.end()) return nullptr;

            std::vector<llvm::Value*> args;
            args.push_back(thisPtr); // self
            for (auto& a : expr.args) {
                auto val = generateExpr(*a);
                if (val && val->getType()->isStructTy()) {
                    auto tmp = createEntryBlockAlloca(currentFunction, val->getType(), "arg.tmp");
                    builder.CreateStore(val, tmp);
                    args.push_back(tmp);
                } else {
                    args.push_back(val);
                }
            }
            builder.CreateCall(it->second, args);
            return nullptr;
        } else {
            // super.method(args) → call parent's method directly
            auto mangledName = mangleMethod(parentClass, expr.method);
            auto it = functionMap.find(mangledName);
            if (it == functionMap.end()) return nullptr;

            std::vector<llvm::Value*> args;
            args.push_back(thisPtr); // self

            auto exprRetTy = getLLVMType(expr.resolvedType);
            bool callHasSret = exprRetTy && exprRetTy->isStructTy();
            llvm::AllocaInst* sretBuf = nullptr;
            if (callHasSret) {
                sretBuf = createEntryBlockAlloca(currentFunction, exprRetTy, "sret.buf");
                args.push_back(sretBuf);
            }

            for (auto& a : expr.args) {
                auto val = generateExpr(*a);
                if (val && val->getType()->isStructTy()) {
                    auto tmp = createEntryBlockAlloca(currentFunction, val->getType(), "arg.tmp");
                    builder.CreateStore(val, tmp);
                    args.push_back(tmp);
                } else {
                    args.push_back(val);
                }
            }

            if (callHasSret) {
                builder.CreateCall(it->second, args);
                return builder.CreateLoad(exprRetTy, sretBuf, "sret.val");
            }
            auto retType = it->second->getReturnType();
            if (retType->isVoidTy()) {
                builder.CreateCall(it->second, args);
                return nullptr;
            }
            return builder.CreateCall(it->second, args, "super.call");
        }
    }

    // Check if sema resolved this as a namespaced constructor call
    // e.g., App.Models.Greeter("Hello") → resolvedType = App.Models.Greeter
    if (expr.resolvedType.kind == TypeSpec::ClassName && !expr.resolvedType.className.empty()) {
        std::string qName = expr.resolvedType.className;
        auto ci = sema.lookupClass(qName);
        if (ci) {
            // Stack construction: allocate and call constructor
            auto classType = getOrCreateClassType(qName);
            auto alloca = createEntryBlockAlloca(currentFunction, classType, "tmp.obj");
            auto ctorName = mangleConstructor(qName);
            auto ctorIt = functionMap.find(ctorName);
            if (ctorIt != functionMap.end()) {
                std::vector<llvm::Value*> args;
                args.push_back(alloca);
                for (auto& arg : expr.args) {
                    auto val = generateExpr(*arg);
                    if (val && val->getType()->isStructTy()) {
                        auto tmp = createEntryBlockAlloca(currentFunction, val->getType(), "arg.tmp");
                        builder.CreateStore(val, tmp);
                        args.push_back(tmp);
                    } else {
                        args.push_back(val);
                    }
                }
                builder.CreateCall(ctorIt->second, args);
            }
            return builder.CreateLoad(classType, alloca, "obj");
        }
    }

    // Resolve the object's class
    std::string className;
    if (expr.object->resolvedType.kind == TypeSpec::ClassName) {
        className = expr.object->resolvedType.className;
    }

    // Handle static calls: the object expression resolves to a class name
    // (not a variable of that class type). This includes:
    //   Console.println(...)          — Expr::Ident
    //   Std.IO.Console.println(...)   — chain of MemberAccess
    bool isStaticClassRef = false;
    if (expr.object->kind == Expr::Ident) {
        auto& ident = static_cast<IdentExpr&>(*expr.object);
        if (sema.lookupClass(ident.name)) {
            className = expr.object->resolvedType.className;
            isStaticClassRef = true;
        }
    } else if (expr.object->kind == Expr::MemberAccess) {
        // Sema may have resolved a dotted chain to a class name
        // Check if the resolvedType.className matches a known class
        if (!className.empty() && sema.lookupClass(className)) {
            isStaticClassRef = true;
        }
    }

    if (isStaticClassRef) {
        auto mangledName = mangleMethod(className, expr.method);
        auto it = functionMap.find(mangledName);
        if (it != functionMap.end()) {
            std::vector<llvm::Value*> args;

            // Look up method param types for coercion
            const MethodDecl* methodDecl = nullptr;
            if (auto ci = sema.lookupClass(className)) {
                for (auto& m : ci->methods) {
                    if (m.name == expr.method) { methodDecl = &m; break; }
                }
            }

            // Check if the method uses sret (returns struct via output pointer)
            auto exprRetTy = getLLVMType(expr.resolvedType);
            bool callHasSret = exprRetTy && exprRetTy->isStructTy();
            llvm::AllocaInst* sretBuf = nullptr;
            if (callHasSret) {
                sretBuf = createEntryBlockAlloca(currentFunction, exprRetTy, "sret.buf");
                args.push_back(sretBuf);
            }

            for (auto& a : expr.args) {
                auto val = generateExpr(*a);
                if (val && val->getType()->isStructTy()) {
                    auto tmp = createEntryBlockAlloca(currentFunction, val->getType(), "arg.tmp");
                    builder.CreateStore(val, tmp);
                    args.push_back(tmp);
                } else {
                    args.push_back(val);
                }
            }

            if (callHasSret) {
                builder.CreateCall(it->second, args);
                return builder.CreateLoad(exprRetTy, sretBuf, "sret.val");
            }
            auto retType = it->second->getReturnType();
            if (retType->isVoidTy()) {
                builder.CreateCall(it->second, args);
                return nullptr;
            }
            return builder.CreateCall(it->second, args, "scall");
        }
    }

    if (className.empty()) return nullptr;

    // Instance method call — search class hierarchy for the method
    auto mangledName = mangleMethod(className, expr.method);
    auto it = functionMap.find(mangledName);
    if (it == functionMap.end()) {
        // Search parent classes
        std::string cur = className;
        while (it == functionMap.end()) {
            auto ci = sema.lookupClass(cur);
            if (!ci || ci->parent.empty()) break;
            cur = ci->parent;
            mangledName = mangleMethod(cur, expr.method);
            it = functionMap.find(mangledName);
        }
    }
    if (it == functionMap.end()) return nullptr;

    // Get object pointer
    llvm::Value* objPtr = nullptr;
    if (expr.object->resolvedType.pointerDepth) {
        // Already a pointer
        objPtr = generateExpr(*expr.object);
    } else {
        // Need address of value
        if (expr.object->kind == Expr::Ident) {
            auto& ident = static_cast<IdentExpr&>(*expr.object);
            auto nv = namedValues.find(ident.name);
            if (nv != namedValues.end()) objPtr = nv->second;
        } else if (expr.object->kind == Expr::This) {
            auto nv = namedValues.find("this");
            if (nv != namedValues.end()) {
                objPtr = builder.CreateLoad(llvm::PointerType::get(context, 0),
                                            nv->second, "this");
            }
        }
    }

    if (!objPtr) return nullptr;

    std::vector<llvm::Value*> args;
    args.push_back(objPtr); // self

    // Look up method param types for coercion
    const MethodDecl* methodDecl = nullptr;
    if (auto ci = sema.lookupClass(className)) {
        for (auto& m : ci->methods) {
            if (m.name == expr.method) { methodDecl = &m; break; }
        }
    }

    // Check if the method uses sret (returns struct via output pointer)
    auto exprRetTy = getLLVMType(expr.resolvedType);
    bool callHasSret = exprRetTy && exprRetTy->isStructTy();
    llvm::AllocaInst* sretBuf = nullptr;
    if (callHasSret) {
        sretBuf = createEntryBlockAlloca(currentFunction, exprRetTy, "sret.buf");
        args.push_back(sretBuf);
    }

    for (auto& a : expr.args) {
        auto val = generateExpr(*a);
        if (val && val->getType()->isStructTy()) {
            auto tmp = createEntryBlockAlloca(currentFunction, val->getType(), "arg.tmp");
            builder.CreateStore(val, tmp);
            args.push_back(tmp);
        } else {
            args.push_back(val);
        }
    }

    if (callHasSret) {
        builder.CreateCall(it->second, args);
        return builder.CreateLoad(exprRetTy, sretBuf, "sret.val");
    }
    auto retType = it->second->getReturnType();
    if (retType->isVoidTy()) {
        builder.CreateCall(it->second, args);
        return nullptr;
    }
    return builder.CreateCall(it->second, args, "mcall");
}

llvm::Value* CodeGen::generateMemberAccess(MemberAccessExpr& expr, bool wantAddress) {
    // Array .length property
    if (expr.object->resolvedType.isArray && expr.member == "length") {
        auto arrPtr = generateExpr(*expr.object);
        if (!arrPtr) return nullptr;
        // Length is stored at (dataPtr - 8)
        auto i64Ty = llvm::Type::getInt64Ty(context);
        auto minusOne = llvm::ConstantInt::getSigned(i64Ty, -1);
        auto lenPtr = builder.CreateGEP(i64Ty, arrPtr, {minusOne}, "len.ptr");
        return builder.CreateLoad(i64Ty, lenPtr, "arr.length");
    }

    std::string className;
    if (expr.object->resolvedType.kind == TypeSpec::ClassName) {
        className = expr.object->resolvedType.className;
    }
    if (className.empty()) return nullptr;

    auto ci = sema.lookupClass(className);
    if (!ci) return nullptr;

    // Find field index
    int parentFieldCount = 0;
    if (!ci->parent.empty()) {
        auto parentCi = sema.lookupClass(ci->parent);
        if (parentCi) parentFieldCount = (int)parentCi->fields.size();
    }

    int fieldIdx = -1;
    TypeSpec fieldType;
    for (int i = 0; i < (int)ci->fields.size(); i++) {
        if (ci->fields[i].name == expr.member) {
            fieldIdx = parentFieldCount + i;
            fieldType = ci->fields[i].type;
            break;
        }
    }
    if (fieldIdx < 0) return nullptr;

    // Get object pointer
    llvm::Value* objPtr = nullptr;
    if (expr.object->resolvedType.pointerDepth) {
        objPtr = generateExpr(*expr.object);
    } else {
        if (expr.object->kind == Expr::Ident) {
            auto& ident = static_cast<IdentExpr&>(*expr.object);
            auto nv = namedValues.find(ident.name);
            if (nv != namedValues.end()) objPtr = nv->second;
        } else if (expr.object->kind == Expr::This) {
            auto nv = namedValues.find("this");
            if (nv != namedValues.end()) {
                objPtr = builder.CreateLoad(llvm::PointerType::get(context, 0),
                                            nv->second, "this");
            }
        }
    }
    if (!objPtr) return nullptr;

    auto classType = getOrCreateClassType(className);
    auto gep = builder.CreateStructGEP(classType, objPtr, fieldIdx, expr.member + ".ptr");

    if (wantAddress) return gep;

    auto ft = getLLVMType(fieldType);
    return builder.CreateLoad(ft, gep, expr.member);
}

llvm::Value* CodeGen::generateNew(NewExpr& expr) {
    auto className = expr.type.className;
    auto classType = getOrCreateClassType(className);

    // Calculate size
    auto dataLayout = module->getDataLayout();
    auto size = dataLayout.getTypeAllocSize(classType);

    // Call malloc
    auto mallocFn = functionMap["malloc"];
    auto sizeVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), size);
    auto raw = builder.CreateCall(mallocFn, {sizeVal}, "new.raw");

    // Zero-initialize
    builder.CreateMemSet(raw, builder.getInt8(0), sizeVal, llvm::MaybeAlign(8));

    // Call constructor
    auto ctorName = mangleConstructor(className);
    auto ctorIt = functionMap.find(ctorName);
    if (ctorIt != functionMap.end()) {
        std::vector<llvm::Value*> args;
        args.push_back(raw);
        for (auto& arg : expr.args) {
            auto val = generateExpr(*arg);
            if (val && val->getType()->isStructTy()) {
                auto tmp = createEntryBlockAlloca(currentFunction, val->getType(), "arg.tmp");
                builder.CreateStore(val, tmp);
                args.push_back(tmp);
            } else {
                args.push_back(val);
            }
        }
        builder.CreateCall(ctorIt->second, args);
    }

    return raw;
}

llvm::Value* CodeGen::generateNewArray(NewArrayExpr& expr) {
    auto i64Ty = llvm::Type::getInt64Ty(context);
    auto elemType = getLLVMType(expr.elementType);
    auto dataLayout = module->getDataLayout();
    uint64_t elemSize = dataLayout.getTypeAllocSize(elemType);

    // Compute count (extend to i64 if needed)
    auto count = generateExpr(*expr.size);
    if (!count) return nullptr;
    if (count->getType() != i64Ty) {
        count = builder.CreateSExt(count, i64Ty, "count.ext");
    }

    // Total bytes = 8 (header for length) + count * elemSize
    auto elemSizeVal = llvm::ConstantInt::get(i64Ty, elemSize);
    auto dataBytes = builder.CreateMul(count, elemSizeVal, "data.bytes");
    auto headerSize = llvm::ConstantInt::get(i64Ty, 8);
    auto totalBytes = builder.CreateAdd(dataBytes, headerSize, "total.bytes");

    // malloc
    auto mallocFn = functionMap["malloc"];
    auto raw = builder.CreateCall(mallocFn, {totalBytes}, "arr.raw");

    // Zero-initialize the entire allocation
    builder.CreateMemSet(raw, builder.getInt8(0), totalBytes, llvm::MaybeAlign(8));

    // Store length in the header (first 8 bytes)
    auto lenPtr = raw;
    builder.CreateStore(count, lenPtr);

    // Return pointer to data (raw + 8)
    auto dataPtr = builder.CreateGEP(i64Ty, raw, {llvm::ConstantInt::get(i64Ty, 1)}, "arr.data");
    return dataPtr;
}

llvm::Value* CodeGen::generateIndex(IndexExpr& expr, bool wantAddress) {
    auto obj = generateExpr(*expr.object);
    auto idx = generateExpr(*expr.index);
    if (!obj || !idx) return nullptr;

    // Determine element type
    auto& objType = expr.object->resolvedType;
    llvm::Type* elemType = nullptr;
    if (objType.isArray) {
        TypeSpec elemTs = objType;
        elemTs.isArray = false;
        elemType = getLLVMType(elemTs);
    } else if (objType.pointerDepth) {
        TypeSpec pointeeTs = objType;
        pointeeTs.pointerDepth--;
        elemType = getLLVMType(pointeeTs);
    } else {
        elemType = llvm::Type::getInt8Ty(context);
    }

    auto gep = builder.CreateGEP(elemType, obj, {idx}, "idx.ptr");
    if (wantAddress) return gep;
    return builder.CreateLoad(elemType, gep, "idx.val");
}

llvm::Value* CodeGen::generateCast(CastExpr& expr) {
    auto val = generateExpr(*expr.operand);
    if (!val) return nullptr;

    auto srcType = val->getType();
    auto dstType = getLLVMType(expr.targetType);

    if (srcType == dstType) return val;

    // Int -> Int
    if (srcType->isIntegerTy() && dstType->isIntegerTy()) {
        auto srcBits = srcType->getIntegerBitWidth();
        auto dstBits = dstType->getIntegerBitWidth();
        if (srcBits < dstBits) return builder.CreateSExt(val, dstType, "cast.sext");
        if (srcBits > dstBits) return builder.CreateTrunc(val, dstType, "cast.trunc");
        return val;
    }
    // Int -> Float
    if (srcType->isIntegerTy() && dstType->isFloatingPointTy()) {
        return builder.CreateSIToFP(val, dstType, "cast.sitofp");
    }
    // Float -> Int
    if (srcType->isFloatingPointTy() && dstType->isIntegerTy()) {
        return builder.CreateFPToSI(val, dstType, "cast.fptosi");
    }
    // Float -> Float
    if (srcType->isFloatingPointTy() && dstType->isFloatingPointTy()) {
        if (srcType->getPrimitiveSizeInBits() < dstType->getPrimitiveSizeInBits())
            return builder.CreateFPExt(val, dstType, "cast.fpext");
        return builder.CreateFPTrunc(val, dstType, "cast.fptrunc");
    }
    // Pointer -> Pointer (opaque pointers — identity)
    if (srcType->isPointerTy() && dstType->isPointerTy()) {
        return val;
    }

    return val;
}

llvm::Value* CodeGen::generateBoundaryConvert(BoundaryConvertExpr& expr) {
    auto val = generateExpr(*expr.operand);
    if (!val) return nullptr;

    // char* -> string: string(charPtr)
    if (expr.targetType.kind == TypeSpec::String && !expr.targetType.pointerDepth
        && expr.operand->resolvedType.kind == TypeSpec::Char
        && expr.operand->resolvedType.pointerDepth == 1) {
        return generateCharPtrToString(val);
    }

    // string -> char*: char*(str) — extract the data pointer from LPLString
    if (expr.operand->resolvedType.kind == TypeSpec::String
        && !expr.operand->resolvedType.pointerDepth
        && expr.targetType.pointerDepth > 0
        && val->getType()->isStructTy()) {
        auto tmp = createEntryBlockAlloca(currentFunction, val->getType(), "str.tmp");
        builder.CreateStore(val, tmp);
        auto dataPtr = builder.CreateStructGEP(getStringType(), tmp, 0, "str.data.ptr");
        return builder.CreateLoad(llvm::PointerType::get(context, 0), dataPtr, "str.data");
    }

    // Pointer -> Pointer (opaque pointers — identity)
    auto srcType = val->getType();
    auto dstType = getLLVMType(expr.targetType);
    if (srcType->isPointerTy() && dstType->isPointerTy()) {
        return val;
    }

    return val;
}

llvm::Value* CodeGen::generateTernary(TernaryExpr& expr) {
    auto cond = generateExpr(*expr.condition);
    if (!cond) return nullptr;

    if (cond->getType() != llvm::Type::getInt1Ty(context)) {
        cond = builder.CreateICmpNE(cond,
                                    llvm::Constant::getNullValue(cond->getType()), "terncond");
    }

    auto thenBB = llvm::BasicBlock::Create(context, "tern.then", currentFunction);
    auto elseBB = llvm::BasicBlock::Create(context, "tern.else", currentFunction);
    auto mergeBB = llvm::BasicBlock::Create(context, "tern.merge", currentFunction);

    builder.CreateCondBr(cond, thenBB, elseBB);

    builder.SetInsertPoint(thenBB);
    auto thenVal = generateExpr(*expr.thenExpr);
    thenBB = builder.GetInsertBlock(); // update in case of nested blocks
    builder.CreateBr(mergeBB);

    builder.SetInsertPoint(elseBB);
    auto elseVal = generateExpr(*expr.elseExpr);
    elseBB = builder.GetInsertBlock();
    builder.CreateBr(mergeBB);

    builder.SetInsertPoint(mergeBB);
    if (!thenVal || !elseVal) return nullptr;

    auto phi = builder.CreatePHI(thenVal->getType(), 2, "ternval");
    phi->addIncoming(thenVal, thenBB);
    phi->addIncoming(elseVal, elseBB);
    return phi;
}

// ============================================================
// String helpers
// ============================================================

llvm::Value* CodeGen::generateStringLit(const std::string& val) {
    // Create global string constant
    auto strConst = builder.CreateGlobalString(val, ".str");

    // Allocate LPLString on stack and call __lpl_string_create
    auto strAlloca = createEntryBlockAlloca(currentFunction, getStringType(), "str.tmp");
    auto createFn = functionMap["__lpl_string_create"];
    auto len = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), val.size());
    builder.CreateCall(createFn, {strAlloca, strConst, len});

    return builder.CreateLoad(getStringType(), strAlloca, "str");
}

llvm::Value* CodeGen::generateCharPtrToString(llvm::Value* charPtr) {
    // Call strlen to get length, then __lpl_string_create
    auto strlenFn = module->getOrInsertFunction("strlen",
        llvm::FunctionType::get(llvm::Type::getInt64Ty(context),
                                {llvm::PointerType::get(context, 0)}, false));
    auto len = builder.CreateCall(strlenFn, {charPtr}, "len");

    auto strAlloca = createEntryBlockAlloca(currentFunction, getStringType(), "str.fromcptr");
    auto createFn = functionMap["__lpl_string_create"];
    builder.CreateCall(createFn, {strAlloca, charPtr, len});

    return builder.CreateLoad(getStringType(), strAlloca, "str.val");
}


llvm::Value* CodeGen::generateStringConcat(llvm::Value* left, llvm::Value* right) {
    auto concatFn = functionMap["__lpl_string_concat"];
    if (!concatFn) return left;

    auto resultAlloca = createEntryBlockAlloca(currentFunction, getStringType(), "concat.result");
    auto leftAlloca = createEntryBlockAlloca(currentFunction, getStringType(), "concat.left");
    auto rightAlloca = createEntryBlockAlloca(currentFunction, getStringType(), "concat.right");

    builder.CreateStore(left, leftAlloca);
    builder.CreateStore(right, rightAlloca);
    builder.CreateCall(concatFn, {resultAlloca, leftAlloca, rightAlloca});

    return builder.CreateLoad(getStringType(), resultAlloca, "concat");
}

// ============================================================
// Optimization passes
// ============================================================

void CodeGen::runOptimizationPasses() {
    if (optLevel <= 0) return;

    llvm::OptimizationLevel level;
    switch (optLevel) {
        case 1:  level = llvm::OptimizationLevel::O1; break;
        case 2:  level = llvm::OptimizationLevel::O2; break;
        case 3:  level = llvm::OptimizationLevel::O3; break;
        case 4:  level = llvm::OptimizationLevel::Os; break;
        case 5:  level = llvm::OptimizationLevel::Oz; break;
        default: level = llvm::OptimizationLevel::O2; break;
    }

    llvm::PassBuilder PB;
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(level);
    MPM.run(*module, MAM);
}

// ============================================================
// Output emission
// ============================================================

bool CodeGen::emitObjectFile(const std::string& filename) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
    if (!target) {
        std::cerr << "error: " << error << "\n";
        return false;
    }

    auto cgOptLevel = llvm::CodeGenOptLevel::None;
    if (optLevel == 1) cgOptLevel = llvm::CodeGenOptLevel::Less;
    else if (optLevel >= 2) cgOptLevel = llvm::CodeGenOptLevel::Default;

    llvm::TargetOptions opt;
    auto tm = target->createTargetMachine(llvm::Triple(targetTriple),
                                           "generic", "", opt,
                                           llvm::Reloc::PIC_,
                                           std::nullopt, cgOptLevel);
    module->setDataLayout(tm->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream dest(filename, ec, llvm::sys::fs::OF_None);
    if (ec) {
        std::cerr << "error: could not open file: " << ec.message() << "\n";
        return false;
    }

    llvm::legacy::PassManager pass;
    if (tm->addPassesToEmitFile(pass, dest, nullptr,
                                 llvm::CodeGenFileType::ObjectFile)) {
        std::cerr << "error: target machine can't emit object file\n";
        return false;
    }

    pass.run(*module);
    dest.flush();
    return true;
}

bool CodeGen::emitLLVMIR(const std::string& filename) {
    std::error_code ec;
    llvm::raw_fd_ostream dest(filename, ec, llvm::sys::fs::OF_None);
    if (ec) {
        std::cerr << "error: could not open file: " << ec.message() << "\n";
        return false;
    }
    module->print(dest, nullptr);
    return true;
}

bool CodeGen::emitAssembly(const std::string& filename) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
    if (!target) {
        std::cerr << "error: " << error << "\n";
        return false;
    }

    auto cgOptLevel = llvm::CodeGenOptLevel::None;
    if (optLevel == 1) cgOptLevel = llvm::CodeGenOptLevel::Less;
    else if (optLevel >= 2) cgOptLevel = llvm::CodeGenOptLevel::Default;

    llvm::TargetOptions opt;
    auto tm = target->createTargetMachine(llvm::Triple(targetTriple),
                                           "generic", "", opt,
                                           llvm::Reloc::PIC_,
                                           std::nullopt, cgOptLevel);
    module->setDataLayout(tm->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream dest(filename, ec, llvm::sys::fs::OF_None);
    if (ec) {
        std::cerr << "error: could not open file: " << ec.message() << "\n";
        return false;
    }

    llvm::legacy::PassManager pass;
    if (tm->addPassesToEmitFile(pass, dest, nullptr,
                                 llvm::CodeGenFileType::AssemblyFile)) {
        std::cerr << "error: target machine can't emit assembly\n";
        return false;
    }

    pass.run(*module);
    dest.flush();
    return true;
}