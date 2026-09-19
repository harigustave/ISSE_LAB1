#ifndef RBC_AST_H
#define RBC_AST_H

#include <stdint.h>

enum rbc_ast_kind {
    RBC_AST_INTEGER,
    RBC_AST_UNARY,
    RBC_AST_BINARY
};

enum rbc_unary_op {
    RBC_UNARY_NEGATE
};

enum rbc_binary_op {
    RBC_BINARY_ADD,
    RBC_BINARY_SUBTRACT,
    RBC_BINARY_MULTIPLY,
    RBC_BINARY_DIVIDE,
    RBC_BINARY_REMAINDER
};

/*
 * An owned AST root owns its entire reachable subtree. Unary and binary child
 * pointers are non-NULL and exclusively owned by their parent. Live owning AST
 * values are non-copyable by project contract despite the public representation.
 */
struct rbc_ast {
    enum rbc_ast_kind kind;

    union {
        int64_t integer;

        struct {
            enum rbc_unary_op op;
            struct rbc_ast *child;
        } unary;

        struct {
            enum rbc_binary_op op;
            struct rbc_ast *left;
            struct rbc_ast *right;
        } binary;
    } data;
};

/* Allocate one fully initialized integer node; caller owns a non-NULL result. */
struct rbc_ast *
rbc_ast_create_integer(int64_t value);

/*
 * child is a valid non-NULL owned root. Success transfers child ownership into
 * the returned parent. Allocation failure returns NULL and consumes nothing.
 */
struct rbc_ast *
rbc_ast_create_unary_take(enum rbc_unary_op op,
                          struct rbc_ast *child);

/*
 * left and right are distinct valid owned trees. Success transfers both into
 * the returned parent. Allocation failure returns NULL and consumes neither.
 */
struct rbc_ast *
rbc_ast_create_binary_take(enum rbc_binary_op op,
                           struct rbc_ast *left,
                           struct rbc_ast *right);

/* Recursively release an owned tree exactly once; NULL is accepted as a no-op. */
void
rbc_ast_destroy(struct rbc_ast *node);

#endif
