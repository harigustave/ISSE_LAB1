#include "rbc/eval.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static int
multiplication_overflows(int64_t left, int64_t right)
{
    if (left > 0) {
        if (right > 0) {
            return left > INT64_MAX / right;
        }
        if (right < 0) {
            return right < INT64_MIN / left;
        }
        return 0;
    }

    if (left < 0) {
        if (right > 0) {
            return left < INT64_MIN / right;
        }
        if (right < 0) {
            return left < INT64_MAX / right;
        }
    }
    return 0;
}

static enum rbc_eval_status
eval_node(const struct rbc_ast *node, int64_t *out_value)
{
    assert(node != NULL);
    assert(out_value != NULL);

    switch (node->kind) {
    case RBC_AST_INTEGER:
        *out_value = node->data.integer;
        return RBC_EVAL_OK;

    case RBC_AST_UNARY: {
        int64_t child;
        enum rbc_eval_status status;

        assert(node->data.unary.op == RBC_UNARY_NEGATE);
        status = eval_node(node->data.unary.child, &child);
        if (status != RBC_EVAL_OK) {
            return status;
        }
        if (child == INT64_MIN) {
            return RBC_EVAL_OVERFLOW;
        }
        *out_value = -child;
        return RBC_EVAL_OK;
    }

    case RBC_AST_BINARY: {
        int64_t left;
        int64_t right;
        enum rbc_eval_status status;

        status = eval_node(node->data.binary.left, &left);
        if (status != RBC_EVAL_OK) {
            return status;
        }
        status = eval_node(node->data.binary.right, &right);
        if (status != RBC_EVAL_OK) {
            return status;
        }

        switch (node->data.binary.op) {
        case RBC_BINARY_ADD:
            if ((right > 0 && left > INT64_MAX - right) ||
                (right < 0 && left < INT64_MIN - right)) {
                return RBC_EVAL_OVERFLOW;
            }
            *out_value = left + right;
            return RBC_EVAL_OK;

        case RBC_BINARY_SUBTRACT:
            if ((right > 0 && left < INT64_MIN + right) ||
                (right < 0 && left > INT64_MAX + right)) {
                return RBC_EVAL_OVERFLOW;
            }
            *out_value = left - right;
            return RBC_EVAL_OK;

        case RBC_BINARY_MULTIPLY: {
            int64_t product = left * right;

            if (multiplication_overflows(left, right)) {
                return RBC_EVAL_OVERFLOW;
            }
            *out_value = product;
            return RBC_EVAL_OK;
        }

        case RBC_BINARY_DIVIDE:
            if (right == 0) {
                return RBC_EVAL_DIVISION_BY_ZERO;
            }
            if (left == INT64_MIN && right == -1) {
                return RBC_EVAL_OVERFLOW;
            }
            *out_value = left / right;
            return RBC_EVAL_OK;

        case RBC_BINARY_REMAINDER:
            if (right == 0) {
                return RBC_EVAL_REMAINDER_BY_ZERO;
            }
            if (left == INT64_MIN && right == -1) {
                return RBC_EVAL_OVERFLOW;
            }
            *out_value = left % right;
            return RBC_EVAL_OK;

        default:
            assert(0 && "invalid binary operator");
            return RBC_EVAL_OVERFLOW;
        }
    }

    default:
        assert(0 && "invalid AST kind");
        return RBC_EVAL_OVERFLOW;
    }
}

enum rbc_eval_status
rbc_eval(const struct rbc_ast *root, int64_t *out_value)
{
    int64_t value;
    enum rbc_eval_status status;

    assert(root != NULL);
    assert(out_value != NULL);

    status = eval_node(root, &value);
    if (status == RBC_EVAL_OK) {
        *out_value = value;
    }
    return status;
}
