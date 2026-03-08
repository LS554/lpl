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

#include "sema.h"
#include <sstream>
#include <iostream>

static std::string makeInstantiatedName(const std::string& baseName,
                                         const std::vector<TypeSpec>& typeArgs);

void Sema::error(const SourceLoc& loc, const std::string& msg) {
    std::ostringstream oss;
    oss << loc.file << ":" << loc.line << ":" << loc.col << ": error: " << msg;
    errors.push_back(oss.str());
}

void Sema::pushScope() { scopes.push_back({}); }
void Sema::popScope() { scopes.pop_back(); }

void Sema::declareVar(const std::string& name, const TypeSpec& type, const SourceLoc& loc, bool isConst) {
    if (!scopes.empty()) {
        auto& scope = scopes.back();
        if (scope.vars.count(name)) {
            error(loc, "variable '" + name + "' already declared in this scope");
            return;
        }
        scope.vars[name] = {type, isConst};
    }
}

TypeSpec* Sema::lookupVar(const std::string& name) {
    for (int i = (int)scopes.size() - 1; i >= 0; i--) {
        auto it = scopes[i].vars.find(name);
        if (it != scopes[i].vars.end()) {
            // Track auto-capture for [=] / [&] lambdas
            if (captureTracker_ && i < captureTracker_->lambdaScopeDepth) {
                auto& rc = captureTracker_->lambda->resolvedCaptures;
                bool found = false;
                for (auto& c : rc) if (c.name == name) { found = true; break; }
                if (!found) {
                    rc.push_back({name, it->second.type, captureTracker_->captureByRef});
                }
            }
            return &it->second.type;
        }
    }
    return nullptr;
}

bool Sema::isVarConst(const std::string& name) {
    for (int i = (int)scopes.size() - 1; i >= 0; i--) {
        auto it = scopes[i].vars.find(name);
        if (it != scopes[i].vars.end()) return it->second.isConst;
    }
    return false;
}

const ClassInfo* Sema::lookupClass(const std::string& name) const {
    // Try exact name first
    auto it = classes.find(name);
    if (it != classes.end()) return &it->second;
    // Try with current namespace prefix
    if (!currentNamespace_.empty()) {
        auto it2 = classes.find(currentNamespace_ + "." + name);
        if (it2 != classes.end()) return &it2->second;
    }
    return nullptr;
}

const FuncInfo* Sema::lookupFunction(const std::string& name) const {
    auto it = functions.find(name);
    if (it != functions.end()) return &it->second;
    if (!currentNamespace_.empty()) {
        auto it2 = functions.find(currentNamespace_ + "." + name);
        if (it2 != functions.end()) return &it2->second;
    }
    return nullptr;
}

std::string Sema::resolveClassName(const std::string& name) const {
    if (classes.count(name)) return name;
    if (!currentNamespace_.empty()) {
        std::string qualified = currentNamespace_ + "." + name;
        if (classes.count(qualified)) return qualified;
    }
    return name; // return as-is, let caller handle error
}

bool Sema::isNamespacePrefix(const std::string& prefix) const {
    std::string test = prefix + ".";
    for (auto& [name, _] : classes) {
        if (name.size() > test.size() && name.substr(0, test.size()) == test)
            return true;
    }
    for (auto& [name, _] : functions) {
        if (name.size() > test.size() && name.substr(0, test.size()) == test)
            return true;
    }
    return false;
}

// Flatten a chain of MemberAccess/Ident expressions into a dotted string.
// Returns empty string if the chain contains non-Ident/MemberAccess nodes.
std::string Sema::flattenDottedExpr(Expr& expr) {
    if (expr.kind == Expr::Ident) {
        return static_cast<IdentExpr&>(expr).name;
    }
    if (expr.kind == Expr::MemberAccess) {
        auto& ma = static_cast<MemberAccessExpr&>(expr);
        std::string prefix = flattenDottedExpr(*ma.object);
        if (prefix.empty()) return "";
        return prefix + "." + ma.member;
    }
    return "";
}

int Sema::getFieldIndex(const std::string& className, const std::string& fieldName) const {
    auto ci = lookupClass(className);
    if (!ci) return -1;
    for (int i = 0; i < (int)ci->fields.size(); i++) {
        if (ci->fields[i].name == fieldName) return i;
    }
    // Check parent
    if (!ci->parent.empty()) {
        return getFieldIndex(ci->parent, fieldName);
    }
    return -1;
}

bool Sema::isTypeValid(const TypeSpec& type) {
    if (type.kind == TypeSpec::ClassName) {
        // If this is a generic instantiation (e.g. Box<int>), ensure it's been instantiated
        if (!type.typeArgs.empty()) {
            // Need a mutable copy to pass to ensureGenericClassInstantiated
            TypeSpec mutable_type = type;
            ensureGenericClassInstantiated(mutable_type);
            return lookupClass(mutable_type.className) != nullptr;
        }
        return lookupClass(type.className) != nullptr;
    }
    if (type.kind == TypeSpec::Callable) {
        return true; // callable types are always valid structurally
    }
    return true;
}

