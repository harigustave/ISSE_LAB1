#include "rbc/parser.h"

#include "rbc/lexer.h"

#include <assert.h>

struct parser_state {
    struct rbc_lexer lexer;
    struct rbc_token token;
    size_t error_offset;
};

static enum rbc_parse_status parse_additive(struct parser_state *state,
                                             struct rbc_ast **out_ast);

static enum rbc_parse_status
advance(struct parser_state *state)
{
    enum rbc_lex_status status = rbc_lexer_next(&state->lexer, &state->token);

    if (status == RBC_LEX_OK) {
        return RBC_PARSE_OK;
    }

    state->error_offset = state->token.offset;
    if (status == RBC_LEX_INTEGER_RANGE) {
        return RBC_PARSE_INTEGER_RANGE;
    }
    return RBC_PARSE_SYNTAX;
}

static enum rbc_parse_status
syntax_error(struct parser_state *state)
{
    state->error_offset = state->token.offset;
    return RBC_PARSE_SYNTAX;
}

static enum rbc_parse_status
parse_primary(struct parser_state *state, struct rbc_ast **out_ast)
{
    struct rbc_ast *node = NULL;
    enum rbc_parse_status status;

    assert(*out_ast == NULL);

    if (state->token.kind == RBC_TOKEN_INTEGER) {
        node = rbc_ast_create_integer(state->token.integer);
        if (node == NULL) {
            state->error_offset = 0;
            return RBC_PARSE_NOMEM;
        }

        status = advance(state);
        if (status != RBC_PARSE_OK) {
            rbc_ast_destroy(node);
            return status;
        }

        *out_ast = node;
        return RBC_PARSE_OK;
    }

    if (state->token.kind == RBC_TOKEN_LPAREN) {
        status = advance(state);
        if (status != RBC_PARSE_OK) {
            return status;
        }

        status = parse_additive(state, &node);
        if (status != RBC_PARSE_OK) {
            return status;
        }

        if (state->token.kind != RBC_TOKEN_RPAREN) {
            rbc_ast_destroy(node);
            return syntax_error(state);
        }

        status = advance(state);
        if (status != RBC_PARSE_OK) {
            rbc_ast_destroy(node);
            return status;
        }

        *out_ast = node;
        return RBC_PARSE_OK;
    }

    return syntax_error(state);
}

static enum rbc_parse_status
parse_unary(struct parser_state *state, struct rbc_ast **out_ast)
{
    enum rbc_parse_status status;
    struct rbc_ast *child = NULL;
    struct rbc_ast *parent;

    assert(*out_ast == NULL);

    if (state->token.kind != RBC_TOKEN_MINUS) {
        return parse_primary(state, out_ast);
    }

    status = advance(state);
    if (status != RBC_PARSE_OK) {
        return status;
    }

    status = parse_unary(state, &child);
    if (status != RBC_PARSE_OK) {
        return status;
    }

    parent = rbc_ast_create_unary_take(RBC_UNARY_NEGATE, child);
    if (parent == NULL) {
        rbc_ast_destroy(child);
        state->error_offset = 0;
        return RBC_PARSE_NOMEM;
    }

    *out_ast = parent;
    return RBC_PARSE_OK;
}

static int
is_multiplicative(enum rbc_token_kind kind)
{
    return kind == RBC_TOKEN_STAR ||
           kind == RBC_TOKEN_SLASH ||
           kind == RBC_TOKEN_PERCENT;
}

static enum rbc_binary_op
multiplicative_op(enum rbc_token_kind kind)
{
    switch (kind) {
    case RBC_TOKEN_STAR:
        return RBC_BINARY_MULTIPLY;
    case RBC_TOKEN_SLASH:
        return RBC_BINARY_DIVIDE;
    case RBC_TOKEN_PERCENT:
        return RBC_BINARY_REMAINDER;
    default:
        assert(0 && "not a multiplicative operator");
        return RBC_BINARY_MULTIPLY;
    }
}

