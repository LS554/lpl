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
#include "token.h"

// ============================================================
// Forward declarations
// ============================================================

struct FuncSignature;

// ============================================================
// Type representation
// ============================================================

struct TypeSpec {
    enum Kind {
        Void, Bool, Byte, Char, Short, Int, Long,
        Float, Double, String, ClassName, Callable, Auto
    };

    Kind kind = Void;
    std::string className;   // only for ClassName kind
    int pointerDepth = 0;
    bool isReference = false;
    bool isOwner = false;
    bool isArray = false;
    std::shared_ptr<FuncSignature> funcSignature; // non-null iff kind == Callable
    std::vector<TypeSpec> typeArgs; // generic type arguments, e.g. Box<int> → [Int]

    bool isNumeric() const {
        return kind == Byte || kind == Short || kind == Int || kind == Long
            || kind == Float || kind == Double || kind == Char;
    }
    bool isIntegral() const {
        return kind == Byte || kind == Short || kind == Int || kind == Long || kind == Char || kind == Bool;
    }
    bool isFloatingPoint() const {
        return kind == Float || kind == Double;
    }
    bool isPrimitive() const {
        return kind != ClassName && kind != Callable && kind != Auto;
    }
    bool isVoid() const { return kind == Void && !pointerDepth && !isReference; }

    bool operator==(const TypeSpec& o) const;
    bool operator!=(const TypeSpec& o) const { return !(*this == o); }
    std::string toString() const;
};

// ============================================================
// Forward declarations
// ============================================================

struct Expr;
struct Stmt;
struct Decl;
struct BlockStmt;

using ExprPtr = std::shared_ptr<Expr>;
using StmtPtr = std::shared_ptr<Stmt>;
using DeclPtr = std::shared_ptr<Decl>;

// ============================================================
// Param (shared by declarations and lambda expressions)
// ============================================================

struct Param {
    TypeSpec type;
    std::string name;
    ExprPtr defaultValue; // may be null
};

// ============================================================
// Expressions
// ============================================================

struct Expr {
    enum Kind {
        IntLit, FloatLit, StringLit, BoolLit, NullLit, CharLit,
        Ident, This, Super,
        Binary, Unary, Assign, Ternary,
        Call, MemberAccess, MethodCall,
        New, NewArray, Move, AddressOf, Deref, Index, Cast, BoundaryConvert,
        Lambda
    };

    Kind kind;
    SourceLoc loc;
    TypeSpec resolvedType; // filled by sema

    virtual ~Expr() = default;
protected:
    Expr(Kind k, SourceLoc l) : kind(k), loc(l) {}
};

struct IntLitExpr : Expr {
    int64_t value;
    IntLitExpr(int64_t v, SourceLoc l) : Expr(IntLit, l), value(v) {}
};

struct FloatLitExpr : Expr {
    double value;
    FloatLitExpr(double v, SourceLoc l) : Expr(FloatLit, l), value(v) {}
};

struct StringLitExpr : Expr {
    std::string value;
    StringLitExpr(std::string v, SourceLoc l) : Expr(StringLit, l), value(std::move(v)) {}
};

struct BoolLitExpr : Expr {
    bool value;
    BoolLitExpr(bool v, SourceLoc l) : Expr(BoolLit, l), value(v) {}
};

struct NullLitExpr : Expr {
    NullLitExpr(SourceLoc l) : Expr(NullLit, l) {}
};

struct CharLitExpr : Expr {
    char value;
    CharLitExpr(char v, SourceLoc l) : Expr(CharLit, l), value(v) {}
};

struct IdentExpr : Expr {
    std::string name;
    IdentExpr(std::string n, SourceLoc l) : Expr(Ident, l), name(std::move(n)) {}
};

struct ThisExpr : Expr {
    ThisExpr(SourceLoc l) : Expr(This, l) {}
};

struct SuperExpr : Expr {
    SuperExpr(SourceLoc l) : Expr(Super, l) {}
};

