# LPL Language Manual

## Table of Contents

1. [Introduction](#introduction)
2. [Philosophy](#philosophy)
3. [Getting Started](#getting-started)
4. [Type System](#type-system)
5. [Variables and Declarations](#variables-and-declarations)
6. [Operators](#operators)
7. [Control Flow](#control-flow)
8. [Functions](#functions)
9. [Lambdas and Closures](#lambdas-and-closures)
10. [Classes](#classes)
11. [Interfaces](#interfaces)
12. [Generics and Templates](#generics-and-templates)
13. [Error Handling](#error-handling)
14. [Type Casting](#type-casting)
15. [Memory Model](#memory-model)
16. [Ownership and RAII](#ownership-and-raii)
17. [Defer](#defer)
18. [Namespaces](#namespaces)
19. [Module System](#module-system)
20. [C Interop](#c-interop)
21. [C++ Interop](#c-interop-1)
22. [Platform Targeting](#platform-targeting)
23. [Standard Library](#standard-library)
24. [Compiler Usage](#compiler-usage)
25. [Editor Support](#editor-support)
26. [Best Practices](#best-practices)
27. [Design Principles](#design-principles)

---

## Introduction

LPL is a modern, compiled, statically-typed programming language that combines the explicit memory control and performance of C++ with the clean class structure and readability of Java. It compiles to native machine code via LLVM, producing fast executables with no garbage collector and no runtime VM.

LPL is designed for programmers who want:

- Full control over memory and object lifetimes
- Clean, readable syntax without punctuation noise
- Deterministic resource cleanup via RAII and `defer`
- Native performance with zero-cost abstractions
- A small, predictable runtime

### Hello, World

```
include <console.lph>;

void main() {
    Console.println("Hello, World!");
}
```

Compile and run:

```sh
lplc hello.lpl -o hello
./hello
```

### A More Complete Example

```
include <console.lph>;

class Person {
    string name;
    int age;

    public Person(string name, int age) {
        this.name = name;
        this.age = age;
    }

    public void greet() {
        Console.println("Hello, " + this.name);
    }
}

void main() {
    // Stack-allocated object — destroyed at end of scope
    Person p = Person("Alex", 32);
    p.greet();

    // Heap-allocated owning pointer — destroyed and freed at end of scope
    owner Person* h = new Person("Jordan", 40);
    h.greet();
}
```

Output:

```
Hello, Alex
Hello, Jordan
```

---

## Philosophy

LPL is built on a clear set of guiding principles:

1. **Explicit is better than implicit.** Types are always visible at declaration — either written explicitly or deduced with `auto` from an obvious initializer. Memory ownership is stated in the language, not hidden in library templates. You always know what you're looking at.

2. **Power without noise.** LPL keeps the full control of C++ — pointers, stack allocation, heap allocation, deterministic destruction — but sheds the syntactic clutter. No `std::unique_ptr<T>`, no `#ifdef`, no `:` inheritance.

3. **Readability is a feature.** Code is read far more often than it is written. Keywords like `extends`, `implements`, and `owner` are chosen for clarity over brevity. Private-by-default class members enforce good encapsulation without extra ceremony.

4. **Zero-cost safety.** Ownership rules, RAII, and `defer` are enforced at compile time. There is no garbage collector, no reference counting overhead, no runtime safety tax. If the program compiles, the resource management is correct.

5. **Small runtime, big reach.** The runtime library is intentionally minimal (target < 50 KB). LPL produces native binaries that depend on nothing beyond the C standard library. It can target any platform LLVM supports.

6. **Familiar by design.** If you know C++ or Java, you already know most of LPL. The learning curve is shallow by intention. Surprises are bugs in the language design.

---

## Getting Started

### File Types

| Extension | Purpose |
|-----------|---------|
| `.lpl` | Source / implementation file |
| `.lph` | Header / export file (public interface declarations) |

### Program Entry Point

Every LPL program needs a `main` function. The simplest form takes no arguments:

```
void main() {
    // program starts here
}
```

For full C/C++ compatibility, `main` can accept `argc` and `argv` and return an exit code:

```
int main(int argc, char** argv) {
    if (argc < 2) {
        Console.println("Usage: program <name>");
        return 1;
    }
    Console.println(argv[1]);
    return 0;
}
```

For modern LPL style signature:

```
int main(int argc, string[] args) {
    if (argc < 2) {
        Console.println("Usage: program <name>");
        return 1;
    }
    Console.println(args[1]);
    return 0;
}
```


Supported `main` signatures:

| Signature | Description |
|-----------|-------------|
| `void main()` | No arguments, no exit code |
| `int main()` | Returns exit code |
| `void main(int argc, char** argv)` | C-style raw pointers |
| `int main(int argc, char** argv)` | C-style with exit code |
| `void main(int argc, string[] args)` | Modern LPL strings |
| `int main(int argc, string[] args)` | Modern LPL with exit code |

The `char**` variants give you raw C pointers (use `string(charPtr)` to convert). The `string[]` variants receive pre-converted LPL strings — use whichever fits your needs.

The compiler generates a native C entry point that initializes the runtime, passes `argc`/`argv`, calls your `main()`, and shuts down cleanly.

### Compilation

```sh
# Compile and link to executable
lplc source.lpl -o myapp

# Compile only to object file
lplc source.lpl -c -o source.o

# Emit LLVM IR for inspection
lplc source.lpl -emit-llvm -o source.ll

# Emit native assembly
lplc source.lpl -S -o source.s
```

---

## Type System

LPL is strongly and statically typed. Every variable has a definite type, either declared explicitly or deduced with `auto`.

### Primitive Types

| Type | Size | Description |
|------|------|-------------|
| `bool` | 1 byte | `true` or `false` |
| `byte` | 1 byte | Unsigned 8-bit integer |
| `char` | 4 bytes | Unicode codepoint |
| `short` | 2 bytes | Signed 16-bit integer |
| `int` | 4 bytes | Signed 32-bit integer |
| `long` | 8 bytes | Signed 64-bit integer |
| `float` | 4 bytes | IEEE 754 single-precision |
| `double` | 8 bytes | IEEE 754 double-precision |
| `string` | struct | UTF-8 encoded, length-tracked string |
| `void` | 0 | No value (return type only) |

### Class Types

Any class you define becomes a type:

```
Person p = Person("Alex", 32);
```

### Pointer Types

```
Person* ptr;            // non-owning pointer
owner Person* optr;     // owning pointer (responsible for destruction)
char** argv;            // pointer to pointer (multi-level)
```

Multi-level pointers (`**`, `***`, etc.) are supported for C interop. Indexing a pointer dereferences one level: `argv[i]` on `char**` yields `char*`.

Use `string(expr)` to explicitly convert a `char*` to `string`:

```
char* p = argv[0];
string s = string(p);   // explicit conversion
```

### String Type

`string` is a built-in value type backed by a heap-allocated UTF-8 buffer with tracked length. Strings support concatenation with `+`:

```
string greeting = "Hello, " + name + "!";
```

### Callable Types

Function pointers and lambdas use the `func` type syntax:

```
func(int, int) -> int       // function taking two ints, returning int
func(string) -> void        // function taking string, returning nothing
func() -> int               // function taking no args, returning int
```

Callable types are fat pointers internally — they carry both a function pointer and an environment pointer for captured variables.

### Literals

```
42              // int
3.14            // double
"hello"         // string
'A'             // char
true            // bool
false           // bool
null            // null pointer
```

---

## Variables and Declarations

Variables are declared with the type first, followed by the name and optional initialization:

```
int x = 5;
string name = "Alex";
bool ready = true;
double pi = 3.14159;
Person p = Person("Jordan", 40);
```

### Type Inference with `auto`

Use `auto` to let the compiler deduce the type from the initializer:

```
auto x = 42;                    // deduced as int
auto name = "hello";            // deduced as string
auto pi = 3.14;                 // deduced as double
auto flag = true;               // deduced as bool
```

`auto` requires an initializer — the compiler needs something to deduce from:

```
auto x;     // ERROR: auto variable must have an initializer
```

`auto` is especially useful with lambdas, where writing the full type is verbose:

```
// Without auto — verbose
func(int, int) -> int add = [](int x, int y) -> int { return x + y; };

// With auto — clean
auto add = [](int x, int y) -> int { return x + y; };
```

### Const Variables

Use `const` to declare immutable variables. Const variables must be initialized:

```
const int maxSize = 100;
const auto name = "Alex";    // const + auto combined
```

Attempting to reassign a `const` variable produces a compile error.

### Pointer Variables

```
Person* ptr = &localPerson;         // non-owning pointer to stack object
owner Person* op = new Person();    // owning pointer to heap object
```

---

## Operators

### Arithmetic

| Operator | Meaning |
|----------|---------|
| `+` | Addition / string concatenation |
| `-` | Subtraction |
| `*` | Multiplication |
| `/` | Division |
| `%` | Modulo |
| `++` | Increment (prefix or postfix) |
| `--` | Decrement (prefix or postfix) |

### Comparison

| Operator | Meaning |
|----------|---------|
| `==` | Equal |
| `!=` | Not equal |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less or equal |
| `>=` | Greater or equal |

### Logical

| Operator | Meaning |
|----------|---------|
| `&&` | Logical AND |
| `\|\|` | Logical OR |
| `!` | Logical NOT |

### Bitwise

| Operator | Meaning |
|----------|---------|
| `&` | Bitwise AND |
| `\|` | Bitwise OR |
| `^` | Bitwise XOR |
| `~` | Bitwise NOT |
| `<<` | Left shift |
| `>>` | Right shift |

### Assignment

| Operator | Meaning |
|----------|---------|
| `=` | Assign |
| `+=` | Add and assign |
| `-=` | Subtract and assign |
| `*=` | Multiply and assign |
| `/=` | Divide and assign |

### Other

| Operator | Meaning |
|----------|---------|
| `&` | Address-of |
| `*` | Dereference |
| `.` | Member access (auto-derefs pointers) |
| `..` | Half-open range (start inclusive, end exclusive) |
| `..=` | Closed range (start inclusive, end inclusive) |
| `? :` | Ternary conditional |
| `as` | Type cast (`expr as Type`) |
| `new` | Heap allocation |
| `delete` | Heap deallocation |
| `move` | Ownership transfer |

### Ternary Operator

The ternary conditional operator provides inline branching:

```
int max = (a > b) ? a : b;
string label = (count == 1) ? "item" : "items";
```

### Operator Overloading

Classes can overload operators by defining `operator` methods:

```
class Vec2 {
    int x;
    int y;

    public Vec2(int x, int y) {
        this.x = x;
        this.y = y;
    }

    public Vec2 operator+(Vec2 other) {
        return Vec2(this.x + other.x, this.y + other.y);
    }

    public Vec2 operator*(int scalar) {
        return Vec2(this.x * scalar, this.y * scalar);
    }

    public bool operator==(Vec2 other) {
        return this.x == other.x && this.y == other.y;
    }
}

void main() {
    Vec2 a = Vec2(1, 2);
    Vec2 b = Vec2(3, 4);
    Vec2 c = a + b;            // calls operator+
    Vec2 d = a * 5;            // calls operator*
    bool eq = a == b;          // calls operator==
}
```

Supported overloadable operators: `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `[]`.

---

## Control Flow

### If / Else

```
if (x > 0) {
    Console.println("positive");
} else if (x == 0) {
    Console.println("zero");
} else {
    Console.println("negative");
}
```

### While Loop

```
int i = 0;
while (i < 10) {
    Console.printInt(i);
    i += 1;
}
```

### For Loop

```
for (int i = 0; i < 10; i += 1) {
    Console.printInt(i);
}
```

### Do-While Loop

The body executes at least once before the condition is checked:

```
int i = 0;
do {
    Console.printInt(i);
    i += 1;
} while (i < 10);
```

### For-Each / Range-Based For

Iterate over a numeric range with `for (Type name : start..end)`:

```
for (int i : 0..10) {
    Console.printInt(i);    // prints 0 through 9
}
```

The `..` operator creates a half-open range: `start` is inclusive, `end` is exclusive.

Use `..=` for a closed (inclusive) range:

```
for (int i : 0..=10) {
    Console.printInt(i);    // prints 0 through 10
}
```

### Switch / Case

Multi-way branching on integer or character values. Cases **do not fall through** by default — no `break` needed:

```
switch (day) {
    case 1:
        Console.println("Monday");
    case 2:
        Console.println("Tuesday");
    case 6:
    case 7:
        Console.println("Weekend");   // empty cases still fall through naturally
    default:
        Console.println("Other day");
}
```

Empty case bodies (like `case 6:` above) fall through to the next case naturally. For intentional fallthrough from a non-empty case, use the `fallthrough` keyword:

```
switch (grade) {
    case 1:
        Console.println("Excellent");
        fallthrough;    // explicit — continues into case 2
    case 2:
        Console.println("Good");
}
```

`break` is still supported but rarely needed since cases stop automatically.

### Break and Continue

```
for (int i = 0; i < 100; i += 1) {
    if (i == 50) break;
    if (i % 2 == 0) continue;
    Console.printInt(i);
}
```

### Return

```
int add(int a, int b) {
    return a + b;
}
```

---

## Functions

Functions are declared at the top level with an explicit return type:

```
int add(int a, int b) {
    return a + b;
}

void greet(string name) {
    Console.println("Hello, " + name);
}

void main() {
    int result = add(3, 4);
    greet("World");
}
```

### Rules

- Return type is always required (use `void` for no return value)
- Parameter types are always required
- Functions must be declared before use (or in an included header)
- The entry point is `void main()` or `int main(int argc, char** argv)`

### Default Parameters

Parameters can have default values. Parameters with defaults must come after parameters without:

```
int add(int a, int b = 10) {
    return a + b;
}

void greet(string name, string prefix = "Hello") {
    Console.println(prefix + ", " + name);
}

void main() {
    int r1 = add(5, 3);     // r1 = 8
    int r2 = add(5);        // r2 = 15 (b defaults to 10)

    greet("World");          // "Hello, World"
    greet("World", "Hi");   // "Hi, World"
}
```

When calling a function, you can omit trailing arguments that have defaults. The compiler fills in the default values automatically.

---

## Lambdas and Closures

Lambdas are anonymous functions that can capture variables from their enclosing scope. LPL uses C++-style lambda syntax with explicit capture lists.

### Basic Syntax

```
[captures](parameters) -> returnType { body }
```

The simplest lambda captures nothing and takes no arguments:

```
auto hello = []() { Console.println("hello"); };
hello();
```

### Capture Modes

The capture list `[...]` controls which variables from the enclosing scope are available inside the lambda:

| Syntax | Meaning |
|--------|---------|
| `[]` | Capture nothing |
| `[x]` | Capture `x` by value |
| `[&x]` | Capture `x` by reference |
| `[x, &y]` | Capture `x` by value and `y` by reference |
| `[=]` | Capture all referenced variables by value |
| `[&]` | Capture all referenced variables by reference |

```
int factor = 10;

auto byVal = [factor](int x) -> int { return x * factor; };
auto byRef = [&factor](int x) -> int { return x * factor; };
auto allVal = [=](int x) -> int { return x * factor; };
auto allRef = [&](int x) -> int { return x * factor; };
```

With `[=]` or `[&]`, you don't need to name the captured variables — the compiler automatically captures any referenced variable from the enclosing scope.

### Return Type Deduction

If you omit the `-> returnType`, the compiler deduces the return type from the `return` statements:

```
auto mul = [](int x, int y) { return x * y; };   // deduced -> int
auto greet = []() { Console.println("hi"); };     // deduced -> void
```

You can still be explicit when needed:

```
auto divide = [](int a, int b) -> double {
    return (a as double) / (b as double);
};
```

### Optional Parameters

Parameters can be omitted entirely if the lambda takes no arguments:

```
auto counter = [=]{ return count; };    // no () needed
```

### Callable Types

Lambda types are expressed with the `func` keyword:

```
func(int, int) -> int           // takes two ints, returns int
func(string) -> void            // takes string, returns nothing
func() -> int                   // takes nothing, returns int
```

Use `auto` to avoid writing verbose callable types:

```
// Without auto
func(int) -> int doubler = [](int x) -> int { return x * 2; };

// With auto — preferred
auto doubler = [](int x) -> int { return x * 2; };
```

### Passing Lambdas to Functions

Functions can accept lambdas as parameters using callable types:

```
int apply(func(int) -> int fn, int value) {
    return fn(value);
}

void main() {
    auto doubler = [](int x) -> int { return x * 2; };
    int result = apply(doubler, 21);    // result = 42
}
```

### Complete Example

```
include <console.lph>;

int apply(func(int) -> int fn, int value) {
    return fn(value);
}

void main() {
    // No capture
    auto add1 = [](int x) -> int { return x + 1; };
    Console.printInt(add1(6));                  // 7

    // Value capture
    int factor = 5;
    auto scale = [factor](int x) -> int { return x * factor; };
    Console.printInt(scale(3));                 // 15

    // Capture-all by value
    int base = 20;
    auto offset = [=](int x) -> int { return x + base + factor; };
    Console.printInt(offset(5));                // 30

    // Passing lambda to function
    Console.printInt(apply(scale, 10));         // 50
}
```

---

## Classes

Classes are the primary unit of data organization.

### Declaration

```
class Person {
    string name;    // private by default
    int age;        // private by default

    public Person(string name, int age) {
        this.name = name;
        this.age = age;
    }

    public void greet() {
        Console.println("Hello, " + this.name);
    }
}
```

### Access Control

Members are **private by default**. Mark them explicitly for visibility:

| Keyword | Meaning |
|---------|---------|
| *(none)* | Private — accessible only within the class |
| `public` | Accessible from anywhere |
| `protected` | Accessible from the class and subclasses |

```
class Account {
    int balance;                    // private
    public string owner;            // public
    protected int accountNumber;    // protected
}
```

This is a deliberate design choice. Private-by-default enforces encapsulation without requiring you to write `private` on every field.

### Constructors

Constructors have the same name as the class. They receive a hidden `this` pointer automatically:

```
class Point {
    int x;
    int y;

    public Point(int x, int y) {
        this.x = x;
        this.y = y;
    }
}
```

### Destructors

Destructors are auto-generated by the compiler. They:

1. Execute user-defined destructor body (if any)
2. Destroy member fields in reverse declaration order
3. Call the parent class destructor (if any)

This mirrors C++ destruction order and guarantees deterministic cleanup.

### Static Members

Use `static` for class-level members not tied to an instance:

```
class Config {
    static int cacheSize = 64;
    public static int version = 1;

    public static void reset() {
        // ...
    }
}
```

Static members are still private by default.

### Stack vs. Heap Allocation

```
// Stack allocation — destroyed at end of scope
Person p = Person("Alex", 32);

// Heap allocation — destroyed when owner pointer goes out of scope
owner Person* h = new Person("Jordan", 40);
```

### Inheritance

LPL uses `extends` for class inheritance (not C++-style `:`):

```
class Animal {
    string name;

    public Animal(string name) {
        this.name = name;
    }

    public void speak() {
        Console.println(this.name + " makes a sound");
    }
}

class Dog extends Animal {
    public Dog(string name) {
        super(name);    // call parent constructor
    }

    public override void speak() {
        Console.println(this.name + " barks");
    }
}
```

Rules:
- Single class inheritance only
- Multiple interfaces are allowed (see [Interfaces](#interfaces))
- Use `extends` keyword, not `:`

### The `super` Keyword

Use `super` to access the parent class:

- `super(args)` — call the parent constructor (only in constructors)
- `super.method()` — call the parent's version of a method

```
class Animal {
    string name;

    public Animal(string name) {
        this.name = name;
    }

    public void speak() {
        Console.println(this.name + " makes a sound");
    }
}

class Dog extends Animal {
    public Dog(string name) {
        super(name);            // call Animal(string)
    }

    public override void speak() {
        super.speak();          // call Animal.speak()
        Console.println(this.name + " barks");
    }
}
```

### The `override` Keyword

Mark methods that override a parent method with `override`. The compiler verifies that the parent class actually defines a method with that name:

```
class Dog extends Animal {
    public override void speak() {    // OK: Animal has speak()
        Console.println("woof");
    }

    public override void fly() {      // ERROR: Animal has no fly()
        Console.println("impossible");
    }
}
```

### Abstract Classes and Methods

Use `abstract` to declare classes that cannot be instantiated and methods that must be implemented by subclasses:

```
abstract class Shape {
    public abstract int area();
    public abstract string name();
}

class Rectangle extends Shape {
    int w;
    int h;

    public Rectangle(int w, int h) {
        this.w = w;
        this.h = h;
    }

    public override int area() {
        return this.w * this.h;
    }

    public override string name() {
        return "Rectangle";
    }
}
```

Rules:
- Abstract methods have no body — they end with `;`
- Abstract methods can only exist in abstract classes
- Concrete subclasses must implement all inherited abstract methods
- Attempting to instantiate an abstract class is a compile error

---

## Interfaces

Interfaces define contracts that classes must implement:

```
interface Printable {
    void print();
}

interface Serializable {
    string serialize();
}

class Document extends Object implements Printable, Serializable {
    public void print() {
        // implementation
    }

    public string serialize() {
        return "...";
    }
}
```

Rules:
- A class can implement multiple interfaces
- Use `implements` keyword, separated by commas
- All interface methods must be implemented

---

## Generics and Templates

LPL supports generics through **monomorphization** — each instantiation of a generic class or function generates specialized code at compile time. This gives the performance of hand-written specialized code with the convenience of parametric polymorphism.

### Generic Classes

Declare a class with one or more type parameters in angle brackets:

```
class Box<T> {
    T value;

    Box(T v) {
        this.value = v;
    }

    T get() {
        return this.value;
    }
}
```

Use the class by specifying concrete type arguments:

```
auto b1 = new Box<int>(42);
Console.printInt(b1.get());    // 42

auto b2 = new Box<string>("hello");
Console.println(b2.get());    // hello

// Stack allocation works too
Box<int> b3 = Box<int>(7);
```

### Multiple Type Parameters

Generic classes can have multiple type parameters:

```
class Pair<A, B> {
    A first;
    B second;

    Pair(A a, B b) {
        this.first = a;
        this.second = b;
    }

    A getFirst()  { return this.first; }
    B getSecond() { return this.second; }
}

auto p = new Pair<int, string>(1, "one");
```

### Generic Functions

Functions can also be generic:

```
T identity<T>(T x) {
    return x;
}

int a = identity<int>(42);
string b = identity<string>("hello");
```

### How It Works

LPL uses monomorphization (like C++ templates). Each unique combination of type arguments creates a separate specialized version of the class or function at compile time:

- `Box<int>` and `Box<string>` generate two separate struct types and method implementations
- No boxing, no runtime type info, no virtual dispatch overhead
- Type checking happens on each instantiation

### Rules

- Type parameters are declared in angle brackets after the class or function name: `<T>`, `<A, B>`
- Type arguments must be specified explicitly at each use site: `Box<int>`, `identity<string>`
- Generic classes can have constructors, methods, fields, and destructors — all are specialized per instantiation
- Nested generics are supported: `Box<Box<int>>` (the `>>` is handled correctly)

---

## Error Handling

LPL provides structured exception handling with `try`, `catch`, `finally`, and `throw`.

### Try / Catch

Wrap code that may throw in a `try` block and handle exceptions with `catch`:

```
try {
    riskyOperation();
} catch (ArithmeticException e) {
    Console.println("Math error: " + e.getMessage());
} catch (IOException e) {
    Console.println("I/O error: " + e.getMessage());
}
```

Each `catch` clause specifies an exception type and a variable name. The exception variable is a pointer to the caught exception object.

### Finally

A `finally` block runs after the try/catch regardless of whether an exception was thrown:

```
try {
    process();
} catch (Exception e) {
    Console.println("error");
} finally {
    cleanup();      // always runs
}
```

You can use `finally` without any `catch`, or combine it with `catch` clauses. At least one `catch` or `finally` is required.

### Throw

Throw an exception with the `throw` keyword:

```
void divide(int a, int b) {
    if (b == 0) {
        throw ArithmeticException("division by zero");
    }
    Console.printInt(a / b);
}
```

### Complete Example

```
include <console.lph>;

void riskyWork(int x) {
    if (x == 0) {
        throw ArithmeticException("cannot be zero");
    }
    Console.printInt(100 / x);
}

void main() {
    try {
        Console.printInt(1);
        riskyWork(0);
        Console.printInt(2);        // skipped — exception thrown
    } catch (ArithmeticException e) {
        Console.println("caught: " + e.getMessage());
    } finally {
        Console.println("done");
    }
}
```

Output:

```
1
caught: cannot be zero
done
```

---

## Type Casting

LPL has two distinct casting mechanisms that map to intent:

- **`x as Type`** — Primitive casts: reinterpret or widen/narrow a value within LPL's type system
- **`Type(x)`** — Boundary conversions: construct a representation of a value in another type, typically when crossing a language boundary (into or out of C)

### The `as` Keyword

Use `as` for any conversion between built-in value types where no significant work is happening beyond reinterpreting or widening/narrowing a number:

```
int x = 42;
double d = x as double;        // 42.0
int y = 3.14 as int;           // 3 (truncated)
long big = x as long;          // widening
byte small = x as byte;        // narrowing
```

`as` is a postfix operator, so it works naturally in expressions:

```
int a = 10;
int b = 3;
double ratio = (a as double) / (b as double);  // 3.333...
int result = ratio as int;                      // 3
```

### Boundary Conversions: `Type(x)`

Use `Type(x)` when real work is happening under the hood — allocating a buffer, extracting a pointer from a struct, or converting between LPL's type system and C's:

```
extern "C" {
    char* getenv(char* name);
}

void main() {
    // string → char*: extracts the raw data pointer from an LPL string
    char* home = getenv(char*("HOME"));

    // char* → string: allocates and copies into an LPL string
    string path = string(home);
}
```

The distinction: `as` says "treat this value as another type", while `Type(x)` says "construct a representation of this value in another type". When you cross a language boundary — into or out of C — you always use `Type(x)`. When you stay within LPL's primitive type system, you always use `as`.

### Supported Conversions

**Primitive casts (`as`):**

| From | To | Effect |
|------|----|--------|
| `int` | `double` | Exact conversion |
| `double` | `int` | Truncation toward zero |
| `int` | `float` | May lose precision |
| `float` | `int` | Truncation toward zero |
| `int` | `long` | Widening |
| `long` | `int` | Narrowing |
| `int` | `byte` | Narrowing |
| `float` | `double` | Widening |
| `double` | `float` | Narrowing |

**Boundary conversions (`Type(x)`):**

| Conversion | Syntax | What Happens |
|-----------|--------|-------------|
| `char*` → `string` | `string(ptr)` | Calls `strlen`, allocates, copies data |
| `string` → `char*` | `char*(str)` | Extracts raw data pointer from LPL string |

---

## Memory Model

LPL uses a C++-style memory model with no garbage collector. You have full control over where objects live and when they are destroyed.

### Stack Allocation

Objects declared as local variables live on the stack and are destroyed automatically when the scope ends:

```
void example() {
    Person p = Person("Alex", 32);
    p.greet();
}   // p is destroyed here automatically
```

### Heap Allocation

Use `new` to allocate objects on the heap:

```
owner Person* p = new Person("Alex", 32);
p.greet();
delete p;   // explicit destruction
```

Or let RAII handle it — see [Ownership and RAII](#ownership-and-raii).

### Pointers

```
Person local = Person("Alex", 32);
Person* ptr = &local;       // non-owning pointer (borrowing)
ptr.greet();                // access through pointer
```

Non-owning pointers (`Person*`) do not manage lifetime. The pointed-to object must outlive the pointer.

### Summary

| Allocation | Syntax | Lifetime |
|------------|--------|----------|
| Stack | `Type x = Type(...)` | End of enclosing scope |
| Heap (owned) | `owner Type* x = new Type(...)` | End of enclosing scope (auto) or `delete` |
| Heap (borrowed) | `Type* x = &something` | Programmer responsibility |

---

## Ownership and RAII

Ownership is LPL's core memory safety mechanism. It is enforced at compile time with zero runtime cost.

### The `owner` Keyword

`owner` marks a pointer as the sole owner of a heap-allocated object. When the owner goes out of scope, the object is automatically destroyed and freed:

```
void example() {
    owner Person* p = new Person("Alex", 32);
    p.greet();
}   // compiler inserts: Person.destroy(p) + free(p)
```

This is conceptually similar to C++'s `std::unique_ptr<T>`, but built into the language rather than a library template.

### Rules

- Only one `owner` pointer may own a given object at a time
- Copying an `owner` pointer is a compile error
- Transferring ownership requires `move`
- If an `owner` pointer leaves scope, the compiler inserts destructor + free calls

### Ownership Transfer with `move`

```
owner Person* createPerson() {
    owner Person* p = new Person("Alex", 32);
    return move p;      // transfers ownership to caller
}

void main() {
    owner Person* p = createPerson();   // caller now owns the object
    p.greet();
}   // destroyed here
```

After `move`, the original variable is invalid. Using it is a compile error:

```
owner Person* a = new Person("Alex", 32);
owner Person* b = move a;
a.greet();  // ERROR: use after move
```

### RAII (Resource Acquisition Is Initialization)

RAII means that resources are acquired in constructors and released in destructors. Because LPL guarantees destructor calls at scope exit — on every path, including early returns — resource leaks are prevented by design:

```
void process() {
    owner File* f = new File("data.txt");

    if (!f.exists()) {
        return;         // f is still destroyed here
    }

    f.write("hello");
}   // f is destroyed here too
```

The compiler inserts cleanup code on **every** exit path — `return`, `break`, end of block.

---

## Defer

`defer` schedules a statement to execute automatically when the current scope ends. It complements RAII by providing ad-hoc cleanup for resources that don't fit neatly into a constructor/destructor pattern.

### Basic Usage

```
include <console.lph>;

void main() {
    Console.println("start");
    defer Console.println("cleanup");
    Console.println("work");
}
```

Output:

```
start
work
cleanup
```

The deferred statement runs after all other statements in the scope, just before the scope is destroyed.

### LIFO Order

Multiple defers in the same scope execute in reverse order (last-in, first-out):

```
defer Console.println("first deferred");
defer Console.println("second deferred");
defer Console.println("third deferred");
```

Output:

```
third deferred
second deferred
first deferred
```

This mirrors the natural cleanup order — resources acquired later should be released first.

### Early Return Safety

Deferred statements execute even on early returns:

```
void process(bool earlyExit) {
    Console.println("enter");
    defer Console.println("cleanup");

    if (earlyExit) {
        Console.println("leaving early");
        return;     // defer still fires
    }

    Console.println("normal path");
}
```

Calling `process(true)` outputs:

```
enter
leaving early
cleanup
```

### Mutex Pattern

The classic use case — guaranteed unlock regardless of control flow:

```
include <sync.lph>;

void criticalSection() {
    Mutex mtx = Mutex();
    mtx.lock();
    defer mtx.unlock();

    // ... any amount of complex logic, early returns, etc.
    // The mutex is ALWAYS unlocked when this scope ends.
}
```

### When to Use Defer vs. RAII

| Situation | Use |
|-----------|-----|
| Object with constructor/destructor | RAII (automatic) |
| Action pair that isn't a class (lock/unlock, begin/end) | `defer` |
| One-off cleanup for a specific resource | `defer` |
| Cleanup that depends on runtime conditions | `defer` |

`defer` and RAII are complementary. RAII handles the common case automatically; `defer` fills the gaps.

---

## Namespaces

Namespaces organize code into hierarchical groups, preventing name collisions in larger codebases. LPL uses dot syntax for namespaces (not `::` like C++).

### Block-Level Namespaces

Wrap declarations in a namespace block:

```
namespace App.Models {
    class User {
        string name;
        public User(string name) {
            this.name = name;
        }
    }
}

namespace App.Models {
    class Post {
        string title;
        public Post(string title) {
            this.title = title;
        }
    }
}
```

### File-Level Namespaces

Apply a namespace to an entire file with a trailing semicolon:

```
namespace App.Models;

class User {
    // automatically in App.Models namespace
}

class Post {
    // also in App.Models namespace
}
```

### Qualified Access

Use the full dotted path to refer to namespaced types:

```
void main() {
    App.Models.User u = App.Models.User("Alex");
    App.Models.Post p = App.Models.Post("Hello");
}
```

### Complete Example

```
include <console.lph>;

namespace App.Models {
    class Greeter {
        string prefix;

        public Greeter(string prefix) {
            this.prefix = prefix;
        }

        public void greet(string name) {
            Console.println(this.prefix + ", " + name + "!");
        }
    }
}

void main() {
    App.Models.Greeter g = App.Models.Greeter("Hello");
    g.greet("World");
}
```

Output: `Hello, World!`

---

## Module System

LPL uses a header-based module system. Headers (`.lph` files) declare public interfaces; source files (`.lpl` files) provide implementations.

### Including Headers

```
// System include — searches the stdlib directory
include <console.lph>;

// Local include — searches relative to the source file
include "mylib.lph";
```

### Key Properties

- **Semantic, not textual.** `include` is a compiler directive, not a text paste. The compiler parses the header and merges its declarations into the program.
- **Duplicate-safe.** Including the same header twice is harmless — the compiler deduplicates automatically.
- **No include guards needed.** Unlike C/C++, there is no `#pragma once` or `#ifndef` boilerplate.
- **No macro pollution.** Headers cannot redefine symbols or inject macros.

### Writing a Header

A `.lph` file declares the public API of a module. Methods have empty bodies — the actual implementation is provided by a compiled library:

```
// mylib.lph
class MyClass {
    public MyClass() {}
    public void doWork() {}
    public int compute(int x) {}
}
```

### Standard Library Headers

LPL ships the following standard library headers:

| Header | Description |
|--------|-------------|
| `<console.lph>` | Console I/O (`print`, `println`, `printInt`, `printFloat`) |
| `<string.lph>` | String utilities (`length`, `substring`, `indexOf`, `toUpper`, ...) |
| `<math.lph>` | Math functions (`sqrt`, `pow`, `sin`, `cos`, `abs`, ...) |
| `<files.lph>` | File I/O (`read`, `write`, `exists`, `readAll`, ...) |
| `<system.lph>` | OS operations (`exit`, `getenv`, `sleep`, `currentTimeMillis`, ...) |
| `<collections.lph>` | Generic collections (`List<T>`, `Map<K, V>`) |
| `<sync.lph>` | Synchronization (`Mutex`) |

### Selective Linking

The compiler only links the runtime libraries for modules you actually include. If you only use `<console.lph>`, only the console runtime is linked. This keeps binaries small.

---

## C Interop

LPL provides direct interoperability with C through `extern "C"` blocks. Any function declared in an `extern "C"` block uses C calling conventions and links by its exact unmangled name, giving LPL seamless access to the entire C ecosystem — system calls, POSIX APIs, legacy libraries, and platform SDKs.

### Declaring C Functions

```
extern "C" {
    int printf(char* fmt, ...);
    void* malloc(long size);
    void free(void* ptr);
    int open(char* path, int flags);
    long read(int fd, void* buf, long count);
    int close(int fd);
}
```

Functions in `extern "C"` blocks:
- Use C calling conventions (cdecl)
- Have **no name mangling** — the function name is the exact linker symbol
- Support variadic arguments (`...`)
- Can accept and return all LPL-compatible types including pointers

### Calling C Functions

Once declared, extern functions are called like any other LPL function. When a C function expects `char*`, use the `char*()` boundary conversion to convert LPL strings:

```
include <console>

extern "C" {
    int abs(int x);
    double sqrt(double x);
    int rand();
    void srand(int seed);
}

void main() {
    srand(42);
    int r = rand();
    Console.printInt(abs(-99));
    Console.println("");
    Console.printFloat(sqrt(144.0));
    Console.println("");
}
```

### Working with C Strings

C functions use `char*` instead of LPL's `string` type. Use boundary conversions to cross the boundary:

- `char*(str)` — extract the raw data pointer from an LPL string
- `string(ptr)` — allocate an LPL string from a C `char*`

```
extern "C" {
    char* getenv(char* name);
}

void main() {
    // char*("HOME") converts the LPL string literal to a char*
    char* home = getenv(char*("HOME"));

    // string(home) creates an LPL string from the returned char*
    string path = string(home);
    Console.println(path);
}
```

> **Note:** LPL does not implicitly convert between `string` and `char*`. All conversions must be explicit using boundary conversion syntax. This makes C interop boundaries visible in the code.

### Wrapping C Libraries

A common pattern is to wrap C functions in an LPL class for a cleaner API:

```
extern "C" {
    void* fopen(char* path, char* mode);
    int fclose(void* stream);
    int fputs(char* str, void* stream);
    char* fgets(char* buf, int size, void* stream);
}

class CFile {
    void* handle;

    public CFile(string path, string mode) {
        this.handle = fopen(char*(path), char*(mode));
    }

    public void write(string text) {
        fputs(char*(text), this.handle);
    }

    public void close() {
        fclose(this.handle);
    }
}
```

### Callback Functions

LPL lambdas and function pointers are compatible with C function pointer parameters:

```
extern "C" {
    void qsort(void* base, long nmemb, long size,
               func(void*, void*) -> int compar);
}
```

### The `as` Clause

The `as` clause maps a convenient LPL-side name to a different linker symbol. This is useful when the C symbol name would be awkward or conflicts with an LPL keyword:

```
extern "C" {
    // Call as exitProcess() in LPL, links to the C symbol "exit"
    void exitProcess(int code) as "exit";

    // Rename for clarity
    void platformSleep(int ms) as "usleep";
}

void main() {
    exitProcess(0);
}
```

### Linking with C Libraries

The standard C library (`-lc`) is always linked automatically. For third-party C libraries, you have two options:

**Option 1:** Let `lplc` handle linking and provide library paths:

```sh
# If the library is in a standard location
lplc app.lpl -o app
```

**Option 2:** Compile to object file and link manually for full control:

```sh
lplc app.lpl -c -o app.o
cc app.o -o app -lsqlite3 -lcurl -llplrt -lc
```

### Complete Example: POSIX File I/O

```
include <console>

extern "C" {
    int open(char* path, int flags);
    long write(int fd, char* buf, long count);
    long read(int fd, char* buf, long count);
    int close(int fd);
}

void main() {
    // O_WRONLY | O_CREAT | O_TRUNC = 577 on macOS
    int fd = open(char*("/tmp/lpl_test.txt"), 577);
    if (fd >= 0) {
        write(fd, char*("Hello from LPL!"), 15);
        close(fd);
        Console.println("File written");
    }
}
```

---

## C++ Interop

LPL supports calling C++ functions through `extern "C++"` blocks. Because C++ compilers apply **name mangling** to encode function signatures into symbol names, you must provide the mangled symbol name using the `as` clause.

### Why Name Mangling?

C++ supports function overloading — multiple functions with the same name but different parameter types. The compiler encodes the parameter types into the symbol name to distinguish them:

| C++ Function | Mangled Symbol |
|-------------|----------------|
| `int add(int, int)` | `_Z3addii` |
| `double add(double, double)` | `_Z3adddd` |
| `void print(const char*)` | `_Z5printPKc` |

LPL needs the mangled name to link against the correct overload.

### Declaring C++ Functions

```
extern "C++" {
    double computeArea(double radius) as "_Z11computeAread";
    int addInts(int a, int b) as "_Z7addIntsii";
}
```

Each declaration takes:
- A normal LPL-style signature — the name you use to **call** the function
- An `as "..."` clause — the **mangled symbol** as it appears in the C++ object file

### Finding Mangled Names

Use `nm` to list symbols in compiled C++ object files or libraries:

```sh
# List all defined symbols
nm -j mylib.o

# Search for a specific function name
nm -j mylib.o | grep computeArea
# Output: _Z11computeAread
```

Use `c++filt` to verify a mangled name maps to the expected signature:

```sh
echo "_Z11computeAread" | c++filt
# Output: computeArea(double)

echo "_Z7addIntsii" | c++filt
# Output: addInts(int, int)
```

On macOS, `nm` output includes an extra leading underscore (e.g., `__Z11computeAread`). Strip the first underscore — use `_Z11computeAread` in your `as` clause.

### Complete Example

Given this C++ library:

```cpp
// geometry.cpp
#include <cmath>

double circleArea(double radius) {
    return M_PI * radius * radius;
}

double distance(double x1, double y1, double x2, double y2) {
    return std::sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
}
```

Compile and check symbols:

```sh
c++ -c geometry.cpp -o geometry.o
nm -j geometry.o | c++filt
# circleArea(double)
# distance(double, double, double, double)

nm -j geometry.o
# _Z10circleAread
# _Z8distancedddd
```

Use from LPL:

```
include <console>

extern "C++" {
    double circleArea(double radius) as "_Z10circleAread";
    double distance(double x1, double y1, double x2, double y2) as "_Z8distancedddd";
}

void main() {
    Console.printFloat(circleArea(5.0));
    Console.println("");

    Console.printFloat(distance(0.0, 0.0, 3.0, 4.0));
    Console.println("");
}
```

Compile and link:

```sh
lplc app.lpl -c -o app.o
c++ app.o geometry.o -o app -llplrt -llpl_console -lc
./app
# 78.5398
# 5
```

### The Recommended Approach: C Wrappers

If you **control the C++ source code**, the easiest and most maintainable approach is to export functions with C linkage from the C++ side. This avoids name mangling entirely:

```cpp
// mathutils.cpp — C++ implementation with C-linkage exports
#include <cmath>
#include <algorithm>

extern "C" {
    double circle_area(double radius) {
        return M_PI * radius * radius;
    }

    double clamp_value(double val, double lo, double hi) {
        return std::clamp(val, lo, hi);
    }

    int* sort_array(int* arr, int len) {
        std::sort(arr, arr + len);
        return arr;
    }
}
```

```
// app.lpl — much simpler, no mangled names
extern "C" {
    double circle_area(double radius);
    double clamp_value(double val, double lo, double hi);
}

void main() {
    Console.printFloat(circle_area(10.0));
}
```

This is **recommended** because:
- No fragile mangled names that break across compiler versions
- Cleaner LPL declarations
- Works with both `cc` and `c++` linkers
- The C++ code still has full access to C++ features internally

Use `extern "C++"` for third-party C++ libraries where you cannot modify the source.

### Calling C++ Class Methods

C++ non-static methods receive a hidden `this` pointer as the first argument. You can call them by passing the object pointer explicitly:

```cpp
// In C++ (point.cpp)
class Point {
public:
    double x, y;
    Point(double x, double y) : x(x), y(y) {}
    double magnitude() { return std::sqrt(x*x + y*y); }
};

// C wrapper is cleaner for classes
extern "C" {
    void* Point_new(double x, double y) {
        return new Point(x, y);
    }
    double Point_magnitude(void* self) {
        return static_cast<Point*>(self)->magnitude();
    }
    void Point_delete(void* self) {
        delete static_cast<Point*>(self);
    }
}
```

```
// In LPL
extern "C" {
    void* Point_new(double x, double y);
    double Point_magnitude(void* self);
    void Point_delete(void* self);
}

void main() {
    void* pt = Point_new(3.0, 4.0);
    Console.printFloat(Point_magnitude(pt));    // 5
    Console.println("");
    Point_delete(pt);
}
```

### Linking

When any `extern "C++"` block is present, the compiler automatically adds `-lc++` to the link command. For `extern "C"`, only the C standard library is linked.

For custom libraries, compile to an object file and link manually:

```sh
# Compile LPL code
lplc app.lpl -c -o app.o

# Link with C++ library (use c++ driver for C++ standard library)
c++ app.o -o app -L/path/to/libs -lmylib -llplrt -lc

# Or with a static C++ object file directly
c++ app.o mylib.o -o app -llplrt -lc
```

### When to Use Each Approach

| Scenario | Approach |
|----------|----------|
| Calling C libraries (libc, SQLite, curl, etc.) | `extern "C"` |
| Calling C++ code you control | C wrapper with `extern "C"` in C++ |
| Calling third-party C++ libraries you can't modify | `extern "C++"` with `as` |
| Platform-specific system calls | `extern "C"` |
| C++ template libraries (STL, Eigen, etc.) | C wrapper (templates can't be called directly) |

---

## Platform Targeting

> **Note:** Platform targeting attributes are currently parsed by the compiler but not yet evaluated. The syntax is reserved for a future release.

LPL will use attribute annotations instead of C-style `#ifdef` preprocessor directives for platform-specific code:

```
@os(macos)
void initPlatform() {
    // macOS-specific initialization
}

@os(linux)
void initPlatform() {
    // Linux-specific initialization
}

@os(windows)
void initPlatform() {
    // Windows-specific initialization
}
```

### Planned Attributes

| Attribute | Values |
|-----------|--------|
| `@os(...)` | `windows`, `linux`, `macos` |
| `@arch(...)` | `x86_64`, `arm64` |

Attributes can be combined — both must match:

```
@os(linux) @arch(arm64)
void armLinuxInit() {
    // only compiled for ARM64 Linux
}
```

---

## Standard Library

### Console (`<console.lph>`)

Terminal output:

```
include <console.lph>;

Console.print("no newline");
Console.println("with newline");
Console.printInt(42);
Console.printFloat(3.14);
```

### Strings (`<string.lph>`)

String utility functions:

```
include <string.lph>;

int len = Strings.length("hello");              // 5
string upper = Strings.toUpper("hello");        // "HELLO"
string lower = Strings.toLower("HELLO");        // "hello"
string trimmed = Strings.trim("  hi  ");        // "hi"
bool has = Strings.contains("hello", "ell");    // true
int idx = Strings.indexOf("hello", "ll");       // 2
string sub = Strings.substring("hello", 1, 4);  // "ell"
string ch = Strings.charAt("hello", 0);         // "h"
bool sw = Strings.startsWith("hello", "he");    // true
bool ew = Strings.endsWith("hello", "lo");      // true
string rep = Strings.replace("hello", "l", "r"); // "herro"

// Conversions
string numStr = Strings.fromInt(42);            // "42"
string fltStr = Strings.fromFloat(3.14);        // "3.140000"
string boolStr = Strings.fromBool(true);        // "true"
int num = Strings.toInt("42");                  // 42
double flt = Strings.toFloat("3.14");           // 3.14
```

### Math (`<math.lph>`)

Mathematical functions:

```
include <math.lph>;

// Basic operations
double a = Math.abs(-5.0);          // 5.0
int ai = Math.absInt(-5);           // 5
double root = Math.sqrt(144.0);     // 12.0
double power = Math.pow(2.0, 10.0); // 1024.0

// Trigonometry
double s = Math.sin(0.0);           // 0.0
double c = Math.cos(0.0);           // 1.0
double t = Math.tan(0.0);           // 0.0

// Logarithms
double ln = Math.log(2.718);        // ~1.0
double lg = Math.log10(100.0);      // 2.0

// Rounding
double fl = Math.floor(3.7);        // 3.0
double cl = Math.ceil(3.2);         // 4.0
double rd = Math.round(3.5);        // 4.0

// Min / Max
double mn = Math.min(3.0, 7.0);     // 3.0
double mx = Math.max(3.0, 7.0);     // 7.0
int mi = Math.minInt(3, 7);         // 3
int ma = Math.maxInt(3, 7);         // 7

// Constants and random
double pi = Math.PI();              // 3.14159...
double e = Math.E();                // 2.71828...
double rn = Math.random();          // [0.0, 1.0)
```

### File I/O (`<files.lph>`)

File operations with both instance and static methods:

```
include <files.lph>;

// Instance methods — use with a File object
File f = File("data.txt");
f.write("hello");
f.append(" world");
string content = f.read();
bool exists = f.exists();
long sz = f.size();
f.close();

// Static convenience methods
string all = File.readAll("data.txt");
File.writeAll("out.txt", "content");
bool e = File.fileExists("data.txt");
File.remove("temp.txt");
```

### System (`<system.lph>`)

OS-level operations:

```
include <system.lph>;

System.exit(0);                                 // terminate process
int ac = System.argc();                         // argument count
string arg = System.argv(0);                    // get argument by index
string home = System.getenv("HOME");            // environment variable
long now = System.currentTimeMillis();          // epoch milliseconds
System.sleep(1000);                             // sleep 1 second
```

### Collections (`<collections.lph>`)

Generic, type-safe dynamic arrays and hash maps with RAII cleanup:

```
include <collections.lph>;

// List<int> — dynamic array of ints
List<int> nums = List<int>();
nums.add(42);
nums.add(99);
int val = nums.get(0);          // 42
int sz = nums.size();           // 2
nums.set(0, 100);
nums.removeAt(1);
// destroyed automatically at scope exit (RAII)

// List<string> — dynamic array of strings
List<string> words = List<string>();
words.add("hello");
words.add("world");
string w = words.get(0);       // "hello"

// Map<string, int> — hash map with string keys
Map<string, int> ages = Map<string, int>();
ages.put("alice", 30);
int a = ages.get("alice");     // 30
bool has = ages.containsKey("alice");  // true
ages.remove("alice");

// Map<string, string>
Map<string, string> dict = Map<string, string>();
dict.put("color", "blue");
string c = dict.get("color");  // "blue"
int ds = dict.size();          // 1
```

Supported instantiations: `List<int>`, `List<string>`, `Map<string, int>`, `Map<string, string>`.

### Synchronization (`<sync.lph>`)

Thread synchronization primitives:

```
include <sync.lph>;

Mutex mtx = Mutex();

mtx.lock();
defer mtx.unlock();

// Critical section — mutex is always unlocked at scope end

bool acquired = mtx.tryLock();  // non-blocking attempt
if (acquired) {
    defer mtx.unlock();
    // ...
}
```

---

## Compiler Usage

### Basic Commands

```sh
# Compile to executable
lplc source.lpl -o myapp

# Compile with debug info
lplc source.lpl -o myapp -g

# Compile to object file only
lplc source.lpl -c -o source.o

# Emit LLVM IR
lplc source.lpl -emit-llvm -o source.ll

# Emit native assembly
lplc source.lpl -S -o source.s
```

### Compiler Flags

| Flag | Description |
|------|-------------|
| `-o <name>` | Output file name |
| `-c`, `-emit-obj` | Compile only, produce object file |
| `-emit-llvm` | Output LLVM IR instead of object code |
| `-S` | Output native assembly |
| `-g` | Emit debug information |
| `-O0` to `-O3` | Optimization level |
| `-target <triple>` | LLVM target triple for cross-compilation |
| `--dump-tokens` | Print lexer token stream (debug) |
| `--dump-ast` | Print parsed AST (debug) |
| `--check` | Parse and type-check only; output diagnostics as JSON |

### Debug vs. Release

```sh
# Debug build — no optimization, full debug info
lplc app.lpl -o app_debug -g -O0

# Release build — full optimization
lplc app.lpl -o app_release -O2
```

---

## Editor Support

LPL ships with a **Language Server Protocol (LSP)** implementation and editor plugins for **VS Code** and **Zed**. The LSP server provides real-time diagnostics, hover documentation, code completion, and signature help for any editor that supports the protocol.

### Language Server (LSP)

The LPL language server lives in `editors/lpl-lsp/`. It is a Node.js server built on the `vscode-languageserver` library.

#### Building

```sh
cd editors/lpl-lsp
npm install
npm run build
```

This produces the server entry point at `editors/lpl-lsp/out/server.js`.

#### Features

| Feature | Description |
|---------|-------------|
| **Diagnostics** | Runs `lplc --check` on every file change (debounced) and reports errors inline. |
| **Hover** | Shows documentation for standard library classes/methods, keywords, and user-defined symbols. |
| **Completion** | Keywords, stdlib methods (triggered by `.`), include headers (triggered by `<`), snippets, and symbols from the current file. |
| **Signature Help** | Parameter hints for stdlib method calls (triggered by `(` and `,`). |

#### Compiler Discovery

The server locates the `lplc` compiler automatically using the following search order:

1. The `LPL_COMPILER` environment variable (absolute path to the binary).
2. A `build/src/lplc` path relative to the server's location (works in the source tree).
3. `lplc` on the system `PATH`.

If the compiler cannot be found, diagnostics are disabled but all other features still work. To set the compiler path explicitly:

```sh
export LPL_COMPILER=/path/to/lplc
```

#### Running Standalone

The server communicates over **stdio** by default, so any editor can launch it:

```sh
node editors/lpl-lsp/out/server.js --stdio
```

### VS Code

The VS Code extension is at `editors/vscode/lpl-lang/`. It provides syntax highlighting via a TextMate grammar and connects to the bundled LSP server for all smart features.

#### Installing

```sh
# Build the extension
cd editors/vscode/lpl-lang
npm install
npm run compile

# Package into a .vsix
npx @vscode/vsce package --allow-missing-repository

# Install the .vsix in VS Code
code --install-extension lpl-syntax-0.1.0.vsix
```

Once installed, open any `.lpl` or `.lph` file to activate the extension. The language server starts automatically.

#### What You Get

- **Syntax highlighting** for all LPL keywords, types, strings, comments, and operators.
- **Real-time error underlining** as you type (powered by `lplc --check`).
- **Hover docs** — hover over `Console.println` to see its signature and description.
- **Autocomplete** — type `Console.` and see all available methods with documentation.
- **Signature help** — inside `Strings.substring(`, see parameter names and types.
- **Snippets** — type `for`, `class`, `trycatch`, `owner`, etc. and expand with Tab.

### Zed

Zed support is configured via the Zed settings file. The LPL language server runs as an external binary.

#### Setup

1. Build the LSP server (see above).

2. Add the following to your Zed settings (`~/.config/zed/settings.json`):

```json
{
  "languages": {
    "LPL": {
      "tab_size": 4
    }
  },
  "lsp": {
    "lpl-lsp": {
      "binary": {
        "path": "node",
        "arguments": ["/absolute/path/to/editors/lpl-lsp/out/server.js", "--stdio"]
      }
    }
  },
  "language_overrides": {
    "LPL": {
      "language_servers": ["lpl-lsp"]
    }
  },
  "file_types": {
    "LPL": ["lpl", "lph"]
  }
}
```

Replace `/absolute/path/to/` with the actual path to your LPL project.

3. Restart Zed. Open a `.lpl` file to see diagnostics, hover, and completion.

### Other Editors

Any editor with LSP support (Neovim, Helix, Sublime Text, Emacs, etc.) can use the LPL language server. The general pattern is:

1. Build the server: `cd editors/lpl-lsp && npm install && npm run build`
2. Configure your editor to launch: `node /path/to/editors/lpl-lsp/out/server.js --stdio`
3. Associate it with `.lpl` and `.lph` file types.

For example, in **Neovim** with `nvim-lspconfig`:

```lua
vim.api.nvim_create_autocmd('FileType', {
  pattern = 'lpl',
  callback = function()
    vim.lsp.start({
      name = 'lpl-lsp',
      cmd = { 'node', '/path/to/editors/lpl-lsp/out/server.js', '--stdio' },
    })
  end,
})
```

---

## Best Practices

### Prefer Stack Allocation

Stack-allocated objects are faster (no `malloc`/`free` overhead) and have guaranteed cleanup:

```
// Good — stack allocated, auto-destroyed
Person p = Person("Alex", 32);

// Use heap only when needed (polymorphism, outlives scope, large objects)
owner Person* h = new Person("Jordan", 40);
```

### Always Use `owner` for Heap Pointers

If you allocate with `new`, the result should almost always be stored in an `owner` pointer. This ensures automatic cleanup:

```
// Good — ownership is clear, cleanup is automatic
owner Person* p = new Person("Alex", 32);

// Dangerous — manual management, easy to leak
Person* p = new Person("Alex", 32);
delete p;   // easy to forget, especially with early returns
```

### Use `defer` for Non-RAII Cleanup

When working with resources that use explicit acquire/release pairs (not constructor/destructor), use `defer` immediately after acquisition:

```
mtx.lock();
defer mtx.unlock();     // guarantee: always unlocked

// ... complex logic with early returns ...
```

The pattern is always: **acquire → defer release → use**.

### Keep Classes Focused

Each class should have a single, clear responsibility. Take advantage of private-by-default to expose only what's necessary:

```
class Logger {
    string prefix;          // private — implementation detail

    public Logger(string prefix) {
        this.prefix = prefix;
    }

    public void log(string message) {
        Console.println(this.prefix + ": " + message);
    }
}
```

### Use Namespaces for Organization

In larger codebases, group related classes under namespaces:

```
namespace App.Models {
    class User { ... }
    class Post { ... }
}

namespace App.Services {
    class AuthService { ... }
    class PostService { ... }
}
```

### Prefer Explicit Types — or `auto` When Obvious

LPL supports both explicit types and `auto` inference. Use whichever communicates intent more clearly:

```
// Explicit when the type matters for understanding
int count = 0;
string name = "Alex";
App.Models.User user = App.Models.User();

// auto when the type is obvious from context
auto x = 42;
auto add = [](int a, int b) -> int { return a + b; };
auto result = compute(data);
```

`auto` is especially valuable for lambdas where the full callable type is verbose.

### Handle All Exit Paths

With RAII and `defer`, all exit paths are covered. But when using raw pointers, be conscious of every `return`:

```
// With owner — safe
void process() {
    owner File* f = new File("data.txt");
    if (!f.exists()) return;    // f is auto-cleaned
    f.write("data");
}   // f is auto-cleaned

// With defer — safe
void critical() {
    mtx.lock();
    defer mtx.unlock();
    if (done) return;           // unlock still happens
    doWork();
}   // unlock still happens
```

---

## Design Principles

### Why Not Garbage Collection?

LPL is designed for applications where you need predictable performance — games, system tools, real-time applications, embedded systems. Garbage collectors introduce unpredictable pauses and memory overhead. LPL's compile-time ownership analysis provides memory safety without runtime cost.

### Why `extends` Instead of `:`?

C++'s `:` syntax for inheritance is overloaded and visually unclear:

```cpp
// C++ — what does the colon mean here?
class Dog : public Animal, protected Mammal, private Owned { }
```

LPL chooses clarity:

```
// LPL — intent is obvious
class Dog extends Animal implements Pet, Trainable { }
```

### Why Private by Default?

In practice, most class members should be private. Making `private` the default:

1. Reduces boilerplate (no `private:` blocks everywhere)
2. Encourages encapsulation by default
3. Makes public surfaces explicit and intentional

### Why `owner` Instead of Smart Pointers?

C++'s `std::unique_ptr<T>` is a library-level workaround for a language-level problem. LPL makes ownership a first-class language concept:

```cpp
// C++ — library template, verbose
std::unique_ptr<Person> p = std::make_unique<Person>("Alex", 32);

// LPL — language keyword, clear
owner Person* p = new Person("Alex", 32);
```

The compiler enforces the same rules (single ownership, move semantics) but the syntax is cleaner and the intent is more obvious.

### Why `defer`?

RAII handles the common case beautifully — objects clean up after themselves. But not every resource fits the constructor/destructor pattern. `defer` fills the gap with minimal syntax:

```
resource.acquire();
defer resource.release();
```

This is a proven pattern from Go, Swift, and Zig. It's simple, composable, and eliminates an entire class of cleanup bugs.

### Why `@os()` Instead of `#ifdef`?

Preprocessor directives operate on text, not on the AST. This leads to:

- Syntax errors hidden in inactive `#ifdef` branches
- Include order dependencies
- Macro pollution across translation units
- Impossible-to-read nested `#if` / `#elif` chains

LPL's `@os()` attributes operate on the parsed AST. The compiler always validates syntax on all branches, and non-matching branches are stripped cleanly before semantic analysis. No macros, no text substitution, no surprises.

---

*LPL — C++ power, Java readability, less noise, more clarity.*
