#!/bin/sh
# INSTRUCTOR-PROVIDED — LOCKED SUPPORT SCRIPT. Do not modify.
#
# Check, and optionally prepare, the supported rbc development environment.
# Linux/GitHub Codespaces is authoritative for grading and Linux-specific tool
# evidence. macOS is natively supported for build, test, and sanitizer work.

set -u

MODE=check
case "$#" in
    0)
        MODE=check
        ;;
    1)
        case "$1" in
            --check)
                MODE=check
                ;;
            --install)
                MODE=install
                ;;
            --help)
                cat <<'HELP'
Usage:
  ./scripts/setup_environment.sh
  ./scripts/setup_environment.sh --check
  ./scripts/setup_environment.sh --install
  ./scripts/setup_environment.sh --help

Default mode / --check
  Check required capabilities without installing packages or changing the
  machine. This is the safe default.

--install
  Install missing package-backed dependencies when this can be done through an
  already-configured supported package manager, then run the same checks.
  The script never adds repositories, installs Homebrew, or executes a remote
  installer downloaded by the script.

Environment policy
  * Linux/GitHub Codespaces is the authoritative reference and grading
    environment.
  * The rbc Makefile uses GCC on Linux and Apple Clang on macOS unless CC is
    explicitly overridden. Both platforms must pass the required C17, warning,
    Criterion, build, test, and sanitizer capability checks.
  * Linux requires the course inspection/debugging tools: file, nm, readelf,
    objdump, GDB, and Valgrind/Memcheck.
  * macOS does not require Linux ELF tools, GDB, or Valgrind. Required evidence
    for those tools must be collected in Linux/Codespaces.
  * Criterion is required on both Linux and macOS. The Makefile uses
    pkg-config metadata when available and falls back to -lcriterion when the
    library is already on the compiler's default search path.
HELP
                exit 0
                ;;
            *)
                printf 'ERROR: unsupported argument: %s\n' "$1" >&2
                printf 'Usage: %s [--check|--install|--help]\n' "$0" >&2
                exit 2
                ;;
        esac
        ;;
    *)
        printf 'ERROR: expected zero arguments or one of --check, --install, --help.\n' >&2
        printf 'Usage: %s [--check|--install|--help]\n' "$0" >&2
        exit 2
        ;;
esac

PASS_COUNT=0
FAIL_COUNT=0
WARN_COUNT=0
INSTALL_PERFORMED=0
MISSING_PACKAGES=""
NEEDS_CLT=0
NEEDS_HOMEBREW=0
PLATFORM=""
OS_NAME=""
PM=""
CC_PROBE=gcc
ARCH="$(uname -m 2>/dev/null || printf unknown)"

WARN_FLAGS='-Wall -Wextra -Wpedantic -Wstrict-prototypes -Wmissing-prototypes'
SAN_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined -g3 -O0'

pass() {
    printf 'PASS: %s\n' "$1"
    PASS_COUNT=$((PASS_COUNT + 1))
}

fail() {
    # Keep ordinary check results on stdout so log ordering stays predictable.
    printf 'FAIL: %s\n' "$1"
    FAIL_COUNT=$((FAIL_COUNT + 1))
}

warn() {
    printf 'WARN: %s\n' "$1"
    WARN_COUNT=$((WARN_COUNT + 1))
}

have_command() {
    command -v "$1" >/dev/null 2>&1
}

add_package() {
    candidate=$1
    case " $MISSING_PACKAGES " in
        *" $candidate "*) ;;
        *) MISSING_PACKAGES="${MISSING_PACKAGES}${MISSING_PACKAGES:+ }$candidate" ;;
    esac
}

uname_s=$(uname -s 2>/dev/null || printf unknown)
case "$uname_s" in
    Linux)
        PLATFORM=linux
        if [ -r /etc/os-release ]; then
            # shellcheck disable=SC1091
            . /etc/os-release
            OS_NAME=${PRETTY_NAME:-${NAME:-Linux}}
        else
            OS_NAME=Linux
        fi
        if have_command apt-get; then
            PM=apt
        else
            PM=unsupported
        fi
        ;;
    Darwin)
        PLATFORM=macos
        OS_NAME="macOS $(sw_vers -productVersion 2>/dev/null || printf unknown)"
        if have_command brew; then
            PM=brew
        else
            PM=none
        fi
        if have_command clang; then
            CC_PROBE=clang
        else
            CC_PROBE=cc
        fi
        ;;
    *)
        printf 'ERROR: unsupported operating system: %s\n' "$uname_s" >&2
        printf 'This support script targets Linux and macOS.\n' >&2
        exit 2
        ;;
