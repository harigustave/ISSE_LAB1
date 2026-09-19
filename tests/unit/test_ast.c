#include <criterion/criterion.h>

#include "rbc/ast.h"
#include "alloc_control.h"

Test(ast, integer_constructor_and_destroy)
{
    struct rbc_ast *node;

    rbc_alloc_test_reset();
    node = rbc_ast_create_integer(17);
    cr_assert_not_null(node);
    cr_assert_eq(node->kind, RBC_AST_INTEGER);
    cr_assert_eq(node->data.integer, 17);
    cr_assert_eq(rbc_alloc_test_live_count(), 1);
    rbc_ast_destroy(node);
    cr_assert_eq(rbc_alloc_test_live_count(), 0);
}

Test(ast, unary_take_transfers_on_success)
{
    struct rbc_ast *child;
    struct rbc_ast *parent;

    rbc_alloc_test_reset();
    child = rbc_ast_create_integer(5);
    cr_assert_not_null(child);
    parent = rbc_ast_create_unary_take(RBC_UNARY_NEGATE, child);
    cr_assert_not_null(parent);
    cr_assert_eq(parent->kind, RBC_AST_UNARY);
    cr_assert_eq(parent->data.unary.child, child);
    cr_assert_eq(rbc_alloc_test_live_count(), 2);
    rbc_ast_destroy(parent);
    cr_assert_eq(rbc_alloc_test_live_count(), 0);
}

Test(ast, binary_take_transfers_on_success)
{
    struct rbc_ast *left;
    struct rbc_ast *right;
    struct rbc_ast *parent;

    rbc_alloc_test_reset();
    left = rbc_ast_create_integer(2);
    right = rbc_ast_create_integer(3);
    cr_assert_not_null(left);
    cr_assert_not_null(right);
    parent = rbc_ast_create_binary_take(RBC_BINARY_ADD, left, right);
    cr_assert_not_null(parent);
    cr_assert_eq(parent->data.binary.left, left);
    cr_assert_eq(parent->data.binary.right, right);
    rbc_ast_destroy(parent);
    cr_assert_eq(rbc_alloc_test_live_count(), 0);
}

Test(ast, destroy_null_is_noop)
{
    rbc_alloc_test_reset();
    rbc_ast_destroy(NULL);
    cr_assert_eq(rbc_alloc_test_live_count(), 0);
}

Test(ast, failed_unary_take_consumes_nothing)
{
    struct rbc_ast *child;
    struct rbc_ast *parent;

    rbc_alloc_test_reset();
    child = rbc_ast_create_integer(5);
    cr_assert_not_null(child);
    rbc_alloc_test_fail_after(0);
    parent = rbc_ast_create_unary_take(RBC_UNARY_NEGATE, child);
    cr_assert_null(parent);
    cr_assert_eq(rbc_alloc_test_live_count(), 1);
    rbc_alloc_test_reset();
    rbc_ast_destroy(child);
    cr_assert_eq(rbc_alloc_test_live_count(), 0);
}

Test(ast, failed_binary_take_consumes_nothing)
{
    struct rbc_ast *left;
    struct rbc_ast *right;
    struct rbc_ast *parent;

    rbc_alloc_test_reset();
    left = rbc_ast_create_integer(2);
    right = rbc_ast_create_integer(3);
    cr_assert_not_null(left);
    cr_assert_not_null(right);
    rbc_alloc_test_fail_after(0);
    parent = rbc_ast_create_binary_take(RBC_BINARY_MULTIPLY, left, right);
    cr_assert_null(parent);
    cr_assert_eq(rbc_alloc_test_live_count(), 2);
    rbc_alloc_test_reset();
    rbc_ast_destroy(left);
    rbc_ast_destroy(right);
    cr_assert_eq(rbc_alloc_test_live_count(), 0);
}
