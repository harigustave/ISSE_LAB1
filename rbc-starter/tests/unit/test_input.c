#include <criterion/criterion.h>

#include "rbc/input.h"

#include <stdio.h>
#include <string.h>

static FILE *
stream_with_bytes(const char *text)
{
    FILE *stream = tmpfile();

    cr_assert_not_null(stream);
    cr_assert(fputs(text, stream) >= 0);
    rewind(stream);
    return stream;
}

Test(input, ordinary_newline_terminated_line)
{
    char buffer[16];
    FILE *stream = stream_with_bytes("12 + 3\n");
    struct rbc_input_result result = rbc_input_read_line(stream, buffer, sizeof buffer);

    cr_assert_eq(result.status, RBC_INPUT_LINE);
    cr_assert_eq(result.length, 6);
    cr_assert_str_eq(buffer, "12 + 3");
    fclose(stream);
}

Test(input, empty_newline_terminated_line)
{
    char buffer[8];
    FILE *stream = stream_with_bytes("\n");
    struct rbc_input_result result = rbc_input_read_line(stream, buffer, sizeof buffer);

    cr_assert_eq(result.status, RBC_INPUT_LINE);
    cr_assert_eq(result.length, 0);
    cr_assert_str_eq(buffer, "");
    fclose(stream);
}

Test(input, final_line_at_eof)
{
    char buffer[8];
    FILE *stream = stream_with_bytes("42");
    struct rbc_input_result result = rbc_input_read_line(stream, buffer, sizeof buffer);

    cr_assert_eq(result.status, RBC_INPUT_LINE);
    cr_assert_eq(result.length, 2);
    cr_assert_str_eq(buffer, "42");
    fclose(stream);
}

Test(input, eof_before_new_line)
{
    char buffer[8] = "x";
    FILE *stream = stream_with_bytes("");
    struct rbc_input_result result = rbc_input_read_line(stream, buffer, sizeof buffer);

    cr_assert_eq(result.status, RBC_INPUT_EOF);
    cr_assert_eq(result.length, 0);
    cr_assert_str_eq(buffer, "");
    fclose(stream);
}

Test(input, exact_capacity_minus_one_boundary)
{
    char buffer[4];
    FILE *stream = stream_with_bytes("abc\n");
    struct rbc_input_result result = rbc_input_read_line(stream, buffer, sizeof buffer);

    cr_assert_eq(result.status, RBC_INPUT_LINE);
    cr_assert_eq(result.length, 3);
    cr_assert_str_eq(buffer, "abc");
    fclose(stream);
}

Test(input, one_extra_payload_byte_is_too_long)
{
    char buffer[4] = "xxx";
    FILE *stream = stream_with_bytes("abcd\n");
    struct rbc_input_result result = rbc_input_read_line(stream, buffer, sizeof buffer);

    cr_assert_eq(result.status, RBC_INPUT_TOO_LONG);
    cr_assert_eq(result.length, 0);
    cr_assert_str_eq(buffer, "");
    fclose(stream);
}

Test(input, overlong_line_recovery_across_calls)
{
    char buffer[4] = "xxx";
    FILE *stream = stream_with_bytes("abcd\nxy\n");
    struct rbc_input_result first = rbc_input_read_line(stream, buffer, sizeof buffer);

    cr_assert_eq(first.status, RBC_INPUT_TOO_LONG);
    cr_assert_eq(first.length, 0);
    cr_assert_str_eq(buffer, "");

    struct rbc_input_result second = rbc_input_read_line(stream, buffer, sizeof buffer);

    cr_assert_eq(second.status, RBC_INPUT_LINE);
    cr_assert_eq(second.length, 2);
    cr_assert_str_eq(buffer, "xy");
    fclose(stream);
}