esac

probe_dir=$(mktemp -d "${TMPDIR:-/tmp}/rbc-env.XXXXXX") || {
    printf 'ERROR: unable to create temporary verification directory.\n' >&2
    exit 5
}
trap 'rm -rf "$probe_dir"' EXIT HUP INT TERM

cat >"$probe_dir/criterion_probe.c" <<'C_EOF'
#include <criterion/criterion.h>

Test(rbc_environment, criterion_runs)
{
    cr_assert_eq(2 + 3, 5);
}
C_EOF

criterion_makefile_works() {
    have_command "$CC_PROBE" || return 1

    criterion_cflags=""
    criterion_libs="-lcriterion"
    if have_command pkg-config \
        && pkg-config --exists criterion >/dev/null 2>&1; then
        criterion_cflags=$(pkg-config --cflags criterion 2>/dev/null || printf '')
        criterion_libs=$(pkg-config --libs criterion 2>/dev/null || printf '')
        [ -n "$criterion_libs" ] || criterion_libs="-lcriterion"
    fi

    # Intentional word splitting: pkg-config emits compiler/linker argument lists.
    # shellcheck disable=SC2086
    "$CC_PROBE" -std=c17 $WARN_FLAGS $criterion_cflags \
        "$probe_dir/criterion_probe.c" $criterion_libs \
        -o "$probe_dir/criterion_makefile" >/dev/null 2>&1 \
        && "$probe_dir/criterion_makefile" >/dev/null 2>&1
}

printf 'rbc environment setup\n'
printf 'Platform: %s\n' "$OS_NAME"
printf 'Architecture: %s\n' "$ARCH"
printf 'Mode: %s\n' "$MODE"
printf 'Package manager: %s\n' "$PM"

# Determine installable missing dependencies. The checker is capability-based;
# package names are used only by explicit --install mode.
if [ "$PLATFORM" = linux ]; then
    if ! have_command gcc || ! have_command make; then
        add_package build-essential
    fi
    if ! have_command git; then add_package git; fi
    if ! have_command gdb; then add_package gdb; fi
    if ! have_command file; then add_package file; fi
    if ! have_command nm || ! have_command readelf || ! have_command objdump; then
        add_package binutils
    fi
    if ! have_command valgrind; then add_package valgrind; fi
    if ! criterion_makefile_works; then add_package libcriterion-dev; fi
else
    # Apple Command Line Tools provide the local compiler driver, make, Git,
    # file, and nm. Their installer is interactive and is never bypassed here.
    if ! have_command clang || ! have_command make || ! have_command git \
        || ! have_command file || ! have_command nm; then
        NEEDS_CLT=1
    fi

    if ! criterion_makefile_works; then
        if [ "$PM" = brew ]; then
            if ! brew list --versions criterion >/dev/null 2>&1; then
                add_package criterion
            fi
            if ! have_command pkg-config; then
                add_package pkg-config
            fi
        else
            NEEDS_HOMEBREW=1
        fi
    fi
fi

