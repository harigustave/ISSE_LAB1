#ifndef RBC_PARSER_H
#define RBC_PARSER_H

#include <stddef.h>

#include "rbc/ast.h"

enum rbc_parse_status {
    RBC_PARSE_OK,
    RBC_PARSE_EMPTY,
    RBC_PARSE_SYNTAX,
    RBC_PARSE_INTEGER_RANGE,
    RBC_PARSE_NOMEM
};

struct rbc_parse_result {
    enum rbc_parse_status status;
    size_t error_offset;
};

/*
 * Parse one complete expression from borrowed line[0..length). out_ast points
 * to writable caller storage that must initially contain NULL; input bytes are
 * not modified or retained.
 *
 * RBC_PARSE_OK transfers exactly one newly owned AST root to *out_ast and has
 * error_offset == 0. Every other status transfers nothing and leaves *out_ast
 * NULL. EMPTY and NOMEM have error_offset == 0; syntax uses the first position
 * where failure is established (length for unexpected end); integer range uses
 * the first digit of the offending literal. Every temporary AST acquired by an
 * unsuccessful parse is required to be released.
 */
struct rbc_parse_result
rbc_parse(const char *line,
          size_t length,
          struct rbc_ast **out_ast);

#endif
