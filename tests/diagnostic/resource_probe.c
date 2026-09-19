#include "rbc/parser.h"
#include "alloc_control.h"

#include <stddef.h>

int
main(void)
{
    struct rbc_ast *root = NULL;
    struct rbc_parse_result result;

    rbc_alloc_test_reset();
    rbc_alloc_test_fail_after(2);
    result = rbc_parse("2*3", 3, &root);

    if (result.status != RBC_PARSE_NOMEM) {
        return 1;
    }
    if (root != NULL) {
        rbc_ast_destroy(root);
        return 1;
    }
    return 0;
}
