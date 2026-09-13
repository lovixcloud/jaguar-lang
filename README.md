# Jaguar Programming Language Compiler

A statically-typed, live-reloading scripting language compiler implemented in C.

## Features

- **Static typing** with explicit type annotations
- **Live reload mode** - automatically re-run on file save
- **Dual backend**: bytecode VM for fast iteration, native C for production
- **Rich type system**: string, num, decimal, bool, scifi, data, list<T>, MixedList, vector<T>, matrix<T>, enum, struct, class
- **Modern syntax** borrowing from C, Python, and template languages
- **Range operator** `<<<` for checking if value is between two bounds

## Building

### Prerequisites

- GCC or Clang (C99/C11)
- Make or CMake
- Linux (primary), macOS (with minor adjustments)

### Quick Build

```bash
make
sudo make install
```

### Manual Build

```bash
gcc -std=c11 -O2 -o jag main.c lexer/lexer.c parser/parser.c typecheck/typecheck.c backend_vm/vm.c backend_c/cgen.c -lm
```

## Usage

```bash
# Run once (interpreter mode)
jag hello.jag

# Run explicitly
jag run hello.jag

# Live reload mode (watches file for changes)
jag -live=1 hello.jag

# Typecheck only
jag check hello.jag

# Native compilation
jag build hello.jag -o hello

# Help and version
jag --help
jag --version
```

## Language Examples

### Hello World

```jaguar
live.on("Hello, World!");
```

### Variables and Types

```jaguar
var name: string = "Joe";
var age: num = 33;
var score: decimal = 98.71;
var isMale: bool = true;

fixed PI: scifi = 3.14E2;
```

### Control Flow

```jaguar
if (age > 18) {
    live.on("adult");
} elif (age == 18) {
    live.on("just turned adult");
} else {
    live.on("minor");
}

var status: string = (age >= 18) ? "adult" : "minor";
```

### Loops

```jaguar
var i: num = 0;
loop (i < 5) {
    live.on(i);
    i += 1;
}

var scores: list<num> = [10, 20, 30];
for-in (s in scores) {
    live.on(s);
}
```

### Functions

```jaguar
fun greetUser(name: string) {
    live.on("hi, {{name}}");
}

async fun fetchUser() {
    var result: data = live.in("waiting for input...");
}
```

### Range Operator

```jaguar
var mark: num = 72;
var passing: bool = mark <<< (50, 100);   // true
```

### Collections

```jaguar
var config: data = { "host": "localhost", "port": "8080" };
var nums: list<num> = [1, 2, 3];
var mixed: MixedList = [1, "two", true];

mixed.append("five");
```

### Classes, Structs, Enums

```jaguar
enum Direction { North, South, East, West }

struct Point {
    x: num;
    y: num;
}

class User {
    var name: string;
    var age: num;
}

public class Account {
    var balance: decimal;
}
```

## Live Mode

Live mode can be enabled two ways:

1. **CLI flag**: `jag -live=1 hello.jag`
2. **Inside file** (must be first line): `live = "1";`

When active, the compiler watches the source file for changes and automatically re-executes on every save. Press Ctrl+C to exit.

## Testing

```bash
make test
```

## Project Structure

```
├── include/
│   └── jaguar.h          # Main header with all types and declarations
├── lexer/
│   └── lexer.c           # Lexical analysis
├── parser/
│   └── parser.c          # Parsing and AST construction
├── typecheck/
│   └── typecheck.c       # Static type checking
├── backend_vm/
│   └── vm.c              # Bytecode VM interpreter
├── backend_c/
│   └── cgen.c            # C code generator for native builds
├── tests/
│   └── *.jag             # Test files
├── main.c                # CLI entry point
├── Makefile              # Build system
├── install.sh            # Installation script
└── README.md             # This file
```

## License

MIT License
