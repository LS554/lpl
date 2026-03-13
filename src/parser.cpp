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

#include "parser.h"
#include <sstream>
#include <iostream>

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

// ============================================================
// Token access helpers
// ============================================================

const Token& Parser::peek() const { return tokens[current]; }

const Token& Parser::peekNext() const {
    if (current + 1 < tokens.size()) return tokens[current + 1];
    return tokens.back();
}

const Token& Parser::previous() const { return tokens[current - 1]; }

Token Parser::advance() {
    if (!atEnd()) current++;
    return tokens[current - 1];
}

bool Parser::check(TokenType t) const { return peek().type == t; }

bool Parser::match(TokenType t) {
    if (check(t)) { advance(); return true; }
    return false;
}

Token Parser::expect(TokenType t, const std::string& msg) {
    if (check(t)) return advance();
    std::ostringstream oss;
    oss << peek().loc.file << ":" << peek().loc.line << ":" << peek().loc.col
        << ": error: expected " << msg << " but got " << tokenTypeName(peek().type);
    if (!peek().value.empty()) oss << " '" << peek().value << "'";
    error(oss.str());
    return peek();
}

bool Parser::atEnd() const { return peek().type == TokenType::Eof; }

void Parser::error(const std::string& msg) {
    errors.push_back(msg);
}

void Parser::synchronize() {
    advance();
    while (!atEnd()) {
        if (previous().type == TokenType::Semicolon) return;
        switch (peek().type) {
            case TokenType::KW_class:
            case TokenType::KW_interface:
            case TokenType::KW_void:
            case TokenType::KW_int:
            case TokenType::KW_bool:
            case TokenType::KW_string:
            case TokenType::KW_if:
            case TokenType::KW_while:
            case TokenType::KW_for:
            case TokenType::KW_do:
            case TokenType::KW_switch:
            case TokenType::KW_return:
            case TokenType::KW_extern:
            case TokenType::KW_try:
            case TokenType::KW_throw:
                return;
            default:
                advance();
        }
    }
}

// ============================================================
// Type parsing
// ============================================================

bool Parser::isTypeStart() const {
    switch (peek().type) {
        case TokenType::KW_void: case TokenType::KW_bool: case TokenType::KW_byte:
        case TokenType::KW_char: case TokenType::KW_short: case TokenType::KW_int:
        case TokenType::KW_long: case TokenType::KW_float: case TokenType::KW_double:
        case TokenType::KW_string: case TokenType::KW_owner: case TokenType::KW_const:
        case TokenType::KW_func: case TokenType::KW_auto:
        case TokenType::Identifier:
            return true;
        default:
            return false;
    }
}

TypeSpec Parser::parseType() {
    TypeSpec ts;

    // Handle 'auto' type
    if (match(TokenType::KW_auto)) {
        ts.kind = TypeSpec::Auto;
        return ts;
    }

    // Check for 'owner' prefix
    if (match(TokenType::KW_owner)) {
        ts.isOwner = true;
    }

    // Parse base type
    switch (peek().type) {
        case TokenType::KW_void:   advance(); ts.kind = TypeSpec::Void; break;
        case TokenType::KW_bool:   advance(); ts.kind = TypeSpec::Bool; break;
        case TokenType::KW_byte:   advance(); ts.kind = TypeSpec::Byte; break;
        case TokenType::KW_char:   advance(); ts.kind = TypeSpec::Char; break;
        case TokenType::KW_short:  advance(); ts.kind = TypeSpec::Short; break;
        case TokenType::KW_int:    advance(); ts.kind = TypeSpec::Int; break;
        case TokenType::KW_long:   advance(); ts.kind = TypeSpec::Long; break;
        case TokenType::KW_float:  advance(); ts.kind = TypeSpec::Float; break;
        case TokenType::KW_double: advance(); ts.kind = TypeSpec::Double; break;
        case TokenType::KW_string: advance(); ts.kind = TypeSpec::String; break;
        case TokenType::KW_func: {
            advance(); // consume 'func'
            ts.kind = TypeSpec::Callable;
            auto sig = std::make_shared<FuncSignature>();
            expect(TokenType::LParen, "'('");
            while (!check(TokenType::RParen) && !atEnd()) {
                sig->paramTypes.push_back(parseType());
                if (!check(TokenType::RParen)) expect(TokenType::Comma, "','");
            }
            expect(TokenType::RParen, "')'");
            expect(TokenType::Arrow, "'->'");
            sig->returnType = parseType();
            ts.funcSignature = sig;
            break;
        }
        case TokenType::Identifier:
            ts.kind = TypeSpec::ClassName;
            ts.className = advance().value;
            // Handle dotted class names (qualified): Std.IO.Console
            while (check(TokenType::Dot) && peekNext().type == TokenType::Identifier) {
                advance(); // consume '.'
                ts.className += "." + advance().value;
            }
            // Parse generic type arguments: ClassName<Type1, Type2>
            tryParseTypeArgs(ts.typeArgs);
            break;
        default:
            error(std::string(peek().loc.file) + ":" + std::to_string(peek().loc.line)
                  + ": error: expected type");
            return ts;
    }

    // Check for pointer or reference suffix
    while (match(TokenType::Star)) {
        ts.pointerDepth++;
    }
    if (!ts.pointerDepth && match(TokenType::Ampersand)) {
        ts.isReference = true;
    }

    // Check for array suffix
    if (check(TokenType::LBracket) && peekNext().type == TokenType::RBracket) {
        advance(); advance();
        ts.isArray = true;
    }

    // Validate: owner only applies to pointers or arrays
    if (ts.isOwner && !ts.pointerDepth && !ts.isArray) {
        error("'owner' qualifier can only be applied to pointer or array types");
    }

    return ts;
}

// Parse type parameters in declarations: <T, U>
std::vector<std::string> Parser::parseTypeParams() {
    std::vector<std::string> params;
    if (!match(TokenType::Lt)) return params;
    if (!check(TokenType::Gt)) {
        params.push_back(expect(TokenType::Identifier, "type parameter name").value);
        while (match(TokenType::Comma)) {
            params.push_back(expect(TokenType::Identifier, "type parameter name").value);
        }
    }
    expect(TokenType::Gt, "'>'");
    return params;
}

// Speculatively parse generic type arguments: <int, Box<string>>
// Returns true if successfully parsed, false if not (restores position on failure)
bool Parser::tryParseTypeArgs(std::vector<TypeSpec>& out) {
    if (!check(TokenType::Lt)) return false;

    size_t saved = current;
    size_t savedErrors = errors.size();
    advance(); // consume '<'

    std::vector<TypeSpec> args;
    int depth = 1;

    // Try to parse a comma-separated list of types until matching '>'
    while (depth > 0 && !atEnd()) {
        // Check that next token can start a type; if not, bail out
        if (!isTypeStart()) {
            current = saved;
            errors.resize(savedErrors);
            return false;
        }
        args.push_back(parseType());
        // If parseType emitted errors, bail out
        if (errors.size() > savedErrors) {
            current = saved;
            errors.resize(savedErrors);
            return false;
        }

        if (check(TokenType::Gt)) {
            advance();
            depth--;
        } else if (check(TokenType::RShift)) {
            // >> in nested generics like Box<List<int>>: split into two >'s
            advance();
            depth -= 2;
        } else if (match(TokenType::Comma)) {
            // continue parsing next type arg
        } else {
            // Not valid type args — backtrack
            current = saved;
            errors.resize(savedErrors);
            return false;
        }
    }

    if (depth != 0) {
        // Unbalanced — backtrack
        current = saved;
        errors.resize(savedErrors);
        return false;
    }

    out = std::move(args);
    return true;
}