static enum rbc_parse_status
parse_multiplicative(struct parser_state *state, struct rbc_ast **out_ast)
{
    struct rbc_ast *left = NULL;
    enum rbc_parse_status status = parse_unary(state, &left);

    assert(*out_ast == NULL);
    if (status != RBC_PARSE_OK) {
        return status;
    }

    while (is_multiplicative(state->token.kind)) {
        enum rbc_binary_op op = multiplicative_op(state->token.kind);
        struct rbc_ast *right = NULL;
        struct rbc_ast *parent;

        status = advance(state);
        if (status != RBC_PARSE_OK) {
            rbc_ast_destroy(left);
            return status;
        }

        status = parse_unary(state, &right);
        if (status != RBC_PARSE_OK) {
            rbc_ast_destroy(left);
            return status;
        }

        parent = rbc_ast_create_binary_take(op, left, right);
        if (parent == NULL) {
            rbc_ast_destroy(right);
            state->error_offset = 0;
            return RBC_PARSE_NOMEM;
        }
        left = parent;
    }

    *out_ast = left;
    return RBC_PARSE_OK;
}

static int
is_additive(enum rbc_token_kind kind)
{
    return kind == RBC_TOKEN_PLUS || kind == RBC_TOKEN_MINUS;
}

static enum rbc_binary_op
additive_op(enum rbc_token_kind kind)
{
    assert(is_additive(kind));
    return kind == RBC_TOKEN_PLUS ? RBC_BINARY_ADD : RBC_BINARY_SUBTRACT;
}

static enum rbc_parse_status
parse_additive(struct parser_state *state, struct rbc_ast **out_ast)
{
    struct rbc_ast *left = NULL;
    enum rbc_parse_status status = parse_multiplicative(state, &left);

    assert(*out_ast == NULL);
    if (status != RBC_PARSE_OK) {
        return status;
    }

    if (is_additive(state->token.kind)) {
        enum rbc_binary_op op = additive_op(state->token.kind);
        struct rbc_ast *right = NULL;
        struct rbc_ast *parent;

        status = advance(state);
        if (status != RBC_PARSE_OK) {
            rbc_ast_destroy(left);
            return status;
        }

        status = parse_additive(state, &right);
        if (status != RBC_PARSE_OK) {
            rbc_ast_destroy(left);
            return status;
        }

        parent = rbc_ast_create_binary_take(op, left, right);
        if (parent == NULL) {
            rbc_ast_destroy(left);
            rbc_ast_destroy(right);
            state->error_offset = 0;
            return RBC_PARSE_NOMEM;
        }
        left = parent;
    }

    *out_ast = left;
    return RBC_PARSE_OK;
}

struct rbc_parse_result
rbc_parse(const char *line, size_t length, struct rbc_ast **out_ast)
{
    struct parser_state state;
    struct rbc_ast *root = NULL;
    enum rbc_parse_status status;

    assert(line != NULL || length == 0);
    assert(out_ast != NULL);
    assert(*out_ast == NULL);

    state.error_offset = 0;
    rbc_lexer_init(&state.lexer, line, length);

    status = advance(&state);
    if (status != RBC_PARSE_OK) {
        return (struct rbc_parse_result){status, state.error_offset};
    }

    if (state.token.kind == RBC_TOKEN_END) {
        return (struct rbc_parse_result){RBC_PARSE_EMPTY, 0};
    }

    status = parse_additive(&state, &root);
    if (status != RBC_PARSE_OK) {
        return (struct rbc_parse_result){status, state.error_offset};
    }

    if (state.token.kind != RBC_TOKEN_END) {
        size_t offset = state.token.offset;
        rbc_ast_destroy(root);
        return (struct rbc_parse_result){RBC_PARSE_SYNTAX, offset};
    }

    *out_ast = root;
    return (struct rbc_parse_result){RBC_PARSE_OK, 0};
}
