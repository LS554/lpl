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

#include <vector>
#include <memory>
#include "token.h"
#include "ast.h"

class Parser {
public:
    Parser(const std::vector<Token>& tokens);
    Program parse();
    bool hasErrors() const { return !errors.empty(); }
    const std::vector<std::string>& getErrors() const { return errors; }

private:
    std::vector<Token> tokens;
    size_t current = 0;
    std::vector<std::string> errors;
    std::string currentNamespace_;  // active namespace for file-level form

    // Token access
    const Token& peek() const;
    const Token& peekNext() const;
    const Token& previous() const;
    Token advance();
    bool check(TokenType t) const;
    bool match(TokenType t);
    Token expect(TokenType t, const std::string& msg);
    bool atEnd() const;

    void error(const std::string& msg);
    void synchronize();

    // Type parsing
    TypeSpec parseType();
    bool isTypeStart() const;

    // Declaration parsing
    DeclPtr parseDecl();
    DeclPtr parseClassDecl();
    DeclPtr parseInterfaceDecl();
    DeclPtr parseFunctionDecl(std::vector<Attribute> attrs, TypeSpec retType, const std::string& name);
    DeclPtr parseExternBlock();
    DeclPtr parseInclude();
    DeclPtr parseNamespace();
    std::string parseDottedName();
    std::vector<Attribute> parseAttributes();

    // Class member parsing
    void parseClassBody(ClassDecl& cls);
    std::vector<Param> parseParamList();

    // Statement parsing
    StmtPtr parseStmt();
    StmtPtr parseBlock();
    StmtPtr parseVarDeclOrExprStmt();
    StmtPtr parseIfStmt();
    StmtPtr parseWhileStmt();
    StmtPtr parseForStmt();
    StmtPtr parseReturnStmt();
    StmtPtr parseDeleteStmt();
    StmtPtr parseDeferStmt();
    StmtPtr parseDoWhileStmt();
    StmtPtr parseSwitchStmt();
    StmtPtr parseTryStmt();
    StmtPtr parseThrowStmt();

    // Lambda parsing
    ExprPtr parseLambdaExpr();

    // Expression parsing (precedence climbing)
    ExprPtr parseExpr();
    ExprPtr parseAssignment();
    ExprPtr parseLogicalOr();
    ExprPtr parseLogicalAnd();
    ExprPtr parseEquality();
    ExprPtr parseComparison();
    ExprPtr parseAddition();
    ExprPtr parseMultiplication();
    ExprPtr parseUnary();
    ExprPtr parsePostfix();
    ExprPtr parsePrimary();
    std::vector<ExprPtr> parseArgList();

    // Generic type parsing helpers
    std::vector<std::string> parseTypeParams(); // <T, U> in declarations
    bool tryParseTypeArgs(std::vector<TypeSpec>& out); // <int, string> in types (speculative)
};