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

import * as vscode from 'vscode';

// ---------------------------------------------------------------------------
// Standard library documentation database
// ---------------------------------------------------------------------------

interface MethodDoc {
    signature: string;
    description: string;
    params?: string[];
    returns?: string;
}

interface ClassDoc {
    description: string;
    header: string;
    methods: Record<string, MethodDoc>;
}

const STDLIB: Record<string, ClassDoc> = {
    Console: {
        description: 'Console I/O — terminal output functions.',
        header: 'console.lph',
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
        header: 'string.lph',
        methods: {
            length: {
                signature: 'static int length(string s)',
                description: 'Returns the length of the string in bytes.',
                params: ['s — the input string'],
                returns: 'int — byte length',
            },
            substring: {
                signature: 'static string substring(string s, int start, int end)',
                description: 'Returns a substring from index `start` (inclusive) to `end` (exclusive).',
                params: ['s — the input string', 'start — start index (inclusive)', 'end — end index (exclusive)'],
                returns: 'string',
            },
            indexOf: {
                signature: 'static int indexOf(string s, string needle)',
                description: 'Returns the index of the first occurrence of `needle`, or -1 if not found.',
                params: ['s — the string to search', 'needle — the substring to find'],
                returns: 'int — index or -1',
            },
            contains: {
                signature: 'static bool contains(string s, string needle)',
                description: 'Returns true if `s` contains `needle`.',
                params: ['s — the string to search', 'needle — the substring to find'],
                returns: 'bool',
            },
            toUpper: {
                signature: 'static string toUpper(string s)',
                description: 'Converts all characters to uppercase.',
                params: ['s — the input string'],
                returns: 'string',
            },
            toLower: {
                signature: 'static string toLower(string s)',
                description: 'Converts all characters to lowercase.',
                params: ['s — the input string'],
                returns: 'string',
            },
            trim: {
                signature: 'static string trim(string s)',
                description: 'Removes leading and trailing whitespace.',
                params: ['s — the input string'],
                returns: 'string',
            },
            fromInt: {
                signature: 'static string fromInt(int value)',
                description: 'Converts an integer to its string representation.',
                params: ['value — the integer to convert'],
                returns: 'string',
            },
            fromFloat: {
                signature: 'static string fromFloat(double value)',
                description: 'Converts a double to its string representation.',
                params: ['value — the double to convert'],
                returns: 'string',
            },
            fromBool: {
                signature: 'static string fromBool(bool value)',
                description: 'Converts a bool to "true" or "false".',
                params: ['value — the bool to convert'],
                returns: 'string',
            },
            toInt: {
                signature: 'static int toInt(string s)',
                description: 'Parses a string as an integer.',
                params: ['s — the string to parse'],
                returns: 'int',
            },
            toFloat: {
                signature: 'static double toFloat(string s)',
                description: 'Parses a string as a double.',
                params: ['s — the string to parse'],
                returns: 'double',
            },
            startsWith: {
                signature: 'static bool startsWith(string s, string prefix)',
                description: 'Returns true if `s` starts with `prefix`.',
                params: ['s — the input string', 'prefix — the prefix to check'],
                returns: 'bool',
            },
            endsWith: {
                signature: 'static bool endsWith(string s, string suffix)',
                description: 'Returns true if `s` ends with `suffix`.',
                params: ['s — the input string', 'suffix — the suffix to check'],
                returns: 'bool',
            },
            replace: {
                signature: 'static string replace(string s, string old, string rep)',
                description: 'Replaces all occurrences of `old` with `rep`.',
                params: ['s — the input string', 'old — substring to replace', 'rep — replacement string'],
                returns: 'string',
            },
            charAt: {
                signature: 'static string charAt(string s, int index)',
                description: 'Returns the character at the given index as a single-character string.',
                params: ['s — the input string', 'index — character position'],
                returns: 'string',
            },
        },
    },
    Math: {
        description: 'Mathematical functions and constants.',
        header: 'math.lph',
        methods: {
            abs: {
                signature: 'static double abs(double x)',
                description: 'Returns the absolute value of a double.',
                params: ['x — input value'],
                returns: 'double',
            },
            absInt: {
                signature: 'static int absInt(int x)',
                description: 'Returns the absolute value of an integer.',
                params: ['x — input value'],
                returns: 'int',
            },
            sqrt: {
                signature: 'static double sqrt(double x)',
                description: 'Returns the square root of x.',
                params: ['x — input value (must be >= 0)'],
                returns: 'double',
            },
            pow: {
                signature: 'static double pow(double base, double exp)',
                description: 'Returns base raised to the power of exp.',
                params: ['base — the base', 'exp — the exponent'],
                returns: 'double',
            },
            sin: {
                signature: 'static double sin(double x)',
                description: 'Returns the sine of x (radians).',
                params: ['x — angle in radians'],
                returns: 'double',
            },
            cos: {
                signature: 'static double cos(double x)',
                description: 'Returns the cosine of x (radians).',
                params: ['x — angle in radians'],
                returns: 'double',
            },
            tan: {
                signature: 'static double tan(double x)',
                description: 'Returns the tangent of x (radians).',
                params: ['x — angle in radians'],
                returns: 'double',
            },
            log: {
                signature: 'static double log(double x)',
                description: 'Returns the natural logarithm (base e) of x.',
                params: ['x — input value (must be > 0)'],
                returns: 'double',
            },
            log10: {
                signature: 'static double log10(double x)',
                description: 'Returns the base-10 logarithm of x.',
                params: ['x — input value (must be > 0)'],
                returns: 'double',
            },
            ceil: {
                signature: 'static double ceil(double x)',
                description: 'Rounds up to the nearest integer.',
                params: ['x — input value'],
                returns: 'double',
            },
            floor: {
                signature: 'static double floor(double x)',
                description: 'Rounds down to the nearest integer.',
                params: ['x — input value'],
                returns: 'double',
            },
            round: {
                signature: 'static double round(double x)',
                description: 'Rounds to the nearest integer.',
                params: ['x — input value'],
                returns: 'double',
            },
            min: {
                signature: 'static double min(double a, double b)',
                description: 'Returns the smaller of two doubles.',
                params: ['a — first value', 'b — second value'],
                returns: 'double',
            },
            max: {
                signature: 'static double max(double a, double b)',
                description: 'Returns the larger of two doubles.',
                params: ['a — first value', 'b — second value'],
                returns: 'double',
            },
            minInt: {
                signature: 'static int minInt(int a, int b)',
                description: 'Returns the smaller of two integers.',
                params: ['a — first value', 'b — second value'],
                returns: 'int',
            },
            maxInt: {
                signature: 'static int maxInt(int a, int b)',
                description: 'Returns the larger of two integers.',
                params: ['a — first value', 'b — second value'],
                returns: 'int',
            },
            PI: {
                signature: 'static double PI()',
                description: 'Returns the value of π (3.14159…).',
                returns: 'double',
            },
            E: {
                signature: 'static double E()',
                description: 'Returns Euler\'s number e (2.71828…).',
                returns: 'double',
            },
            random: {
                signature: 'static double random()',
                description: 'Returns a pseudo-random double in [0.0, 1.0).',
                returns: 'double',
            },
        },
    },
    File: {
        description: 'File I/O — read, write, and manage files.',
        header: 'files.lph',
        methods: {
            read: {
                signature: 'string read()',
                description: 'Reads the entire file contents as a string.',
                returns: 'string',
            },
            write: {
                signature: 'void write(string content)',
                description: 'Writes content to the file, replacing existing contents.',
                params: ['content — the text to write'],
            },
            append: {
                signature: 'void append(string content)',
                description: 'Appends content to the end of the file.',
                params: ['content — the text to append'],
            },
            exists: {
                signature: 'bool exists()',
                description: 'Returns true if the file exists.',
                returns: 'bool',
            },
            close: {
                signature: 'void close()',
                description: 'Closes the file handle.',
            },
            size: {
                signature: 'long size()',
                description: 'Returns the file size in bytes.',
                returns: 'long',
            },
            readAll: {
                signature: 'static string readAll(string path)',
                description: 'Reads the entire file at `path` and returns its contents.',
                params: ['path — file path'],
                returns: 'string',
            },
            writeAll: {
                signature: 'static void writeAll(string path, string content)',
                description: 'Writes `content` to the file at `path`, creating or overwriting it.',
                params: ['path — file path', 'content — the text to write'],
            },
            fileExists: {
                signature: 'static bool fileExists(string path)',
                description: 'Returns true if a file exists at the given path.',
                params: ['path — file path'],
                returns: 'bool',
            },
            remove: {
                signature: 'static void remove(string path)',
                description: 'Deletes the file at the given path.',
                params: ['path — file path'],
            },
        },
    },
    System: {
        description: 'OS-level operations — process control, environment, time.',
        header: 'system.lph',
        methods: {
            exit: {
                signature: 'static void exit(int code)',
                description: 'Terminates the process with the given exit code.',
                params: ['code — exit code'],
            },
            argc: {
                signature: 'static int argc()',
                description: 'Returns the number of command-line arguments.',
                returns: 'int',
            },
            argv: {
                signature: 'static string argv(int index)',
                description: 'Returns the command-line argument at the given index.',
                params: ['index — argument index'],
                returns: 'string',
            },
            getenv: {
                signature: 'static string getenv(string name)',
                description: 'Returns the value of the environment variable, or empty string if unset.',
                params: ['name — environment variable name'],
                returns: 'string',
            },
            currentTimeMillis: {
                signature: 'static long currentTimeMillis()',
                description: 'Returns the current time in milliseconds since the Unix epoch.',
                returns: 'long',
            },
            sleep: {
                signature: 'static void sleep(int milliseconds)',
                description: 'Pauses execution for the given number of milliseconds.',
                params: ['milliseconds — duration to sleep'],
            },
        },
    },
    Mutex: {
        description: 'Mutual exclusion lock for thread synchronization.',
        header: 'sync.lph',
        methods: {
            lock: {
                signature: 'void lock()',
                description: 'Acquires the mutex. Blocks until available.',
            },
            unlock: {
                signature: 'void unlock()',
                description: 'Releases the mutex.',
            },
            tryLock: {
                signature: 'bool tryLock()',
                description: 'Attempts to acquire the mutex without blocking. Returns true if successful.',
                returns: 'bool',
            },
        },
    },
    'List<T>': {
        description: 'Generic dynamic array. Supported: List\\<int\\>, List\\<string\\>.',
        header: 'collections.lph',
        methods: {
            add: {
                signature: 'void add(T value)',
                description: 'Appends a value to the end of the list.',
                params: ['value — the element to add'],
            },
            get: {
                signature: 'T get(int index)',
                description: 'Returns the element at the given index.',
                params: ['index — position (0-based)'],
                returns: 'T',
            },
            set: {
                signature: 'void set(int index, T value)',
                description: 'Replaces the element at the given index.',
                params: ['index — position (0-based)', 'value — the new element'],
            },
            removeAt: {
                signature: 'void removeAt(int index)',
                description: 'Removes the element at the given index, shifting subsequent elements.',
                params: ['index — position (0-based)'],
            },
            size: {
                signature: 'int size()',
                description: 'Returns the number of elements in the list.',
                returns: 'int',
            },
        },
    },
    'Map<K,V>': {
        description: 'Generic hash map. Supported: Map\\<string, int\\>, Map\\<string, string\\>.',
        header: 'collections.lph',
        methods: {
            put: {
                signature: 'void put(K key, V value)',
                description: 'Inserts or updates the value for the given key.',
                params: ['key — the map key', 'value — the value to associate'],
            },
            get: {
                signature: 'V get(K key)',
                description: 'Returns the value associated with the key.',
                params: ['key — the map key'],
                returns: 'V',
            },
            containsKey: {
                signature: 'bool containsKey(K key)',
                description: 'Returns true if the map contains the given key.',
                params: ['key — the map key'],
                returns: 'bool',
            },
            remove: {
                signature: 'void remove(K key)',
                description: 'Removes the entry for the given key.',
                params: ['key — the map key'],
            },
            size: {
                signature: 'int size()',
                description: 'Returns the number of entries in the map.',
                returns: 'int',
            },
        },
    },
};

