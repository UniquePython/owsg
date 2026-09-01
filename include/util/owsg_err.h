#ifndef UTIL_OWSG_ERR_H_
#define UTIL_OWSG_ERR_H_

#include <stddef.h>

typedef struct
{
    char *msg;
    size_t len;
} owsg_err;

/*
 * Formats an error message and stores it in err.
 *
 * err: non-NULL error object to populate. Any existing message is
 *      freed before the new message is stored.
 *
 * fmt: printf-style format string.
 *
 * On allocation or formatting failure, err is cleared.
 *
 * Ownership: err owns the allocated message. The message remains
 * valid until owsgErrSet() is called again or owsgErrFree() is called.
 */
void owsgErrSet(owsg_err *err, const char *fmt, ...);

/*
 * Releases the message owned by err and resets the error object to
 * an empty state.
 *
 * err: error object to clear. May be NULL.
 */
void owsgErrFree(owsg_err *err);

#define ERR_FMT "%.*s"
#define ERR_ARG(err) (int)(err).len, (err).msg

#endif /* UTIL_OWSG_ERR_H_ */
