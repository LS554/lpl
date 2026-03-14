# LPL Runtime C Function Guide

Reference for writing C functions that back LPL stdlib modules.

## Struct Return (sret) Convention

When an LPL function returns a struct type (e.g. `string`), the compiler does **not** return it directly. Instead, it uses the **sret (struct return)** convention:

1. The return type becomes `void`
2. A hidden pointer parameter is inserted as the **first argument**
3. The callee writes the result into that pointer
4. The caller allocates stack space and reads from it after the call

### How it works

Given this `.lph` declaration:

```lpl
class File {
    public string read() {}
}
```

The compiler sees that `read()` returns `string` (an `LPLString` struct), so it rewrites the signature internally. The C implementation must match the rewritten form:

```c
void __lpl_files_read_file(LPLString* result, LPLString* path) {
    __lpl_string_create(result, "hello", 5);
}
```

The first `LPLString*` parameter is the hidden output — the compiler inserts it automatically. The function is `void` because the result is written through the pointer, not returned.

This would be **wrong**:

```c
// WRONG - do NOT return LPLString directly
LPLString __lpl_files_read_file(LPLString* path) { ... }
```

### When does sret apply?

sret is used when the return type maps to an LLVM struct. In practice:

| LPL type   | C type      | Uses sret? |
|------------|-------------|------------|
| `string`   | `LPLString` | Yes        |
| `int`      | `int32_t`   | No         |
| `long`     | `int64_t`   | No         |
| `bool`     | `int8_t`    | No         |
| `byte`     | `int8_t`    | No         |
| `float`    | `float`     | No         |
| `double`   | `double`    | No         |
| `void`     | `void`      | No         |
| Class type | struct      | Yes        |

### Parameter order with sret

For a function declared in LPL as returning `string`:

```lpl
class Strings {
    public static string getName(int id, string prefix) {}
}
```

The C signature becomes:

```c
void __lpl_strings_get_name(LPLString* result, int32_t id, LPLString* prefix);
//                           ^result (sret)     ^id         ^prefix (pointer because struct)
```

Note: struct parameters (like `string`) are also passed by pointer, not by value.

### What the caller sees

LPL code writes a normal-looking assignment:

```lpl
File f = File("README.md");
string content = f.read();
```

The compiler transforms this behind the scenes — the LPL programmer never sees the sret mechanism. The `string` return looks and behaves like a normal return value.

## Writing a new runtime function

### Step 1: Write the C function

In `runtime/modules/lpl_<module>.c`:

```c
// Returns string -> void + LPLString* first param
void __lpl_mymod_get_name(LPLString* result, LPLString* input) {
    // ... compute ...
    __lpl_string_create(result, buf, len);
}

// Returns primitive -> normal return
int64_t __lpl_mymod_get_count(LPLString* path) {
    // ...
    return count;
}

// Returns void -> normal void
void __lpl_mymod_do_thing(LPLString* path) {
    // ...
}
```

### Step 2: Declare in the .lpl stdlib file

The `.lpl` file contains both the extern declarations and the class wrapper:

```lpl
extern "C" {
    string __lpl_mymod_get_name(string input);
    long __lpl_mymod_get_count(string path);
    void __lpl_mymod_do_thing(string path);
}

class MyMod {
    public static string getName(string input) {
        return __lpl_mymod_get_name(input);
    }

    public static long getCount(string path) {
        return __lpl_mymod_get_count(path);
    }

    public static void doThing(string path) {
        __lpl_mymod_do_thing(path);
    }
}
```

### Step 3: Write the .lph header

The `.lph` file declares the public API without implementation:

```lpl
class MyMod {
    public static string getName(string input) {}
    public static long getCount(string path) {}
    public static void doThing(string path) {}
}
```

See `stdlib/files.lph` and `stdlib/files.lpl` for a complete real-world example.

## Common mistakes

1. **Declaring C function as returning `LPLString`** — causes argument count mismatch because the compiler already inserts the sret pointer
2. **Forgetting that `string` params are pointers** — in C, `string` becomes `LPLString*`, not `LPLString`
3. **Wrong parameter order** — sret pointer is always first, before any other params