if [ "$MODE" = install ]; then
    if [ "$PLATFORM" = macos ] && [ "$NEEDS_CLT" -eq 1 ]; then
        if have_command xcode-select; then
            printf '\nApple Command Line Tools are required. Requesting the Apple installer...\n'
            if xcode-select --install >/dev/null 2>&1; then
                printf 'Complete the Apple installer, then rerun this command.\n'
            else
                printf 'The Command Line Tools installer may already be open or unavailable.\n' >&2
                printf 'Install/complete Apple Command Line Tools, then rerun this command.\n' >&2
            fi
            exit 4
        fi
        printf 'ERROR: Apple Command Line Tools are missing and xcode-select is unavailable.\n' >&2
        exit 4
    fi

    if [ "$PLATFORM" = linux ] && [ -n "$MISSING_PACKAGES" ]; then
        if [ "$PM" != apt ]; then
            printf 'ERROR: automatic Linux installation is supported only with apt-get.\n' >&2
            printf 'Missing package-backed requirements: %s\n' "$MISSING_PACKAGES" >&2
            printf 'Install them with your distribution package manager, then rerun --check.\n' >&2
            exit 4
        fi

        if [ "$(id -u)" -eq 0 ]; then
            SUDO=""
        elif have_command sudo; then
            SUDO=sudo
        else
            printf 'ERROR: package installation requires root or sudo.\n' >&2
            exit 3
        fi

        printf '\nInstalling missing Linux requirements: %s\n' "$MISSING_PACKAGES"
        $SUDO apt-get update || exit 4
        # Intentional word splitting: package list is internally generated.
        # shellcheck disable=SC2086
        $SUDO apt-get install -y --no-install-recommends $MISSING_PACKAGES || exit 4
        INSTALL_PERFORMED=1
    elif [ "$PLATFORM" = macos ] && [ -n "$MISSING_PACKAGES" ]; then
        if [ "$PM" != brew ]; then
            printf 'ERROR: required local macOS packages are missing and Homebrew is unavailable.\n' >&2
            printf 'Install Homebrew through its official installer, then rerun --install.\n' >&2
            printf 'This script intentionally does not install Homebrew itself.\n' >&2
            exit 4
        fi
        printf '\nInstalling missing macOS requirements: %s\n' "$MISSING_PACKAGES"
        # Intentional word splitting: package list is internally generated.
        # shellcheck disable=SC2086
        brew install $MISSING_PACKAGES || exit 4
        INSTALL_PERFORMED=1
    else
        printf '\nNo package installation is needed.\n'
    fi

    if [ "$PLATFORM" = macos ] && [ "$NEEDS_HOMEBREW" -eq 1 ]; then
        printf 'WARN: Criterion is unavailable and Homebrew is not installed.\n'
        printf '      Install Homebrew and Criterion for native macOS make test support.\n'
    fi
else
    if [ -n "$MISSING_PACKAGES" ]; then
        warn "Package-backed requirements appear missing: $MISSING_PACKAGES"
    fi
    if [ "$PLATFORM" = macos ] && [ "$NEEDS_CLT" -eq 1 ]; then
        warn 'Apple Command Line Tools appear incomplete or missing'
    fi
    if [ "$PLATFORM" = macos ] && [ "$NEEDS_HOMEBREW" -eq 1 ]; then
        warn 'Criterion is unavailable and Homebrew is not installed; native macOS make test requires Criterion'
    fi
fi

printf '\nVerification\n'

# POSIX shell and support-script utility surface.
if /bin/sh -c 'set -eu; x=ok; [ "$x" = ok ]' >/dev/null 2>&1; then
    pass '/bin/sh support'
else
    fail '/bin/sh support'
fi

utils_ok=1
for c in basename cat cmp cp dirname grep mkdir mktemp rm sort tail wc diff; do
    if ! have_command "$c"; then
        utils_ok=0
    fi
done
if [ "$utils_ok" -eq 1 ]; then
    pass 'portable support-script command set'
else
    fail 'portable support-script command set'
fi

# Compiler + C17 + the exact frozen warning/debug policy.
cat >"$probe_dir/c17_probe.c" <<'C_EOF'
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int probe_length(void)
{
    const char *s = "rbc";
    return strlen(s) == (size_t)3 ? 0 : 1;
}