struct BinaryExpr : Expr {
    TokenType op;
    ExprPtr left, right;
    BinaryExpr(TokenType o, ExprPtr l, ExprPtr r, SourceLoc loc)
        : Expr(Binary, loc), op(o), left(std::move(l)), right(std::move(r)) {}
};

struct UnaryExpr : Expr {
    TokenType op;
    ExprPtr operand;
    UnaryExpr(TokenType o, ExprPtr e, SourceLoc l)
        : Expr(Unary, l), op(o), operand(std::move(e)) {}
};

struct AssignExpr : Expr {
    ExprPtr target, value;
    AssignExpr(ExprPtr t, ExprPtr v, SourceLoc l)
        : Expr(Assign, l), target(std::move(t)), value(std::move(v)) {}
};

struct CallExpr : Expr {
    std::string callee;
    std::vector<ExprPtr> args;
    CallExpr(std::string name, std::vector<ExprPtr> a, SourceLoc l)
        : Expr(Call, l), callee(std::move(name)), args(std::move(a)) {}
};

struct MemberAccessExpr : Expr {
    ExprPtr object;
    std::string member;
    MemberAccessExpr(ExprPtr obj, std::string m, SourceLoc l)
        : Expr(MemberAccess, l), object(std::move(obj)), member(std::move(m)) {}
};

struct MethodCallExpr : Expr {
    ExprPtr object;
    std::string method;
    std::vector<ExprPtr> args;
    MethodCallExpr(ExprPtr obj, std::string m, std::vector<ExprPtr> a, SourceLoc l)
        : Expr(MethodCall, l), object(std::move(obj)), method(std::move(m)), args(std::move(a)) {}
};

struct NewExpr : Expr {
    TypeSpec type;
    std::vector<ExprPtr> args;
    NewExpr(TypeSpec t, std::vector<ExprPtr> a, SourceLoc l)
        : Expr(New, l), type(std::move(t)), args(std::move(a)) {}
};

struct MoveExpr : Expr {
    ExprPtr operand;
    MoveExpr(ExprPtr e, SourceLoc l)
        : Expr(Move, l), operand(std::move(e)) {}
};

struct AddressOfExpr : Expr {
    ExprPtr operand;
    AddressOfExpr(ExprPtr e, SourceLoc l)
        : Expr(AddressOf, l), operand(std::move(e)) {}
};

struct DerefExpr : Expr {
    ExprPtr operand;
    DerefExpr(ExprPtr e, SourceLoc l)
        : Expr(Deref, l), operand(std::move(e)) {}
};

struct IndexExpr : Expr {
    ExprPtr object, index;
    IndexExpr(ExprPtr obj, ExprPtr idx, SourceLoc l)
        : Expr(Index, l), object(std::move(obj)), index(std::move(idx)) {}
};

struct NewArrayExpr : Expr {
    TypeSpec elementType;
    ExprPtr size;
    NewArrayExpr(TypeSpec et, ExprPtr sz, SourceLoc l)
        : Expr(NewArray, l), elementType(std::move(et)), size(std::move(sz)) {}
};

struct CastExpr : Expr {
    ExprPtr operand;
    TypeSpec targetType;
    CastExpr(ExprPtr e, TypeSpec t, SourceLoc l)
        : Expr(Cast, l), operand(std::move(e)), targetType(std::move(t)) {}
};

// Type(x) — language boundary conversion (e.g. string(cptr), char*(str))
struct BoundaryConvertExpr : Expr {
    ExprPtr operand;
    TypeSpec targetType;
    BoundaryConvertExpr(ExprPtr e, TypeSpec t, SourceLoc l)
        : Expr(BoundaryConvert, l), operand(std::move(e)), targetType(std::move(t)) {}
};

struct TernaryExpr : Expr {
    ExprPtr condition, thenExpr, elseExpr;
    TernaryExpr(ExprPtr c, ExprPtr t, ExprPtr e, SourceLoc l)
        : Expr(Ternary, l), condition(std::move(c)), thenExpr(std::move(t)), elseExpr(std::move(e)) {}
};

struct CaptureItem {
    std::string name;    // variable name
    bool byRef = false;  // true for &x
};

