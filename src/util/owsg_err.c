#include "util/owsg_err.h"
#include "util/alloc.h"

#include <stdarg.h>
#include <stdio.h>

void owsgErrSet(owsg_err *err, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (len < 0)
    {
        owsgErrFree(err);
        return;
    }

    char *msg;

    if (!owsgAlloc((size_t)len + 1, &msg))
    {
        owsgErrFree(err);
        return;
    }

    va_start(args, fmt);
    vsnprintf(msg, (size_t)len + 1, fmt, args);
    va_end(args);

    owsgErrFree(err);

    err->msg = msg;
    err->len = (size_t)len;
}

void owsgErrFree(owsg_err *err)
{
    if (err == NULL)
        return;

    owsgFree(&err->msg);
    err->len = 0;
}