// ============================================================
// Top-level parsing
// ============================================================

Program Parser::parse() {
    Program prog;

    while (!atEnd()) {
        try {
            // Handle includes before attributes
            if (check(TokenType::KW_include)) {
                prog.declarations.push_back(parseInclude());
                continue;
            }

            // Handle namespace declarations
            if (check(TokenType::KW_namespace)) {
                auto nsDecl = parseNamespace();
                auto ns = std::dynamic_pointer_cast<NamespaceDecl>(nsDecl);
                if (ns && ns->isFileLevel) {
                    // File-level namespace: applies to all subsequent declarations
                    currentNamespace_ = ns->path;
                } else if (ns) {
                    // Block-level namespace: declarations already have namespacePath set
                    for (auto& d : ns->declarations) {
                        prog.declarations.push_back(d);
                    }
                }
                continue;
            }

            auto attrs = parseAttributes();

            if (check(TokenType::KW_abstract) && peekNext().type == TokenType::KW_class) {
                advance(); // consume 'abstract'
                auto decl = parseClassDecl();
                auto cls = std::dynamic_pointer_cast<ClassDecl>(decl);
                if (cls) {
                    cls->isAbstract = true;
                    if (!currentNamespace_.empty() && cls->namespacePath.empty())
                        cls->namespacePath = currentNamespace_;
                }
                prog.declarations.push_back(decl);
            } else if (check(TokenType::KW_class)) {
                auto decl = parseClassDecl();
                if (!currentNamespace_.empty()) {
                    auto cls = std::dynamic_pointer_cast<ClassDecl>(decl);
                    if (cls && cls->namespacePath.empty())
                        cls->namespacePath = currentNamespace_;
                }
                prog.declarations.push_back(decl);
            } else if (check(TokenType::KW_interface)) {
                auto decl = parseInterfaceDecl();
                if (!currentNamespace_.empty()) {
                    auto ifc = std::dynamic_pointer_cast<InterfaceDecl>(decl);
                    if (ifc && ifc->namespacePath.empty())
                        ifc->namespacePath = currentNamespace_;
                }
                prog.declarations.push_back(decl);
            } else if (check(TokenType::KW_extern)) {
                prog.declarations.push_back(parseExternBlock());
            } else if (check(TokenType::KW_squib) || isTypeStart()) {
                // Function declaration: [squib] type name(...)
                bool fnSquib = match(TokenType::KW_squib);
                TypeSpec retType = parseType();
                std::string name = expect(TokenType::Identifier, "function name").value;
                auto decl = parseFunctionDecl(attrs, retType, name);
                auto fn = std::dynamic_pointer_cast<FunctionDecl>(decl);
                if (fn) {
                    fn->isSquib = fnSquib;
                    if (!currentNamespace_.empty() && fn->namespacePath.empty())
                        fn->namespacePath = currentNamespace_;
                }
                prog.declarations.push_back(decl);
            } else {
                error(std::string(peek().loc.file) + ":" + std::to_string(peek().loc.line)
                      + ": error: unexpected token '" + peek().value + "'");
                synchronize();
            }
        } catch (...) {
            synchronize();
        }
    }

    return prog;
}

std::vector<Attribute> Parser::parseAttributes() {
    std::vector<Attribute> attrs;
    while (check(TokenType::At)) {
        advance(); // @
        Attribute attr;
        attr.name = expect(TokenType::Identifier, "attribute name").value;
        if (match(TokenType::LParen)) {
            attr.arg = expect(TokenType::Identifier, "attribute argument").value;
            expect(TokenType::RParen, "')'");
        }
        attrs.push_back(attr);
    }
    return attrs;
}

// ============================================================
// Include parsing
// ============================================================

DeclPtr Parser::parseInclude() {
    SourceLoc loc = peek().loc;
    expect(TokenType::KW_include, "'include'");

    auto inc = std::make_shared<IncludeDecl>(loc);

    if (check(TokenType::Lt)) {
        // System include: include <console.lph>;
        advance(); // consume '<'
        std::string path;
        // Collect tokens until '>'
        while (!check(TokenType::Gt) && !atEnd()) {
            auto tok = advance();
            path += tok.value;
            // Handle dots in filenames
            if (check(TokenType::Dot) && !check(TokenType::Gt)) {
                advance();
                path += ".";
            }
        }
        expect(TokenType::Gt, "'>'");
        inc->path = path;
        inc->isSystem = true;
        // Auto-append .lph if extension was omitted
        if (inc->path.size() < 4 || inc->path.substr(inc->path.size() - 4) != ".lph") {
            inc->path += ".lph";
        }
    } else if (check(TokenType::StringLiteral)) {
        // Local include: include "person.lph";
        auto tok = advance();
        inc->path = tok.value;
        inc->isSystem = false;
    } else {
        error(std::string(peek().loc.file) + ":" + std::to_string(peek().loc.line)
              + ": error: expected string or '<' after 'include'");
    }

    // Semicolon is optional after include directives
    if (check(TokenType::Semicolon)) advance();
    return inc;
}

// ============================================================
// Namespace parsing
// ============================================================

std::string Parser::parseDottedName() {
    std::string name = expect(TokenType::Identifier, "name").value;
    while (check(TokenType::Dot) && peekNext().type == TokenType::Identifier) {
        advance(); // consume '.'
        name += "." + advance().value;
    }
    return name;
}

