#include "util/file.h"
#include "util/alloc.h"
#include "util/owsg_err.h"

#include <stdio.h>

bool readFile(const char *path, size_t *outLength, char **outData, owsg_err *err)
{
    if (path == NULL)
    {
        owsgErrSet(err, "File path is NULL");
        return false;
    }

    if (outLength == NULL)
    {
        owsgErrSet(err, "Output length pointer is NULL");
        return false;
    }

    if (outData == NULL)
    {
        owsgErrSet(err, "Output data pointer is NULL");
        return false;
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        owsgErrSet(err, "Failed to open file '%s'", path);
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        owsgErrSet(err, "Failed to seek to the end of file '%s'", path);
        fclose(file);
        return false;
    }

    size_t length = (size_t)ftell(file);
    if (length < 0)
    {
        owsgErrSet(err, "Failed to determine size of file '%s'", path);
        fclose(file);
        return false;
    }

    if (fseek(file, 0, SEEK_SET) != 0)
    {
        owsgErrSet(err, "Failed to seek to the beginning of file '%s'", path);
        fclose(file);
        return false;
    }

    char *data;

    if (!owsgAlloc(length + 1, &data))
    {
        owsgErrSet(err, "Failed to allocate %zu bytes for file '%s'", length + 1, path);
        fclose(file);
        return false;
    }

    size_t bytesRead = fread(data, 1, length, file);

    if (bytesRead != length)
    {
        if (ferror(file))
            owsgErrSet(err, "Failed to read file '%s'", path);
        else
        {
            owsgErrSet(err,
                       "Unexpected end of file while reading '%s' "
                       "(expected %zu bytes, got %zu)",
                       path, length, bytesRead);
        }

        owsgFree(&data);
        fclose(file);
        return false;
    }

    data[length] = '\0';

    if (fclose(file) != 0)
    {
        owsgErrSet(err, "Failed to close file '%s'", path);
        owsgFree(&data);
        return false;
    }

    *outLength = length;
    *outData = data;

    return true;
}