// ---------------------------------------------------------------------------
// Keyword documentation for hover
// ---------------------------------------------------------------------------

const KEYWORD_DOCS: Record<string, string> = {
    owner: '`owner` — Marks a pointer as the sole owner of a heap-allocated object. The object is automatically destroyed and freed when the owner leaves scope.\n\n```\nowner Person* p = new Person("Alex", 32);\n```',
    move: '`move` — Transfers ownership of a heap object from one owner pointer to another. The source becomes invalid after the move.\n\n```\nowner Person* b = move a;\n```',
    defer: '`defer` — Schedules a statement to execute when the current scope exits (LIFO order). Runs on all exit paths including early returns.\n\n```\ndefer mtx.unlock();\n```',
    extends: '`extends` — Declares single-class inheritance.\n\n```\nclass Dog extends Animal { }\n```',
    implements: '`implements` — Declares interface implementation (comma-separated for multiples).\n\n```\nclass Doc extends Object implements Printable, Serializable { }\n```',
    override: '`override` — Marks a method that overrides a parent class method. The compiler verifies the parent has a matching method.',
    abstract: '`abstract` — Declares a class that cannot be instantiated, or a method with no body that subclasses must implement.',
    func: '`func` — Callable type syntax for function pointers and lambdas.\n\n```\nfunc(int, int) -> int\n```',
    auto: '`auto` — Type inference. The compiler deduces the type from the initializer.\n\n```\nauto x = 42;        // int\nauto add = [](int a, int b) -> int { return a + b; };\n```',
    extern: '`extern "C"` — Declares C-linkage functions for direct interop with C libraries.\n\n```\nextern "C" {\n    int printf(char* fmt, ...);\n}\n```',
    include: '`include` — Imports a header file. Use angle brackets for stdlib, quotes for local.\n\n```\ninclude <console.lph>;\ninclude "mylib.lph";\n```',
    namespace: '`namespace` — Organizes code into hierarchical dot-separated groups.\n\n```\nnamespace App.Models { ... }\n```',
    fallthrough: '`fallthrough` — Explicitly continues execution into the next switch case. Cases do not fall through by default in LPL.',
    as: '`as` — Postfix type cast operator.\n\n```\ndouble d = x as double;\n```',
    'switch': '`switch` — Multi-way branching. Cases do **not** fall through by default (no `break` needed).',
    'const': '`const` — Declares an immutable variable. Must be initialized at declaration.',
};