DeclPtr Parser::parseNamespace() {
    SourceLoc loc = peek().loc;
    expect(TokenType::KW_namespace, "'namespace'");

    auto ns = std::make_shared<NamespaceDecl>(loc);
    ns->path = parseDottedName();

    if (match(TokenType::Semicolon)) {
        // File-level namespace: namespace Std.IO;
        ns->isFileLevel = true;
        return ns;
    }

    // Block-level namespace: namespace Std.IO { ... }
    expect(TokenType::LBrace, "'{' or ';' after namespace path");

    std::string savedNamespace = currentNamespace_;
    currentNamespace_ = ns->path;

    while (!check(TokenType::RBrace) && !atEnd()) {
        try {
            if (check(TokenType::KW_include)) {
                ns->declarations.push_back(parseInclude());
                continue;
            }
            if (check(TokenType::KW_namespace)) {
                auto inner = parseNamespace();
                auto innerNs = std::dynamic_pointer_cast<NamespaceDecl>(inner);
                if (innerNs) {
                    // Nested namespace: prepend outer path
                    for (auto& d : innerNs->declarations) {
                        ns->declarations.push_back(d);
                    }
                }
                continue;
            }

            auto attrs = parseAttributes();

            if (check(TokenType::KW_abstract) && peekNext().type == TokenType::KW_class) {
                advance(); // consume 'abstract'
                auto decl = parseClassDecl();
                auto cls = std::dynamic_pointer_cast<ClassDecl>(decl);
                if (cls) {
                    cls->isAbstract = true;
                    if (cls->namespacePath.empty())
                        cls->namespacePath = ns->path;
                }
                ns->declarations.push_back(decl);
            } else if (check(TokenType::KW_class)) {
                auto decl = parseClassDecl();
                auto cls = std::dynamic_pointer_cast<ClassDecl>(decl);
                if (cls && cls->namespacePath.empty())
                    cls->namespacePath = ns->path;
                ns->declarations.push_back(decl);
            } else if (check(TokenType::KW_interface)) {
                auto decl = parseInterfaceDecl();
                auto ifc = std::dynamic_pointer_cast<InterfaceDecl>(decl);
                if (ifc && ifc->namespacePath.empty())
                    ifc->namespacePath = ns->path;
                ns->declarations.push_back(decl);
            } else if (check(TokenType::KW_extern)) {
                ns->declarations.push_back(parseExternBlock());
            } else if (isTypeStart()) {
                TypeSpec retType = parseType();
                std::string name = expect(TokenType::Identifier, "function name").value;
                auto decl = parseFunctionDecl(attrs, retType, name);
                auto fn = std::dynamic_pointer_cast<FunctionDecl>(decl);
                if (fn && fn->namespacePath.empty())
                    fn->namespacePath = ns->path;
                ns->declarations.push_back(decl);
            } else {
                error(std::string(peek().loc.file) + ":" + std::to_string(peek().loc.line)
                      + ": error: unexpected token in namespace block");
                synchronize();
            }
        } catch (...) {
            synchronize();
        }
    }

    expect(TokenType::RBrace, "'}'");
    currentNamespace_ = savedNamespace;
    return ns;
}

// ============================================================
// Class parsing
// ============================================================

DeclPtr Parser::parseClassDecl() {
    SourceLoc loc = peek().loc;
    expect(TokenType::KW_class, "'class'");
    auto cls = std::make_shared<ClassDecl>(loc);
    cls->name = expect(TokenType::Identifier, "class name").value;

    // Parse generic type parameters: class Box<T> { ... }
    cls->typeParams = parseTypeParams();

    // extends (supports qualified names)
    if (match(TokenType::KW_extends)) {
        cls->parentClass = parseDottedName();
    }

    // implements (supports qualified names)
    if (match(TokenType::KW_implements)) {
        cls->interfaces.push_back(parseDottedName());
        while (match(TokenType::Comma)) {
            cls->interfaces.push_back(parseDottedName());
        }
    }

    expect(TokenType::LBrace, "'{'");
    parseClassBody(*cls);
    expect(TokenType::RBrace, "'}'");

    return cls;
}

void Parser::parseClassBody(ClassDecl& cls) {
    while (!check(TokenType::RBrace) && !atEnd()) {
        SourceLoc memberLoc = peek().loc;

        // Parse access modifier
        AccessLevel access = AccessLevel::Private;
        if (match(TokenType::KW_public)) access = AccessLevel::Public;
        else if (match(TokenType::KW_protected)) access = AccessLevel::Protected;
        else if (match(TokenType::KW_private)) access = AccessLevel::Private;

        // Parse static
        bool isStatic = match(TokenType::KW_static);

        // Parse override
        bool isOverride = match(TokenType::KW_override);

        // Parse abstract (for methods)
        bool isAbstractMember = match(TokenType::KW_abstract);

        // Destructor: ~ClassName()
        if (check(TokenType::Tilde)) {
            advance(); // ~
            std::string name = expect(TokenType::Identifier, "destructor name").value;
            expect(TokenType::LParen, "'('");
            expect(TokenType::RParen, "')'");
            auto body = std::dynamic_pointer_cast<BlockStmt>(parseBlock());
            auto dtor = std::make_shared<DestructorDecl>();
            dtor->className = name;
            dtor->body = body;
            dtor->loc = memberLoc;
            cls.destructor = dtor;
            continue;
        }

        // Check if this is a constructor: ClassName(...)
        // A constructor looks like: [public] ClassName(params) { body }
        // We detect it by: identifier followed by '(' where the identifier matches the class name
        if (check(TokenType::Identifier) && peek().value == cls.name
            && peekNext().type == TokenType::LParen) {
            // Constructor
            advance(); // class name
            expect(TokenType::LParen, "'('");
            auto params = parseParamList();
            expect(TokenType::RParen, "')'");
            auto body = std::dynamic_pointer_cast<BlockStmt>(parseBlock());

            ConstructorDecl ctor;
            ctor.access = access;
            ctor.className = cls.name;
            ctor.params = std::move(params);
            ctor.body = body;
            ctor.loc = memberLoc;
            cls.constructors.push_back(std::move(ctor));
            continue;
        }

        // Otherwise it's a field or method: type name [= expr]; or type name(params) { body }
        // Check for const qualifier on fields
        bool isConst = match(TokenType::KW_const);

        TypeSpec type = parseType();

        std::string name;
        if (check(TokenType::KW_operator)) {
            advance(); // consume 'operator'
            switch (peek().type) {
                case TokenType::Plus:    name = "operator+"; advance(); break;
                case TokenType::Minus:   name = "operator-"; advance(); break;
                case TokenType::Star:    name = "operator*"; advance(); break;
                case TokenType::Slash:   name = "operator/"; advance(); break;
                case TokenType::Percent: name = "operator%"; advance(); break;
                case TokenType::EqEq:    name = "operator=="; advance(); break;
                case TokenType::BangEq:  name = "operator!="; advance(); break;
                case TokenType::Lt:      name = "operator<"; advance(); break;
                case TokenType::Gt:      name = "operator>"; advance(); break;
                case TokenType::LtEq:    name = "operator<="; advance(); break;
                case TokenType::GtEq:    name = "operator>="; advance(); break;
                case TokenType::LBracket:
                    advance(); // [
                    expect(TokenType::RBracket, "']'");
                    name = "operator[]";
                    break;
                default:
                    error("expected operator symbol after 'operator'");
                    name = "operator+"; // recovery
            }
        } else {
            name = expect(TokenType::Identifier, "member name").value;
        }

        if (match(TokenType::LParen)) {
            // Method
            auto params = parseParamList();
            expect(TokenType::RParen, "')'");

            std::shared_ptr<BlockStmt> body;
            if (isAbstractMember) {
                // Abstract method — no body, terminated by semicolon
                expect(TokenType::Semicolon, "';' after abstract method");
                body = nullptr;
            } else {
                body = std::dynamic_pointer_cast<BlockStmt>(parseBlock());
            }

            MethodDecl method;
            method.access = access;
            method.isStatic = isStatic;
            method.isOverride = isOverride;
            method.isAbstract = isAbstractMember;
            method.returnType = type;
            method.name = name;
            method.params = std::move(params);
            method.body = body;
            method.loc = memberLoc;
            cls.methods.push_back(std::move(method));
        } else {
            // Field
            FieldDecl field;
            field.access = access;
            field.isStatic = isStatic;
            field.isConst = isConst;
            field.type = type;
            field.name = name;
            field.loc = memberLoc;

            if (match(TokenType::Eq)) {
                field.init = parseExpr();
            }
            expect(TokenType::Semicolon, "';'");
            cls.fields.push_back(std::move(field));
        }
    }
}

