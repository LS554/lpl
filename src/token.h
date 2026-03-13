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
#include <unordered_map>

enum class TokenType {
    // Literals
    IntLiteral,
    FloatLiteral,
    StringLiteral,
    CharLiteral,

    // Identifier
    Identifier,

    // Keywords - Types
    KW_void,
    KW_bool,
    KW_byte,
    KW_char,
    KW_short,
    KW_int,
    KW_long,
    KW_float,
    KW_double,
    KW_string,

    // Keywords - OOP
    KW_class,
    KW_interface,
    KW_extends,
    KW_implements,
    KW_public,
    KW_protected,
    KW_private,
    KW_static,
    KW_this,
    KW_new,
    KW_delete,

    // Keywords - Control flow
    KW_if,
    KW_else,
    KW_while,
    KW_for,
    KW_do,
    KW_switch,
    KW_case,
    KW_default,
    KW_return,
    KW_break,
    KW_continue,
    KW_fallthrough,

    // Keywords - Values
    KW_true,
    KW_false,
    KW_null,

    // Keywords - Memory/ownership
    KW_owner,
    KW_move,

    // Keywords - Interop
    KW_extern,

    // Keywords - Module system
    KW_include,
    KW_namespace,
    KW_defer,

    // Keywords - Cast
    KW_as,

    // Keywords - Qualifiers
    KW_const,
    KW_override,
    KW_abstract,
    KW_super,
    KW_squib,

    // Keywords - Exception handling
    KW_try,
    KW_catch,
    KW_throw,
    KW_finally,

    // Keywords - Operator overloading / Lambdas
    KW_operator,
    KW_func,

    // Keywords - Type inference
    KW_auto,

    // Operators
    Plus,       // +
    Minus,      // -
    Star,       // *
    Slash,      // /
    Percent,    // %
    Ampersand,  // &
    Pipe,       // |
    Caret,      // ^
    Tilde,      // ~
    Bang,       // !
    Eq,         // =
    EqEq,       // ==
    BangEq,     // !=
    Lt,         // <
    Gt,         // >
    LtEq,       // <=
    GtEq,       // >=
    AmpAmp,     // &&
    PipePipe,   // ||
    LShift,     // <<
    RShift,     // >>
    PlusEq,     // +=
    MinusEq,    // -=
    StarEq,     // *=
    SlashEq,    // /=
    PlusPlus,   // ++
    MinusMinus, // --
    Question,   // ?

    // Punctuation
    LParen,     // (
    RParen,     // )
    LBrace,     // {
    RBrace,     // }
    LBracket,   // [
    RBracket,   // ]
    Semicolon,  // ;
    Comma,      // ,
    Dot,        // .
    Arrow,      // ->
    ColonColon, // ::
    At,         // @

    // Punctuation - extra
    Colon,      // :
    DotDot,     // ..
    DotDotEq,   // ..=

    // Special
    Ellipsis,   // ...
    Hash,       // #

    // Meta
    Eof,
    Error,
};

struct SourceLoc {
    int line = 1;
    int col = 1;
    std::string file;
};

struct Token {
    TokenType type = TokenType::Eof;
    std::string value;
    SourceLoc loc;
};

inline const std::unordered_map<std::string, TokenType>& getKeywordMap() {
    static const std::unordered_map<std::string, TokenType> kw = {
        {"void",       TokenType::KW_void},
        {"bool",       TokenType::KW_bool},
        {"byte",       TokenType::KW_byte},
        {"char",       TokenType::KW_char},
        {"short",      TokenType::KW_short},
        {"int",        TokenType::KW_int},
        {"long",       TokenType::KW_long},
        {"float",      TokenType::KW_float},
        {"double",     TokenType::KW_double},
        {"string",     TokenType::KW_string},
        {"class",      TokenType::KW_class},
        {"interface",  TokenType::KW_interface},
        {"extends",    TokenType::KW_extends},
        {"implements", TokenType::KW_implements},
        {"public",     TokenType::KW_public},
        {"protected",  TokenType::KW_protected},
        {"private",    TokenType::KW_private},
        {"static",     TokenType::KW_static},
        {"this",       TokenType::KW_this},
        {"new",        TokenType::KW_new},
        {"delete",     TokenType::KW_delete},
        {"if",         TokenType::KW_if},
        {"else",       TokenType::KW_else},
        {"while",      TokenType::KW_while},
        {"for",        TokenType::KW_for},
        {"return",     TokenType::KW_return},
        {"break",      TokenType::KW_break},
        {"continue",   TokenType::KW_continue},
        {"fallthrough", TokenType::KW_fallthrough},
        {"true",       TokenType::KW_true},
        {"false",      TokenType::KW_false},
        {"null",       TokenType::KW_null},
        {"owner",      TokenType::KW_owner},
        {"move",       TokenType::KW_move},
        {"extern",     TokenType::KW_extern},
        {"include",    TokenType::KW_include},
        {"namespace",  TokenType::KW_namespace},
        {"defer",      TokenType::KW_defer},
        {"do",         TokenType::KW_do},
        {"switch",     TokenType::KW_switch},
        {"case",       TokenType::KW_case},
        {"default",    TokenType::KW_default},
        {"as",         TokenType::KW_as},
        {"const",      TokenType::KW_const},
        {"override",   TokenType::KW_override},
        {"abstract",   TokenType::KW_abstract},
        {"super",      TokenType::KW_super},
        {"try",        TokenType::KW_try},
        {"catch",      TokenType::KW_catch},
        {"throw",      TokenType::KW_throw},
        {"finally",    TokenType::KW_finally},
        {"operator",   TokenType::KW_operator},
        {"func",       TokenType::KW_func},
        {"auto",       TokenType::KW_auto},
        {"squib",      TokenType::KW_squib},
    };
    return kw;
}

inline std::string tokenTypeName(TokenType t) {
    switch (t) {
        case TokenType::IntLiteral:   return "int literal";
        case TokenType::FloatLiteral: return "float literal";
        case TokenType::StringLiteral:return "string literal";
        case TokenType::CharLiteral:  return "char literal";
        case TokenType::Identifier:   return "identifier";
        case TokenType::Eof:          return "end of file";
        case TokenType::Error:        return "error";
        case TokenType::Plus:         return "'+'";
        case TokenType::Minus:        return "'-'";
        case TokenType::Star:         return "'*'";
        case TokenType::Slash:        return "'/'";
        case TokenType::Eq:           return "'='";
        case TokenType::EqEq:         return "'=='";
        case TokenType::BangEq:       return "'!='";
        case TokenType::Lt:           return "'<'";
        case TokenType::Gt:           return "'>'";
        case TokenType::LParen:       return "'('";
        case TokenType::RParen:       return "')'";
        case TokenType::LBrace:       return "'{'";
        case TokenType::RBrace:       return "'}'";
        case TokenType::Semicolon:    return "';'";
        case TokenType::Comma:        return "','";
        case TokenType::Dot:          return "'.'";
        default:                      return "token";
    }
}