bool Sema::typesCompatible(const TypeSpec& target, const TypeSpec& source) {
    if (target == source) return true;

    // null is compatible with any pointer type
    if (target.pointerDepth && source.kind == TypeSpec::Void) return true;

    // Numeric widening
    if (target.isNumeric() && source.isNumeric()) return true;

    // Owner pointer to non-owner pointer (implicit borrow)
    if (target.pointerDepth && source.pointerDepth && !target.isOwner && source.isOwner
        && target.kind == source.kind && target.className == source.className) {
        return true;
    }

    // Class inheritance compatibility
    if (target.kind == TypeSpec::ClassName && source.kind == TypeSpec::ClassName
        && !target.pointerDepth && !source.pointerDepth) {
        // Check if source is a subclass of target
        std::string cur = source.className;
        while (!cur.empty()) {
            if (cur == target.className) return true;
            auto ci = lookupClass(cur);
            if (!ci) break;
            cur = ci->parent;
        }
    }

    // Pointer to class inheritance
    if (target.pointerDepth && source.pointerDepth
        && target.kind == TypeSpec::ClassName && source.kind == TypeSpec::ClassName) {
        std::string cur = source.className;
        while (!cur.empty()) {
            if (cur == target.className) return true;
            auto ci = lookupClass(cur);
            if (!ci) break;
            cur = ci->parent;
        }
    }

    return false;
}

// ============================================================
// Top-level analysis
// ============================================================

bool Sema::analyze(Program& prog) {
    currentProgram_ = &prog;
    registerBuiltins();
    registerDeclarations(prog);

    for (auto& decl : prog.declarations) {
        analyzeDecl(*decl);
    }

    currentProgram_ = nullptr;
    return errors.empty();
}

void Sema::registerBuiltins() {
    // No hardcoded builtins — everything comes from .lph headers.
    // This method is kept as an extension point for future compiler intrinsics.
}

void Sema::registerDeclarations(Program& prog) {
    for (auto& decl : prog.declarations) {
        if (auto cls = std::dynamic_pointer_cast<ClassDecl>(decl)) {
            // Generic class template — store as template, don't register as concrete class
            if (!cls->typeParams.empty()) {
                genericClassTemplates[cls->qualifiedName()] = cls;
                continue;
            }
            ClassInfo ci;
            ci.name = cls->qualifiedName();
            ci.parent = cls->parentClass;
            ci.fields = cls->fields;
            ci.methods = cls->methods;
            ci.constructors = cls->constructors;
            ci.hasDestructor = cls->destructor != nullptr;
            ci.isAbstract = cls->isAbstract;
            classes[cls->qualifiedName()] = std::move(ci);
        } else if (auto fn = std::dynamic_pointer_cast<FunctionDecl>(decl)) {
            // Generic function template — store as template, don't register as concrete function
            if (!fn->typeParams.empty()) {
                genericFuncTemplates[fn->qualifiedName()] = fn;
                continue;
            }
            FuncInfo fi;
            fi.returnType = fn->returnType;
            fi.params = fn->params;
            functions[fn->qualifiedName()] = std::move(fi);
        } else if (auto ext = std::dynamic_pointer_cast<ExternBlockDecl>(decl)) {
            for (auto& ef : ext->functions) {
                FuncInfo fi;
                fi.returnType = ef.returnType;
                fi.params = ef.params;
                fi.isExtern = true;
                fi.isVariadic = ef.isVariadic;
                functions[ef.name] = std::move(fi);
            }
        }
    }
}

void Sema::analyzeDecl(Decl& decl) {
    if (auto cls = dynamic_cast<ClassDecl*>(&decl)) {
        // Skip generic class templates — they're analyzed after instantiation
        if (!cls->typeParams.empty()) return;
        analyzeClass(*cls);
    } else if (auto fn = dynamic_cast<FunctionDecl*>(&decl)) {
        // Skip generic function templates
        if (!fn->typeParams.empty()) return;
        analyzeFunction(*fn);
    }
    // interfaces and extern blocks are validated during registration
}

// ============================================================
// Class analysis
// ============================================================

void Sema::analyzeClass(ClassDecl& cls) {
    // Validate parent exists
    if (!cls.parentClass.empty() && !lookupClass(cls.parentClass)) {
        error(cls.loc, "parent class '" + cls.parentClass + "' not found");
    }

    // Validate abstract methods are only in abstract classes
    for (auto& method : cls.methods) {
        if (method.isAbstract && !cls.isAbstract) {
            error(method.loc, "abstract method '" + method.name + "' in non-abstract class '" + cls.name + "'");
        }
    }

    // If concrete class with parent, check all abstract methods are implemented
    if (!cls.isAbstract && !cls.parentClass.empty()) {
        auto parentCi = lookupClass(cls.parentClass);
        while (parentCi) {
            for (auto& pm : parentCi->methods) {
                if (pm.isAbstract) {
                    bool implemented = false;
                    for (auto& m : cls.methods) {
                        if (m.name == pm.name && !m.isAbstract) {
                            implemented = true;
                            break;
                        }
                    }
                    if (!implemented) {
                        error(cls.loc, "concrete class '" + cls.name
                              + "' must implement abstract method '" + pm.name + "'");
                    }
                }
            }
            if (parentCi->parent.empty()) break;
            parentCi = lookupClass(parentCi->parent);
        }
    }

    // Validate field types
    for (auto& field : cls.fields) {
        if (!isTypeValid(field.type)) {
            error(field.loc, "unknown type '" + field.type.toString() + "'");
        }
    }

    std::string savedNamespace = currentNamespace_;
    currentNamespace_ = cls.namespacePath;
    currentClassName = cls.qualifiedName();

    // Analyze constructors
    for (auto& ctor : cls.constructors) {
        analyzeConstructor(cls.qualifiedName(), ctor);
    }

    // Analyze methods
    for (auto& method : cls.methods) {
        analyzeMethod(cls.qualifiedName(), method);
    }

    // Analyze destructor body
    if (cls.destructor && cls.destructor->body) {
        currentClassName = cls.qualifiedName();
        pushScope();
        TypeSpec thisType;
        thisType.kind = TypeSpec::ClassName;
        thisType.className = cls.qualifiedName();
        thisType.pointerDepth = 1;
        declareVar("this", thisType, cls.destructor->loc);
        for (auto& stmt : cls.destructor->body->stmts) {
            analyzeStmt(*stmt);
        }
        popScope();
    }

    currentClassName = "";
    currentNamespace_ = savedNamespace;
}