std::vector<Param> Parser::parseParamList() {
    std::vector<Param> params;
    if (check(TokenType::RParen)) return params; // empty

    // Check for variadic (...)
    if (check(TokenType::Ellipsis)) {
        advance();
        return params; // variadic marker handled at call site
    }

    do {
        Param p;
        p.type = parseType();
        p.name = expect(TokenType::Identifier, "parameter name").value;
        if (match(TokenType::Eq)) {
            p.defaultValue = parseExpr();
        }
        params.push_back(std::move(p));
    } while (match(TokenType::Comma) && !check(TokenType::Ellipsis) && !check(TokenType::RParen));

    if (match(TokenType::Ellipsis)) {
        // variadic - handled by caller
    }

    return params;
}

// ============================================================
// Interface parsing
// ============================================================

DeclPtr Parser::parseInterfaceDecl() {
    SourceLoc loc = peek().loc;
    expect(TokenType::KW_interface, "'interface'");
    auto iface = std::make_shared<InterfaceDecl>(loc);
    iface->name = expect(TokenType::Identifier, "interface name").value;
    expect(TokenType::LBrace, "'{'");

    while (!check(TokenType::RBrace) && !atEnd()) {
        InterfaceDecl::MethodSig sig;
        sig.returnType = parseType();
        sig.name = expect(TokenType::Identifier, "method name").value;
        expect(TokenType::LParen, "'('");
        sig.params = parseParamList();
        expect(TokenType::RParen, "')'");
        expect(TokenType::Semicolon, "';'");
        iface->methods.push_back(std::move(sig));
    }

    expect(TokenType::RBrace, "'}'");
    return iface;
}

// ============================================================
// Function parsing
// ============================================================

DeclPtr Parser::parseFunctionDecl(std::vector<Attribute> attrs, TypeSpec retType, const std::string& name) {
    SourceLoc loc = previous().loc;
    auto fn = std::make_shared<FunctionDecl>(loc);
    fn->attributes = std::move(attrs);
    fn->returnType = retType;
    fn->name = name;

    // Parse generic type parameters: T max<T>(T a, T b)
    fn->typeParams = parseTypeParams();

    expect(TokenType::LParen, "'('");
    fn->params = parseParamList();
    expect(TokenType::RParen, "')'");
    fn->body = std::dynamic_pointer_cast<BlockStmt>(parseBlock());

    return fn;
}

// ============================================================
// Extern block parsing
// ============================================================

DeclPtr Parser::parseExternBlock() {
    SourceLoc loc = peek().loc;
    expect(TokenType::KW_extern, "'extern'");
    auto block = std::make_shared<ExternBlockDecl>(loc);
    block->convention = expect(TokenType::StringLiteral, "calling convention string").value;

    if (block->convention != "C" && block->convention != "C++") {
        error(std::string(loc.file) + ":" + std::to_string(loc.line)
              + ": error: unsupported calling convention '" + block->convention
              + "' (expected \"C\" or \"C++\")");
    }

    expect(TokenType::LBrace, "'{'");

    while (!check(TokenType::RBrace) && !atEnd()) {
        // Handle #include directives inside extern "C" blocks
        if (check(TokenType::Hash)) {
            SourceLoc incLoc = peek().loc;
            advance(); // consume '#'
            if (!check(TokenType::KW_include)) {
                error(std::string(incLoc.file) + ":" + std::to_string(incLoc.line)
                      + ": error: expected 'include' after '#' in extern block");
                while (!check(TokenType::RBrace) && !atEnd()) advance();
                break;
            }
            advance(); // consume 'include'
            ExternCInclude inc;
            inc.loc = incLoc;
            if (check(TokenType::Lt)) {
                advance(); // consume '<'
                inc.isSystem = true;
                std::string path;
                while (!check(TokenType::Gt) && !check(TokenType::RBrace) && !atEnd()) {
                    path += advance().value;
                }
                expect(TokenType::Gt, "'>'");
                inc.path = path;
            } else if (check(TokenType::StringLiteral)) {
                inc.path = advance().value;
                inc.isSystem = false;
            } else {
                error(std::string(incLoc.file) + ":" + std::to_string(incLoc.line)
                      + ": error: expected '<...>' or '\"...\"' after #include");
                while (!check(TokenType::RBrace) && !atEnd()) advance();
                break;
            }
            block->cincludes.push_back(std::move(inc));
            continue;
        }

        ExternFuncDecl efd;
        efd.loc = peek().loc;
        efd.returnType = parseType();
        efd.name = expect(TokenType::Identifier, "function name").value;
        expect(TokenType::LParen, "'('");

        // Parse params, handling variadics
        if (!check(TokenType::RParen)) {
            if (check(TokenType::Ellipsis)) {
                advance();
                efd.isVariadic = true;
            } else {
                do {
                    if (check(TokenType::Ellipsis)) {
                        advance();
                        efd.isVariadic = true;
                        break;
                    }
                    Param p;
                    p.type = parseType();
                    if (check(TokenType::Identifier)) {
                        p.name = advance().value;
                    }
                    efd.params.push_back(std::move(p));
                } while (match(TokenType::Comma));
            }
        }

        expect(TokenType::RParen, "')'");

        // Optional link name: as "mangled_symbol"
        if (check(TokenType::KW_as)) {
            advance();
            efd.linkName = expect(TokenType::StringLiteral, "link name string").value;
        }

        expect(TokenType::Semicolon, "';'");
        block->functions.push_back(std::move(efd));
    }

    expect(TokenType::RBrace, "'}'");
    return block;
}

// ============================================================
// Statement parsing
// ============================================================

StmtPtr Parser::parseStmt() {
    if (check(TokenType::LBrace)) return parseBlock();
    if (check(TokenType::KW_if)) return parseIfStmt();
    if (check(TokenType::KW_while)) return parseWhileStmt();
    if (check(TokenType::KW_for)) return parseForStmt();
    if (check(TokenType::KW_do)) return parseDoWhileStmt();
    if (check(TokenType::KW_switch)) return parseSwitchStmt();
    if (check(TokenType::KW_try)) return parseTryStmt();
    if (check(TokenType::KW_throw)) return parseThrowStmt();
    if (check(TokenType::KW_return)) return parseReturnStmt();
    if (check(TokenType::KW_delete)) return parseDeleteStmt();
    if (check(TokenType::KW_defer)) return parseDeferStmt();
    if (check(TokenType::KW_break)) {
        SourceLoc l = peek().loc; advance();
        expect(TokenType::Semicolon, "';'");
        return std::make_shared<BreakStmt>(l);
    }
    if (check(TokenType::KW_continue)) {
        SourceLoc l = peek().loc; advance();
        expect(TokenType::Semicolon, "';'");
        return std::make_shared<ContinueStmt>(l);
    }
    if (check(TokenType::KW_fallthrough)) {
        SourceLoc l = peek().loc; advance();
        expect(TokenType::Semicolon, "';'");
        return std::make_shared<FallthroughStmt>(l);
    }
    return parseVarDeclOrExprStmt();
}