// ---------------------------------------------------------------------------
// Snippet templates
// ---------------------------------------------------------------------------

interface Snippet {
    label: string;
    insertText: string;
    description: string;
}

const SNIPPETS: Snippet[] = [
    {
        label: 'class',
        insertText: 'class ${1:Name} {\n\tpublic ${1:Name}() {\n\t\t$0\n\t}\n}',
        description: 'Class declaration with constructor',
    },
    {
        label: 'main',
        insertText: 'void main() {\n\t$0\n}',
        description: 'Program entry point',
    },
    {
        label: 'mainargs',
        insertText: 'int main(int argc, string[] args) {\n\t$0\n\treturn 0;\n}',
        description: 'Entry point with arguments',
    },
    {
        label: 'for',
        insertText: 'for (int ${1:i} = 0; ${1:i} < ${2:n}; ${1:i}++) {\n\t$0\n}',
        description: 'For loop',
    },
    {
        label: 'foreach',
        insertText: 'for (int ${1:i} : ${2:0}..${3:10}) {\n\t$0\n}',
        description: 'Range-based for loop',
    },
    {
        label: 'while',
        insertText: 'while (${1:condition}) {\n\t$0\n}',
        description: 'While loop',
    },
    {
        label: 'if',
        insertText: 'if (${1:condition}) {\n\t$0\n}',
        description: 'If statement',
    },
    {
        label: 'ifelse',
        insertText: 'if (${1:condition}) {\n\t$2\n} else {\n\t$0\n}',
        description: 'If/else statement',
    },
    {
        label: 'switch',
        insertText: 'switch (${1:expr}) {\n\tcase ${2:value}:\n\t\t$0\n\tdefault:\n\t\tbreak;\n}',
        description: 'Switch statement',
    },
    {
        label: 'trycatch',
        insertText: 'try {\n\t$1\n} catch (${2:Exception} ${3:e}) {\n\t$0\n}',
        description: 'Try/catch block',
    },
    {
        label: 'lambda',
        insertText: '[${1:}](${2:params}) -> ${3:int} { return ${0:expr}; }',
        description: 'Lambda expression',
    },
    {
        label: 'owner',
        insertText: 'owner ${1:Type}* ${2:name} = new ${1:Type}(${0:args});',
        description: 'Owner pointer with heap allocation',
    },
    {
        label: 'defer',
        insertText: 'defer ${0:statement};',
        description: 'Defer statement',
    },
    {
        label: 'include',
        insertText: 'include <${1:console}.lph>;',
        description: 'Include standard library header',
    },
];