void Sema::analyzeConstructor(const std::string& className, ConstructorDecl& ctor) {
    currentClassName = className;
    pushScope();

    // Declare 'this'
    TypeSpec thisType;
    thisType.kind = TypeSpec::ClassName;
    thisType.className = className;
    thisType.pointerDepth = 1;
    declareVar("this", thisType, ctor.loc);

    // Declare parameters
    for (auto& p : ctor.params) {
        if (!isTypeValid(p.type)) {
            error(ctor.loc, "unknown parameter type '" + p.type.toString() + "'");
        }
        declareVar(p.name, p.type, ctor.loc);
    }

    // Analyze body
    if (ctor.body) {
        for (auto& stmt : ctor.body->stmts) {
            analyzeStmt(*stmt);
        }
    }

    popScope();
}

void Sema::analyzeMethod(const std::string& className, MethodDecl& method) {
    currentClassName = className;

    // Validate override keyword
    if (method.isOverride) {
        auto ci = lookupClass(className);
        if (!ci || ci->parent.empty()) {
            error(method.loc, "method '" + method.name + "' marked override but class has no parent");
        } else {
            auto parentCi = lookupClass(ci->parent);
            bool found = false;
            while (parentCi) {
                for (auto& pm : parentCi->methods) {
                    if (pm.name == method.name) { found = true; break; }
                }
                if (found) break;
                if (parentCi->parent.empty()) break;
                parentCi = lookupClass(parentCi->parent);
            }
            if (!found) {
                error(method.loc, "method '" + method.name + "' marked override but no parent method found");
            }
        }
    }

    pushScope();

    if (!method.isStatic) {
        TypeSpec thisType;
        thisType.kind = TypeSpec::ClassName;
        thisType.className = className;
        thisType.pointerDepth = 1;
        declareVar("this", thisType, method.loc);
    }

    for (auto& p : method.params) {
        if (!isTypeValid(p.type)) {
            error(method.loc, "unknown parameter type '" + p.type.toString() + "'");
        }
        declareVar(p.name, p.type, method.loc);
    }

    if (method.body) {
        for (auto& stmt : method.body->stmts) {
            analyzeStmt(*stmt);
        }
    }

    popScope();
    currentClassName = "";
}

void Sema::analyzeFunction(FunctionDecl& fn) {
    std::string savedNamespace = currentNamespace_;
    currentNamespace_ = fn.namespacePath;
    pushScope();

    for (auto& p : fn.params) {
        if (!isTypeValid(p.type)) {
            error(fn.loc, "unknown parameter type '" + p.type.toString() + "'");
        }
        declareVar(p.name, p.type, fn.loc);
    }

    if (fn.body) {
        for (auto& stmt : fn.body->stmts) {
            analyzeStmt(*stmt);
        }
    }

    popScope();
    currentNamespace_ = savedNamespace;
}

// ============================================================
// Statement analysis
// ============================================================

