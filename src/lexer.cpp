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

#include "lexer.h"
#include <cctype>
#include <stdexcept>

Lexer::Lexer(const std::string& source, const std::string& filename)
    : src(source), filename(filename) {}

char Lexer::peek() const {
    if (pos >= src.size()) return '\0';
    return src[pos];
}

char Lexer::peekNext() const {
    if (pos + 1 >= src.size()) return '\0';
    return src[pos + 1];
}

char Lexer::advance() {
    char c = src[pos++];
    if (c == '\n') { line++; col = 1; }
    else { col++; }
    return c;
}

bool Lexer::atEnd() const {
    return pos >= src.size();
}

SourceLoc Lexer::currentLoc() const {
    return {line, col, filename};
}

Token Lexer::makeToken(TokenType type, const std::string& val, SourceLoc loc) {
    return {type, val, loc};
}

void Lexer::skipWhitespaceAndComments() {
    while (!atEnd()) {
        char c = peek();
        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
        } else if (c == '/' && peekNext() == '/') {
            // Single-line comment
            while (!atEnd() && peek() != '\n') advance();
        } else if (c == '/' && peekNext() == '*') {
            // Multi-line comment
            advance(); advance(); // skip /*
            while (!atEnd()) {
                if (peek() == '*' && peekNext() == '/') {
                    advance(); advance();
                    break;
                }
                advance();
            }
        } else {
            break;
        }
    }
}

Token Lexer::readString() {
    SourceLoc loc = currentLoc();
    advance(); // skip opening "
    std::string val;
    while (!atEnd() && peek() != '"') {
        if (peek() == '\\') {
            advance();
            if (atEnd()) break;
            char esc = advance();
            switch (esc) {
                case 'n':  val += '\n'; break;
                case 't':  val += '\t'; break;
                case 'r':  val += '\r'; break;
                case '\\': val += '\\'; break;
                case '"':  val += '"';  break;
                case '0':  val += '\0'; break;
                default:   val += esc;  break;
            }
        } else {
            val += advance();
        }
    }
    if (!atEnd()) advance(); // skip closing "
    return makeToken(TokenType::StringLiteral, val, loc);
}

Token Lexer::readChar() {
    SourceLoc loc = currentLoc();
    advance(); // skip opening '
    std::string val;
    if (!atEnd() && peek() == '\\') {
        advance();
        if (!atEnd()) {
            char esc = advance();
            switch (esc) {
                case 'n':  val += '\n'; break;
                case 't':  val += '\t'; break;
                case '\\': val += '\\'; break;
                case '\'': val += '\''; break;
                case '0':  val += '\0'; break;
                default:   val += esc;  break;
            }
        }
    } else if (!atEnd()) {
        val += advance();
    }
    if (!atEnd() && peek() == '\'') advance(); // skip closing '
    return makeToken(TokenType::CharLiteral, val, loc);
}

Token Lexer::readNumber() {
    SourceLoc loc = currentLoc();
    std::string val;
    bool isFloat = false;

    while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
        val += advance();
    }
    if (!atEnd() && peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
        isFloat = true;
        val += advance(); // the dot
        while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
            val += advance();
        }
    }
    // Optional suffixes (f, l, etc.) - not implemented yet

    return makeToken(isFloat ? TokenType::FloatLiteral : TokenType::IntLiteral, val, loc);
}

