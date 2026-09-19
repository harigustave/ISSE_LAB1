#include "rbc/ast.h"
#include "rbc/eval.h"
#include "rbc/input.h"
#include "rbc/parser.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

static void
report_parse_error(size_t line_number, enum rbc_parse_status status)
{
    switch (status) {
    case RBC_PARSE_SYNTAX:
        fprintf(stderr, "rbc: line %zu: syntax error\n", line_number);
        break;
    case RBC_PARSE_INTEGER_RANGE:
        fprintf(stderr, "rbc: line %zu: integer literal out of range\n", line_number);
        break;
    default:
        break;
    }
}

static void
report_eval_error(size_t line_number, enum rbc_eval_status status)
{
    switch (status) {
    case RBC_EVAL_DIVISION_BY_ZERO:
        fprintf(stderr, "rbc: line %zu: division by zero\n", line_number);
        break;
    case RBC_EVAL_REMAINDER_BY_ZERO:
        fprintf(stderr, "rbc: line %zu: remainder by zero\n", line_number);
        break;
    case RBC_EVAL_OVERFLOW:
        fprintf(stderr, "rbc: line %zu: integer overflow\n", line_number);
        break;
    case RBC_EVAL_OK:
        break;
    }
}

int
main(int argc, char **argv)
{
    char line[RBC_LINE_CAPACITY];
    size_t line_number = 1;
    int had_recoverable_error = 0;
    const int interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);

    (void)argv;
    if (argc != 1) {
        fputs("usage: rbc\n", stderr);
        return 2;
    }

    for (;;) {
        if (interactive) {
            fputs(">>> ", stdout);
            fflush(stdout);
        }

        struct rbc_input_result input =
            rbc_input_read_line(stdin, line, sizeof line);

        if (input.status == RBC_INPUT_EOF) {
            return had_recoverable_error ? 1 : 0;
        }
        if (input.status == RBC_INPUT_ERROR) {
            fputs("rbc: input error\n", stderr);
            return 2;
        }
        if (input.status == RBC_INPUT_TOO_LONG) {
            fprintf(stderr, "rbc: line %zu: input line too long\n", line_number);
            had_recoverable_error = 1;
            ++line_number;
            continue;
        }

        {
            struct rbc_ast *root = NULL;
            struct rbc_parse_result parsed = rbc_parse(line, input.length, &root);

            if (parsed.status == RBC_PARSE_NOMEM) {
                fprintf(stderr, "rbc: line %zu: out of memory\n", line_number);
                return 2;
            }
            if (parsed.status == RBC_PARSE_EMPTY) {
                ++line_number;
                continue;
            }
            if (parsed.status != RBC_PARSE_OK) {
                report_parse_error(line_number, parsed.status);
                had_recoverable_error = 1;
                ++line_number;
                continue;
            }

            {
                int64_t value;
                enum rbc_eval_status evaluated = rbc_eval(root, &value);

                rbc_ast_destroy(root);
                if (evaluated == RBC_EVAL_OK) {
                    printf("%" PRId64 "\n", value);
                } else {
                    report_eval_error(line_number, evaluated);
                    had_recoverable_error = 1;
                }
            }
        }

        ++line_number;
    }
}