struct LambdaExpr : Expr {
    enum CaptureDefault { None, AllByValue, AllByRef };
    CaptureDefault captureDefault = None;
    std::vector<CaptureItem> captures;     // explicit captures like [x, &y]
    std::vector<Param> params;
    TypeSpec returnType;
    bool hasExplicitReturnType = false;
    std::shared_ptr<BlockStmt> body;

    // Filled by sema: resolved captures with their types and by-ref flag
    struct ResolvedCapture {
        std::string name;
        TypeSpec type;
        bool byRef = false;
    };
    std::vector<ResolvedCapture> resolvedCaptures;

    LambdaExpr(CaptureDefault cd, std::vector<CaptureItem> caps,
               std::vector<Param> p, TypeSpec rt, bool explicitRet,
               std::shared_ptr<BlockStmt> b, SourceLoc l)
        : Expr(Lambda, l), captureDefault(cd), captures(std::move(caps)),
          params(std::move(p)), returnType(std::move(rt)),
          hasExplicitReturnType(explicitRet), body(std::move(b)) {}
};

// ============================================================
// FuncSignature (for Callable type)
// ============================================================

struct FuncSignature {
    TypeSpec returnType;
    std::vector<TypeSpec> paramTypes;
};

// Out-of-line TypeSpec methods (need FuncSignature to be complete)
inline bool TypeSpec::operator==(const TypeSpec& o) const {
    if (kind != o.kind || className != o.className
        || pointerDepth != o.pointerDepth || isReference != o.isReference
        || isOwner != o.isOwner || isArray != o.isArray
        || typeArgs != o.typeArgs)
        return false;
    if (kind == Callable) {
        if (!funcSignature && !o.funcSignature) return true;
        if (!funcSignature || !o.funcSignature) return false;
        if (funcSignature->returnType != o.funcSignature->returnType) return false;
        if (funcSignature->paramTypes.size() != o.funcSignature->paramTypes.size()) return false;
        for (size_t i = 0; i < funcSignature->paramTypes.size(); i++) {
            if (funcSignature->paramTypes[i] != o.funcSignature->paramTypes[i]) return false;
        }
    }
    return true;
}

inline std::string TypeSpec::toString() const {
    std::string s;
    if (isOwner) s += "owner ";
    switch (kind) {
        case Void:   s += "void"; break;
        case Bool:   s += "bool"; break;
        case Byte:   s += "byte"; break;
        case Char:   s += "char"; break;
        case Short:  s += "short"; break;
        case Int:    s += "int"; break;
        case Long:   s += "long"; break;
        case Float:  s += "float"; break;
        case Double: s += "double"; break;
        case String: s += "string"; break;
        case ClassName:
            s += className;
            if (!typeArgs.empty()) {
                s += "<";
                for (size_t i = 0; i < typeArgs.size(); i++) {
                    if (i > 0) s += ", ";
                    s += typeArgs[i].toString();
                }
                s += ">";
            }
            break;
        case Callable:
            s += "func(";
            if (funcSignature) {
                for (size_t i = 0; i < funcSignature->paramTypes.size(); i++) {
                    if (i > 0) s += ", ";
                    s += funcSignature->paramTypes[i].toString();
                }
                s += ") -> ";
                s += funcSignature->returnType.toString();
            } else {
                s += ")";
            }
            break;
        case Auto:   s += "auto"; break;
    }
    for (int i = 0; i < pointerDepth; i++) s += "*";
    if (isReference) s += "&";
    if (isArray) s += "[]";
    return s;
}

// ============================================================
// Statements
// ============================================================

struct Stmt {
    enum Kind {
        Block, ExprS, VarDecl, Return, If, While, For, DoWhile, ForEach, Switch,
        Delete, Break, Continue, Defer, Try, Throw, Fallthrough
    };

    Kind kind;
    SourceLoc loc;

    virtual ~Stmt() = default;
protected:
    Stmt(Kind k, SourceLoc l) : kind(k), loc(l) {}
};

