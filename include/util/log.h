#ifndef UTIL_LOG_H_
#define UTIL_LOG_H_

#include <stdbool.h>

/*
 * Decides whether subsequent logError()/logInfo() calls emit ANSI
 * color codes around the [ERROR]/[INFO] label.
 *
 * Must be called once during startup before the first log call that
 * should respect it - logs emitted before this is called default to
 * uncolored output (see the static initializer in log.c).
 *
 * enabled: true to color output (e.g. because stderr is a tty), false
 *          to emit plain text (e.g. when stderr is redirected to a
 *          file/pipe, where ANSI codes would just be noise).
 */
void logSetColorEnabled(bool enabled);

/*
 * Logs a printf-style message to stderr with an "[ERROR]" label.
 *
 * fmt: printf-style format string.
 * ...: arguments matching fmt, same rules as printf.
 *
 * A trailing newline is added automatically - do not include one in
 * fmt.
 */
void logError(const char *fmt, ...);

/*
 * Logs a printf-style message to stderr with an "[INFO]" label.
 *
 * fmt: printf-style format string.
 * ...: arguments matching fmt, same rules as printf.
 *
 * A trailing newline is added automatically - do not include one in
 * fmt.
 */
void logInfo(const char *fmt, ...);

#endif /* UTIL_LOG_H_ */
