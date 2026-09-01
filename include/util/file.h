#ifndef UTIL_FILE_H_
#define UTIL_FILE_H_

#include "util/owsg_err.h"

#include <stddef.h>
#include <stdbool.h>

/*
 * Reads an entire file into a newly heap-allocated, NUL-terminated
 * buffer. Intended for small text files (shader source, config, etc.)
 * that are read wholesale rather than large or binary files.
 *
 * path: filesystem path to open, relative to the process's current
 *       working directory.
 *
 * outLength: if non-NULL, receives the length of the file's contents
 *            in bytes, excluding the NUL terminator.
 *
 * outData: if non-NULL, receives a heap-allocated buffer containing
 *          the file's contents followed by a NUL terminator. The
 *          caller owns this buffer and must free() it when no longer
 *          needed.
 *
 * err: if non-NULL and the function fails, receives an error message.
 *      The caller owns the message and must free when no
 *      longer needed. On success, err is left unchanged.
 *
 * Returns true on success, false on failure.
 */
bool readFile(const char *path, size_t *outLength, char **outData, owsg_err *err);

#endif /* UTIL_FILE_H_ */