struct BlockStmt : Stmt {
    std::vector<StmtPtr> stmts;
    BlockStmt(std::vector<StmtPtr> s, SourceLoc l)
        : Stmt(Block, l), stmts(std::move(s)) {}
};

struct ExprStmt : Stmt {
    ExprPtr expr;
    ExprStmt(ExprPtr e, SourceLoc l) : Stmt(ExprS, l), expr(std::move(e)) {}
};

struct VarDeclStmt : Stmt {
    TypeSpec type;
    std::string name;
    ExprPtr init; // may be null
    bool isConst = false;
    VarDeclStmt(TypeSpec t, std::string n, ExprPtr i, SourceLoc l)
        : Stmt(VarDecl, l), type(std::move(t)), name(std::move(n)), init(std::move(i)) {}
};

struct ReturnStmt : Stmt {
    ExprPtr value; // may be null
    bool isMove = false;
    ReturnStmt(ExprPtr v, bool mv, SourceLoc l)
        : Stmt(Return, l), value(std::move(v)), isMove(mv) {}
};

struct IfStmt : Stmt {
    ExprPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch; // may be null
    IfStmt(ExprPtr c, StmtPtr t, StmtPtr e, SourceLoc l)
        : Stmt(If, l), condition(std::move(c)), thenBranch(std::move(t)), elseBranch(std::move(e)) {}
};

struct WhileStmt : Stmt {
    ExprPtr condition;
    StmtPtr body;
    WhileStmt(ExprPtr c, StmtPtr b, SourceLoc l)
        : Stmt(While, l), condition(std::move(c)), body(std::move(b)) {}
};

struct ForStmt : Stmt {
    StmtPtr init;    // VarDecl or ExprStmt, may be null
    ExprPtr condition; // may be null
    ExprPtr increment; // may be null
    StmtPtr body;
    ForStmt(StmtPtr i, ExprPtr c, ExprPtr inc, StmtPtr b, SourceLoc l)
        : Stmt(For, l), init(std::move(i)), condition(std::move(c)),
          increment(std::move(inc)), body(std::move(b)) {}
};

struct DeleteStmt : Stmt {
    ExprPtr expr;
    DeleteStmt(ExprPtr e, SourceLoc l) : Stmt(Delete, l), expr(std::move(e)) {}
};

struct BreakStmt : Stmt {
    BreakStmt(SourceLoc l) : Stmt(Break, l) {}
};

struct ContinueStmt : Stmt {
    ContinueStmt(SourceLoc l) : Stmt(Continue, l) {}
};

struct DeferStmt : Stmt {
    StmtPtr body;  // the statement to execute at scope exit
    DeferStmt(StmtPtr b, SourceLoc l)
        : Stmt(Defer, l), body(std::move(b)) {}
};

struct DoWhileStmt : Stmt {
    ExprPtr condition;
    StmtPtr body;
    DoWhileStmt(StmtPtr b, ExprPtr c, SourceLoc l)
        : Stmt(DoWhile, l), condition(std::move(c)), body(std::move(b)) {}
};

struct SwitchCase {
    ExprPtr value; // null for default case
    std::vector<StmtPtr> body;
};

struct SwitchStmt : Stmt {
    ExprPtr expr;
    std::vector<SwitchCase> cases;
    SwitchStmt(ExprPtr e, std::vector<SwitchCase> c, SourceLoc l)
        : Stmt(Switch, l), expr(std::move(e)), cases(std::move(c)) {}
};

struct ForEachStmt : Stmt {
    TypeSpec varType;
    std::string varName;
    ExprPtr rangeStart;
    ExprPtr rangeEnd;
    bool isInclusive = false; // true for ..=, false for ..
    StmtPtr body;
    ForEachStmt(TypeSpec vt, std::string vn, ExprPtr rs, ExprPtr re, bool incl, StmtPtr b, SourceLoc l)
        : Stmt(ForEach, l), varType(std::move(vt)), varName(std::move(vn)),
          rangeStart(std::move(rs)), rangeEnd(std::move(re)), isInclusive(incl), body(std::move(b)) {}
};

