#include <criterion/criterion.h>

#include "rbc/ast.h"
#include "rbc/eval.h"

#include <stdint.h>

static struct rbc_ast *
make_binary(enum rbc_binary_op op, int64_t left_value, int64_t right_value)
{
    struct rbc_ast *left = rbc_ast_create_integer(left_value);
    struct rbc_ast *right = rbc_ast_create_integer(right_value);
    struct rbc_ast *root;

    cr_assert_not_null(left);
    cr_assert_not_null(right);
    root = rbc_ast_create_binary_take(op, left, right);
    cr_assert_not_null(root);
    return root;
}

Test(eval, integer_node)
{
    struct rbc_ast *root = rbc_ast_create_integer(17);
    int64_t value = 0;

    cr_assert_not_null(root);
    cr_assert_eq(rbc_eval(root, &value), RBC_EVAL_OK);
    cr_assert_eq(value, 17);
    rbc_ast_destroy(root);
}

Test(eval, unary_negation)
{
    struct rbc_ast *child = rbc_ast_create_integer(9);
    struct rbc_ast *root;
    int64_t value = 0;

    cr_assert_not_null(child);
    root = rbc_ast_create_unary_take(RBC_UNARY_NEGATE, child);
    cr_assert_not_null(root);
    cr_assert_eq(rbc_eval(root, &value), RBC_EVAL_OK);
    cr_assert_eq(value, -9);
    rbc_ast_destroy(root);
}

Test(eval, safe_addition)
{
    struct rbc_ast *root = make_binary(RBC_BINARY_ADD, 20, 22);
    int64_t value = 0;

    cr_assert_eq(rbc_eval(root, &value), RBC_EVAL_OK);
    cr_assert_eq(value, 42);
    rbc_ast_destroy(root);
}

Test(eval, safe_subtraction)
{
    struct rbc_ast *root = make_binary(RBC_BINARY_SUBTRACT, 20, 8);
    int64_t value = 0;

    cr_assert_eq(rbc_eval(root, &value), RBC_EVAL_OK);
    cr_assert_eq(value, 12);
    rbc_ast_destroy(root);
}

Test(eval, safe_multiplication)
{
    struct rbc_ast *root = make_binary(RBC_BINARY_MULTIPLY, -6, 7);
    int64_t value = 0;

    cr_assert_eq(rbc_eval(root, &value), RBC_EVAL_OK);
    cr_assert_eq(value, -42);
    rbc_ast_destroy(root);
}

Test(eval, signed_division)
{
    int64_t value = 0;
    struct rbc_ast *a = make_binary(RBC_BINARY_DIVIDE, 7, 3);
    struct rbc_ast *b = make_binary(RBC_BINARY_DIVIDE, -7, 3);
    struct rbc_ast *c = make_binary(RBC_BINARY_DIVIDE, 7, -3);

    cr_assert_eq(rbc_eval(a, &value), RBC_EVAL_OK);
    cr_assert_eq(value, 2);
    cr_assert_eq(rbc_eval(b, &value), RBC_EVAL_OK);
    cr_assert_eq(value, -2);
    cr_assert_eq(rbc_eval(c, &value), RBC_EVAL_OK);
    cr_assert_eq(value, -2);
    rbc_ast_destroy(a);
    rbc_ast_destroy(b);
    rbc_ast_destroy(c);
}

Test(eval, signed_remainder)
{
    int64_t value = 0;
    struct rbc_ast *a = make_binary(RBC_BINARY_REMAINDER, 7, 3);
    struct rbc_ast *b = make_binary(RBC_BINARY_REMAINDER, -7, 3);
    struct rbc_ast *c = make_binary(RBC_BINARY_REMAINDER, 7, -3);

    cr_assert_eq(rbc_eval(a, &value), RBC_EVAL_OK);
    cr_assert_eq(value, 1);
    cr_assert_eq(rbc_eval(b, &value), RBC_EVAL_OK);
    cr_assert_eq(value, -1);
    cr_assert_eq(rbc_eval(c, &value), RBC_EVAL_OK);
    cr_assert_eq(value, 1);
    rbc_ast_destroy(a);
    rbc_ast_destroy(b);
    rbc_ast_destroy(c);
}

Test(eval, division_by_zero)
{
    struct rbc_ast *root = make_binary(RBC_BINARY_DIVIDE, 7, 0);
    int64_t value = 1234;

    cr_assert_eq(rbc_eval(root, &value), RBC_EVAL_DIVISION_BY_ZERO);
    cr_assert_eq(value, 1234);
    rbc_ast_destroy(root);
}

Test(eval, remainder_by_zero)
{
    struct rbc_ast *root = make_binary(RBC_BINARY_REMAINDER, 7, 0);
    int64_t value = 1234;

    cr_assert_eq(rbc_eval(root, &value), RBC_EVAL_REMAINDER_BY_ZERO);
    cr_assert_eq(value, 1234);
    rbc_ast_destroy(root);
}

Test(eval, addition_overflow)
{
    struct rbc_ast *root = make_binary(RBC_BINARY_ADD, INT64_MAX, 1);
    int64_t value = 1234;

    cr_assert_eq(rbc_eval(root, &value), RBC_EVAL_OVERFLOW);
    cr_assert_eq(value, 1234);
    rbc_ast_destroy(root);
}

Test(eval, subtraction_overflow)
{
    struct rbc_ast *root = make_binary(RBC_BINARY_SUBTRACT, INT64_MIN, 1);
    int64_t value = 1234;

    cr_assert_eq(rbc_eval(root, &value), RBC_EVAL_OVERFLOW);
    cr_assert_eq(value, 1234);
    rbc_ast_destroy(root);
}

Test(eval, minimum_value_negation_overflow)
{
    struct rbc_ast *child = rbc_ast_create_integer(INT64_MIN);
    struct rbc_ast *root;
    int64_t value = 1234;

    cr_assert_not_null(child);
    root = rbc_ast_create_unary_take(RBC_UNARY_NEGATE, child);
    cr_assert_not_null(root);
    cr_assert_eq(rbc_eval(root, &value), RBC_EVAL_OVERFLOW);
    cr_assert_eq(value, 1234);
    rbc_ast_destroy(root);
}

Test(eval, minimum_divided_by_negative_one_overflow)
{
    struct rbc_ast *root = make_binary(RBC_BINARY_DIVIDE, INT64_MIN, -1);
    int64_t value = 1234;

    cr_assert_eq(rbc_eval(root, &value), RBC_EVAL_OVERFLOW);
    cr_assert_eq(value, 1234);
    rbc_ast_destroy(root);
}

Test(eval, minimum_remainder_negative_one_overflow)
{
    struct rbc_ast *root = make_binary(RBC_BINARY_REMAINDER, INT64_MIN, -1);
    int64_t value = 1234;

    cr_assert_eq(rbc_eval(root, &value), RBC_EVAL_OVERFLOW);
    cr_assert_eq(value, 1234);
    rbc_ast_destroy(root);
}

Test(eval, output_unchanged_on_failure)
{
    struct rbc_ast *root = make_binary(RBC_BINARY_DIVIDE, 10, 0);
    int64_t value = INT64_C(0x123456789);

    cr_assert_eq(rbc_eval(root, &value), RBC_EVAL_DIVISION_BY_ZERO);
    cr_assert_eq(value, INT64_C(0x123456789));
    rbc_ast_destroy(root);
}
