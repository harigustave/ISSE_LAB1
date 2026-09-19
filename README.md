# rbc

`rbc` is a small C17 integer calculator for Linux. It reads one expression per line from standard input.

Supported expressions contain decimal `int64_t` literals, unary `-`, binary `+ - * / %`, parentheses, and spaces/tabs/carriage returns. Binary operators use the usual precedence, with multiplication/division/remainder above addition/subtraction.

Build and run interactively:

```sh
make
build/normal/bin/rbc
```

When connected to a terminal, `rbc` displays a `>>> ` prompt before each input line. Redirected or piped input remains prompt-free, so it can still be used in scripts and tests.

For non-interactive input:

```sh
printf '2 + 3 * 4\n' | build/normal/bin/rbc
```

The Makefile also provides `test`, `sanitize`, `valgrind`, and `clean` targets.

Normal objects are generated under `build/normal/obj/`, and the normal executable is `build/normal/bin/rbc`. Test and sanitizer artifacts are kept under `build/test/` and `build/sanitize/`.