int main(void)
{
    if (probe_length() != 0) {
        return EXIT_FAILURE;
    }
    return printf("ok\n") > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
C_EOF

compiler_log="$probe_dir/compiler.log"
# Intentional word splitting: fixed internal flag list.
# shellcheck disable=SC2086
if have_command "$CC_PROBE" \
    && "$CC_PROBE" -std=c17 $WARN_FLAGS -g3 -O0 \
        "$probe_dir/c17_probe.c" -o "$probe_dir/c17_probe" \
        >"$compiler_log" 2>&1 \
    && [ "$("$probe_dir/c17_probe")" = ok ]; then
    if grep -i 'warning:' "$compiler_log" >/dev/null 2>&1; then
        fail 'C17 compile/link/run is not warning-clean under frozen warning policy'
    elif [ "$PLATFORM" = linux ]; then
        if "$CC_PROBE" --version 2>/dev/null | grep 'gcc' >/dev/null 2>&1; then
            pass 'GCC C17 compile/link/run with frozen warning policy'
        else
            fail 'gcc command is not GCC on authoritative Linux environment'
        fi
    else
        pass 'Apple gcc/Clang C17 compile/link/run with frozen warning policy'
    fi
else
    fail 'C17 compile/link/run with frozen warning policy'
fi

# Compile a relocatable object for inspection-tool capability checks.
cat >"$probe_dir/symbol_probe.c" <<'C_EOF'
int rbc_probe_symbol(void);

int rbc_probe_symbol(void)
{
    return 0;
}
C_EOF

# shellcheck disable=SC2086
if have_command "$CC_PROBE" \
    && "$CC_PROBE" -std=c17 $WARN_FLAGS -g3 -O0 -c \
        "$probe_dir/symbol_probe.c" -o "$probe_dir/symbol_probe.o" \
        >/dev/null 2>&1; then
    if have_command file && file "$probe_dir/symbol_probe.o" >/dev/null 2>&1; then
        pass 'file object inspection'
    else
        fail 'file object inspection'
    fi

    if have_command nm \
        && nm "$probe_dir/symbol_probe.o" 2>/dev/null \
            | grep 'rbc_probe_symbol' >/dev/null 2>&1; then
        pass 'nm object-symbol inspection'
    else
        fail 'nm object-symbol inspection'
    fi

    if [ "$PLATFORM" = linux ]; then
        if have_command readelf \
            && readelf -h "$probe_dir/symbol_probe.o" >/dev/null 2>&1 \
            && readelf -s "$probe_dir/symbol_probe.o" >/dev/null 2>&1; then
            pass 'readelf ELF header/symbol inspection'
        else
            fail 'readelf ELF header/symbol inspection'
        fi

        if have_command objdump \
            && objdump -d "$probe_dir/symbol_probe.o" >/dev/null 2>&1; then
            pass 'objdump disassembly'
        else
            fail 'objdump disassembly'
        fi
    else
        warn 'readelf/ELF objdump checks are Linux-only; use Codespaces for required ELF evidence'
    fi
else
    fail 'compiler-produced object for inspection probes'
fi

if have_command make && make --version 2>/dev/null | grep 'GNU Make' >/dev/null 2>&1; then
    cat >"$probe_dir/Makefile" <<'M_EOF'
all:
	@printf 'make-ok\n'
M_EOF
    if [ "$(make -s -f "$probe_dir/Makefile" 2>/dev/null)" = make-ok ]; then
        pass 'GNU Make'
    else
        fail 'GNU Make execution'
    fi
else
    fail 'GNU Make'
fi

if have_command git && git --version >/dev/null 2>&1; then
    pass 'Git'
else
    fail 'Git'
fi

# Criterion: mirror the Makefile's supported discovery model. Prefer pkg-config
# metadata when available, otherwise fall back to a direct -lcriterion link.
if criterion_makefile_works; then
    pass 'Criterion compile/link/run with Makefile-compatible discovery'
else
    fail 'Criterion compile/link/run with Makefile-compatible discovery'
fi

# ASan/UBSan: use the same relevant sanitizer/debug flags as the frozen build.
cat >"$probe_dir/sanitize_clean.c" <<'C_EOF'
#include <stdlib.h>

int main(void)
{
    int *p = malloc(sizeof(*p));
    if (p == NULL) {
        return 2;
    }
    *p = 7;
    free(p);
    return 0;
}
C_EOF

# shellcheck disable=SC2086
if have_command "$CC_PROBE" \
    && "$CC_PROBE" -std=c17 $WARN_FLAGS $SAN_FLAGS \
        "$probe_dir/sanitize_clean.c" -o "$probe_dir/sanitize_clean" \
        >/dev/null 2>&1 \
    && ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
        "$probe_dir/sanitize_clean" >/dev/null 2>&1; then
    pass 'ASan/UBSan clean compile/link/run with frozen sanitizer flags'
else
    fail 'ASan/UBSan clean compile/link/run with frozen sanitizer flags'
fi

cat >"$probe_dir/ubsan_probe.c" <<'C_EOF'
#include <stdint.h>

int main(void)
{
    volatile int64_t left = INT64_C(3037000500);
    volatile int64_t right = INT64_C(3037000500);
    volatile int64_t product = left * right;
    return product == 0 ? 0 : 0;
}
C_EOF

ubsan_rc=0
# shellcheck disable=SC2086
if have_command "$CC_PROBE" \
    && "$CC_PROBE" -std=c17 $WARN_FLAGS $SAN_FLAGS \
        "$probe_dir/ubsan_probe.c" -o "$probe_dir/ubsan_probe" \
        >/dev/null 2>&1; then
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
        "$probe_dir/ubsan_probe" >"$probe_dir/ubsan.out" 2>"$probe_dir/ubsan.err" \
        || ubsan_rc=$?
    if [ "$ubsan_rc" -ne 0 ] \
        && grep -E 'signed integer overflow|runtime error' \
            "$probe_dir/ubsan.err" >/dev/null 2>&1; then
        pass 'UBSan detects signed overflow and terminates nonzero'
    else
        fail "UBSan signed-overflow detection/termination (exit $ubsan_rc)"
    fi
else
    fail 'UBSan signed-overflow probe compilation'
fi

if [ "$PLATFORM" = linux ]; then
    # GDB must be able to execute a debug build, not merely print its version.
    cat >"$probe_dir/debug_probe.c" <<'C_EOF'
int main(void)
{
    int value = 7;
    return value == 7 ? 0 : 1;
}
C_EOF

    # shellcheck disable=SC2086
    if have_command gdb \
        && "$CC_PROBE" -std=c17 $WARN_FLAGS -g3 -O0 \
            "$probe_dir/debug_probe.c" -o "$probe_dir/debug_probe" \
            >/dev/null 2>&1 \
        && gdb -q -batch -ex 'start' -ex 'continue' \
            --args "$probe_dir/debug_probe" >/dev/null 2>&1; then
        pass 'GDB can execute a debug build'
    else
        fail 'GDB can execute a debug build'
    fi

    # Valgrind/Memcheck is checked both on a clean run and on a deliberate leak
    # so the configured nonzero error path is known to work.
    cat >"$probe_dir/valgrind_clean.c" <<'C_EOF'
#include <stdlib.h>

int main(void)
{
    void *p = malloc(16);
    if (p == NULL) {
        return 2;
    }
    free(p);
    return 0;
}
C_EOF

    cat >"$probe_dir/valgrind_leak.c" <<'C_EOF'
#include <stdlib.h>

int main(void)
{
    void *p = malloc(16);
    return p == NULL ? 2 : 0;
}
C_EOF

    valgrind_compile_ok=1
    # shellcheck disable=SC2086
    "$CC_PROBE" -std=c17 $WARN_FLAGS -g3 -O0 \
        "$probe_dir/valgrind_clean.c" -o "$probe_dir/valgrind_clean" \
        >/dev/null 2>&1 || valgrind_compile_ok=0
    # shellcheck disable=SC2086
    "$CC_PROBE" -std=c17 $WARN_FLAGS -g3 -O0 \
        "$probe_dir/valgrind_leak.c" -o "$probe_dir/valgrind_leak" \
        >/dev/null 2>&1 || valgrind_compile_ok=0

    if [ "$valgrind_compile_ok" -eq 1 ] && have_command valgrind \
        && valgrind --tool=memcheck --quiet --leak-check=full \
            --show-leak-kinds=all --errors-for-leak-kinds=all \
            --error-exitcode=99 "$probe_dir/valgrind_clean" \
            >/dev/null 2>&1; then
        pass 'Valgrind/Memcheck clean execution'
    else
        fail 'Valgrind/Memcheck clean execution'
    fi

    valgrind_rc=0
    if [ "$valgrind_compile_ok" -eq 1 ] && have_command valgrind; then
        valgrind --tool=memcheck --quiet --leak-check=full \
            --show-leak-kinds=all --errors-for-leak-kinds=all \
            --error-exitcode=99 "$probe_dir/valgrind_leak" \
            >/dev/null 2>&1 || valgrind_rc=$?
        if [ "$valgrind_rc" -eq 99 ]; then
            pass 'Valgrind/Memcheck leak detection and error-exit path'
        else
            fail "Valgrind/Memcheck leak detection/error-exit path (expected 99, got $valgrind_rc)"
        fi
    else
        fail 'Valgrind/Memcheck leak detection/error-exit path'
    fi
else
    if have_command lldb && lldb --version >/dev/null 2>&1; then
        pass 'LLDB available for optional local debugging'
    else
        warn 'LLDB unavailable; local macOS debugging is optional'
    fi
    warn 'GDB, Valgrind/Memcheck, and ELF-specific evidence are Linux/Codespaces requirements'
fi

if [ "$INSTALL_PERFORMED" -eq 1 ]; then
    install_text=performed
else
    install_text='not performed'
fi

printf '\nSummary: %d passed, %d failed, %d warning(s); installation %s.\n' \
    "$PASS_COUNT" "$FAIL_COUNT" "$WARN_COUNT" "$install_text"

if [ "$FAIL_COUNT" -ne 0 ]; then
    printf 'NOT READY\n'
    exit 1
fi

if [ "$PLATFORM" = macos ] && [ "$WARN_COUNT" -ne 0 ]; then
    printf 'READY\n'
    printf 'macOS native build/test support is available; use Linux/Codespaces for Linux-only tool evidence.\n'
else
    printf 'READY\n'
fi
exit 0
