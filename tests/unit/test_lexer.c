#include <criterion/criterion.h>

#include "rbc/lexer.h"

#include <stdint.h>
#include <string.h>

static struct rbc_token
next_ok(struct rbc_lexer *lexer)
{
    struct rbc_token token;
    enum rbc_lex_status status = rbc_lexer_next(lexer, &token);

    cr_assert_eq(status, RBC_LEX_OK);
    return token;
}

Test(lexer, decimal_integer)
{
    struct rbc_lexer lexer;
    struct rbc_token token;

    rbc_lexer_init(&lexer, "007", 3);
    token = next_ok(&lexer);
    cr_assert_eq(token.kind, RBC_TOKEN_INTEGER);
    cr_assert_eq(token.integer, 7);
    cr_assert_eq(token.offset, 0);
}

Test(lexer, binary_operator_tokens)
{
    static const enum rbc_token_kind expected[] = {
        RBC_TOKEN_PLUS, RBC_TOKEN_MINUS, RBC_TOKEN_STAR,
        RBC_TOKEN_SLASH, RBC_TOKEN_PERCENT
    };
    struct rbc_lexer lexer;
    size_t i;

    rbc_lexer_init(&lexer, "+-*/%", 5);
    for (i = 0; i < sizeof expected / sizeof expected[0]; ++i) {
        struct rbc_token token = next_ok(&lexer);
        cr_assert_eq(token.kind, expected[i]);
        cr_assert_eq(token.integer, 0);
        cr_assert_eq(token.offset, i);
    }
}

Test(lexer, minus_token_for_unary_spelling)
{
    struct rbc_lexer lexer;
    struct rbc_token token;

    rbc_lexer_init(&lexer, "-5", 2);
    token = next_ok(&lexer);
    cr_assert_eq(token.kind, RBC_TOKEN_MINUS);
    cr_assert_eq(token.offset, 0);
}

Test(lexer, parentheses)
{
    struct rbc_lexer lexer;

    rbc_lexer_init(&lexer, "()", 2);
    cr_assert_eq(next_ok(&lexer).kind, RBC_TOKEN_LPAREN);
    cr_assert_eq(next_ok(&lexer).kind, RBC_TOKEN_RPAREN);
}

Test(lexer, accepted_whitespace)
{
    struct rbc_lexer lexer;
    struct rbc_token token;

    rbc_lexer_init(&lexer, " \t\r42", 5);
    token = next_ok(&lexer);
    cr_assert_eq(token.kind, RBC_TOKEN_INTEGER);
    cr_assert_eq(token.integer, 42);
    cr_assert_eq(token.offset, 3);
}

Test(lexer, sequential_tokenization)
{
    struct rbc_lexer lexer;

    rbc_lexer_init(&lexer, "12+3", 4);
    cr_assert_eq(next_ok(&lexer).kind, RBC_TOKEN_INTEGER);
    cr_assert_eq(next_ok(&lexer).kind, RBC_TOKEN_PLUS);
    cr_assert_eq(next_ok(&lexer).kind, RBC_TOKEN_INTEGER);
}

Test(lexer, end_token)
{
    struct rbc_lexer lexer;
    struct rbc_token token;

    rbc_lexer_init(&lexer, " ", 1);
    token = next_ok(&lexer);
    cr_assert_eq(token.kind, RBC_TOKEN_END);
    cr_assert_eq(token.integer, 0);
    cr_assert_eq(token.offset, 1);
}

Test(lexer, invalid_byte)
{
    struct rbc_lexer lexer;
    struct rbc_token token = {RBC_TOKEN_INTEGER, 99, 99};
    enum rbc_lex_status status;

    rbc_lexer_init(&lexer, "@", 1);
    status = rbc_lexer_next(&lexer, &token);
    cr_assert_eq(status, RBC_LEX_INVALID_CHAR);
    cr_assert_eq(token.kind, RBC_TOKEN_END);
    cr_assert_eq(token.integer, 0);
    cr_assert_eq(token.offset, 0);
}

Test(lexer, integer_range_boundary)
{
    const char *maximum = "9223372036854775807";
    const char *too_large = "9223372036854775808";
    struct rbc_lexer lexer;
    struct rbc_token token;
    enum rbc_lex_status status;

    rbc_lexer_init(&lexer, maximum, strlen(maximum));
    token = next_ok(&lexer);
    cr_assert_eq(token.kind, RBC_TOKEN_INTEGER);
    cr_assert_eq(token.integer, INT64_MAX);

    rbc_lexer_init(&lexer, too_large, strlen(too_large));
    status = rbc_lexer_next(&lexer, &token);
    cr_assert_eq(status, RBC_LEX_INTEGER_RANGE);
    cr_assert_eq(token.kind, RBC_TOKEN_END);
    cr_assert_eq(token.integer, 0);
    cr_assert_eq(token.offset, 0);
}