// ---------------------------------------------------------------------------
// Document symbol parsing — extracts functions, variables, classes, and methods
// ---------------------------------------------------------------------------

interface SymbolInfo {
    name: string;
    kind: 'function' | 'variable' | 'class' | 'method' | 'field' | 'constructor';
    type: string;           // return type or variable type
    signature?: string;     // full signature for functions/methods
    className?: string;     // owning class (for methods/fields)
    line: number;
}

const PRIMITIVE_TYPES = new Set([
    'void', 'bool', 'byte', 'char', 'short', 'int', 'long',
    'float', 'double', 'string', 'auto',
]);

const ALL_KEYWORDS = new Set([
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
]);

// Type pattern: handles primitives, class names, owner pointers, func types, generics
const TYPE_RE = /(?:owner\s+)?(?:func\s*\([^)]*\)\s*->\s*\w+|(?:auto|void|bool|byte|char|short|int|long|float|double|string|[A-Z][a-zA-Z0-9_]*(?:<[^>]+>)?))\s*\**/;

function parseDocument(document: vscode.TextDocument): SymbolInfo[] {
    const symbols: SymbolInfo[] = [];
    const text = document.getText();
    const lines = text.split('\n');

    let currentClass: string | undefined;
    let braceDepth = 0;
    let classBraceDepth = -1;

    for (let i = 0; i < lines.length; i++) {
        const line = lines[i];
        const trimmed = line.replace(/\/\/.*$/, '').trim();
        if (!trimmed) { continue; }

        // Track brace depth for class scope
        for (const ch of trimmed) {
            if (ch === '{') { braceDepth++; }
            if (ch === '}') {
                braceDepth--;
                if (currentClass && braceDepth <= classBraceDepth) {
                    currentClass = undefined;
                    classBraceDepth = -1;
                }
            }
        }

        // Class declaration
        const classMatch = trimmed.match(/^(?:abstract\s+)?class\s+([A-Z][a-zA-Z0-9_]*)(?:<[^>]+>)?/);
        if (classMatch) {
            symbols.push({
                name: classMatch[1],
                kind: 'class',
                type: 'class',
                line: i,
            });
            currentClass = classMatch[1];
            classBraceDepth = braceDepth - 1; // The { on this line already incremented
            continue;
        }

        // Strip access modifiers and qualifiers for matching
        const stripped = trimmed.replace(/^(?:public|protected|private|static|override|abstract)\s+/g, '')
                                .replace(/^(?:public|protected|private|static|override|abstract)\s+/g, '');

        // Constructor: ClassName(params) {
        if (currentClass) {
            const ctorRe = new RegExp(`^${currentClass}\\s*\\(([^)]*)\\)`);
            const ctorMatch = stripped.match(ctorRe);
            if (ctorMatch) {
                symbols.push({
                    name: currentClass,
                    kind: 'constructor',
                    type: currentClass,
                    signature: `${currentClass}(${ctorMatch[1]})`,
                    className: currentClass,
                    line: i,
                });
                continue;
            }
        }

        // Function or method: returnType name(params)
        const funcMatch = stripped.match(
            /^((?:owner\s+)?(?:func\s*\([^)]*\)\s*->\s*\w+|(?:auto|void|bool|byte|char|short|int|long|float|double|string|[A-Z][a-zA-Z0-9_]*(?:<[^>]+>)?)\s*\**))\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(([^)]*)\)/
        );
        if (funcMatch && !ALL_KEYWORDS.has(funcMatch[2])) {
            const retType = funcMatch[1].trim();
            const name = funcMatch[2];
            const params = funcMatch[3].trim();

            symbols.push({
                name,
                kind: currentClass ? 'method' : 'function',
                type: retType,
                signature: `${retType} ${name}(${params})`,
                className: currentClass,
                line: i,
            });
            continue;
        }

        // Variable declaration: Type name = ... ; or Type name;
        // Must not be inside a function parameter list
        if (braceDepth > (currentClass ? classBraceDepth + 1 : 0)) {
            const varMatch = stripped.match(
                /^(const\s+)?((?:owner\s+)?(?:func\s*\([^)]*\)\s*->\s*\w+|(?:auto|void|bool|byte|char|short|int|long|float|double|string|[A-Z][a-zA-Z0-9_]*(?:<[^>]+>)?)\s*\**))\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*[=;]/
            );
            if (varMatch && !ALL_KEYWORDS.has(varMatch[3])) {
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

        // Fields (class-level variables)
        if (currentClass && braceDepth === classBraceDepth + 1) {
            const fieldMatch = stripped.match(
                /^((?:owner\s+)?(?:auto|void|bool|byte|char|short|int|long|float|double|string|[A-Z][a-zA-Z0-9_]*(?:<[^>]+>)?)\s*\**)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*[=;]/
            );
            if (fieldMatch && !ALL_KEYWORDS.has(fieldMatch[2])) {
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

/** Get the set of included header filenames from a document */
function getIncludedHeaders(document: vscode.TextDocument): Set<string> {
    const headers = new Set<string>();
    const text = document.getText();
    const re = /include\s+<([^>]+)>/g;
    let m;
    while ((m = re.exec(text)) !== null) {
        headers.add(m[1]);
    }
    // Also local includes
    const re2 = /include\s+"([^"]+)"/g;
    while ((m = re2.exec(text)) !== null) {
        headers.add(m[1]);
    }
    return headers;
}

/** Check if a stdlib class is available given included headers */
function isClassAvailable(className: string, headers: Set<string>): boolean {
    const resolved = resolveClass(className);
    if (!resolved) { return false; }
    return headers.has(resolved[1].header);
}

function formatMethodDoc(className: string, methodName: string, doc: MethodDoc): vscode.MarkdownString {
    const md = new vscode.MarkdownString();
    md.isTrusted = true;
    md.appendCodeblock(`${className}.${methodName}: ${doc.signature}`, 'lpl');
    md.appendMarkdown('\n' + doc.description + '\n');
    if (doc.params && doc.params.length > 0) {
        md.appendMarkdown('\n**Parameters:**\n');
        for (const p of doc.params) {
            md.appendMarkdown(`- \`${p}\`\n`);
        }
    }
    if (doc.returns) {
        md.appendMarkdown(`\n**Returns:** \`${doc.returns}\`\n`);
    }
    return md;
}

function formatClassDoc(className: string, cls: ClassDoc): vscode.MarkdownString {
    const md = new vscode.MarkdownString();
    md.isTrusted = true;
    md.appendCodeblock(`class ${className}`, 'lpl');
    md.appendMarkdown('\n' + cls.description + '\n');
    md.appendMarkdown(`\n*Header:* \`<${cls.header}>\`\n`);
    const methods = Object.keys(cls.methods);
    if (methods.length > 0) {
        md.appendMarkdown('\n**Methods:** ' + methods.map(m => `\`${m}\``).join(', ') + '\n');
    }
    return md;
}

function formatSymbolHover(sym: SymbolInfo): vscode.MarkdownString {
    const md = new vscode.MarkdownString();
    md.isTrusted = true;
    switch (sym.kind) {
        case 'function':
            md.appendCodeblock(`(function) ${sym.signature}`, 'lpl');
            break;
        case 'method':
            md.appendCodeblock(`(method) ${sym.className}.${sym.signature}`, 'lpl');
            break;
        case 'constructor':
            md.appendCodeblock(`(constructor) ${sym.signature}`, 'lpl');
            break;
        case 'variable':
            md.appendCodeblock(`(variable) ${sym.type} ${sym.name}`, 'lpl');
            break;
        case 'field':
            md.appendCodeblock(`(field) ${sym.className}.${sym.name}: ${sym.type}`, 'lpl');
            break;
        case 'class':
            md.appendCodeblock(`class ${sym.name}`, 'lpl');
            break;
    }
    return md;
}

/** Resolve "List" / "Map" lookups which are stored under "List<T>" / "Map<K,V>" */
function resolveClass(name: string): [string, ClassDoc] | undefined {
    if (STDLIB[name]) {
        return [name, STDLIB[name]];
    }
    if (name === 'List') {
        return ['List<T>', STDLIB['List<T>']];
    }
    if (name === 'Map') {
        return ['Map<K,V>', STDLIB['Map<K,V>']];
    }
    return undefined;
}

// ---------------------------------------------------------------------------
// Extension activation
// ---------------------------------------------------------------------------

export function activate(context: vscode.ExtensionContext) {
    const selector: vscode.DocumentSelector = { language: 'lpl' };

    // ----- Completion Provider -----
    const completionProvider = vscode.languages.registerCompletionItemProvider(
        selector,
        {
            provideCompletionItems(document, position) {
                const items: vscode.CompletionItem[] = [];
                const lineText = document.lineAt(position).text;
                const linePrefix = lineText.substring(0, position.character);
                const headers = getIncludedHeaders(document);
                const symbols = parseDocument(document);

                // After "ClassName." → suggest methods (only if header included or user class)
                const dotMatch = linePrefix.match(/\b([A-Z][a-zA-Z0-9_]*)\.\s*$/);
                if (dotMatch) {
                    const className = dotMatch[1];
                    const resolved = resolveClass(className);
                    if (resolved && headers.has(resolved[1].header)) {
                        const [displayName, cls] = resolved;
                        for (const [name, doc] of Object.entries(cls.methods)) {
                            const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Method);
                            item.detail = `${displayName}.${name}`;
                            item.documentation = formatMethodDoc(displayName, name, doc);
                            items.push(item);
                        }
                    }
                    // User-defined class methods
                    for (const sym of symbols) {
                        if (sym.className === className && (sym.kind === 'method' || sym.kind === 'constructor')) {
                            const item = new vscode.CompletionItem(sym.name, vscode.CompletionItemKind.Method);
                            item.detail = sym.signature || sym.name;
                            item.documentation = formatSymbolHover(sym);
                            items.push(item);
                        }
                        if (sym.className === className && sym.kind === 'field') {
                            const item = new vscode.CompletionItem(sym.name, vscode.CompletionItemKind.Field);
                            item.detail = `${sym.type} ${sym.name}`;
                            item.documentation = formatSymbolHover(sym);
                            items.push(item);
                        }
                    }
                    if (items.length > 0) { return items; }
                }

                // After "instance." where instance is a known variable → suggest class methods
                const instanceDotMatch = linePrefix.match(/\b([a-z_][a-zA-Z0-9_]*)\.\s*$/);
                if (instanceDotMatch) {
                    const varName = instanceDotMatch[1];
                    const varSym = symbols.find(s => s.name === varName && (s.kind === 'variable' || s.kind === 'field'));
                    if (varSym) {
                        // Strip pointer/owner prefixes to get the class name
                        const baseType = varSym.type.replace(/^(?:const\s+)?(?:owner\s+)?/, '').replace(/\s*\*+$/, '').trim();
                        // Check stdlib
                        const resolved = resolveClass(baseType);
                        if (resolved && headers.has(resolved[1].header)) {
                            const [displayName, cls] = resolved;
                            for (const [name, doc] of Object.entries(cls.methods)) {
                                const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Method);
                                item.detail = `${displayName}.${name}`;
                                item.documentation = formatMethodDoc(displayName, name, doc);
                                items.push(item);
                            }
                        }
                        // Check user-defined class methods
                        for (const sym of symbols) {
                            if (sym.className === baseType && (sym.kind === 'method' || sym.kind === 'constructor')) {
                                const item = new vscode.CompletionItem(sym.name, vscode.CompletionItemKind.Method);
                                item.detail = sym.signature || sym.name;
                                item.documentation = formatSymbolHover(sym);
                                items.push(item);
                            }
                            if (sym.className === baseType && sym.kind === 'field') {
                                const item = new vscode.CompletionItem(sym.name, vscode.CompletionItemKind.Field);
                                item.detail = `${sym.type} ${sym.name}`;
                                item.documentation = formatSymbolHover(sym);
                                items.push(item);
                            }
                        }
                        if (items.length > 0) { return items; }
                    }
                }

                // After "include <" → suggest headers
                const includeMatch = linePrefix.match(/include\s+<\s*$/);
                if (includeMatch) {
                    const allHeaders = ['console.lph', 'string.lph', 'math.lph', 'files.lph', 'system.lph', 'collections.lph', 'sync.lph'];
                    for (const h of allHeaders) {
                        const item = new vscode.CompletionItem(h, vscode.CompletionItemKind.File);
                        item.detail = 'LPL standard library header';
                        items.push(item);
                    }
                    return items;
                }

                // General completions: keywords, types, snippets
                const keywords = [
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

                for (const kw of keywords) {
                    const item = new vscode.CompletionItem(kw, vscode.CompletionItemKind.Keyword);
                    if (KEYWORD_DOCS[kw]) {
                        item.documentation = new vscode.MarkdownString(KEYWORD_DOCS[kw]);
                    }
                    items.push(item);
                }

                // Only suggest stdlib classes whose header is included
                for (const [name, cls] of Object.entries(STDLIB)) {
                    if (!headers.has(cls.header)) { continue; }
                    const displayName = name.replace(/<.*>/, '');
                    const item = new vscode.CompletionItem(displayName, vscode.CompletionItemKind.Class);
                    item.detail = `include <${cls.header}>`;
                    item.documentation = formatClassDoc(name, cls);
                    items.push(item);
                }

                // User-defined symbols
                for (const sym of symbols) {
                    if (sym.kind === 'function') {
                        const item = new vscode.CompletionItem(sym.name, vscode.CompletionItemKind.Function);
                        item.detail = sym.signature;
                        item.documentation = formatSymbolHover(sym);
                        items.push(item);
                    } else if (sym.kind === 'class') {
                        const item = new vscode.CompletionItem(sym.name, vscode.CompletionItemKind.Class);
                        item.detail = `class ${sym.name}`;
                        item.documentation = formatSymbolHover(sym);
                        items.push(item);
                    } else if (sym.kind === 'variable') {
                        const item = new vscode.CompletionItem(sym.name, vscode.CompletionItemKind.Variable);
                        item.detail = `${sym.type} ${sym.name}`;
                        item.documentation = formatSymbolHover(sym);
                        items.push(item);
                    }
                }

                // Snippets
                for (const snip of SNIPPETS) {
                    const item = new vscode.CompletionItem(snip.label, vscode.CompletionItemKind.Snippet);
                    item.insertText = new vscode.SnippetString(snip.insertText);
                    item.detail = snip.description;
                    items.push(item);
                }

                return items;
            },
        },
        '.', '<'
    );

    // ----- Hover Provider -----
    const hoverProvider = vscode.languages.registerHoverProvider(selector, {
        provideHover(document, position) {
            const wordRange = document.getWordRangeAtPosition(position, /[a-zA-Z_][a-zA-Z0-9_]*/);
            if (!wordRange) { return; }
            const word = document.getText(wordRange);
            const lineText = document.lineAt(position).text;
            const charBefore = wordRange.start.character;
            const headers = getIncludedHeaders(document);
            const symbols = parseDocument(document);

            // Pattern: ClassName.word — method hover
            const beforeWord = lineText.substring(0, charBefore);
            const classMatch = beforeWord.match(/\b([A-Z][a-zA-Z0-9_]*)\.\s*$/);
            if (classMatch) {
                // Stdlib method
                const resolved = resolveClass(classMatch[1]);
                if (resolved && headers.has(resolved[1].header)) {
                    const [displayName, cls] = resolved;
                    const method = cls.methods[word];
                    if (method) {
                        return new vscode.Hover(formatMethodDoc(displayName, word, method), wordRange);
                    }
                }
                // User-defined method
                const userMethod = symbols.find(s => s.className === classMatch[1] && s.name === word && (s.kind === 'method' || s.kind === 'constructor'));
                if (userMethod) {
                    return new vscode.Hover(formatSymbolHover(userMethod), wordRange);
                }
                const userField = symbols.find(s => s.className === classMatch[1] && s.name === word && s.kind === 'field');
                if (userField) {
                    return new vscode.Hover(formatSymbolHover(userField), wordRange);
                }
            }

            // instance.word — resolve variable type, then show method
            const instanceMatch = beforeWord.match(/\b([a-z_][a-zA-Z0-9_]*)\.\s*$/);
            if (instanceMatch) {
                const varSym = symbols.find(s => s.name === instanceMatch[1] && (s.kind === 'variable' || s.kind === 'field'));
                if (varSym) {
                    const baseType = varSym.type.replace(/^(?:const\s+)?(?:owner\s+)?/, '').replace(/\s*\*+$/, '').trim();
                    const resolved = resolveClass(baseType);
                    if (resolved && headers.has(resolved[1].header)) {
                        const method = resolved[1].methods[word];
                        if (method) {
                            return new vscode.Hover(formatMethodDoc(resolved[0], word, method), wordRange);
                        }
                    }
                    const userMethod = symbols.find(s => s.className === baseType && s.name === word && (s.kind === 'method' || s.kind === 'constructor'));
                    if (userMethod) {
                        return new vscode.Hover(formatSymbolHover(userMethod), wordRange);
                    }
                    const userField = symbols.find(s => s.className === baseType && s.name === word && s.kind === 'field');
                    if (userField) {
                        return new vscode.Hover(formatSymbolHover(userField), wordRange);
                    }
                }
            }

            // Check if hovering over a stdlib class name
            const resolved = resolveClass(word);
            if (resolved && headers.has(resolved[1].header)) {
                return new vscode.Hover(formatClassDoc(resolved[0], resolved[1]), wordRange);
            }

            // User-defined symbol (function, variable, class, etc.)
            const userSym = symbols.find(s => s.name === word);
            if (userSym) {
                return new vscode.Hover(formatSymbolHover(userSym), wordRange);
            }

            // Keyword docs
            if (KEYWORD_DOCS[word]) {
                return new vscode.Hover(new vscode.MarkdownString(KEYWORD_DOCS[word]), wordRange);
            }

            return undefined;
        },
    });

    // ----- Signature Help Provider -----
    const signatureProvider = vscode.languages.registerSignatureHelpProvider(
        selector,
        {
            provideSignatureHelp(document, position) {
                const lineText = document.lineAt(position).text;
                const textBefore = lineText.substring(0, position.character);

                // Find the innermost unclosed function call: ClassName.method(
                const callMatch = textBefore.match(/\b([A-Z][a-zA-Z0-9_]*)\.([a-zA-Z_][a-zA-Z0-9_]*)\s*\(([^)]*)$/);
                if (!callMatch) { return; }

                const resolved = resolveClass(callMatch[1]);
                if (!resolved) { return; }
                const [displayName, cls] = resolved;
                const method = cls.methods[callMatch[2]];
                if (!method) { return; }

                const sig = new vscode.SignatureInformation(
                    `${displayName}.${callMatch[2]}(${method.params ? method.params.map(p => p.split(' — ')[0]).join(', ') : ''})`,
                    method.description
                );

                if (method.params) {
                    for (const p of method.params) {
                        const parts = p.split(' — ');
                        sig.parameters.push(new vscode.ParameterInformation(parts[0], parts[1] || ''));
                    }
                }

                const help = new vscode.SignatureHelp();
                help.signatures = [sig];
                help.activeSignature = 0;
                // Count commas in the args to determine active parameter
                const argsText = callMatch[3];
                help.activeParameter = (argsText.match(/,/g) || []).length;
                return help;
            },
        },
        '(', ','
    );

    context.subscriptions.push(completionProvider, hoverProvider, signatureProvider);
}

export function deactivate() {}
