#include "util/alloc.h"

#include <stdlib.h>

bool owsgAllocImpl(size_t len, void **out)
{
    if (len == 0 || out == NULL)
        return false;

    void *ptr = malloc(len);
    if (ptr == NULL)
        return false;

    *out = ptr;
    return true;
}

void owsgFreeImpl(void **ptr)
{
    if (ptr == NULL || *ptr == NULL)
        return;

    free(*ptr);
    *ptr = NULL;
}