StmtPtr Parser::parseBlock() {
    SourceLoc loc = peek().loc;
    expect(TokenType::LBrace, "'{'");
    std::vector<StmtPtr> stmts;
    while (!check(TokenType::RBrace) && !atEnd()) {
        stmts.push_back(parseStmt());
    }
    expect(TokenType::RBrace, "'}'");
    return std::make_shared<BlockStmt>(std::move(stmts), loc);
}

StmtPtr Parser::parseVarDeclOrExprStmt() {
    SourceLoc loc = peek().loc;

    // Try to detect variable declaration: [squib] [const] [owner] type name = ...
    // Heuristic: if it starts with a type keyword (or 'owner', 'const', 'squib'), it's a var decl
    bool looksLikeVarDecl = false;
    bool isConst = false;
    bool isSquib = false;

    if (check(TokenType::KW_squib)) {
        looksLikeVarDecl = true;
        isSquib = true;
    } else if (check(TokenType::KW_const)) {
        looksLikeVarDecl = true;
        isConst = true;
    } else if (check(TokenType::KW_owner)) {
        looksLikeVarDecl = true;
    } else if (check(TokenType::KW_auto)) {
        looksLikeVarDecl = true;
    } else if (isTypeStart()) {
        // Need lookahead to distinguish:  Type name = ...  vs  expr;
        // Save position and try to parse as type + identifier
        size_t saved = current;
        bool savedHasError = !errors.empty();

        // For primitive types, it's definitely a type if followed by identifier
        if (peek().type != TokenType::Identifier) {
            // Primitive type keyword -> definitely var decl
            looksLikeVarDecl = true;
        } else {
            // Identifier - could be a type (class name) or a variable/expression
            // Look ahead: Identifier Identifier => var decl
            // Identifier.Identifier...Identifier varName => var decl (qualified type)
            // Identifier . member ( => expression (method call)
            // Identifier ( => expression (function call)
            // Identifier * Identifier => pointer var decl
            size_t saved = current;
            advance(); // consume the first identifier

            // Skip through dotted name: Std.IO.Console
            while (check(TokenType::Dot) && peekNext().type == TokenType::Identifier) {
                advance(); // consume '.'
                advance(); // consume next identifier
            }

            // Skip generic type arguments: Box<int>, Pair<int, string>
            if (check(TokenType::Lt)) {
                // Speculatively skip balanced <...> without calling parseType
                // (which could emit errors on non-type tokens like `i < 5`)
                size_t savedGeneric = current;
                advance(); // skip '<'
                int depth = 1;
                bool valid = true;
                while (depth > 0 && !atEnd()) {
                    if (check(TokenType::Lt)) { depth++; advance(); }
                    else if (check(TokenType::Gt)) { depth--; advance(); }
                    else if (check(TokenType::RShift)) { depth -= 2; advance(); }
                    else if (check(TokenType::Semicolon) || check(TokenType::LBrace)) {
                        valid = false; break; // clearly not type args
                    }
                    else { advance(); }
                }
                if (!valid || depth != 0) {
                    current = savedGeneric; // restore on failure
                }
            }

            if (check(TokenType::Identifier)) {
                // TypeName varName - definitely a var decl
                looksLikeVarDecl = true;
            } else if (check(TokenType::Star)) {
                // Could be ClassName* varName (pointer decl) or multiplication
                advance(); // consume *
                if (check(TokenType::Identifier)) {
                    looksLikeVarDecl = true;
                }
            } else if (check(TokenType::Ampersand)) {
                // ClassName& varName - reference var decl
                advance();
                if (check(TokenType::Identifier)) {
                    looksLikeVarDecl = true;
                }
            }

            // Restore position
            current = saved;
        }
    }

    if (looksLikeVarDecl) {
        if (isSquib) advance(); // consume 'squib'
        if (check(TokenType::KW_const)) {
            isConst = true;
            advance(); // consume 'const'
        } else if (isConst) {
            advance(); // consume 'const'
        }
        TypeSpec type = parseType();
        std::string name = expect(TokenType::Identifier, "variable name").value;
        ExprPtr init = nullptr;
        if (match(TokenType::Eq)) {
            init = parseExpr();
        }
        expect(TokenType::Semicolon, "';'");
        auto decl = std::make_shared<VarDeclStmt>(std::move(type), std::move(name), std::move(init), loc);
        decl->isConst = isConst;
        decl->isSquib = isSquib;
        return decl;
    }

    // Expression statement
    ExprPtr expr = parseExpr();
    expect(TokenType::Semicolon, "';'");
    return std::make_shared<ExprStmt>(std::move(expr), loc);
}

StmtPtr Parser::parseIfStmt() {
    SourceLoc loc = peek().loc;
    expect(TokenType::KW_if, "'if'");
    expect(TokenType::LParen, "'('");
    auto cond = parseExpr();
    expect(TokenType::RParen, "')'");
    auto then = parseStmt();
    StmtPtr elseB = nullptr;
    if (match(TokenType::KW_else)) {
        elseB = parseStmt();
    }
    return std::make_shared<IfStmt>(std::move(cond), std::move(then), std::move(elseB), loc);
}

StmtPtr Parser::parseWhileStmt() {
    SourceLoc loc = peek().loc;
    expect(TokenType::KW_while, "'while'");
    expect(TokenType::LParen, "'('");
    auto cond = parseExpr();
    expect(TokenType::RParen, "')'");
    auto body = parseStmt();
    return std::make_shared<WhileStmt>(std::move(cond), std::move(body), loc);
}

StmtPtr Parser::parseForStmt() {
    SourceLoc loc = peek().loc;
    expect(TokenType::KW_for, "'for'");
    expect(TokenType::LParen, "'('");

    // Detect for-each pattern: for (Type name : start..end)
    // Lookahead: if we see Type Ident Colon, it's for-each
    if (isTypeStart()) {
        size_t saved = current;
        // Try parsing Type name :
        parseType(); // skip type
        if (check(TokenType::Identifier)) {
            advance(); // skip name
            if (check(TokenType::Colon)) {
                // It's a for-each! Rewind and parse properly
                current = saved;
                TypeSpec varType = parseType();
                std::string varName = expect(TokenType::Identifier, "variable name").value;
                expect(TokenType::Colon, "':'");
                auto rangeStart = parseExpr();
                bool inclusive = false;
                if (match(TokenType::DotDotEq)) {
                    inclusive = true;
                } else {
                    expect(TokenType::DotDot, "'..'");
                }
                auto rangeEnd = parseExpr();
                expect(TokenType::RParen, "')'");
                auto body = parseStmt();
                return std::make_shared<ForEachStmt>(
                    std::move(varType), std::move(varName),
                    std::move(rangeStart), std::move(rangeEnd),
                    inclusive, std::move(body), loc);
            }
        }
        // Not a for-each, rewind
        current = saved;
    }

    // Standard C-style for loop
    // Init
    StmtPtr init = nullptr;
    if (!check(TokenType::Semicolon)) {
        init = parseVarDeclOrExprStmt(); // includes the semicolon
    } else {
        advance(); // skip ;
    }

    // Condition
    ExprPtr cond = nullptr;
    if (!check(TokenType::Semicolon)) {
        cond = parseExpr();
    }
    expect(TokenType::Semicolon, "';'");

    // Increment
    ExprPtr incr = nullptr;
    if (!check(TokenType::RParen)) {
        incr = parseExpr();
    }
    expect(TokenType::RParen, "')'");

    auto body = parseStmt();
    return std::make_shared<ForStmt>(std::move(init), std::move(cond), std::move(incr), std::move(body), loc);
}

