#include <criterion/criterion.h>

#include "rbc/parser.h"
#include "alloc_control.h"

#include <string.h>

static struct rbc_ast *
parse_ok(const char *text)
{
    struct rbc_ast *root = NULL;
    struct rbc_parse_result result;

    rbc_alloc_test_reset();
    result = rbc_parse(text, strlen(text), &root);
    cr_assert_eq(result.status, RBC_PARSE_OK);
    cr_assert_eq(result.error_offset, 0);
    cr_assert_not_null(root);
    return root;
}

Test(parser, single_literal)
{
    struct rbc_ast *root = parse_ok("42");

    cr_assert_eq(root->kind, RBC_AST_INTEGER);
    cr_assert_eq(root->data.integer, 42);
    rbc_ast_destroy(root);
}

Test(parser, one_addition)
{
    struct rbc_ast *root = parse_ok("1+2");

    cr_assert_eq(root->kind, RBC_AST_BINARY);
    cr_assert_eq(root->data.binary.op, RBC_BINARY_ADD);
    cr_assert_eq(root->data.binary.left->data.integer, 1);
    cr_assert_eq(root->data.binary.right->data.integer, 2);
    rbc_ast_destroy(root);
}

Test(parser, one_subtraction)
{
    struct rbc_ast *root = parse_ok("8-3");

    cr_assert_eq(root->kind, RBC_AST_BINARY);
    cr_assert_eq(root->data.binary.op, RBC_BINARY_SUBTRACT);
    rbc_ast_destroy(root);
}

Test(parser, one_multiplication)
{
    struct rbc_ast *root = parse_ok("6*7");

    cr_assert_eq(root->kind, RBC_AST_BINARY);
    cr_assert_eq(root->data.binary.op, RBC_BINARY_MULTIPLY);
    rbc_ast_destroy(root);
}

Test(parser, multiplicative_precedence)
{
    struct rbc_ast *root = parse_ok("2 + 3 * 4");

    cr_assert_eq(root->kind, RBC_AST_BINARY);
    cr_assert_eq(root->data.binary.op, RBC_BINARY_ADD);
    cr_assert_eq(root->data.binary.left->kind, RBC_AST_INTEGER);
    cr_assert_eq(root->data.binary.right->kind, RBC_AST_BINARY);
    cr_assert_eq(root->data.binary.right->data.binary.op, RBC_BINARY_MULTIPLY);
    rbc_ast_destroy(root);
}

Test(parser, parenthesized_expression)
{
    struct rbc_ast *root = parse_ok("(2 + 3) * 4");

    cr_assert_eq(root->kind, RBC_AST_BINARY);
    cr_assert_eq(root->data.binary.op, RBC_BINARY_MULTIPLY);
    cr_assert_eq(root->data.binary.left->kind, RBC_AST_BINARY);
    cr_assert_eq(root->data.binary.left->data.binary.op, RBC_BINARY_ADD);
    rbc_ast_destroy(root);
}

Test(parser, accepted_whitespace)
{
    struct rbc_ast *root = parse_ok(" \t2\r + 3 ");

    cr_assert_eq(root->kind, RBC_AST_BINARY);
    cr_assert_eq(root->data.binary.op, RBC_BINARY_ADD);
    rbc_ast_destroy(root);
}

Test(parser, unary_negation)
{
    struct rbc_ast *root = parse_ok("-5");

    cr_assert_eq(root->kind, RBC_AST_UNARY);
    cr_assert_eq(root->data.unary.op, RBC_UNARY_NEGATE);
    cr_assert_eq(root->data.unary.child->data.integer, 5);
    rbc_ast_destroy(root);
}

Test(parser, ordinary_syntax_rejection)
{
    const char *text = "1 +";
    struct rbc_ast *root = NULL;
    struct rbc_parse_result result;

    rbc_alloc_test_reset();
    result = rbc_parse(text, strlen(text), &root);
    cr_assert_eq(result.status, RBC_PARSE_SYNTAX);
    cr_assert_eq(result.error_offset, strlen(text));
    cr_assert_null(root);
    cr_assert_eq(rbc_alloc_test_live_count(), 0);
}

Test(parser, numeric_literal_range_propagates)
{
    const char *text = "9223372036854775808";
    struct rbc_ast *root = NULL;
    struct rbc_parse_result result;

    rbc_alloc_test_reset();
    result = rbc_parse(text, strlen(text), &root);
    cr_assert_eq(result.status, RBC_PARSE_INTEGER_RANGE);
    cr_assert_eq(result.error_offset, 0);
    cr_assert_null(root);
    cr_assert_eq(rbc_alloc_test_live_count(), 0);
}

Test(parser, first_ast_allocation_failure)
{
    struct rbc_ast *root = NULL;
    struct rbc_parse_result result;

    rbc_alloc_test_reset();
    rbc_alloc_test_fail_after(0);
    result = rbc_parse("7", 1, &root);
    cr_assert_eq(result.status, RBC_PARSE_NOMEM);
    cr_assert_eq(result.error_offset, 0);
    cr_assert_null(root);
    cr_assert_eq(rbc_alloc_test_attempt_count(), 1);
    cr_assert_eq(rbc_alloc_test_live_count(), 0);
    rbc_alloc_test_reset();
}
