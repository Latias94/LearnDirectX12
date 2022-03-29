#pragma once

#define LOG_WARN_ENABLED 1
#define LOG_INFO_ENABLED 1
#define LOG_DEBUG_ENABLED 1
#define LOG_TRACE_ENABLED 1

// Disable debug and trace logging for release builds.
#if DRELEASE == 1
#define LOG_DEBUG_ENABLED 0
#define LOG_TRACE_ENABLED 0
#endif

typedef enum log_level
{
    LOG_LEVEL_FATAL = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4,
    LOG_LEVEL_TRACE = 5,
} log_level;

void log_output(log_level level, const char *message, ...);

#ifndef DFATAL
// Logs a fatal-level message.
#define DFATAL(message, ...) log_output(LOG_LEVEL_FATAL, message, ##__VA_ARGS__)
#endif

#ifndef DERROR
// Logs an error-level message.
#define DERROR(message, ...) log_output(LOG_LEVEL_ERROR, message, ##__VA_ARGS__)
#endif

#if LOG_WARN_ENABLED == 1
#define DWARN(message, ...) log_output(LOG_LEVEL_WARN, message, ##__VA_ARGS__)
#else
#define DWARN(message, ...)
#endif

#if LOG_INFO_ENABLED == 1
#define DINFO(message, ...) log_output(LOG_LEVEL_INFO, message, ##__VA_ARGS__)
#else
#define DINFO(message, ...)
#endif

#if LOG_DEBUG_ENABLED == 1
#define DDEBUG(message, ...) log_output(LOG_LEVEL_DEBUG, message, ##__VA_ARGS__)
#else
#define DDEBUG(message, ...)
#endif

#if LOG_TRACE_ENABLED == 1
#define DTRACE(message, ...) log_output(LOG_LEVEL_TRACE, message, ##__VA_ARGS__)
#else
#define DTRACE(message, ...)
#endif