Token Lexer::readIdentifierOrKeyword() {
    SourceLoc loc = currentLoc();
    std::string val;
    while (!atEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
        val += advance();
    }

    auto& kw = getKeywordMap();
    auto it = kw.find(val);
    if (it != kw.end()) {
        return makeToken(it->second, val, loc);
    }
    return makeToken(TokenType::Identifier, val, loc);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skipWhitespaceAndComments();
        if (atEnd()) {
            tokens.push_back(makeToken(TokenType::Eof, "", currentLoc()));
            break;
        }

        SourceLoc loc = currentLoc();
        char c = peek();

        // String literal
        if (c == '"') {
            tokens.push_back(readString());
            continue;
        }

        // Char literal
        if (c == '\'') {
            tokens.push_back(readChar());
            continue;
        }

        // Number
        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(readNumber());
            continue;
        }

        // Identifier or keyword
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(readIdentifierOrKeyword());
            continue;
        }

        // Operators and punctuation
        advance();
        switch (c) {
            case '(':
                tokens.push_back(makeToken(TokenType::LParen, "(", loc)); break;
            case ')':
                tokens.push_back(makeToken(TokenType::RParen, ")", loc)); break;
            case '{':
                tokens.push_back(makeToken(TokenType::LBrace, "{", loc)); break;
            case '}':
                tokens.push_back(makeToken(TokenType::RBrace, "}", loc)); break;
            case '[':
                tokens.push_back(makeToken(TokenType::LBracket, "[", loc)); break;
            case ']':
                tokens.push_back(makeToken(TokenType::RBracket, "]", loc)); break;
            case ';':
                tokens.push_back(makeToken(TokenType::Semicolon, ";", loc)); break;
            case ',':
                tokens.push_back(makeToken(TokenType::Comma, ",", loc)); break;
            case '@':
                tokens.push_back(makeToken(TokenType::At, "@", loc)); break;
            case '~':
                tokens.push_back(makeToken(TokenType::Tilde, "~", loc)); break;
            case '^':
                tokens.push_back(makeToken(TokenType::Caret, "^", loc)); break;
            case '.':
                if (peek() == '.' && peekNext() == '.') {
                    advance(); advance();
                    tokens.push_back(makeToken(TokenType::Ellipsis, "...", loc));
                } else if (peek() == '.' && peekNext() == '=') {
                    advance(); advance();
                    tokens.push_back(makeToken(TokenType::DotDotEq, "..=", loc));
                } else if (peek() == '.') {
                    advance();
                    tokens.push_back(makeToken(TokenType::DotDot, "..", loc));
                } else {
                    tokens.push_back(makeToken(TokenType::Dot, ".", loc));
                }
                break;
            case '+':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::PlusEq, "+=", loc)); }
                else if (peek() == '+') { advance(); tokens.push_back(makeToken(TokenType::PlusPlus, "++", loc)); }
                else tokens.push_back(makeToken(TokenType::Plus, "+", loc));
                break;
            case '-':
                if (peek() == '>') { advance(); tokens.push_back(makeToken(TokenType::Arrow, "->", loc)); }
                else if (peek() == '-') { advance(); tokens.push_back(makeToken(TokenType::MinusMinus, "--", loc)); }
                else if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::MinusEq, "-=", loc)); }
                else tokens.push_back(makeToken(TokenType::Minus, "-", loc));
                break;
            case '*':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::StarEq, "*=", loc)); }
                else tokens.push_back(makeToken(TokenType::Star, "*", loc));
                break;
            case '/':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::SlashEq, "/=", loc)); }
                else tokens.push_back(makeToken(TokenType::Slash, "/", loc));
                break;
            case '%':
                tokens.push_back(makeToken(TokenType::Percent, "%", loc)); break;
            case '=':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::EqEq, "==", loc)); }
                else tokens.push_back(makeToken(TokenType::Eq, "=", loc));
                break;
            case '!':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::BangEq, "!=", loc)); }
                else tokens.push_back(makeToken(TokenType::Bang, "!", loc));
                break;
            case '<':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::LtEq, "<=", loc)); }
                else if (peek() == '<') { advance(); tokens.push_back(makeToken(TokenType::LShift, "<<", loc)); }
                else tokens.push_back(makeToken(TokenType::Lt, "<", loc));
                break;
            case '>':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::GtEq, ">=", loc)); }
                else if (peek() == '>') { advance(); tokens.push_back(makeToken(TokenType::RShift, ">>", loc)); }
                else tokens.push_back(makeToken(TokenType::Gt, ">", loc));
                break;
            case '&':
                if (peek() == '&') { advance(); tokens.push_back(makeToken(TokenType::AmpAmp, "&&", loc)); }
                else tokens.push_back(makeToken(TokenType::Ampersand, "&", loc));
                break;
            case '|':
                if (peek() == '|') { advance(); tokens.push_back(makeToken(TokenType::PipePipe, "||", loc)); }
                else tokens.push_back(makeToken(TokenType::Pipe, "|", loc));
                break;
            case '?':
                tokens.push_back(makeToken(TokenType::Question, "?", loc));
                break;
            case ':':
                if (peek() == ':') { advance(); tokens.push_back(makeToken(TokenType::ColonColon, "::", loc)); }
                else tokens.push_back(makeToken(TokenType::Colon, ":", loc));
                break;
            case '#':
                tokens.push_back(makeToken(TokenType::Hash, "#", loc)); break;
            default:
                tokens.push_back(makeToken(TokenType::Error, std::string(1, c), loc));
                break;
        }
    }

    return tokens;
}