struct ThrowStmt : Stmt {
    ExprPtr expr;
    ThrowStmt(ExprPtr e, SourceLoc l) : Stmt(Throw, l), expr(std::move(e)) {}
};

struct FallthroughStmt : Stmt {
    FallthroughStmt(SourceLoc l) : Stmt(Fallthrough, l) {}
};

struct CatchClause {
    TypeSpec exceptionType;
    std::string varName;
    std::shared_ptr<BlockStmt> body;
    SourceLoc loc;
};

struct TryStmt : Stmt {
    std::shared_ptr<BlockStmt> body;
    std::vector<CatchClause> catches;
    std::shared_ptr<BlockStmt> finallyBody; // may be null
    TryStmt(std::shared_ptr<BlockStmt> b, std::vector<CatchClause> c,
            std::shared_ptr<BlockStmt> f, SourceLoc l)
        : Stmt(Try, l), body(std::move(b)), catches(std::move(c)), finallyBody(std::move(f)) {}
};

// ============================================================
// Declarations
// ============================================================

enum class AccessLevel { Private, Public, Protected };

struct Attribute {
    std::string name;  // e.g. "os"
    std::string arg;   // e.g. "linux"
};

// Field in a class
struct FieldDecl {
    AccessLevel access = AccessLevel::Private;
    bool isStatic = false;
    bool isConst = false;
    TypeSpec type;
    std::string name;
    ExprPtr init; // may be null
    SourceLoc loc;
};

// Method in a class
struct MethodDecl {
    AccessLevel access = AccessLevel::Private;
    bool isStatic = false;
    bool isOverride = false;
    bool isAbstract = false;
    TypeSpec returnType;
    std::string name;
    std::vector<Param> params;
    std::shared_ptr<BlockStmt> body;
    SourceLoc loc;
};

// Constructor in a class
struct ConstructorDecl {
    AccessLevel access = AccessLevel::Private;
    std::string className;
    std::vector<Param> params;
    std::shared_ptr<BlockStmt> body;
    SourceLoc loc;
};

// Destructor in a class
struct DestructorDecl {
    std::string className;
    std::shared_ptr<BlockStmt> body; // may be null (auto-generated)
    SourceLoc loc;
};

struct Decl {
    enum Kind { Class, Interface, Function, ExternBlock, Include, Namespace };
    Kind kind;
    SourceLoc loc;
    virtual ~Decl() = default;
protected:
    Decl(Kind k, SourceLoc l) : kind(k), loc(l) {}
};

struct ClassDecl : Decl {
    std::string namespacePath; // e.g., "Std.IO" — empty if no namespace
    std::string name;
    std::vector<std::string> typeParams; // generic type parameters, e.g. <T, U>
    std::string parentClass; // empty if no parent
    std::vector<std::string> interfaces;
    std::vector<FieldDecl> fields;
    std::vector<MethodDecl> methods;
    std::vector<ConstructorDecl> constructors;
    std::shared_ptr<DestructorDecl> destructor; // may be null
    bool isExtern = false; // true if from .lph header (methods are external)
    bool isAbstract = false; // true if class is declared abstract

    std::string qualifiedName() const {
        return namespacePath.empty() ? name : namespacePath + "." + name;
    }

    ClassDecl(SourceLoc l) : Decl(Class, l) {}
};

struct InterfaceDecl : Decl {
    std::string namespacePath;
    std::string name;
    struct MethodSig {
        TypeSpec returnType;
        std::string name;
        std::vector<Param> params;
    };
    std::vector<MethodSig> methods;

    InterfaceDecl(SourceLoc l) : Decl(Interface, l) {}
};

struct FunctionDecl : Decl {
    std::string namespacePath;
    std::vector<Attribute> attributes;
    TypeSpec returnType;
    std::string name;
    std::vector<std::string> typeParams; // generic type parameters, e.g. <T>
    std::vector<Param> params;
    std::shared_ptr<BlockStmt> body;

    std::string qualifiedName() const {
        return namespacePath.empty() ? name : namespacePath + "." + name;
    }

