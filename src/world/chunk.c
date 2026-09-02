#include "world/chunk.h"

bool chunkGetBlock(const chunk_t *chunk, int x, int y, int z, blockId_t *outBlock, owsg_err *err)
{
    if (chunk == NULL)
    {
        owsgErrSet(err, "Chunk is NULL");
        return false;
    }

    if (outBlock == NULL)
    {
        owsgErrSet(err, "Output block pointer is NULL");
        return false;
    }

    if (x < 0 || x >= CHUNK_SIZE_X || y < 0 || y >= CHUNK_SIZE_Y || z < 0 || z >= CHUNK_SIZE_Z)
    {
        owsgErrSet(err, "Chunk coordinates out of bounds: (%d, %d, %d)", x, y, z);
        return false;
    }

    *outBlock = chunk->blocks[chunkBlockIndex(x, y, z)];

    return true;
}

bool chunkSetBlock(chunk_t *chunk, int x, int y, int z, blockId_t block, owsg_err *err)
{
    if (chunk == NULL)
    {
        owsgErrSet(err, "Chunk is NULL");
        return false;
    }

    if (x < 0 || x >= CHUNK_SIZE_X || y < 0 || y >= CHUNK_SIZE_Y || z < 0 || z >= CHUNK_SIZE_Z)
    {
        owsgErrSet(err, "Chunk coordinates out of bounds: (%d, %d, %d)", x, y, z);
        return false;
    }

    chunk->blocks[chunkBlockIndex(x, y, z)] = block;

    return true;
}
