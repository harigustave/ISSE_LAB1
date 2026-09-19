#include "rbc/input.h"

#include <assert.h>

struct rbc_input_result
rbc_input_read_line(FILE *stream, char *buffer, size_t capacity)
{
    size_t length = 0;

    assert(stream != NULL);
    assert(buffer != NULL);
    assert(capacity >= 2);

    buffer[0] = '\0';

    for (;;) {
        int ch = fgetc(stream);

        if (ch == '\n') {
            buffer[length] = '\0';
            return (struct rbc_input_result){RBC_INPUT_LINE, length};
        }

        if (ch == EOF) {
            if (ferror(stream)) {
                buffer[0] = '\0';
                return (struct rbc_input_result){RBC_INPUT_ERROR, 0};
            }
            if (length == 0) {
                return (struct rbc_input_result){RBC_INPUT_EOF, 0};
            }
            buffer[length] = '\0';
            return (struct rbc_input_result){RBC_INPUT_LINE, length};
        }

        if (length < capacity - 1) {
            buffer[length++] = (char)ch;
            continue;
        }

        buffer[0] = '\0';
        /* TODO(student): discard the rest of this logical line through newline/EOF,
         * returning input error instead if a true stream error occurs. */
        return (struct rbc_input_result){RBC_INPUT_TOO_LONG, 0};
    }
}
