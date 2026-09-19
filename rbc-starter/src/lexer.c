#include "rbc/lexer.h"

#include <assert.h>
#include <stdint.h>

static int
is_space_byte(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r';
}

void
rbc_lexer_init(struct rbc_lexer *lexer, const char *input, size_t length)
{
    assert(lexer != NULL);
    assert(input != NULL || length == 0);

    lexer->input = input;
    lexer->length = length;
    lexer->position = 0;
}

enum rbc_lex_status
rbc_lexer_next(struct rbc_lexer *lexer, struct rbc_token *out_token)
{
    size_t start;

    assert(lexer != NULL);
    assert(out_token != NULL);
    assert(lexer->position <= lexer->length);
    assert(lexer->input != NULL || lexer->length == 0);

    while (lexer->position < lexer->length &&
           is_space_byte(lexer->input[lexer->position])) {
        ++lexer->position;
    }

    if (lexer->position == lexer->length) {
        *out_token = (struct rbc_token){RBC_TOKEN_END, 0, lexer->length};
        return RBC_LEX_OK;
    }

    start = lexer->position;

    if (lexer->input[start] >= '0' && lexer->input[start] <= '9') {
        int64_t value = 0;

        while (lexer->position < lexer->length) {
            char ch = lexer->input[lexer->position];
            int digit;

            if (ch < '0' || ch > '9') {
                break;
            }
            digit = ch - '0';
            if (value > (INT64_MAX - digit) / 10) {
                *out_token = (struct rbc_token){RBC_TOKEN_END, 0, start};
                return RBC_LEX_INTEGER_RANGE;
            }
            value = value * 10 + digit;
            ++lexer->position;
        }

        *out_token = (struct rbc_token){RBC_TOKEN_INTEGER, value, start};
        return RBC_LEX_OK;
    }

    ++lexer->position;
    switch (lexer->input[start]) {
    case '+':
        *out_token = (struct rbc_token){RBC_TOKEN_PLUS, 0, start};
        return RBC_LEX_OK;
    case '-':
        *out_token = (struct rbc_token){RBC_TOKEN_MINUS, 0, start};
        return RBC_LEX_OK;
    case '*':
        *out_token = (struct rbc_token){RBC_TOKEN_STAR, 0, start};
        return RBC_LEX_OK;
    case '/':
        *out_token = (struct rbc_token){RBC_TOKEN_SLASH, 0, start};
        return RBC_LEX_OK;
    case '%':
        *out_token = (struct rbc_token){RBC_TOKEN_PERCENT, 0, start};
        return RBC_LEX_OK;
    case '(':
        *out_token = (struct rbc_token){RBC_TOKEN_LPAREN, 0, start};
        return RBC_LEX_OK;
    case ')':
        *out_token = (struct rbc_token){RBC_TOKEN_RPAREN, 0, start};
        return RBC_LEX_OK;
    default:
        *out_token = (struct rbc_token){RBC_TOKEN_END, 0, start};
        return RBC_LEX_INVALID_CHAR;
    }
}