StmtPtr Parser::parseReturnStmt() {
    SourceLoc loc = peek().loc;
    expect(TokenType::KW_return, "'return'");

    ExprPtr val = nullptr;
    bool isMove = false;

    if (!check(TokenType::Semicolon)) {
        if (match(TokenType::KW_move)) {
            isMove = true;
        }
        val = parseExpr();
    }
    expect(TokenType::Semicolon, "';'");
    return std::make_shared<ReturnStmt>(std::move(val), isMove, loc);
}

StmtPtr Parser::parseDeleteStmt() {
    SourceLoc loc = peek().loc;
    expect(TokenType::KW_delete, "'delete'");
    auto expr = parseExpr();
    expect(TokenType::Semicolon, "';'");
    return std::make_shared<DeleteStmt>(std::move(expr), loc);
}

StmtPtr Parser::parseDeferStmt() {
    SourceLoc loc = peek().loc;
    expect(TokenType::KW_defer, "'defer'");
    auto body = parseStmt();
    return std::make_shared<DeferStmt>(std::move(body), loc);
}

StmtPtr Parser::parseDoWhileStmt() {
    SourceLoc loc = peek().loc;
    expect(TokenType::KW_do, "'do'");
    auto body = parseStmt();
    expect(TokenType::KW_while, "'while'");
    expect(TokenType::LParen, "'('");
    auto cond = parseExpr();
    expect(TokenType::RParen, "')'");
    expect(TokenType::Semicolon, "';'");
    return std::make_shared<DoWhileStmt>(std::move(body), std::move(cond), loc);
}

StmtPtr Parser::parseSwitchStmt() {
    SourceLoc loc = peek().loc;
    expect(TokenType::KW_switch, "'switch'");
    expect(TokenType::LParen, "'('");
    auto expr = parseExpr();
    expect(TokenType::RParen, "')'");
    expect(TokenType::LBrace, "'{'");

    std::vector<SwitchCase> cases;
    while (!check(TokenType::RBrace) && !atEnd()) {
        SwitchCase sc;
        if (match(TokenType::KW_case)) {
            sc.value = parseExpr();
            expect(TokenType::Colon, "':'");
        } else if (match(TokenType::KW_default)) {
            sc.value = nullptr; // default case
            expect(TokenType::Colon, "':'");
        } else {
            error("expected 'case' or 'default'");
            synchronize();
            continue;
        }
        // Parse statements until next case/default/closing brace
        while (!check(TokenType::KW_case) && !check(TokenType::KW_default)
               && !check(TokenType::RBrace) && !atEnd()) {
            sc.body.push_back(parseStmt());
        }
        cases.push_back(std::move(sc));
    }
    expect(TokenType::RBrace, "'}'");
    return std::make_shared<SwitchStmt>(std::move(expr), std::move(cases), loc);
}

StmtPtr Parser::parseTryStmt() {
    SourceLoc loc = peek().loc;
    expect(TokenType::KW_try, "'try'");
    auto body = std::dynamic_pointer_cast<BlockStmt>(parseBlock());

    std::vector<CatchClause> catches;
    while (check(TokenType::KW_catch)) {
        SourceLoc catchLoc = peek().loc;
        advance(); // consume 'catch'
        expect(TokenType::LParen, "'('");
        TypeSpec excType = parseType();
        std::string varName = expect(TokenType::Identifier, "exception variable name").value;
        expect(TokenType::RParen, "')'");
        auto catchBody = std::dynamic_pointer_cast<BlockStmt>(parseBlock());

        CatchClause cc;
        cc.exceptionType = excType;
        cc.varName = varName;
        cc.body = catchBody;
        cc.loc = catchLoc;
        catches.push_back(std::move(cc));
    }

    std::shared_ptr<BlockStmt> finallyBody;
    if (match(TokenType::KW_finally)) {
        finallyBody = std::dynamic_pointer_cast<BlockStmt>(parseBlock());
    }

    if (catches.empty() && !finallyBody) {
        error(std::string(loc.file) + ":" + std::to_string(loc.line)
              + ": error: try without catch or finally");
    }

    return std::make_shared<TryStmt>(std::move(body), std::move(catches), std::move(finallyBody), loc);
}

StmtPtr Parser::parseThrowStmt() {
    SourceLoc loc = peek().loc;
    expect(TokenType::KW_throw, "'throw'");
    auto expr = parseExpr();
    expect(TokenType::Semicolon, "';'");
    return std::make_shared<ThrowStmt>(std::move(expr), loc);
}

// ============================================================
// Lambda parsing
// ============================================================

ExprPtr Parser::parseLambdaExpr() {
    SourceLoc loc = peek().loc;

    // Parse capture list: [captures]
    expect(TokenType::LBracket, "'['");
    LambdaExpr::CaptureDefault captureDefault = LambdaExpr::None;
    std::vector<CaptureItem> captures;

    if (!check(TokenType::RBracket)) {
        if (check(TokenType::Eq)) {
            advance();
            captureDefault = LambdaExpr::AllByValue;
        } else if (check(TokenType::Ampersand) && peekNext().type == TokenType::RBracket) {
            advance();
            captureDefault = LambdaExpr::AllByRef;
        } else {
            // Parse individual captures: x, &y, this
            do {
                CaptureItem item;
                if (check(TokenType::Ampersand)) {
                    advance();
                    item.byRef = true;
                }
                if (check(TokenType::KW_this)) {
                    advance();
                    item.name = "this";
                } else {
                    item.name = expect(TokenType::Identifier, "capture name").value;
                }
                captures.push_back(std::move(item));
            } while (match(TokenType::Comma));
        }
    }
    expect(TokenType::RBracket, "']'");

    // Parse optional parameter list
    std::vector<Param> params;
    if (check(TokenType::LParen)) {
        advance();
        params = parseParamList();
        expect(TokenType::RParen, "')'");
    }

    // Parse optional return type
    TypeSpec returnType;
    returnType.kind = TypeSpec::Void;
    bool hasExplicitReturnType = false;
    if (check(TokenType::Arrow)) {
        advance();
        returnType = parseType();
        hasExplicitReturnType = true;
    }

    // Parse body
    auto body = std::dynamic_pointer_cast<BlockStmt>(parseBlock());

    return std::make_shared<LambdaExpr>(captureDefault, std::move(captures),
        std::move(params), std::move(returnType), hasExplicitReturnType,
        std::move(body), loc);
}