    FunctionDecl(SourceLoc l) : Decl(Function, l) {}
};

struct ExternFuncDecl {
    TypeSpec returnType;
    std::string name;       // LPL-side callable name
    std::string linkName;   // linker symbol (empty = same as name)
    std::vector<Param> params;
    bool isVariadic = false;
    SourceLoc loc;
};

struct ExternCInclude {
    std::string path;       // the header path as written, e.g. "mylib.h" or "stdio.h"
    bool isSystem = false;  // true for <...>, false for "..."
    SourceLoc loc;
};

struct ExternBlockDecl : Decl {
    std::string convention; // "C" or "C++"
    std::vector<ExternCInclude> cincludes; // #include directives inside this block
    std::vector<ExternFuncDecl> functions;

    ExternBlockDecl(SourceLoc l) : Decl(ExternBlock, l) {}
};

// ============================================================
// Include declaration
// ============================================================

struct IncludeDecl : Decl {
    std::string path;       // the file path as written
    bool isSystem = false;  // true for <...>, false for "..."

    IncludeDecl(SourceLoc l) : Decl(Include, l) {}
};

// ============================================================
// Namespace declaration
// ============================================================

struct NamespaceDecl : Decl {
    std::string path;                   // e.g., "Std.IO"
    std::vector<DeclPtr> declarations;  // declarations inside block form
    bool isFileLevel = false;           // true for `namespace X.Y;`

    NamespaceDecl(SourceLoc l) : Decl(Namespace, l) {}
};

// ============================================================
// Program (root AST node)
// ============================================================

struct Program {
    std::vector<DeclPtr> declarations;
};

// ============================================================
// Deep clone utilities (for generic instantiation)
// ============================================================

inline ExprPtr cloneExpr(const ExprPtr& e);
inline StmtPtr cloneStmt(const StmtPtr& s);

inline std::vector<ExprPtr> cloneExprs(const std::vector<ExprPtr>& v) {
    std::vector<ExprPtr> r;
    r.reserve(v.size());
    for (auto& e : v) r.push_back(cloneExpr(e));
    return r;
}

inline std::vector<Param> cloneParams(const std::vector<Param>& v) {
    std::vector<Param> r;
    r.reserve(v.size());
    for (auto& p : v) {
        Param np = p;
        np.defaultValue = cloneExpr(p.defaultValue);
        r.push_back(std::move(np));
    }
    return r;
}

