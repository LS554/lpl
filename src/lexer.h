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
#include "token.h"

class Lexer {
public:
    Lexer(const std::string& source, const std::string& filename);
    std::vector<Token> tokenize();

private:
    std::string src;
    std::string filename;
    size_t pos = 0;
    int line = 1;
    int col = 1;

    char peek() const;
    char peekNext() const;
    char advance();
    bool atEnd() const;
    void skipWhitespaceAndComments();
    Token makeToken(TokenType type, const std::string& val, SourceLoc loc);
    Token readString();
    Token readChar();
    Token readNumber();
    Token readIdentifierOrKeyword();
    SourceLoc currentLoc() const;
};