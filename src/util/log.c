#include "util/log.h"

#include <stdarg.h>
#include <stdio.h>

static bool colorEnabled = false;

#define ANSI_RED "\033[31m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_BOLD "\033[1m"
#define ANSI_NO_BOLD "\033[22m"
#define ANSI_RESET "\033[0m"

void logSetColorEnabled(bool enabled)
{
    colorEnabled = enabled;
}

/*
 * Shared implementation behind logError()/logInfo() - both just supply
 * a different label/color and forward here, so the actual
 * formatting/printing logic lives in exactly one place.
 *
 * label: short tag printed in brackets, e.g. "ERROR" or "INFO".
 * color: ANSI color escape to wrap the label in when colorEnabled is
 *        true. Ignored otherwise.
 * fmt, args: printf-style format string and its already-started
 *            va_list.
 */
static void logMessage(const char *label, const char *color, const char *fmt, va_list args)
{
    if (colorEnabled)
        fprintf(stderr, "%s%s[%s]%s ", color, ANSI_BOLD, label, ANSI_NO_BOLD);

    vfprintf(stderr, fmt, args);

    if (colorEnabled)
        fprintf(stderr, ANSI_RESET);

    fprintf(stderr, "\n");
}

void logError(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logMessage("ERROR", ANSI_RED, fmt, args);
    va_end(args);
}

void logInfo(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logMessage("INFO", ANSI_YELLOW, fmt, args);
    va_end(args);
}