inline ExprPtr cloneExpr(const ExprPtr& e) {
    if (!e) return nullptr;
    switch (e->kind) {
        case Expr::IntLit: {
            auto& x = static_cast<IntLitExpr&>(*e);
            return std::make_shared<IntLitExpr>(x.value, x.loc);
        }
        case Expr::FloatLit: {
            auto& x = static_cast<FloatLitExpr&>(*e);
            return std::make_shared<FloatLitExpr>(x.value, x.loc);
        }
        case Expr::StringLit: {
            auto& x = static_cast<StringLitExpr&>(*e);
            return std::make_shared<StringLitExpr>(x.value, x.loc);
        }
        case Expr::BoolLit: {
            auto& x = static_cast<BoolLitExpr&>(*e);
            return std::make_shared<BoolLitExpr>(x.value, x.loc);
        }
        case Expr::NullLit: return std::make_shared<NullLitExpr>(e->loc);
        case Expr::CharLit: {
            auto& x = static_cast<CharLitExpr&>(*e);
            return std::make_shared<CharLitExpr>(x.value, x.loc);
        }
        case Expr::Ident: {
            auto& x = static_cast<IdentExpr&>(*e);
            return std::make_shared<IdentExpr>(x.name, x.loc);
        }
        case Expr::This: return std::make_shared<ThisExpr>(e->loc);
        case Expr::Super: return std::make_shared<SuperExpr>(e->loc);
        case Expr::Binary: {
            auto& x = static_cast<BinaryExpr&>(*e);
            return std::make_shared<BinaryExpr>(x.op, cloneExpr(x.left), cloneExpr(x.right), x.loc);
        }
        case Expr::Unary: {
            auto& x = static_cast<UnaryExpr&>(*e);
            return std::make_shared<UnaryExpr>(x.op, cloneExpr(x.operand), x.loc);
        }
        case Expr::Assign: {
            auto& x = static_cast<AssignExpr&>(*e);
            return std::make_shared<AssignExpr>(cloneExpr(x.target), cloneExpr(x.value), x.loc);
        }
        case Expr::Ternary: {
            auto& x = static_cast<TernaryExpr&>(*e);
            return std::make_shared<TernaryExpr>(cloneExpr(x.condition), cloneExpr(x.thenExpr), cloneExpr(x.elseExpr), x.loc);
        }
        case Expr::Call: {
            auto& x = static_cast<CallExpr&>(*e);
            return std::make_shared<CallExpr>(x.callee, cloneExprs(x.args), x.loc);
        }
        case Expr::MemberAccess: {
            auto& x = static_cast<MemberAccessExpr&>(*e);
            return std::make_shared<MemberAccessExpr>(cloneExpr(x.object), x.member, x.loc);
        }
        case Expr::MethodCall: {
            auto& x = static_cast<MethodCallExpr&>(*e);
            return std::make_shared<MethodCallExpr>(cloneExpr(x.object), x.method, cloneExprs(x.args), x.loc);
        }
        case Expr::New: {
            auto& x = static_cast<NewExpr&>(*e);
            return std::make_shared<NewExpr>(x.type, cloneExprs(x.args), x.loc);
        }
        case Expr::NewArray: {
            auto& x = static_cast<NewArrayExpr&>(*e);
            return std::make_shared<NewArrayExpr>(x.elementType, cloneExpr(x.size), x.loc);
        }
        case Expr::Move: {
            auto& x = static_cast<MoveExpr&>(*e);
            return std::make_shared<MoveExpr>(cloneExpr(x.operand), x.loc);
        }
        case Expr::AddressOf: {
            auto& x = static_cast<AddressOfExpr&>(*e);
            return std::make_shared<AddressOfExpr>(cloneExpr(x.operand), x.loc);
        }
        case Expr::Deref: {
            auto& x = static_cast<DerefExpr&>(*e);
            return std::make_shared<DerefExpr>(cloneExpr(x.operand), x.loc);
        }
        case Expr::Index: {
            auto& x = static_cast<IndexExpr&>(*e);
            return std::make_shared<IndexExpr>(cloneExpr(x.object), cloneExpr(x.index), x.loc);
        }
        case Expr::Cast: {
            auto& x = static_cast<CastExpr&>(*e);
            return std::make_shared<CastExpr>(cloneExpr(x.operand), x.targetType, x.loc);
        }
        case Expr::BoundaryConvert: {
            auto& x = static_cast<BoundaryConvertExpr&>(*e);
            return std::make_shared<BoundaryConvertExpr>(cloneExpr(x.operand), x.targetType, x.loc);
        }
        case Expr::Lambda: {
            auto& x = static_cast<LambdaExpr&>(*e);
            auto body = std::dynamic_pointer_cast<BlockStmt>(cloneStmt(x.body));
            return std::make_shared<LambdaExpr>(x.captureDefault, x.captures,
                cloneParams(x.params), x.returnType, x.hasExplicitReturnType, body, x.loc);
        }
    }
    return nullptr;
}

inline std::shared_ptr<BlockStmt> cloneBlock(const std::shared_ptr<BlockStmt>& b) {
    if (!b) return nullptr;
    std::vector<StmtPtr> stmts;
    stmts.reserve(b->stmts.size());
    for (auto& s : b->stmts) stmts.push_back(cloneStmt(s));
    return std::make_shared<BlockStmt>(std::move(stmts), b->loc);
}

