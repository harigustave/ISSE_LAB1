#include "rbc/ast.h"

#include "internal/alloc.h"

#include <assert.h>

static int
valid_unary_op(enum rbc_unary_op op)
{
    return op == RBC_UNARY_NEGATE;
}

static int
valid_binary_op(enum rbc_binary_op op)
{
    return op >= RBC_BINARY_ADD && op <= RBC_BINARY_REMAINDER;
}

struct rbc_ast *
rbc_ast_create_integer(int64_t value)
{
    struct rbc_ast *node = rbc_alloc(sizeof *node);

    if (node == NULL) {
        return NULL;
    }
    node->kind = RBC_AST_INTEGER;
    node->data.integer = value;
    return node;
}

struct rbc_ast *
rbc_ast_create_unary_take(enum rbc_unary_op op, struct rbc_ast *child)
{
    struct rbc_ast *node;

    assert(valid_unary_op(op));
    assert(child != NULL);

    node = rbc_alloc(sizeof *node);
    if (node == NULL) {
        return NULL;
    }

    node->kind = RBC_AST_UNARY;
    node->data.unary.op = op;
    node->data.unary.child = child;
    return node;
}

struct rbc_ast *
rbc_ast_create_binary_take(enum rbc_binary_op op,
                           struct rbc_ast *left,
                           struct rbc_ast *right)
{
    struct rbc_ast *node;

    assert(valid_binary_op(op));
    assert(left != NULL);
    assert(right != NULL);
    assert(left != right);

    node = rbc_alloc(sizeof *node);
    if (node == NULL) {
        return NULL;
    }

    node->kind = RBC_AST_BINARY;
    node->data.binary.op = op;
    node->data.binary.left = left;
    node->data.binary.right = right;
    return node;
}

void
rbc_ast_destroy(struct rbc_ast *node)
{
    if (node == NULL) {
        return;
    }

    switch (node->kind) {
    case RBC_AST_INTEGER:
        break;
    case RBC_AST_UNARY:
        rbc_ast_destroy(node->data.unary.child);
        break;
    case RBC_AST_BINARY:
        rbc_ast_destroy(node->data.binary.left);
        rbc_ast_destroy(node->data.binary.right);
        break;
    default:
        assert(0 && "invalid AST kind");
    }

    rbc_dealloc(node);
}
