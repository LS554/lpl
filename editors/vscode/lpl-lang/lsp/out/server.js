#!/usr/bin/env node
"use strict";
// Copyright 2026 London Sheard
// Licensed under the Apache License, Version 2.0.
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
const node_1 = require("vscode-languageserver/node");
const vscode_languageserver_textdocument_1 = require("vscode-languageserver-textdocument");
const child_process_1 = require("child_process");
const path = __importStar(require("path"));
const fs = __importStar(require("fs"));
// ---------------------------------------------------------------------------
// Connection setup
// ---------------------------------------------------------------------------
const connection = (0, node_1.createConnection)(node_1.ProposedFeatures.all);
const documents = new node_1.TextDocuments(vscode_languageserver_textdocument_1.TextDocument);
let compilerPath = '';
const STDLIB = {
    Console: {
        description: 'Console I/O — terminal output functions.',
        header: 'console',
        methods: {
            print: {
                signature: 'static void print(string message)',
                description: 'Prints a string to stdout without a trailing newline.',
                params: ['message — the text to print'],
            },
            println: {
                signature: 'static void println(string message)',
                description: 'Prints a string to stdout followed by a newline.',
                params: ['message — the text to print'],
            },
            printInt: {
                signature: 'static void printInt(int value)',
                description: 'Prints an integer to stdout followed by a newline.',
                params: ['value — the integer to print'],
            },
            printFloat: {
                signature: 'static void printFloat(double value)',
                description: 'Prints a double to stdout followed by a newline.',
                params: ['value — the double to print'],
            },
        },
    },
    Strings: {
        description: 'String utility functions.',
        header: 'string',
        methods: {
            length: { signature: 'static int length(string s)', description: 'Returns the length of the string in bytes.', params: ['s — the input string'], returns: 'int' },
            substring: { signature: 'static string substring(string s, int start, int end)', description: 'Returns a substring from index `start` (inclusive) to `end` (exclusive).', params: ['s — the input string', 'start — start index', 'end — end index'], returns: 'string' },
            indexOf: { signature: 'static int indexOf(string s, string needle)', description: 'Returns the index of the first occurrence of `needle`, or -1.', params: ['s — the string to search', 'needle — the substring to find'], returns: 'int' },
            contains: { signature: 'static bool contains(string s, string needle)', description: 'Returns true if `s` contains `needle`.', params: ['s — the string to search', 'needle — the substring to find'], returns: 'bool' },
            toUpper: { signature: 'static string toUpper(string s)', description: 'Converts all characters to uppercase.', params: ['s — the input string'], returns: 'string' },
            toLower: { signature: 'static string toLower(string s)', description: 'Converts all characters to lowercase.', params: ['s — the input string'], returns: 'string' },
            trim: { signature: 'static string trim(string s)', description: 'Removes leading and trailing whitespace.', params: ['s — the input string'], returns: 'string' },
            fromInt: { signature: 'static string fromInt(int value)', description: 'Converts an integer to its string representation.', params: ['value — the integer to convert'], returns: 'string' },
            fromFloat: { signature: 'static string fromFloat(double value)', description: 'Converts a double to its string representation.', params: ['value — the double to convert'], returns: 'string' },
            fromBool: { signature: 'static string fromBool(bool value)', description: 'Converts a bool to "true" or "false".', params: ['value — the bool to convert'], returns: 'string' },
            toInt: { signature: 'static int toInt(string s)', description: 'Parses a string as an integer.', params: ['s — the string to parse'], returns: 'int' },
            toFloat: { signature: 'static double toFloat(string s)', description: 'Parses a string as a double.', params: ['s — the string to parse'], returns: 'double' },
            startsWith: { signature: 'static bool startsWith(string s, string prefix)', description: 'Returns true if `s` starts with `prefix`.', params: ['s — the input string', 'prefix — the prefix to check'], returns: 'bool' },
            endsWith: { signature: 'static bool endsWith(string s, string suffix)', description: 'Returns true if `s` ends with `suffix`.', params: ['s — the input string', 'suffix — the suffix to check'], returns: 'bool' },
            replace: { signature: 'static string replace(string s, string old, string rep)', description: 'Replaces all occurrences of `old` with `rep`.', params: ['s — the input string', 'old — substring to replace', 'rep — replacement string'], returns: 'string' },
            charAt: { signature: 'static string charAt(string s, int index)', description: 'Returns the character at the given index as a single-character string.', params: ['s — the input string', 'index — character position'], returns: 'string' },
        },
    },
    Math: {
        description: 'Mathematical functions and constants.',
        header: 'math',
        methods: {
            abs: { signature: 'static double abs(double x)', description: 'Returns the absolute value.', params: ['x — input value'], returns: 'double' },
            absInt: { signature: 'static int absInt(int x)', description: 'Returns the absolute value of an integer.', params: ['x — input value'], returns: 'int' },
            sqrt: { signature: 'static double sqrt(double x)', description: 'Returns the square root.', params: ['x — input value'], returns: 'double' },
            pow: { signature: 'static double pow(double base, double exp)', description: 'Returns base raised to the power of exp.', params: ['base — the base', 'exp — the exponent'], returns: 'double' },
            sin: { signature: 'static double sin(double x)', description: 'Returns the sine (radians).', params: ['x — angle in radians'], returns: 'double' },
            cos: { signature: 'static double cos(double x)', description: 'Returns the cosine (radians).', params: ['x — angle in radians'], returns: 'double' },
            tan: { signature: 'static double tan(double x)', description: 'Returns the tangent (radians).', params: ['x — angle in radians'], returns: 'double' },
            log: { signature: 'static double log(double x)', description: 'Returns the natural logarithm.', params: ['x — input value'], returns: 'double' },
            log10: { signature: 'static double log10(double x)', description: 'Returns the base-10 logarithm.', params: ['x — input value'], returns: 'double' },
            ceil: { signature: 'static double ceil(double x)', description: 'Rounds up to the nearest integer.', params: ['x — input value'], returns: 'double' },
            floor: { signature: 'static double floor(double x)', description: 'Rounds down to the nearest integer.', params: ['x — input value'], returns: 'double' },
            round: { signature: 'static double round(double x)', description: 'Rounds to the nearest integer.', params: ['x — input value'], returns: 'double' },
            min: { signature: 'static double min(double a, double b)', description: 'Returns the smaller of two doubles.', params: ['a — first value', 'b — second value'], returns: 'double' },
            max: { signature: 'static double max(double a, double b)', description: 'Returns the larger of two doubles.', params: ['a — first value', 'b — second value'], returns: 'double' },
            minInt: { signature: 'static int minInt(int a, int b)', description: 'Returns the smaller of two integers.', params: ['a — first value', 'b — second value'], returns: 'int' },
            maxInt: { signature: 'static int maxInt(int a, int b)', description: 'Returns the larger of two integers.', params: ['a — first value', 'b — second value'], returns: 'int' },
            PI: { signature: 'static double PI()', description: 'Returns the value of π (3.14159…).', returns: 'double' },
            E: { signature: 'static double E()', description: "Returns Euler's number e (2.71828…).", returns: 'double' },
            random: { signature: 'static double random()', description: 'Returns a pseudo-random double in [0.0, 1.0).', returns: 'double' },
        },
    },
    File: {
        description: 'File I/O — read, write, and manage files.',
        header: 'files',
        methods: {
            read: { signature: 'string read()', description: 'Reads the entire file contents as a string.', returns: 'string' },
            write: { signature: 'void write(string content)', description: 'Writes content to the file, replacing existing contents.', params: ['content — the text to write'] },
            append: { signature: 'void append(string content)', description: 'Appends content to the end of the file.', params: ['content — the text to append'] },
            exists: { signature: 'bool exists()', description: 'Returns true if the file exists.', returns: 'bool' },
            close: { signature: 'void close()', description: 'Closes the file handle.' },
            size: { signature: 'long size()', description: 'Returns the file size in bytes.', returns: 'long' },
            readAll: { signature: 'static string readAll(string path)', description: 'Reads the entire file at path.', params: ['path — file path'], returns: 'string' },
            writeAll: { signature: 'static void writeAll(string path, string content)', description: 'Writes content to the file at path.', params: ['path — file path', 'content — the text to write'] },
            fileExists: { signature: 'static bool fileExists(string path)', description: 'Returns true if a file exists at the given path.', params: ['path — file path'], returns: 'bool' },
            remove: { signature: 'static void remove(string path)', description: 'Deletes the file at the given path.', params: ['path — file path'] },
        },
    },
    System: {
        description: 'OS-level operations — process control, environment, time.',
        header: 'system',
        methods: {
            exit: { signature: 'static void exit(int code)', description: 'Terminates the process with the given exit code.', params: ['code — exit code'] },
            argc: { signature: 'static int argc()', description: 'Returns the number of command-line arguments.', returns: 'int' },
            argv: { signature: 'static string argv(int index)', description: 'Returns the command-line argument at the given index.', params: ['index — argument index'], returns: 'string' },
            getenv: { signature: 'static string getenv(string name)', description: 'Returns the value of the environment variable.', params: ['name — environment variable name'], returns: 'string' },
            currentTimeMillis: { signature: 'static long currentTimeMillis()', description: 'Returns the current time in milliseconds since the Unix epoch.', returns: 'long' },
            sleep: { signature: 'static void sleep(int milliseconds)', description: 'Pauses execution for the given number of milliseconds.', params: ['milliseconds — duration to sleep'] },
        },
    },
    Mutex: {
        description: 'Mutual exclusion lock for thread synchronization.',
        header: 'sync',
        methods: {
            lock: { signature: 'void lock()', description: 'Acquires the mutex. Blocks until available.' },
            unlock: { signature: 'void unlock()', description: 'Releases the mutex.' },
            tryLock: { signature: 'bool tryLock()', description: 'Attempts to acquire the mutex without blocking.', returns: 'bool' },
        },
    },
    'List<T>': {
        description: 'Generic dynamic array.',
        header: 'collections',
        methods: {
            add: { signature: 'void add(T value)', description: 'Appends a value to the end of the list.', params: ['value — the element to add'] },
            get: { signature: 'T get(int index)', description: 'Returns the element at the given index.', params: ['index — position (0-based)'], returns: 'T' },
            set: { signature: 'void set(int index, T value)', description: 'Replaces the element at the given index.', params: ['index — position', 'value — the new element'] },
            removeAt: { signature: 'void removeAt(int index)', description: 'Removes the element at the given index.', params: ['index — position'] },
            size: { signature: 'int size()', description: 'Returns the number of elements.', returns: 'int' },
        },
    },
    'Map<K,V>': {
        description: 'Generic hash map.',
        header: 'collections',
        methods: {
            put: { signature: 'void put(K key, V value)', description: 'Inserts or updates the value for the given key.', params: ['key — the map key', 'value — the value'] },
            get: { signature: 'V get(K key)', description: 'Returns the value associated with the key.', params: ['key — the map key'], returns: 'V' },
            containsKey: { signature: 'bool containsKey(K key)', description: 'Returns true if the map contains the given key.', params: ['key — the map key'], returns: 'bool' },
            remove: { signature: 'void remove(K key)', description: 'Removes the entry for the given key.', params: ['key — the map key'] },
            size: { signature: 'int size()', description: 'Returns the number of entries.', returns: 'int' },
        },
    },
};
const KEYWORD_DOCS = {
    owner: '`owner` — Marks a pointer as the sole owner of a heap-allocated object. The object is automatically destroyed and freed when the owner leaves scope.\n\n```lpl\nowner Person* p = new Person("Alex", 32);\n```',
    move: '`move` — Transfers ownership of a heap object from one owner pointer to another. The source becomes invalid after the move.\n\n```lpl\nowner Person* b = move a;\n```',
    defer: '`defer` — Schedules a statement to execute when the current scope exits (LIFO order).\n\n```lpl\ndefer mtx.unlock();\n```',
    extends: '`extends` — Declares single-class inheritance.\n\n```lpl\nclass Dog extends Animal { }\n```',
    implements: '`implements` — Declares interface implementation.\n\n```lpl\nclass Doc extends Object implements Printable, Serializable { }\n```',
    override: '`override` — Marks a method that overrides a parent class method.',
    abstract: '`abstract` — Declares a class that cannot be instantiated, or a method with no body that subclasses must implement.',
    func: '`func` — Callable type syntax for function pointers and lambdas.\n\n```lpl\nfunc(int, int) -> int\n```',
    auto: '`auto` — Type inference. The compiler deduces the type from the initializer.\n\n```lpl\nauto x = 42;\n```',
    extern: '`extern "C"` — Declares C-linkage functions for interop with C libraries.\n\n```lpl\nextern "C" {\n    int printf(char* fmt, ...);\n}\n```',
    include: '`include` — Imports a header file. Use `<>` for stdlib, `""` for local.\n\n```lpl\ninclude <console>\ninclude "mylib.lph"\n```',
    namespace: '`namespace` — Organizes code into hierarchical groups.\n\n```lpl\nnamespace App.Models { ... }\n```',
    fallthrough: '`fallthrough` — Explicitly continues execution into the next switch case.',
    as: '`as` — Postfix type cast for primitive conversions.\n\n```lpl\ndouble d = x as double;\n```',
    switch: '`switch` — Multi-way branching. Cases do **not** fall through by default.',
    const: '`const` — Declares an immutable variable. Must be initialized at declaration.',
};
const ALL_KEYWORDS = [
    'void', 'bool', 'byte', 'char', 'short', 'int', 'long', 'float', 'double', 'string',
    'class', 'interface', 'extends', 'implements',
    'public', 'protected', 'private', 'static', 'const',
    'this', 'super', 'new', 'delete', 'move', 'owner', 'auto',
    'if', 'else', 'while', 'for', 'do', 'switch', 'case', 'default',
    'return', 'break', 'continue', 'fallthrough',
    'try', 'catch', 'throw', 'finally',
    'defer', 'as', 'func', 'override', 'abstract',
    'extern', 'include', 'namespace', 'operator',
    'true', 'false', 'null',
];
const KEYWORD_SET = new Set(ALL_KEYWORDS);
const STDLIB_HEADERS = [
    'console', 'console.lph', 'string', 'string.lph',
    'math', 'math.lph', 'files', 'files.lph',
    'system', 'system.lph', 'collections', 'collections.lph',
    'sync', 'sync.lph',
];
const SNIPPETS = [
    { label: 'class', insertText: 'class ${1:Name} {\n\tpublic ${1:Name}() {\n\t\t$0\n\t}\n}', description: 'Class declaration with constructor' },
    { label: 'main', insertText: 'void main() {\n\t$0\n}', description: 'Program entry point' },
    { label: 'mainargs', insertText: 'int main(int argc, string[] args) {\n\t$0\n\treturn 0;\n}', description: 'Entry point with arguments' },
    { label: 'for', insertText: 'for (int ${1:i} = 0; ${1:i} < ${2:n}; ${1:i}++) {\n\t$0\n}', description: 'For loop' },
    { label: 'foreach', insertText: 'for (int ${1:i} : ${2:0}..${3:10}) {\n\t$0\n}', description: 'Range-based for loop' },
    { label: 'while', insertText: 'while (${1:condition}) {\n\t$0\n}', description: 'While loop' },
    { label: 'if', insertText: 'if (${1:condition}) {\n\t$0\n}', description: 'If statement' },
    { label: 'ifelse', insertText: 'if (${1:condition}) {\n\t$2\n} else {\n\t$0\n}', description: 'If/else statement' },
    { label: 'switch', insertText: 'switch (${1:expr}) {\n\tcase ${2:value}:\n\t\t$0\n\tdefault:\n\t\tbreak;\n}', description: 'Switch statement' },
    { label: 'trycatch', insertText: 'try {\n\t$1\n} catch (${2:Exception} ${3:e}) {\n\t$0\n}', description: 'Try/catch block' },
    { label: 'lambda', insertText: '[${1:}](${2:params}) -> ${3:int} { return ${0:expr}; }', description: 'Lambda expression' },
    { label: 'owner', insertText: 'owner ${1:Type}* ${2:name} = new ${1:Type}(${0:args});', description: 'Owner pointer with heap allocation' },
    { label: 'defer', insertText: 'defer ${0:statement};', description: 'Defer statement' },
    { label: 'include', insertText: 'include <${1:console}>', description: 'Include standard library header' },
    { label: 'externc', insertText: 'extern "C" {\n\t$0\n}', description: 'Extern C block' },
    { label: 'externcpp', insertText: 'extern "C++" {\n\t$0\n}', description: 'Extern C++ block' },
];
function parseDocumentSymbols(text) {
    const symbols = [];
    const lines = text.split('\n');
    let currentClass;
    let braceDepth = 0;
    let classBraceDepth = -1;
    for (let i = 0; i < lines.length; i++) {
        const line = lines[i];
        const trimmed = line.replace(/\/\/.*$/, '').trim();
        if (!trimmed)
            continue;
        for (const ch of trimmed) {
            if (ch === '{')
                braceDepth++;
            if (ch === '}') {
                braceDepth--;
                if (currentClass && braceDepth <= classBraceDepth) {
                    currentClass = undefined;
                    classBraceDepth = -1;
                }
            }
        }
        const classMatch = trimmed.match(/^(?:abstract\s+)?class\s+([A-Z][a-zA-Z0-9_]*)(?:<[^>]+>)?/);
        if (classMatch) {
            symbols.push({ name: classMatch[1], kind: 'class', type: 'class', line: i });
            currentClass = classMatch[1];
            classBraceDepth = braceDepth - 1;
            continue;
        }
        const stripped = trimmed
            .replace(/^(?:public|protected|private|static|override|abstract)\s+/g, '')
            .replace(/^(?:public|protected|private|static|override|abstract)\s+/g, '');
        if (currentClass) {
            const ctorRe = new RegExp(`^${currentClass}\\s*\\(([^)]*)\\)`);
            const ctorMatch = stripped.match(ctorRe);
            if (ctorMatch) {
                symbols.push({ name: currentClass, kind: 'constructor', type: currentClass, signature: `${currentClass}(${ctorMatch[1]})`, className: currentClass, line: i });
                continue;
            }
        }
        const funcMatch = stripped.match(/^((?:owner\s+)?(?:func\s*\([^)]*\)\s*->\s*\w+|(?:auto|void|bool|byte|char|short|int|long|float|double|string|[A-Z][a-zA-Z0-9_]*(?:<[^>]+>)?)\s*\**))\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(([^)]*)\)/);
        if (funcMatch && !KEYWORD_SET.has(funcMatch[2])) {
            symbols.push({
                name: funcMatch[2],
                kind: currentClass ? 'method' : 'function',
                type: funcMatch[1].trim(),
                signature: `${funcMatch[1].trim()} ${funcMatch[2]}(${funcMatch[3].trim()})`,
                className: currentClass,
                line: i,
            });
            continue;
        }
        if (braceDepth > (currentClass ? classBraceDepth + 1 : 0)) {
            const varMatch = stripped.match(/^(const\s+)?((?:owner\s+)?(?:func\s*\([^)]*\)\s*->\s*\w+|(?:auto|void|bool|byte|char|short|int|long|float|double|string|[A-Z][a-zA-Z0-9_]*(?:<[^>]+>)?)\s*\**))\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*[=;]/);
            if (varMatch && !KEYWORD_SET.has(varMatch[3])) {
                symbols.push({
                    name: varMatch[3],
                    kind: 'variable',
                    type: (varMatch[1] ? 'const ' : '') + varMatch[2].trim(),
                    line: i,
                    className: currentClass,
                });
                continue;
            }
        }
        if (currentClass && braceDepth === classBraceDepth + 1) {
            const fieldMatch = stripped.match(/^((?:owner\s+)?(?:auto|void|bool|byte|char|short|int|long|float|double|string|[A-Z][a-zA-Z0-9_]*(?:<[^>]+>)?)\s*\**)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*[=;]/);
            if (fieldMatch && !KEYWORD_SET.has(fieldMatch[2])) {
                symbols.push({
                    name: fieldMatch[2],
                    kind: 'field',
                    type: fieldMatch[1].trim(),
                    className: currentClass,
                    line: i,
                });
            }
        }
    }
    return symbols;
}
// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
function getIncludedHeaders(text) {
    const headers = new Set();
    const re1 = /include\s+<([^>]+)>/g;
    const re2 = /include\s+"([^"]+)"/g;
    let m;
    while ((m = re1.exec(text)) !== null) {
        // Normalize: strip .lph extension for matching
        let h = m[1];
        if (h.endsWith('.lph'))
            h = h.slice(0, -4);
        headers.add(h);
    }
    while ((m = re2.exec(text)) !== null) {
        let h = m[1];
        if (h.endsWith('.lph'))
            h = h.slice(0, -4);
        headers.add(h);
    }
    return headers;
}
function resolveClass(name) {
    if (STDLIB[name])
        return [name, STDLIB[name]];
    if (name === 'List')
        return ['List<T>', STDLIB['List<T>']];
    if (name === 'Map')
        return ['Map<K,V>', STDLIB['Map<K,V>']];
    return undefined;
}
function formatMethodDoc(className, methodName, doc) {
    let md = '```lpl\n' + `${className}.${methodName}: ${doc.signature}` + '\n```\n\n';
    md += doc.description + '\n';
    if (doc.params && doc.params.length > 0) {
        md += '\n**Parameters:**\n';
        for (const p of doc.params)
            md += `- \`${p}\`\n`;
    }
    if (doc.returns)
        md += `\n**Returns:** \`${doc.returns}\`\n`;
    return md;
}
function formatClassDoc(className, cls) {
    let md = '```lpl\nclass ' + className + '\n```\n\n';
    md += cls.description + '\n';
    md += `\n*Header:* \`<${cls.header}>\`\n`;
    const methods = Object.keys(cls.methods);
    if (methods.length > 0) {
        md += '\n**Methods:** ' + methods.map(m => `\`${m}\``).join(', ') + '\n';
    }
    return md;
}
function formatSymbolHover(sym) {
    switch (sym.kind) {
        case 'function': return '```lpl\n(function) ' + sym.signature + '\n```';
        case 'method': return '```lpl\n(method) ' + sym.className + '.' + sym.signature + '\n```';
        case 'constructor': return '```lpl\n(constructor) ' + sym.signature + '\n```';
        case 'variable': return '```lpl\n(variable) ' + sym.type + ' ' + sym.name + '\n```';
        case 'field': return '```lpl\n(field) ' + sym.className + '.' + sym.name + ': ' + sym.type + '\n```';
        case 'class': return '```lpl\nclass ' + sym.name + '\n```';
    }
}
const pendingValidations = new Map();
function validateDocument(doc) {
    const uri = doc.uri;
    // Debounce: wait 300ms after last change
    const existing = pendingValidations.get(uri);
    if (existing)
        clearTimeout(existing);
    pendingValidations.set(uri, setTimeout(() => {
        pendingValidations.delete(uri);
        runValidation(doc);
    }, 300));
}
function runValidation(doc) {
    if (!compilerPath) {
        connection.sendDiagnostics({ uri: doc.uri, diagnostics: [] });
        return;
    }
    const filePath = uriToPath(doc.uri);
    if (!filePath) {
        connection.sendDiagnostics({ uri: doc.uri, diagnostics: [] });
        return;
    }
    // Write document content to a temp file for unsaved changes
    const tmpDir = path.join(require('os').tmpdir(), 'lpl-lsp');
    if (!fs.existsSync(tmpDir))
        fs.mkdirSync(tmpDir, { recursive: true });
    const tmpFile = path.join(tmpDir, path.basename(filePath));
    fs.writeFileSync(tmpFile, doc.getText(), 'utf-8');
    (0, child_process_1.execFile)(compilerPath, [tmpFile, '--check'], { timeout: 10000 }, (error, stdout, _stderr) => {
        const diagnostics = [];
        try {
            const diags = JSON.parse(stdout || '[]');
            for (const d of diags) {
                diagnostics.push({
                    severity: node_1.DiagnosticSeverity.Error,
                    range: {
                        start: { line: Math.max(0, d.line - 1), character: Math.max(0, d.col - 1) },
                        end: { line: Math.max(0, d.line - 1), character: Math.max(0, d.col - 1) + 1 },
                    },
                    message: d.message,
                    source: 'lplc',
                });
            }
        }
        catch {
            // If JSON parse fails, try to parse stderr line-by-line
            if (_stderr) {
                const lines = _stderr.split('\n');
                for (const line of lines) {
                    const m = line.match(/^(.+?):(\d+):(\d+):\s*error:\s*(.+)$/);
                    if (m) {
                        diagnostics.push({
                            severity: node_1.DiagnosticSeverity.Error,
                            range: {
                                start: { line: parseInt(m[2]) - 1, character: parseInt(m[3]) - 1 },
                                end: { line: parseInt(m[2]) - 1, character: parseInt(m[3]) },
                            },
                            message: m[4],
                            source: 'lplc',
                        });
                    }
                }
            }
        }
        // Clean up temp file
        try {
            fs.unlinkSync(tmpFile);
        }
        catch { }
        connection.sendDiagnostics({ uri: doc.uri, diagnostics });
    });
}
function uriToPath(uri) {
    if (uri.startsWith('file://')) {
        return decodeURIComponent(uri.slice(7));
    }
    return undefined;
}
// ---------------------------------------------------------------------------
// Find compiler binary
// ---------------------------------------------------------------------------
function findCompiler() {
    // 1. Check LPL_COMPILER env var
    const envPath = process.env['LPL_COMPILER'];
    if (envPath && fs.existsSync(envPath))
        return envPath;
    // 2. Check relative to this server's location (common in project layout)
    const serverDir = __dirname;
    const candidates = [
        path.resolve(serverDir, '../../..', 'build/src/lplc'),
        path.resolve(serverDir, '../../../..', 'build/src/lplc'),
        path.resolve(serverDir, '..', 'build/src/lplc'),
    ];
    for (const c of candidates) {
        if (fs.existsSync(c))
            return c;
    }
    // 3. Check PATH
    const pathDirs = (process.env['PATH'] || '').split(':');
    for (const dir of pathDirs) {
        const p = path.join(dir, 'lplc');
        if (fs.existsSync(p))
            return p;
    }
    return '';
}
// ---------------------------------------------------------------------------
// LSP Handlers
// ---------------------------------------------------------------------------
connection.onInitialize((params) => {
    compilerPath = findCompiler();
    if (compilerPath) {
        connection.console.log(`LPL LSP: using compiler at ${compilerPath}`);
    }
    else {
        connection.console.warn('LPL LSP: compiler not found. Set LPL_COMPILER env var. Diagnostics disabled.');
    }
    return {
        capabilities: {
            textDocumentSync: node_1.TextDocumentSyncKind.Full,
            completionProvider: {
                triggerCharacters: ['.', '<'],
                resolveProvider: false,
            },
            hoverProvider: true,
            signatureHelpProvider: {
                triggerCharacters: ['(', ','],
            },
        },
    };
});
// Validate on open and change
documents.onDidOpen(e => validateDocument(e.document));
documents.onDidChangeContent(e => validateDocument(e.document));
documents.onDidClose(e => {
    connection.sendDiagnostics({ uri: e.document.uri, diagnostics: [] });
});
// ---------------------------------------------------------------------------
// Completion
// ---------------------------------------------------------------------------
connection.onCompletion((params) => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc)
        return [];
    const items = [];
    const text = doc.getText();
    const lineText = doc.getText({
        start: { line: params.position.line, character: 0 },
        end: params.position,
    });
    const headers = getIncludedHeaders(text);
    const symbols = parseDocumentSymbols(text);
    // After "ClassName." → suggest methods
    const dotMatch = lineText.match(/\b([A-Z][a-zA-Z0-9_]*)\.\s*$/);
    if (dotMatch) {
        const className = dotMatch[1];
        const resolved = resolveClass(className);
        if (resolved && headers.has(resolved[1].header)) {
            const [displayName, cls] = resolved;
            for (const [name, mdoc] of Object.entries(cls.methods)) {
                items.push({
                    label: name,
                    kind: node_1.CompletionItemKind.Method,
                    detail: `${displayName}.${name}`,
                    documentation: { kind: node_1.MarkupKind.Markdown, value: formatMethodDoc(displayName, name, mdoc) },
                });
            }
        }
        for (const sym of symbols) {
            if (sym.className === className && (sym.kind === 'method' || sym.kind === 'constructor')) {
                items.push({ label: sym.name, kind: node_1.CompletionItemKind.Method, detail: sym.signature || sym.name });
            }
            if (sym.className === className && sym.kind === 'field') {
                items.push({ label: sym.name, kind: node_1.CompletionItemKind.Field, detail: `${sym.type} ${sym.name}` });
            }
        }
        if (items.length > 0)
            return items;
    }
    // After "instance." → resolve variable type → suggest class methods
    const instanceDotMatch = lineText.match(/\b([a-z_][a-zA-Z0-9_]*)\.\s*$/);
    if (instanceDotMatch) {
        const varName = instanceDotMatch[1];
        const varSym = symbols.find(s => s.name === varName && (s.kind === 'variable' || s.kind === 'field'));
        if (varSym) {
            const baseType = varSym.type.replace(/^(?:const\s+)?(?:owner\s+)?/, '').replace(/\s*\*+$/, '').trim();
            const resolved = resolveClass(baseType);
            if (resolved && headers.has(resolved[1].header)) {
                const [displayName, cls] = resolved;
                for (const [name, mdoc] of Object.entries(cls.methods)) {
                    items.push({
                        label: name,
                        kind: node_1.CompletionItemKind.Method,
                        detail: `${displayName}.${name}`,
                        documentation: { kind: node_1.MarkupKind.Markdown, value: formatMethodDoc(displayName, name, mdoc) },
                    });
                }
            }
            for (const sym of symbols) {
                if (sym.className === baseType && (sym.kind === 'method' || sym.kind === 'constructor')) {
                    items.push({ label: sym.name, kind: node_1.CompletionItemKind.Method, detail: sym.signature || sym.name });
                }
                if (sym.className === baseType && sym.kind === 'field') {
                    items.push({ label: sym.name, kind: node_1.CompletionItemKind.Field, detail: `${sym.type} ${sym.name}` });
                }
            }
            if (items.length > 0)
                return items;
        }
    }
    // After "include <" → suggest headers
    if (lineText.match(/include\s+<\s*$/)) {
        for (const h of ['console', 'string', 'math', 'files', 'system', 'collections', 'sync']) {
            items.push({ label: h, kind: node_1.CompletionItemKind.File, detail: 'LPL standard library header' });
        }
        return items;
    }
    // General completions
    for (const kw of ALL_KEYWORDS) {
        const item = { label: kw, kind: node_1.CompletionItemKind.Keyword };
        if (KEYWORD_DOCS[kw]) {
            item.documentation = { kind: node_1.MarkupKind.Markdown, value: KEYWORD_DOCS[kw] };
        }
        items.push(item);
    }
    for (const [name, cls] of Object.entries(STDLIB)) {
        if (!headers.has(cls.header))
            continue;
        const displayName = name.replace(/<.*>/, '');
        items.push({
            label: displayName,
            kind: node_1.CompletionItemKind.Class,
            detail: `include <${cls.header}>`,
            documentation: { kind: node_1.MarkupKind.Markdown, value: formatClassDoc(name, cls) },
        });
    }
    for (const sym of symbols) {
        if (sym.kind === 'function') {
            items.push({ label: sym.name, kind: node_1.CompletionItemKind.Function, detail: sym.signature });
        }
        else if (sym.kind === 'class') {
            items.push({ label: sym.name, kind: node_1.CompletionItemKind.Class, detail: `class ${sym.name}` });
        }
        else if (sym.kind === 'variable') {
            items.push({ label: sym.name, kind: node_1.CompletionItemKind.Variable, detail: `${sym.type} ${sym.name}` });
        }
    }
    for (const snip of SNIPPETS) {
        items.push({
            label: snip.label,
            kind: node_1.CompletionItemKind.Snippet,
            insertText: snip.insertText,
            insertTextFormat: node_1.InsertTextFormat.Snippet,
            detail: snip.description,
        });
    }
    return items;
});
// ---------------------------------------------------------------------------
// Hover
// ---------------------------------------------------------------------------
connection.onHover((params) => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc)
        return null;
    const text = doc.getText();
    const line = doc.getText({
        start: { line: params.position.line, character: 0 },
        end: { line: params.position.line, character: 10000 },
    });
    // Get word at position
    const pos = params.position.character;
    let start = pos, end = pos;
    while (start > 0 && /[a-zA-Z0-9_]/.test(line[start - 1]))
        start--;
    while (end < line.length && /[a-zA-Z0-9_]/.test(line[end]))
        end++;
    const word = line.substring(start, end);
    if (!word)
        return null;
    const headers = getIncludedHeaders(text);
    const symbols = parseDocumentSymbols(text);
    // ClassName.method
    const beforeWord = line.substring(0, start);
    const classMatch = beforeWord.match(/\b([A-Z][a-zA-Z0-9_]*)\.\s*$/);
    if (classMatch) {
        const resolved = resolveClass(classMatch[1]);
        if (resolved && headers.has(resolved[1].header)) {
            const method = resolved[1].methods[word];
            if (method) {
                return { contents: { kind: node_1.MarkupKind.Markdown, value: formatMethodDoc(resolved[0], word, method) } };
            }
        }
        const userMethod = symbols.find(s => s.className === classMatch[1] && s.name === word && (s.kind === 'method' || s.kind === 'constructor'));
        if (userMethod)
            return { contents: { kind: node_1.MarkupKind.Markdown, value: formatSymbolHover(userMethod) } };
        const userField = symbols.find(s => s.className === classMatch[1] && s.name === word && s.kind === 'field');
        if (userField)
            return { contents: { kind: node_1.MarkupKind.Markdown, value: formatSymbolHover(userField) } };
    }
    // instance.method
    const instanceMatch = beforeWord.match(/\b([a-z_][a-zA-Z0-9_]*)\.\s*$/);
    if (instanceMatch) {
        const varSym = symbols.find(s => s.name === instanceMatch[1] && (s.kind === 'variable' || s.kind === 'field'));
        if (varSym) {
            const baseType = varSym.type.replace(/^(?:const\s+)?(?:owner\s+)?/, '').replace(/\s*\*+$/, '').trim();
            const resolved = resolveClass(baseType);
            if (resolved && headers.has(resolved[1].header)) {
                const method = resolved[1].methods[word];
                if (method)
                    return { contents: { kind: node_1.MarkupKind.Markdown, value: formatMethodDoc(resolved[0], word, method) } };
            }
        }
    }
    // Stdlib class name
    const resolved = resolveClass(word);
    if (resolved && headers.has(resolved[1].header)) {
        return { contents: { kind: node_1.MarkupKind.Markdown, value: formatClassDoc(resolved[0], resolved[1]) } };
    }
    // User-defined symbol
    const userSym = symbols.find(s => s.name === word);
    if (userSym) {
        return { contents: { kind: node_1.MarkupKind.Markdown, value: formatSymbolHover(userSym) } };
    }
    // Keyword docs
    if (KEYWORD_DOCS[word]) {
        return { contents: { kind: node_1.MarkupKind.Markdown, value: KEYWORD_DOCS[word] } };
    }
    return null;
});
// ---------------------------------------------------------------------------
// Signature Help
// ---------------------------------------------------------------------------
connection.onSignatureHelp((params) => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc)
        return null;
    const lineText = doc.getText({
        start: { line: params.position.line, character: 0 },
        end: params.position,
    });
    const callMatch = lineText.match(/\b([A-Z][a-zA-Z0-9_]*)\.([a-zA-Z_][a-zA-Z0-9_]*)\s*\(([^)]*)$/);
    if (!callMatch)
        return null;
    const resolved = resolveClass(callMatch[1]);
    if (!resolved)
        return null;
    const [displayName, cls] = resolved;
    const method = cls.methods[callMatch[2]];
    if (!method)
        return null;
    const paramLabels = method.params ? method.params.map(p => p.split(' — ')[0]) : [];
    const sig = {
        label: `${displayName}.${callMatch[2]}(${paramLabels.join(', ')})`,
        documentation: method.description,
        parameters: (method.params || []).map(p => {
            const parts = p.split(' — ');
            return { label: parts[0], documentation: parts[1] || '' };
        }),
    };
    return {
        signatures: [sig],
        activeSignature: 0,
        activeParameter: (callMatch[3].match(/,/g) || []).length,
    };
});
// ---------------------------------------------------------------------------
// Start
// ---------------------------------------------------------------------------
documents.listen(connection);
connection.listen();
//# sourceMappingURL=server.js.map