inline StmtPtr cloneStmt(const StmtPtr& s) {
    if (!s) return nullptr;
    switch (s->kind) {
        case Stmt::Block: {
            auto& x = static_cast<BlockStmt&>(*s);
            std::vector<StmtPtr> stmts;
            for (auto& st : x.stmts) stmts.push_back(cloneStmt(st));
            return std::make_shared<BlockStmt>(std::move(stmts), x.loc);
        }
        case Stmt::ExprS: {
            auto& x = static_cast<ExprStmt&>(*s);
            return std::make_shared<ExprStmt>(cloneExpr(x.expr), x.loc);
        }
        case Stmt::VarDecl: {
            auto& x = static_cast<VarDeclStmt&>(*s);
            auto r = std::make_shared<VarDeclStmt>(x.type, x.name, cloneExpr(x.init), x.loc);
            r->isConst = x.isConst;
            return r;
        }
        case Stmt::Return: {
            auto& x = static_cast<ReturnStmt&>(*s);
            return std::make_shared<ReturnStmt>(cloneExpr(x.value), x.isMove, x.loc);
        }
        case Stmt::If: {
            auto& x = static_cast<IfStmt&>(*s);
            return std::make_shared<IfStmt>(cloneExpr(x.condition), cloneStmt(x.thenBranch), cloneStmt(x.elseBranch), x.loc);
        }
        case Stmt::While: {
            auto& x = static_cast<WhileStmt&>(*s);
            return std::make_shared<WhileStmt>(cloneExpr(x.condition), cloneStmt(x.body), x.loc);
        }
        case Stmt::For: {
            auto& x = static_cast<ForStmt&>(*s);
            return std::make_shared<ForStmt>(cloneStmt(x.init), cloneExpr(x.condition), cloneExpr(x.increment), cloneStmt(x.body), x.loc);
        }
        case Stmt::DoWhile: {
            auto& x = static_cast<DoWhileStmt&>(*s);
            return std::make_shared<DoWhileStmt>(cloneStmt(x.body), cloneExpr(x.condition), x.loc);
        }
        case Stmt::ForEach: {
            auto& x = static_cast<ForEachStmt&>(*s);
            return std::make_shared<ForEachStmt>(x.varType, x.varName, cloneExpr(x.rangeStart), cloneExpr(x.rangeEnd), x.isInclusive, cloneStmt(x.body), x.loc);
        }
        case Stmt::Switch: {
            auto& x = static_cast<SwitchStmt&>(*s);
            std::vector<SwitchCase> cases;
            for (auto& c : x.cases) {
                SwitchCase nc;
                nc.value = cloneExpr(c.value);
                for (auto& st : c.body) nc.body.push_back(cloneStmt(st));
                cases.push_back(std::move(nc));
            }
            return std::make_shared<SwitchStmt>(cloneExpr(x.expr), std::move(cases), x.loc);
        }
        case Stmt::Delete: {
            auto& x = static_cast<DeleteStmt&>(*s);
            return std::make_shared<DeleteStmt>(cloneExpr(x.expr), x.loc);
        }
        case Stmt::Break: return std::make_shared<BreakStmt>(s->loc);
        case Stmt::Continue: return std::make_shared<ContinueStmt>(s->loc);
        case Stmt::Fallthrough: return std::make_shared<FallthroughStmt>(s->loc);
        case Stmt::Defer: {
            auto& x = static_cast<DeferStmt&>(*s);
            return std::make_shared<DeferStmt>(cloneStmt(x.body), x.loc);
        }
        case Stmt::Try: {
            auto& x = static_cast<TryStmt&>(*s);
            auto body = cloneBlock(x.body);
            std::vector<CatchClause> catches;
            for (auto& c : x.catches) {
                CatchClause nc;
                nc.exceptionType = c.exceptionType;
                nc.varName = c.varName;
                nc.body = cloneBlock(c.body);
                nc.loc = c.loc;
                catches.push_back(std::move(nc));
            }
            return std::make_shared<TryStmt>(body, std::move(catches), cloneBlock(x.finallyBody), x.loc);
        }
        case Stmt::Throw: {
            auto& x = static_cast<ThrowStmt&>(*s);
            return std::make_shared<ThrowStmt>(cloneExpr(x.expr), x.loc);
        }
    }
    return nullptr;
}