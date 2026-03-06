# ⚗️ Chim Programming Language

> *"Why use normal keywords when you can use the periodic table?"*

Chim is an esoteric programming language where every keyword is a chemical element symbol. It transpiles to C and compiles via `gcc`.

**This is not a serious language. Seriously.**

---

## 📦 Installation

### Requirements
- `gcc` in PATH ([MinGW-w64](https://www.mingw-w64.org/) on Windows, `gcc` on Linux/Mac)

### Build the transpiler

```bash
gcc chim.c -o chimc
```

That's it. You now have `chimc` — the Chim transpiler.

---

## 🚀 Usage

```bash
./chimc input.chim output
```

This will:
1. Read `input.chim`
2. Transpile it to `output.c`
3. Compile `output.c` via `gcc` → `output` / `output.exe`

---

## 🧪 Hello World

```
Pr "Hello, World!".
Es.
```

---

## 📖 Language Reference

### Statement terminator
Every statement ends with `.` (dot).

```
Pr "Hello".
I(x) 42.
```

### Comments
```
// This is a comment
```

---

### Types

| Symbol | Element  | C type   | Example                  |
|--------|----------|----------|--------------------------|
| `S`    | Sulfur   | `char[]` | `S(name) "Alice".`       |
| `I`    | Iodine   | `int`    | `I(x) 42.`               |
| `F`    | Fluorine | `double` | `F(pi) 3_14159.`         |
| `C`    | Carbon   | `char`   | `C(grade) 'A'.`          |

> **Note:** Floats use `_` as the decimal separator to avoid conflict with `.` (statement terminator). So `3.14` is written as `3_14`.

---

### Print — `Pr`

```
Pr "Hello, World!".
Pr x.
Pr "Hello, {name}!".
```

Supports **string interpolation** with `{varname}`:
```
S(name) "Alice".
I(age) 30.
Pr "Name: {name}, Age: {age}".
```
Output: `Name: Alice, Age: 30`

---

### Input — `In`

```
S(name) In "What's your name?: ".
I(age)  In "Your age: ".
```

---

### Lists — `Li`

```
Li(nums)  I(1, 2, 3, 4).
Li(words) S("hello", "world").
Li(temps) F(3_6, 7_2, 99_9).
```

Access by index:
```
Pr nums(0).    // 1
Pr words(1).   // world
```

---

### Functions — `Fe`, `Ru`, `Rn`

| Symbol | Element    | Role            |
|--------|------------|-----------------|
| `Fe`   | Iron       | Define function |
| `Ru`   | Ruthenium  | Call function   |
| `Rn`   | Radon      | Return value    |

**Define:**
```
Fe(sayHi) (Pr "Hello!").
Fe(greet, name) (Pr "Hello, {name}!").
Fe(echo, text) (Rn text).
```

**Call:**
```
Ru sayHi.
Ru greet "Alice".
Ru greet ("Alice").
```

Both call styles work — with or without parentheses around arguments.

**Return:**
```
Fe(getVersion) (Rn "1.0").
```

> Functions with `Rn` return `char*`. Use `Pr` to print the result.

---

### Conditions — `Co`

```
Co(condition) (then_block) (else_block).
```

```
I(x) 5.
Co(x=5) (Pr "x is 5") (Pr "x is not 5").
```

**Multiple conditions** (AND):
```
I(a) 1.
I(b) 2.
Co(a=1,b=2) (Pr "both match") (Pr "nope").
```

**List index in condition:**
```
Li(nums) I(10, 20, 30).
Co(nums(1)=20) (Pr "nums[1] is 20") (Pr "nope").
```

---

### Error — `Er`

```
Er(TypeError) "Something went wrong!".
```

Prints to `stderr` and exits with code `1`.

---

### File operations — `Fl`

```
Fl("file.txt") create.
Fl("file.txt") write "hello".
Fl("file.txt") writeLine "hello".
Fl("file.txt") getText.
Fl("file.txt") getLines.
Fl("file.txt") clearText.
Fl("file.txt") copy "copy.txt".
Fl("file.txt") move "new.txt".
Fl("file.txt") delete.
```

---

### End program — `Es`

```
Es.
```

Exits with code `0`. Named after Einsteinium, because only Einstein would design a language like this.

---

## 🗂️ Project Structure

```
chim/
├── chim.c           # Entry point — reads .chim, calls gcc
├── chim_lexer.h     # Lexer: source text → tokens
├── chim_codegen.h   # Code buffers, variable type table
├── chim_parser.h    # Parser + C code generator
├── Makefile
└── examples/
    ├── hello.chim
    ├── vars.chim
    ├── condition.chim
    ├── functions.chim
    ├── files.chim
    └── all_fixed.chim
```

---

## 🔬 How it works

```
your_program.chim
       │
       ▼
  [chimc lexer]        tokenizes source into chemical tokens
       │
       ▼
  [chimc parser]       walks token stream, emits C code
       │
       ▼
  your_program.c       glorious generated C
       │
       ▼
     [gcc]
       │
       ▼
  your_program.exe     actually runs
```

---

## ⚠️ Known limitations

- `F` (float) uses `_` as decimal point: `3_14` = `3.14`
- `V + Ge` (dynamic variables) are intentionally unsupported — attempting to use them raises `[ChimError] Function return value can not keep in dynamic variable`
- All function parameters are `char*` — Chim does not judge your type safety
- No loops (yet)
- No recursion limit warnings (stack overflow is a Chim tradition)

---

## 📄 License

MIT License