// ============================================================
// Expression parsing (precedence climbing)
// ============================================================

ExprPtr Parser::parseExpr() {
    return parseAssignment();
}

ExprPtr Parser::parseAssignment() {
    auto left = parseLogicalOr();

    // Ternary operator: expr ? thenExpr : elseExpr
    if (check(TokenType::Question)) {
        SourceLoc loc = peek().loc;
        advance(); // consume '?'
        auto thenExpr = parseExpr();
        expect(TokenType::Colon, "':'");
        auto elseExpr = parseAssignment();
        return std::make_shared<TernaryExpr>(std::move(left), std::move(thenExpr), std::move(elseExpr), loc);
    }

    if (check(TokenType::Eq) || check(TokenType::PlusEq) || check(TokenType::MinusEq)
        || check(TokenType::StarEq) || check(TokenType::SlashEq)) {
        SourceLoc loc = peek().loc;
        TokenType op = advance().type;

        auto right = parseAssignment(); // right-associative

        if (op == TokenType::Eq) {
            return std::make_shared<AssignExpr>(std::move(left), std::move(right), loc);
        }
        // Desugar compound assignment: a += b -> a = a + b
        TokenType binOp;
        switch (op) {
            case TokenType::PlusEq:  binOp = TokenType::Plus; break;
            case TokenType::MinusEq: binOp = TokenType::Minus; break;
            case TokenType::StarEq:  binOp = TokenType::Star; break;
            case TokenType::SlashEq: binOp = TokenType::Slash; break;
            default: binOp = TokenType::Plus; break;
        }
        auto binExpr = std::make_shared<BinaryExpr>(binOp, left, std::move(right), loc);
        return std::make_shared<AssignExpr>(std::move(left), std::move(binExpr), loc);
    }

    return left;
}

ExprPtr Parser::parseLogicalOr() {
    auto left = parseLogicalAnd();
    while (match(TokenType::PipePipe)) {
        SourceLoc loc = previous().loc;
        auto right = parseLogicalAnd();
        left = std::make_shared<BinaryExpr>(TokenType::PipePipe, std::move(left), std::move(right), loc);
    }
    return left;
}

ExprPtr Parser::parseLogicalAnd() {
    auto left = parseEquality();
    while (match(TokenType::AmpAmp)) {
        SourceLoc loc = previous().loc;
        auto right = parseEquality();
        left = std::make_shared<BinaryExpr>(TokenType::AmpAmp, std::move(left), std::move(right), loc);
    }
    return left;
}

ExprPtr Parser::parseEquality() {
    auto left = parseComparison();
    while (check(TokenType::EqEq) || check(TokenType::BangEq)) {
        TokenType op = advance().type;
        SourceLoc loc = previous().loc;
        auto right = parseComparison();
        left = std::make_shared<BinaryExpr>(op, std::move(left), std::move(right), loc);
    }
    return left;
}

ExprPtr Parser::parseComparison() {
    auto left = parseAddition();
    while (check(TokenType::Lt) || check(TokenType::Gt)
           || check(TokenType::LtEq) || check(TokenType::GtEq)) {
        TokenType op = advance().type;
        SourceLoc loc = previous().loc;
        auto right = parseAddition();
        left = std::make_shared<BinaryExpr>(op, std::move(left), std::move(right), loc);
    }
    return left;
}

ExprPtr Parser::parseAddition() {
    auto left = parseMultiplication();
    while (check(TokenType::Plus) || check(TokenType::Minus)) {
        TokenType op = advance().type;
        SourceLoc loc = previous().loc;
        auto right = parseMultiplication();
        left = std::make_shared<BinaryExpr>(op, std::move(left), std::move(right), loc);
    }
    return left;
}

ExprPtr Parser::parseMultiplication() {
    auto left = parseUnary();
    while (check(TokenType::Star) || check(TokenType::Slash) || check(TokenType::Percent)) {
        TokenType op = advance().type;
        SourceLoc loc = previous().loc;
        auto right = parseUnary();
        left = std::make_shared<BinaryExpr>(op, std::move(left), std::move(right), loc);
    }
    return left;
}

ExprPtr Parser::parseUnary() {
    SourceLoc loc = peek().loc;

    if (match(TokenType::Bang)) {
        auto operand = parseUnary();
        return std::make_shared<UnaryExpr>(TokenType::Bang, std::move(operand), loc);
    }
    if (match(TokenType::Minus)) {
        auto operand = parseUnary();
        return std::make_shared<UnaryExpr>(TokenType::Minus, std::move(operand), loc);
    }
    if (match(TokenType::Ampersand)) {
        auto operand = parseUnary();
        return std::make_shared<AddressOfExpr>(std::move(operand), loc);
    }
    if (match(TokenType::Star)) {
        auto operand = parseUnary();
        return std::make_shared<DerefExpr>(std::move(operand), loc);
    }
    if (match(TokenType::KW_move)) {
        auto operand = parseUnary();
        return std::make_shared<MoveExpr>(std::move(operand), loc);
    }
    // Prefix ++/--: desugar ++x to (x = x + 1), --x to (x = x - 1)
    if (match(TokenType::PlusPlus)) {
        auto operand = parseUnary();
        auto one = std::make_shared<IntLitExpr>(1, loc);
        auto add = std::make_shared<BinaryExpr>(TokenType::Plus, operand, std::move(one), loc);
        return std::make_shared<AssignExpr>(std::move(operand), std::move(add), loc);
    }
    if (match(TokenType::MinusMinus)) {
        auto operand = parseUnary();
        auto one = std::make_shared<IntLitExpr>(1, loc);
        auto sub = std::make_shared<BinaryExpr>(TokenType::Minus, operand, std::move(one), loc);
        return std::make_shared<AssignExpr>(std::move(operand), std::move(sub), loc);
    }

    return parsePostfix();
}

ExprPtr Parser::parsePostfix() {
    auto expr = parsePrimary();

    while (true) {
        SourceLoc loc = peek().loc;

        if (match(TokenType::Dot)) {
            std::string member = expect(TokenType::Identifier, "member name").value;
            if (match(TokenType::LParen)) {
                auto args = parseArgList();
                expect(TokenType::RParen, "')'");
                expr = std::make_shared<MethodCallExpr>(std::move(expr), member, std::move(args), loc);
            } else {
                expr = std::make_shared<MemberAccessExpr>(std::move(expr), member, loc);
            }
        } else if (match(TokenType::LBracket)) {
            auto index = parseExpr();
            expect(TokenType::RBracket, "']'");
            expr = std::make_shared<IndexExpr>(std::move(expr), std::move(index), loc);
        } else if (check(TokenType::KW_as)) {
            advance(); // consume 'as'
            TypeSpec targetType = parseType();
            expr = std::make_shared<CastExpr>(std::move(expr), std::move(targetType), loc);
        } else if (match(TokenType::PlusPlus)) {
            // Postfix x++: desugar to (x = x + 1)
            auto one = std::make_shared<IntLitExpr>(1, loc);
            auto add = std::make_shared<BinaryExpr>(TokenType::Plus, expr, std::move(one), loc);
            expr = std::make_shared<AssignExpr>(std::move(expr), std::move(add), loc);
        } else if (match(TokenType::MinusMinus)) {
            // Postfix x--: desugar to (x = x - 1)
            auto one = std::make_shared<IntLitExpr>(1, loc);
            auto sub = std::make_shared<BinaryExpr>(TokenType::Minus, expr, std::move(one), loc);
            expr = std::make_shared<AssignExpr>(std::move(expr), std::move(sub), loc);
        } else {
            break;
        }
    }

    return expr;
}