void Sema::analyzeStmt(Stmt& stmt) {
    switch (stmt.kind) {
        case Stmt::Block: {
            auto& s = static_cast<BlockStmt&>(stmt);
            analyzeBlock(s);
            break;
        }
        case Stmt::ExprS: {
            auto& s = static_cast<ExprStmt&>(stmt);
            analyzeExpr(*s.expr);
            break;
        }
        case Stmt::VarDecl: {
            auto& s = static_cast<VarDeclStmt&>(stmt);

            // Handle auto type deduction
            if (s.type.kind == TypeSpec::Auto) {
                if (!s.init) {
                    error(s.loc, "'auto' variable '" + s.name + "' must have an initializer");
                } else {
                    TypeSpec initType = analyzeExpr(*s.init);
                    s.type = initType; // replace auto with deduced type
                }
                declareVar(s.name, s.type, s.loc, s.isConst);
                break;
            }

            // Resolve qualified class name
            if (s.type.kind == TypeSpec::ClassName) {
                // Handle generic instantiation
                if (!s.type.typeArgs.empty()) {
                    ensureGenericClassInstantiated(s.type);
                } else {
                    s.type.className = resolveClassName(s.type.className);
                }
            }
            if (!isTypeValid(s.type)) {
                error(s.loc, "unknown type '" + s.type.toString() + "'");
            }
            if (s.isConst && !s.init) {
                error(s.loc, "const variable '" + s.name + "' must be initialized");
            }
            if (s.init) {
                TypeSpec initType = analyzeExpr(*s.init);
                if (!typesCompatible(s.type, initType)) {
                    error(s.loc, "cannot initialize '" + s.type.toString()
                          + "' with '" + initType.toString() + "'");
                }
            }
            declareVar(s.name, s.type, s.loc, s.isConst);
            break;
        }
        case Stmt::Return: {
            auto& s = static_cast<ReturnStmt&>(stmt);
            if (s.value) {
                analyzeExpr(*s.value);
            }
            break;
        }
        case Stmt::If: {
            auto& s = static_cast<IfStmt&>(stmt);
            analyzeExpr(*s.condition);
            analyzeStmt(*s.thenBranch);
            if (s.elseBranch) analyzeStmt(*s.elseBranch);
            break;
        }
        case Stmt::While: {
            auto& s = static_cast<WhileStmt&>(stmt);
            analyzeExpr(*s.condition);
            analyzeStmt(*s.body);
            break;
        }
        case Stmt::For: {
            auto& s = static_cast<ForStmt&>(stmt);
            pushScope();
            if (s.init) analyzeStmt(*s.init);
            if (s.condition) analyzeExpr(*s.condition);
            if (s.increment) analyzeExpr(*s.increment);
            analyzeStmt(*s.body);
            popScope();
            break;
        }
        case Stmt::Delete: {
            auto& s = static_cast<DeleteStmt&>(stmt);
            TypeSpec t = analyzeExpr(*s.expr);
            if (!t.pointerDepth) {
                error(s.loc, "delete requires a pointer type");
            }
            break;
        }
        case Stmt::Break:
        case Stmt::Continue:
            break;
        case Stmt::Defer: {
            auto& s = static_cast<DeferStmt&>(stmt);
            analyzeStmt(*s.body);
            break;
        }
        case Stmt::DoWhile: {
            auto& s = static_cast<DoWhileStmt&>(stmt);
            analyzeStmt(*s.body);
            analyzeExpr(*s.condition);
            break;
        }
        case Stmt::Switch: {
            auto& s = static_cast<SwitchStmt&>(stmt);
            analyzeExpr(*s.expr);
            for (auto& c : s.cases) {
                if (c.value) analyzeExpr(*c.value);
                for (auto& st : c.body) analyzeStmt(*st);
            }
            break;
        }
        case Stmt::ForEach: {
            auto& s = static_cast<ForEachStmt&>(stmt);
            pushScope();
            declareVar(s.varName, s.varType, s.loc);
            analyzeExpr(*s.rangeStart);
            analyzeExpr(*s.rangeEnd);
            analyzeStmt(*s.body);
            popScope();
            break;
        }
        case Stmt::Throw: {
            auto& s = static_cast<ThrowStmt&>(stmt);
            analyzeExpr(*s.expr);
            break;
        }
        case Stmt::Try: {
            auto& s = static_cast<TryStmt&>(stmt);
            // Analyze try body
            if (s.body) analyzeBlock(*s.body);
            // Analyze catch clauses
            for (auto& cc : s.catches) {
                pushScope();
                if (cc.exceptionType.kind == TypeSpec::ClassName) {
                    cc.exceptionType.className = resolveClassName(cc.exceptionType.className);
                }
                // Declare the exception variable as a pointer to the exception type
                TypeSpec varType = cc.exceptionType;
                varType.pointerDepth = 1;
                declareVar(cc.varName, varType, cc.loc);
                if (cc.body) {
                    for (auto& st : cc.body->stmts) analyzeStmt(*st);
                }
                popScope();
            }
            // Analyze finally block
            if (s.finallyBody) analyzeBlock(*s.finallyBody);
            break;
        }
        case Stmt::Fallthrough:
            // Valid only inside switch cases; no analysis needed
            break;
    }
}

void Sema::analyzeBlock(BlockStmt& block) {
    pushScope();
    for (auto& stmt : block.stmts) {
        analyzeStmt(*stmt);
    }
    popScope();
}

// ============================================================
// Expression analysis
// ============================================================

