#ifndef RBC_INPUT_H
#define RBC_INPUT_H

#include <stddef.h>
#include <stdio.h>

#define RBC_LINE_CAPACITY 256u

enum rbc_input_status {
    RBC_INPUT_LINE,
    RBC_INPUT_EOF,
    RBC_INPUT_TOO_LONG,
    RBC_INPUT_ERROR
};

struct rbc_input_result {
    enum rbc_input_status status;
    size_t length;
};

/*
 * Read one logical line from borrowed stream into borrowed buffer.
 * Preconditions: stream is a live readable C stream; buffer designates at
 * least capacity writable chars; capacity >= 2. No allocation or ownership
 * transfer occurs.
 *
 * At most capacity - 1 payload bytes are accepted. RBC_INPUT_LINE consumes
 * the terminating newline when present, excludes it from length, and leaves
 * buffer[length] == '\0'. A fitting final line ending at EOF is also a line.
 * RBC_INPUT_EOF means EOF occurred before any byte of a new logical line.
 * RBC_INPUT_TOO_LONG means the complete overlong logical line has been
 * discarded through newline/EOF. RBC_INPUT_ERROR reports a true stream error.
 * EOF, too-long, and error returns have length == 0 and buffer[0] == '\0'.
 */
struct rbc_input_result
rbc_input_read_line(FILE *stream,
                    char *buffer,
                    size_t capacity);

#endif