ExprPtr Parser::parsePrimary() {
    SourceLoc loc = peek().loc;

    // Integer literal
    if (check(TokenType::IntLiteral)) {
        int64_t val = std::stoll(advance().value);
        return std::make_shared<IntLitExpr>(val, loc);
    }

    // Float literal
    if (check(TokenType::FloatLiteral)) {
        double val = std::stod(advance().value);
        return std::make_shared<FloatLitExpr>(val, loc);
    }

    // String literal
    if (check(TokenType::StringLiteral)) {
        return std::make_shared<StringLitExpr>(advance().value, loc);
    }

    // Char literal
    if (check(TokenType::CharLiteral)) {
        std::string val = advance().value;
        char c = val.empty() ? '\0' : val[0];
        return std::make_shared<CharLitExpr>(c, loc);
    }

    // Boolean literals
    if (match(TokenType::KW_true))  return std::make_shared<BoolLitExpr>(true, loc);
    if (match(TokenType::KW_false)) return std::make_shared<BoolLitExpr>(false, loc);

    // null
    if (match(TokenType::KW_null))  return std::make_shared<NullLitExpr>(loc);

    // this
    if (match(TokenType::KW_this))  return std::make_shared<ThisExpr>(loc);

    // super — either super(args) for parent constructor or super.method() via postfix
    if (match(TokenType::KW_super)) {
        if (match(TokenType::LParen)) {
            // super(args) → parent constructor call
            auto args = parseArgList();
            expect(TokenType::RParen, "')'");
            auto superExpr = std::make_shared<SuperExpr>(loc);
            return std::make_shared<MethodCallExpr>(std::move(superExpr), "<init>", std::move(args), loc);
        }
        // super by itself → will be used in super.method() via postfix
        return std::make_shared<SuperExpr>(loc);
    }

    // new Type(args) or new Type[size]
    if (match(TokenType::KW_new)) {
        // Try parsing as a type first (supports primitives and class names)
        TypeSpec type = parseType();

        // Check for array allocation: new Type[expr]
        if (match(TokenType::LBracket)) {
            auto sizeExpr = parseExpr();
            expect(TokenType::RBracket, "']'");
            return std::make_shared<NewArrayExpr>(std::move(type), std::move(sizeExpr), loc);
        }

        // Class construction: new Type(args)
        expect(TokenType::LParen, "'('");
        auto args = parseArgList();
        expect(TokenType::RParen, "')'");
        return std::make_shared<NewExpr>(std::move(type), std::move(args), loc);
    }

    // Lambda expression: [captures](params) -> retType { body }
    if (check(TokenType::LBracket)) {
        // In primary position, [ starts a lambda (not array indexing which is postfix)
        return parseLambdaExpr();
    }

    // Boundary conversion: Type(x) or Type*(x) — language boundary conversions
    // string(expr): C string → LPL string
    // char*(expr): LPL string → C string (extract raw pointer)
    // Other type*(expr): pointer boundary conversions
    {
        // Check for: primitive_keyword [*...] '('
        bool isBoundaryConvert = false;
        size_t saved = current;
        switch (peek().type) {
            case TokenType::KW_string:
                // string( is always a boundary conversion
                if (peekNext().type == TokenType::LParen) {
                    isBoundaryConvert = true;
                }
                break;
            case TokenType::KW_void: case TokenType::KW_bool: case TokenType::KW_byte:
            case TokenType::KW_char: case TokenType::KW_short: case TokenType::KW_int:
            case TokenType::KW_long: case TokenType::KW_float: case TokenType::KW_double: {
                // Need at least one * before ( for boundary conversion: char*(x)
                advance(); // consume type keyword
                if (check(TokenType::Star)) {
                    while (check(TokenType::Star)) advance();
                    if (check(TokenType::LParen)) {
                        isBoundaryConvert = true;
                    }
                }
                current = saved;
                break;
            }
            default:
                break;
        }

        if (isBoundaryConvert) {
            TypeSpec targetType = parseType();
            expect(TokenType::LParen, "'('");
            auto operand = parseExpr();
            expect(TokenType::RParen, "')'");
            return std::make_shared<BoundaryConvertExpr>(std::move(operand), std::move(targetType), loc);
        }
    }

    // Parenthesized expression (C-style casts removed — use 'as' for primitive casts)
    if (check(TokenType::LParen)) {
        advance(); // skip (
        auto expr = parseExpr();
        expect(TokenType::RParen, "')'");
        return expr;
    }

    // Identifier (variable or function call)
    if (check(TokenType::Identifier)) {
        std::string name = advance().value;

        // Check for generic call: name<Type>(args) or Name<Type>(args)
        if (check(TokenType::Lt)) {
            size_t savedPos = current;
            std::vector<TypeSpec> typeArgs;
            if (tryParseTypeArgs(typeArgs) && check(TokenType::LParen)) {
                // Successfully parsed type args and followed by '(' — generic call
                advance(); // consume '('
                auto args = parseArgList();
                expect(TokenType::RParen, "')'");
                // Create a CallExpr with the type args embedded in a TypeSpec
                // The callee name carries the generic info; sema resolves it
                auto call = std::make_shared<CallExpr>(name, std::move(args), loc);
                call->resolvedType.typeArgs = std::move(typeArgs);
                return call;
            }
            // Backtrack — not a generic call, will be parsed as comparison
            current = savedPos;
        }

        // Check for static method call: ClassName.method(args) or ClassName.field
        // Also handles: name(args)
        if (match(TokenType::LParen)) {
            auto args = parseArgList();
            expect(TokenType::RParen, "')'");
            return std::make_shared<CallExpr>(name, std::move(args), loc);
        }

        // Check for ClassName.staticMember
        if (check(TokenType::Dot)) {
            // This will be handled by postfix parsing above
            // For now, just return the identifier
        }

        return std::make_shared<IdentExpr>(name, loc);
    }

    error(std::string(loc.file) + ":" + std::to_string(loc.line)
          + ": error: expected expression, got " + tokenTypeName(peek().type));
    advance(); // skip the bad token
    return std::make_shared<IntLitExpr>(0, loc); // recovery
}

std::vector<ExprPtr> Parser::parseArgList() {
    std::vector<ExprPtr> args;
    if (check(TokenType::RParen)) return args;

    args.push_back(parseExpr());
    while (match(TokenType::Comma)) {
        args.push_back(parseExpr());
    }
    return args;
}