TypeSpec Sema::analyzeExpr(Expr& expr) {
    TypeSpec result;

    switch (expr.kind) {
        case Expr::IntLit:
            result.kind = TypeSpec::Int;
            break;

        case Expr::FloatLit:
            result.kind = TypeSpec::Double;
            break;

        case Expr::StringLit:
            result.kind = TypeSpec::String;
            break;

        case Expr::BoolLit:
            result.kind = TypeSpec::Bool;
            break;

        case Expr::CharLit:
            result.kind = TypeSpec::Char;
            break;

        case Expr::NullLit:
            result.kind = TypeSpec::Void; // null compatible with pointers
            result.pointerDepth = 1;
            break;

        case Expr::This: {
            if (currentClassName.empty()) {
                error(expr.loc, "'this' used outside of a class");
            } else {
                result.kind = TypeSpec::ClassName;
                result.className = currentClassName;
                result.pointerDepth = 1;
            }
            break;
        }

        case Expr::Super: {
            if (currentClassName.empty()) {
                error(expr.loc, "'super' used outside of a class");
            } else {
                auto ci = lookupClass(currentClassName);
                if (!ci || ci->parent.empty()) {
                    error(expr.loc, "'super' used in class with no parent");
                } else {
                    result.kind = TypeSpec::ClassName;
                    result.className = ci->parent;
                    result.pointerDepth = 1;
                }
            }
            break;
        }

        case Expr::Ident: {
            auto& e = static_cast<IdentExpr&>(expr);
            TypeSpec* v = lookupVar(e.name);
            if (v) {
                result = *v;
            } else if (lookupClass(e.name)) {
                // Class name used as type reference (for static calls etc.)
                result.kind = TypeSpec::ClassName;
                result.className = resolveClassName(e.name);
            } else if (isNamespacePrefix(e.name)) {
                // Namespace prefix — don't error, will be resolved by parent expr
                result.kind = TypeSpec::Void;
            } else {
                error(expr.loc, "undeclared identifier '" + e.name + "'");
            }
            break;
        }

        case Expr::Binary: {
            auto& e = static_cast<BinaryExpr&>(expr);
            TypeSpec lt = analyzeExpr(*e.left);
            TypeSpec rt = analyzeExpr(*e.right);

            // Check for operator overloading on class types
            if (lt.kind == TypeSpec::ClassName && !lt.pointerDepth) {
                std::string opName;
                switch (e.op) {
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
                    auto ci = lookupClass(lt.className);
                    if (ci) {
                        const ClassInfo* cur = ci;
                        bool found = false;
                        while (cur) {
                            for (auto& m : cur->methods) {
                                if (m.name == opName) {
                                    result = m.returnType;
                                    found = true;
                                    break;
                                }
                            }
                            if (found) break;
                            if (cur->parent.empty()) break;
                            cur = lookupClass(cur->parent);
                        }
                        if (found) break;
                    }
                }
            }

            if (e.op == TokenType::Plus && (lt.kind == TypeSpec::String || rt.kind == TypeSpec::String)) {
                result.kind = TypeSpec::String;
            } else if (e.op == TokenType::EqEq || e.op == TokenType::BangEq
                       || e.op == TokenType::Lt || e.op == TokenType::Gt
                       || e.op == TokenType::LtEq || e.op == TokenType::GtEq
                       || e.op == TokenType::AmpAmp || e.op == TokenType::PipePipe) {
                result.kind = TypeSpec::Bool;
            } else {
                // Arithmetic: result is the wider of the two types
                if (lt.isFloatingPoint() || rt.isFloatingPoint()) {
                    result.kind = TypeSpec::Double;
                } else if (lt.kind == TypeSpec::Long || rt.kind == TypeSpec::Long) {
                    result.kind = TypeSpec::Long;
                } else {
                    result.kind = TypeSpec::Int;
                }
            }
            break;
        }

        case Expr::Unary: {
            auto& e = static_cast<UnaryExpr&>(expr);
            result = analyzeExpr(*e.operand);
            if (e.op == TokenType::Bang) {
                result.kind = TypeSpec::Bool;
            }
            break;
        }

        case Expr::Assign: {
            auto& e = static_cast<AssignExpr&>(expr);
            TypeSpec lt = analyzeExpr(*e.target);
            TypeSpec rt = analyzeExpr(*e.value);
            if (!typesCompatible(lt, rt)) {
                error(expr.loc, "cannot assign '" + rt.toString() + "' to '" + lt.toString() + "'");
            }
            // Check const
            if (e.target->kind == Expr::Ident) {
                auto& ident = static_cast<IdentExpr&>(*e.target);
                if (isVarConst(ident.name)) {
                    error(expr.loc, "cannot assign to const variable '" + ident.name + "'");
                }
            }
            result = lt;
            break;
        }

        case Expr::Call: {
            auto& e = static_cast<CallExpr&>(expr);

            // Handle generic calls: Name<Type>(args)
            // Parser stores type args in resolvedType.typeArgs
            if (!e.resolvedType.typeArgs.empty()) {
                auto& typeArgs = e.resolvedType.typeArgs;

                // Check if it's a generic class constructor
                if (genericClassTemplates.count(e.callee) ||
                    (!currentNamespace_.empty() && genericClassTemplates.count(currentNamespace_ + "." + e.callee))) {
                    // Generic constructor call: Box<int>(42)
                    TypeSpec instType;
                    instType.kind = TypeSpec::ClassName;
                    instType.className = e.callee;
                    instType.typeArgs = typeArgs;
                    ensureGenericClassInstantiated(instType);
                    e.callee = instType.className; // now "Box<int>"
                    result.kind = TypeSpec::ClassName;
                    result.className = instType.className;
                    for (auto& arg : e.args) analyzeExpr(*arg);
                    // Clear typeArgs from resolvedType (now encoded in callee name)
                    e.resolvedType.typeArgs.clear();
                    break;
                }

                // Check if it's a generic function call
                if (genericFuncTemplates.count(e.callee) ||
                    (!currentNamespace_.empty() && genericFuncTemplates.count(currentNamespace_ + "." + e.callee))) {
                    ensureGenericFuncInstantiated(e.callee, typeArgs);
                    std::string instName = makeInstantiatedName(e.callee, typeArgs);
                    e.callee = instName;
                    auto fi = lookupFunction(instName);
                    if (fi) result = fi->returnType;
                    for (auto& arg : e.args) analyzeExpr(*arg);
                    e.resolvedType.typeArgs.clear();
                    break;
                }
            }

            // Check if callee is a class name (constructor call as value)
            if (lookupClass(e.callee)) {
                auto ci = lookupClass(e.callee);
                if (ci && ci->isAbstract) {
                    error(expr.loc, "cannot instantiate abstract class '" + e.callee + "'");
                }
                result.kind = TypeSpec::ClassName;
                result.className = resolveClassName(e.callee);
                for (auto& arg : e.args) analyzeExpr(*arg);
            } else {
                // Check if callee is a callable variable (lambda / function pointer)
                TypeSpec* varType = lookupVar(e.callee);
                if (varType && varType->kind == TypeSpec::Callable && varType->funcSignature) {
                    result = varType->funcSignature->returnType;
                    for (auto& arg : e.args) analyzeExpr(*arg);
                } else {
                    auto fi = lookupFunction(e.callee);
                    if (fi) {
                        result = fi->returnType;
                        // Validate arg count (accounting for defaults)
                        size_t minArgs = 0;
                        for (auto& p : fi->params) {
                            if (!p.defaultValue) minArgs++;
                            else break;
                        }
                        if (!fi->isVariadic && e.args.size() < minArgs) {
                            error(expr.loc, "too few arguments to function '" + e.callee + "'");
                        }
                    } else {
                        error(expr.loc, "undeclared function '" + e.callee + "'");
                    }
                    for (auto& arg : e.args) analyzeExpr(*arg);
                }
            }
            break;
        }

        case Expr::MemberAccess: {
            auto& e = static_cast<MemberAccessExpr&>(expr);

            // Try to flatten the whole chain (including this member) as a qualified class name
            // e.g., Std.IO.Console → lookupClass("Std.IO.Console")
            std::string flattened = flattenDottedExpr(expr);
            if (!flattened.empty()) {
                if (lookupClass(flattened)) {
                    result.kind = TypeSpec::ClassName;
                    result.className = resolveClassName(flattened);
                    break;
                }
                // May be a namespace prefix — don't error yet
                if (isNamespacePrefix(flattened)) {
                    result.kind = TypeSpec::Void;
                    break;
                }
            }

            TypeSpec objType = analyzeExpr(*e.object);

            // Array .length property
            if (objType.isArray && e.member == "length") {
                result.kind = TypeSpec::Long;
                break;
            }

            std::string className;
            if (objType.kind == TypeSpec::ClassName) {
                className = objType.className;
            }

            if (!className.empty()) {
                auto ci = lookupClass(className);
                if (ci) {
                    bool found = false;
                    // Search for field in the class hierarchy
                    const ClassInfo* cur = ci;
                    while (cur) {
                        for (auto& f : cur->fields) {
                            if (f.name == e.member) {
                                result = f.type;
                                found = true;
                                break;
                            }
                        }
                        if (found) break;
                        if (cur->parent.empty()) break;
                        cur = lookupClass(cur->parent);
                    }
                    if (!found) {
                        error(expr.loc, "class '" + className + "' has no field '" + e.member + "'");
                    }
                }
            }
            break;
        }

        case Expr::MethodCall: {
            auto& e = static_cast<MethodCallExpr&>(expr);

            // Handle super method calls: super(args) or super.method(args)
            if (e.object->kind == Expr::Super) {
                TypeSpec objType = analyzeExpr(*e.object);
                std::string parentClass = objType.className;
                if (!parentClass.empty()) {
                    auto ci = lookupClass(parentClass);
                    if (ci) {
                        if (e.method == "<init>") {
                            // super(args) — parent constructor call, void result
                            result.kind = TypeSpec::Void;
                        } else {
                            bool found = false;
                            for (auto& m : ci->methods) {
                                if (m.name == e.method) {
                                    result = m.returnType;
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                error(expr.loc, "parent class '" + parentClass + "' has no method '" + e.method + "'");
                            }
                        }
                    }
                }
                for (auto& arg : e.args) analyzeExpr(*arg);
                break;
            }

            // First check: does object + method form a qualified class name?
            // e.g., App.Models.Greeter("Hello") → "App.Models" + "Greeter" = "App.Models.Greeter"
            std::string flattened = flattenDottedExpr(*e.object);
            if (!flattened.empty()) {
                std::string fullName = flattened + "." + e.method;
                if (lookupClass(fullName)) {
                    // Constructor call on a namespaced class
                    result.kind = TypeSpec::ClassName;
                    result.className = resolveClassName(fullName);
                    for (auto& arg : e.args) analyzeExpr(*arg);
                    break;
                }
            }

            // Try to flatten the object as a qualified class name for static method call
            std::string className;
            if (!flattened.empty() && lookupClass(flattened)) {
                className = resolveClassName(flattened);
                e.object->resolvedType.kind = TypeSpec::ClassName;
                e.object->resolvedType.className = className;
            } else {
                TypeSpec objType = analyzeExpr(*e.object);
                if (objType.kind == TypeSpec::ClassName) {
                    className = objType.className;
                }
            }

            if (!className.empty()) {
                auto ci = lookupClass(className);
                if (ci) {
                    bool found = false;
                    // Search for method in the class hierarchy
                    const ClassInfo* cur = ci;
                    while (cur) {
                        for (auto& m : cur->methods) {
                            if (m.name == e.method) {
                                result = m.returnType;
                                found = true;
                                break;
                            }
                        }
                        if (found) break;
                        if (cur->parent.empty()) break;
                        cur = lookupClass(cur->parent);
                    }
                    if (!found) {
                        error(expr.loc, "class '" + className + "' has no method '" + e.method + "'");
                    }
                }
            }

            for (auto& arg : e.args) analyzeExpr(*arg);
            break;
        }

        case Expr::New: {
            auto& e = static_cast<NewExpr&>(expr);
            if (e.type.kind == TypeSpec::ClassName) {
                // Handle generic instantiation for new Box<int>(42)
                if (!e.type.typeArgs.empty()) {
                    ensureGenericClassInstantiated(e.type);
                } else if (!lookupClass(e.type.className)) {
                    error(expr.loc, "unknown class '" + e.type.className + "'");
                } else {
                    // Resolve to qualified name
                    e.type.className = resolveClassName(e.type.className);
                }
                // Check abstract
                auto ci = lookupClass(e.type.className);
                if (ci && ci->isAbstract) {
                    error(expr.loc, "cannot instantiate abstract class '" + e.type.className + "'");
                }
            }
            result = e.type;
            result.pointerDepth = 1;
            result.isOwner = true;
            for (auto& arg : e.args) analyzeExpr(*arg);
            break;
        }

        case Expr::NewArray: {
            auto& e = static_cast<NewArrayExpr&>(expr);
            analyzeExpr(*e.size);
            result = e.elementType;
            result.isArray = true;
            break;
        }

        case Expr::Move: {
            auto& e = static_cast<MoveExpr&>(expr);
            result = analyzeExpr(*e.operand);
            break;
        }

        case Expr::AddressOf: {
            auto& e = static_cast<AddressOfExpr&>(expr);
            result = analyzeExpr(*e.operand);
            result.pointerDepth++;
            break;
        }

        case Expr::Deref: {
            auto& e = static_cast<DerefExpr&>(expr);
            result = analyzeExpr(*e.operand);
            if (!result.pointerDepth) {
                error(expr.loc, "cannot dereference non-pointer type");
            }
            result.pointerDepth--;
            result.isOwner = false;
            break;
        }

        case Expr::Index: {
            auto& e = static_cast<IndexExpr&>(expr);
            TypeSpec objType = analyzeExpr(*e.object);
            analyzeExpr(*e.index);
            if (objType.isArray) {
                result = objType;
                result.isArray = false;
            } else if (objType.pointerDepth > 0) {
                // Pointer indexing: T** -> T*, T* -> T
                result = objType;
                result.pointerDepth--;
            } else if (objType.kind == TypeSpec::String) {
                result.kind = TypeSpec::Char;
            } else {
                error(expr.loc, "subscript requires array, pointer, or string type");
            }
            break;
        }

        case Expr::Cast: {
            auto& e = static_cast<CastExpr&>(expr);
            analyzeExpr(*e.operand);
            result = e.targetType;
            break;
        }

        case Expr::Ternary: {
            auto& e = static_cast<TernaryExpr&>(expr);
            analyzeExpr(*e.condition);
            TypeSpec tt = analyzeExpr(*e.thenExpr);
            TypeSpec et = analyzeExpr(*e.elseExpr);
            if (!typesCompatible(tt, et) && !typesCompatible(et, tt)) {
                error(expr.loc, "ternary branches have incompatible types: '"
                      + tt.toString() + "' and '" + et.toString() + "'");
            }
            result = tt;
            break;
        }

        case Expr::Lambda: {
            auto& e = static_cast<LambdaExpr&>(expr);

            // Resolve explicit captures [x, &y]
            for (auto& cap : e.captures) {
                if (cap.name == "this") continue; // TODO: capture this
                TypeSpec* varType = lookupVar(cap.name);
                if (!varType) {
                    error(e.loc, "captured variable '" + cap.name + "' not found");
                } else {
                    e.resolvedCaptures.push_back({cap.name, *varType, cap.byRef});
                }
            }

            // Set up auto-capture tracking for [=] / [&]
            CaptureTracker tracker;
            CaptureTracker* savedTracker = captureTracker_;
            if (e.captureDefault != LambdaExpr::None) {
                tracker.lambda = &e;
                tracker.captureByRef = (e.captureDefault == LambdaExpr::AllByRef);
                // Will set lambdaScopeDepth after pushScope
            }

            // Analyze body in a new scope
            pushScope();

            if (e.captureDefault != LambdaExpr::None) {
                tracker.lambdaScopeDepth = (int)scopes.size() - 1;
                captureTracker_ = &tracker;
            }

            for (auto& p : e.params) {
                if (!isTypeValid(p.type)) {
                    error(e.loc, "unknown parameter type '" + p.type.toString() + "'");
                }
                declareVar(p.name, p.type, e.loc);
            }

            // Declare explicit captures in lambda scope so body can reference them
            for (auto& rc : e.resolvedCaptures) {
                declareVar(rc.name, rc.type, e.loc);
            }

            // Track return types for deduction
            TypeSpec deducedReturnType;
            deducedReturnType.kind = TypeSpec::Void;
            bool foundReturn = false;

            if (e.body) {
                for (auto& stmt : e.body->stmts) {
                    analyzeStmt(*stmt);
                    // Check for return statements to deduce type
                    if (!e.hasExplicitReturnType && stmt->kind == Stmt::Return) {
                        auto& ret = static_cast<ReturnStmt&>(*stmt);
                        if (ret.value) {
                            deducedReturnType = ret.value->resolvedType;
                            foundReturn = true;
                        }
                    }
                }
            }

            // Restore capture tracker
            captureTracker_ = savedTracker;
            popScope();

            // Deduce return type if not explicit
            if (!e.hasExplicitReturnType && foundReturn) {
                e.returnType = deducedReturnType;
            }

            // Create callable type
            result.kind = TypeSpec::Callable;
            auto sig = std::make_shared<FuncSignature>();
            sig->returnType = e.returnType;
            for (auto& p : e.params) {
                sig->paramTypes.push_back(p.type);
            }
            result.funcSignature = sig;
            break;
        }

        default:
            break;
    }

    expr.resolvedType = result;
    return result;
}

// ============================================================
// Generic template instantiation
// ============================================================

TypeSpec Sema::substituteType(const TypeSpec& type,
                               const std::vector<std::string>& typeParams,
                               const std::vector<TypeSpec>& typeArgs) {
    // If this is a ClassName that matches a type parameter, substitute it
    if (type.kind == TypeSpec::ClassName && type.typeArgs.empty()
        && !type.pointerDepth && !type.isReference && !type.isArray) {
        for (size_t i = 0; i < typeParams.size(); i++) {
            if (type.className == typeParams[i]) {
                TypeSpec result = typeArgs[i];
                // Preserve pointer/ref/array from outer context
                return result;
            }
        }
    }

    // Recurse into typeArgs (for nested generics like Box<Pair<T, U>>)
    TypeSpec result = type;
    for (auto& ta : result.typeArgs) {
        ta = substituteType(ta, typeParams, typeArgs);
    }

    // Substitute in function signature if callable
    if (result.funcSignature) {
        auto sig = std::make_shared<FuncSignature>(*result.funcSignature);
        sig->returnType = substituteType(sig->returnType, typeParams, typeArgs);
        for (auto& pt : sig->paramTypes) {
            pt = substituteType(pt, typeParams, typeArgs);
        }
        result.funcSignature = sig;
    }

    return result;
}

// Build the instantiated name like "Box<int>" or "Pair<int, string>"
static std::string makeInstantiatedName(const std::string& baseName,
                                         const std::vector<TypeSpec>& typeArgs) {
    std::string name = baseName + "<";
    for (size_t i = 0; i < typeArgs.size(); i++) {
        if (i > 0) name += ", ";
        name += typeArgs[i].toString();
    }
    name += ">";
    return name;
}

void Sema::ensureGenericClassInstantiated(TypeSpec& type) {
    if (type.kind != TypeSpec::ClassName || type.typeArgs.empty()) return;

    std::string instName = makeInstantiatedName(type.className, type.typeArgs);

    // Already instantiated?
    if (instantiatedGenerics.count(instName)) {
        type.className = instName;
        type.typeArgs.clear();
        return;
    }

    // Find the generic template
    auto it = genericClassTemplates.find(type.className);
    if (it == genericClassTemplates.end()) {
        // Try with namespace
        if (!currentNamespace_.empty()) {
            it = genericClassTemplates.find(currentNamespace_ + "." + type.className);
        }
        if (it == genericClassTemplates.end()) {
            error(SourceLoc{}, "unknown generic class '" + type.className + "'");
            return;
        }
    }

    auto& tmpl = *it->second;
    if (tmpl.typeParams.size() != type.typeArgs.size()) {
        error(SourceLoc{}, "generic class '" + type.className + "' expects "
              + std::to_string(tmpl.typeParams.size()) + " type argument(s), got "
              + std::to_string(type.typeArgs.size()));
        return;
    }

    // Mark as instantiated (before analysis to prevent infinite recursion)
    instantiatedGenerics.insert(instName);

    // Create a new ClassDecl with substituted types
    auto inst = std::make_shared<ClassDecl>(tmpl.loc);
    inst->namespacePath = tmpl.namespacePath;
    inst->name = instName;
    inst->parentClass = tmpl.parentClass;
    inst->interfaces = tmpl.interfaces;
    inst->isExtern = tmpl.isExtern;
    inst->isAbstract = tmpl.isAbstract;

    // Substitute fields
    for (auto& field : tmpl.fields) {
        FieldDecl f = field;
        f.type = substituteType(f.type, tmpl.typeParams, type.typeArgs);
        inst->fields.push_back(std::move(f));
    }

    // Substitute constructors
    for (auto& ctor : tmpl.constructors) {
        ConstructorDecl c = ctor;
        c.className = instName;
        c.body = cloneBlock(ctor.body);
        for (auto& p : c.params) {
            p.type = substituteType(p.type, tmpl.typeParams, type.typeArgs);
        }
        inst->constructors.push_back(std::move(c));
    }

    // Substitute methods
    for (auto& method : tmpl.methods) {
        MethodDecl m = method;
        m.returnType = substituteType(m.returnType, tmpl.typeParams, type.typeArgs);
        m.body = cloneBlock(method.body);
        for (auto& p : m.params) {
            p.type = substituteType(p.type, tmpl.typeParams, type.typeArgs);
        }
        inst->methods.push_back(std::move(m));
    }

    // Copy destructor
    if (tmpl.destructor) {
        inst->destructor = std::make_shared<DestructorDecl>(*tmpl.destructor);
        inst->destructor->className = instName;
        inst->destructor->body = cloneBlock(tmpl.destructor->body);
    }

    // Register the instantiated class
    ClassInfo ci;
    ci.name = inst->qualifiedName();
    ci.parent = inst->parentClass;
    ci.fields = inst->fields;
    ci.methods = inst->methods;
    ci.constructors = inst->constructors;
    ci.hasDestructor = inst->destructor != nullptr;
    ci.isAbstract = inst->isAbstract;
    classes[inst->qualifiedName()] = std::move(ci);

    // Add to the program so codegen sees it
    if (currentProgram_) {
        currentProgram_->declarations.push_back(inst);
    }

    // Update the TypeSpec to use the instantiated name
    type.className = instName;
    type.typeArgs.clear(); // clear so subsequent lookups don't re-enter

    // Analyze the instantiated class
    analyzeClass(*inst);
}

void Sema::ensureGenericFuncInstantiated(const std::string& name,
                                          const std::vector<TypeSpec>& typeArgs) {
    std::string instName = makeInstantiatedName(name, typeArgs);

    // Already instantiated?
    if (instantiatedGenerics.count(instName)) return;

    // Find the generic template
    auto it = genericFuncTemplates.find(name);
    if (it == genericFuncTemplates.end()) {
        if (!currentNamespace_.empty()) {
            it = genericFuncTemplates.find(currentNamespace_ + "." + name);
        }
        if (it == genericFuncTemplates.end()) {
            error(SourceLoc{}, "unknown generic function '" + name + "'");
            return;
        }
    }

    auto& tmpl = *it->second;
    if (tmpl.typeParams.size() != typeArgs.size()) {
        error(SourceLoc{}, "generic function '" + name + "' expects "
              + std::to_string(tmpl.typeParams.size()) + " type argument(s), got "
              + std::to_string(typeArgs.size()));
        return;
    }

    instantiatedGenerics.insert(instName);

    // Create a new FunctionDecl with substituted types
    auto inst = std::make_shared<FunctionDecl>(tmpl.loc);
    inst->namespacePath = tmpl.namespacePath;
    inst->attributes = tmpl.attributes;
    inst->name = instName;
    inst->returnType = substituteType(tmpl.returnType, tmpl.typeParams, typeArgs);
    inst->body = cloneBlock(tmpl.body); // deep clone body AST

    for (auto& p : tmpl.params) {
        Param np = p;
        np.type = substituteType(np.type, tmpl.typeParams, typeArgs);
        inst->params.push_back(std::move(np));
    }

    // Register as a concrete function
    FuncInfo fi;
    fi.returnType = inst->returnType;
    fi.params = inst->params;
    functions[inst->qualifiedName()] = std::move(fi);

    // Add to program
    if (currentProgram_) {
        currentProgram_->declarations.push_back(inst);
    }

    analyzeFunction(*inst);
}