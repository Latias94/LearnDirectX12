#include "logger.h"
#include "dassert.h"

// TODO: temporary
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void log_output(log_level level, const char *message, ...)
{
    const char *level_strings[6] = {"[FATAL]: ", "[ERROR]: ", "[WARN]: ", "[INFO]: ", "[DEBUG]: ", "[TRACE]: "};
    // b8 is_error = level < 2;

    // Technically imposes a 32k character limit on a single log entry, but...
    // DON'T DO THAT!
    char out_message[32000];
    memset(out_message, 0, sizeof(out_message));
// Format original message.
// NOTE: Oddly enough, MS's headers override the GCC/Clang va_list type with a "typedef char* va_list" in some
// cases, and as a result throws a strange error here. The workaround for now is to just use __builtin_va_list,
// which is the type GCC/Clang's va_start expects.
// msvc use va_list, maybe there is a good way to implement it?
#if defined(_MSC_VER)
    va_list arg_ptr;
#else
    __builtin_va_list arg_ptr;
#endif
    va_start(arg_ptr, message);
    vsnprintf(out_message, 32000, message, arg_ptr);
    va_end(arg_ptr);

    char out_message2[32000];
    sprintf(out_message2, "%s%s\n", level_strings[level], out_message);

    // TODO: platform-specific output.
    printf("%s", out_message2);
}

void report_assertion_failure(const char *expression, const char *message, const char *file, int line)
{
    log_output(LOG_LEVEL_FATAL, "Assertion Failure: %s, message: '%s', in file: %s, line: %d\n", expression, message,
               file, line);
}
