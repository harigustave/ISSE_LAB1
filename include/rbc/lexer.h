#ifndef RBC_LEXER_H
#define RBC_LEXER_H

#include <stddef.h>
#include <stdint.h>

enum rbc_token_kind {
    RBC_TOKEN_END,
    RBC_TOKEN_INTEGER,
    RBC_TOKEN_PLUS,
    RBC_TOKEN_MINUS,
    RBC_TOKEN_STAR,
    RBC_TOKEN_SLASH,
    RBC_TOKEN_PERCENT,
    RBC_TOKEN_LPAREN,
    RBC_TOKEN_RPAREN
};

struct rbc_token {
    enum rbc_token_kind kind;
    int64_t integer;
    size_t offset;
};

struct rbc_lexer {
    const char *input;
    size_t length;
    size_t position;
};

enum rbc_lex_status {
    RBC_LEX_OK,
    RBC_LEX_INVALID_CHAR,
    RBC_LEX_INTEGER_RANGE
};

/*
 * Initialize caller-owned lexer state over a borrowed pointer-plus-length
 * range. The range must remain live while the lexer is used. No allocation or
 * ownership transfer occurs. On return position is zero.
 */
void
rbc_lexer_init(struct rbc_lexer *lexer,
               const char *input,
               size_t length);

/*
 * Produce the next token while maintaining position <= length. On success the
 * token is fully initialized; END has offset == length. Integer tokens contain
 * their value and all other tokens contain integer == 0. On lexical failure
 * the token is END with integer == 0 and offset at the offending byte/literal.
 * No continuation contract is provided after a lexical failure.
 */
enum rbc_lex_status
rbc_lexer_next(struct rbc_lexer *lexer,
               struct rbc_token *out_token);

#endif
