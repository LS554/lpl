# LPL

A modern, compiled, statically-typed language that combines C++ performance and memory control with Java's clean class syntax. Compiles to native code via LLVM — no garbage collector, no VM.

## Features

- **Ownership & RAII** — `owner` pointers with compile-time lifetime enforcement, automatic destructor calls
- **Defer** — scope-based cleanup for resources that don't fit RAII
- **Generics** — monomorphized templates with zero runtime cost
- **Lambdas & Closures** — C++-style capture lists with value and reference capture
- **Classes & Interfaces** — single inheritance, multiple interfaces, abstract classes, operator overloading
- **C Interop** — `extern "C"` blocks for direct access to C libraries
- **Standard Library** — console, strings, math, file I/O, collections (`List<T>`, `Map<K,V>`), synchronization

## Quick Start

```
include <console>

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
    Person p = Person("Alex", 32);
    p.greet();

    owner Person* h = new Person("Jordan", 40);
    h.greet();
}
```

```sh
lplc hello.lpl -o hello
./hello
```

## Building

Requires LLVM and CMake.

Using script:

```sh
bash build.bash
```

Or manually:

```sh
mkdir build && cd build
cmake ..
cmake --build .
```

This produces the `lplc` compiler and pre-compiled standard library.

## Documentation

See [MANUAL.md](MANUAL.md) for the full language reference — type system, memory model, ownership, generics, standard library API, and more.

## License

Apache 2.0 — see [LICENSE](LICENSE). Compiled output and linked runtime code are not subject to the license (runtime library exception).
