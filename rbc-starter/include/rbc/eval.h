#ifndef RBC_EVAL_H
#define RBC_EVAL_H

#include <stdint.h>

#include "rbc/ast.h"

enum rbc_eval_status {
    RBC_EVAL_OK,
    RBC_EVAL_DIVISION_BY_ZERO,
    RBC_EVAL_REMAINDER_BY_ZERO,
    RBC_EVAL_OVERFLOW
};

/*
 * Evaluate a borrowed valid live AST without mutation, retention, or allocation.
 * Binary children are evaluated left-to-right. On RBC_EVAL_OK exactly one
 * int64_t result is written. On every evaluation failure *out_value is unchanged.
 */
enum rbc_eval_status
rbc_eval(const struct rbc_ast *root,
         int64_t *out_value);

#endif
