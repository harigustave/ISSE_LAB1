#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: test_cli.sh /path/to/rbc" >&2
    exit 2
fi

rbc=$1
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

case_number=0

run_case() {
    case_number=$((case_number + 1))
    name=$1
    input=$2
    expected_stdout=$3
    expected_stderr=$4
    expected_status=$5

    printf '%b' "$input" >"$tmpdir/input"
    printf '%b' "$expected_stdout" >"$tmpdir/expected.out"
    printf '%b' "$expected_stderr" >"$tmpdir/expected.err"

    set +e
    "$rbc" <"$tmpdir/input" >"$tmpdir/actual.out" 2>"$tmpdir/actual.err"
    actual_status=$?
    set -e

    if [ "$actual_status" -ne "$expected_status" ] ||
       ! cmp -s "$tmpdir/expected.out" "$tmpdir/actual.out" ||
       ! cmp -s "$tmpdir/expected.err" "$tmpdir/actual.err"; then
        echo "CLI case $case_number ($name) failed" >&2
        echo "expected status: $expected_status; actual: $actual_status" >&2
        diff -u "$tmpdir/expected.out" "$tmpdir/actual.out" >&2 || true
        diff -u "$tmpdir/expected.err" "$tmpdir/actual.err" >&2 || true
        exit 1
    fi
}

run_case "single expression" \
    '1 + 2\n' \
    '3\n' \
    '' \
    0

run_case "multiple expressions" \
    '2 + 3 * 4\n(2 + 3) * 4\n' \
    '14\n20\n' \
    '' \
    0

run_case "empty and whitespace lines" \
    '\n \t\r\n4\n' \
    '4\n' \
    '' \
    0

run_case "syntax recovery" \
    '1 +\n7\n' \
    '7\n' \
    'rbc: line 1: syntax error\n' \
    1

run_case "evaluation recovery" \
    '8 / 0\n9\n' \
    '9\n' \
    'rbc: line 1: division by zero\n' \
    1

run_case "literal range recovery" \
    '9223372036854775808\n5\n' \
    '5\n' \
    'rbc: line 1: integer literal out of range\n' \
    1

run_case "final line without newline" \
    '6 * 7' \
    '42\n' \
    '' \
    0

case_number=$((case_number + 1))
printf '' >"$tmpdir/expected.out"
printf 'usage: rbc\n' >"$tmpdir/expected.err"
set +e
"$rbc" unexpected >"$tmpdir/actual.out" 2>"$tmpdir/actual.err"
actual_status=$?
set -e
if [ "$actual_status" -ne 2 ] ||
   ! cmp -s "$tmpdir/expected.out" "$tmpdir/actual.out" ||
   ! cmp -s "$tmpdir/expected.err" "$tmpdir/actual.err"; then
    echo "CLI case $case_number (invalid invocation) failed" >&2
    exit 1
fi

exit 0
