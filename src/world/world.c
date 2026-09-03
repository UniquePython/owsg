#include "world/world.h"
#include "util/alloc.h"
#include "util/math.h"

#include <stb_ds.h>
#include <stddef.h>

void worldInit(world_t *world)
{
    world->chunks = NULL; /* stb_ds: NULL is the valid empty-map state */
}

bool worldGetChunk(const world_t *world, chunkCoord_t coord, chunk_t **outChunk, owsg_err *err)
{
    if (world == NULL)
    {
        owsgErrSet(err, "World is NULL");
        return false;
    }

    if (outChunk == NULL)
    {
        owsgErrSet(err, "Output chunk pointer is NULL");
        return false;
    }

    /*
     * stb_ds hashmap macros require a modifiable lvalue. The hashmap
     * itself is not modified by hmgeti(), so making a local mutable
     * pointer is safe here.
     */
    chunkEntry_t *chunks = world->chunks;

    ptrdiff_t index = hmgeti(chunks, coord);

    if (index < 0)
        return false;

    *outChunk = chunks[index].value;

    return true;
}

bool worldSetChunk(world_t *world, chunkCoord_t coord, chunk_t *chunk, owsg_err *err)
{
    if (world == NULL)
    {
        owsgErrSet(err, "World is NULL");
        return false;
    }

    if (chunk == NULL)
    {
        owsgErrSet(err, "Chunk is NULL");
        return false;
    }

    /*
     * If a chunk already exists at this coordinate, the world owns
     * that heap allocation, so free it before replacing the entry.
     */
    chunk_t *oldChunk = NULL;

    if (worldGetChunk(world, coord, &oldChunk, err))
    {
        owsgFree(&oldChunk);
    }
    else if (err != NULL && ERR_IS_NONEMPTY(err))
    {
        /*
         * worldGetChunk() only reports errors for invalid arguments.
         * world and outChunk have already been checked above, so this
         * should be unreachable, but don't silently swallow a future
         * lookup error if its contract changes.
         */
        return false;
    }

    /*
     * hmput() inserts a new key or replaces the existing value.
     *
     * stb_ds performs its own internal allocation/reallocation here.
     * It does not provide an owsgAlloc()-style boolean failure result,
     * so there is no meaningful allocation failure to propagate
     * through owsg_err with the current stb_ds API/configuration.
     */
    hmput(world->chunks, coord, chunk);

    return true;
}

void worldBlockToChunkLocal(int32_t wx, int32_t wy, int32_t wz,
                            chunkCoord_t *outCoord,
                            int *outLocalX, int *outLocalY, int *outLocalZ)
{
    outCoord->x = floorDiv(wx, CHUNK_SIZE_X);
    outCoord->y = floorDiv(wy, CHUNK_SIZE_Y);
    outCoord->z = floorDiv(wz, CHUNK_SIZE_Z);

    *outLocalX = floorMod(wx, CHUNK_SIZE_X);
    *outLocalY = floorMod(wy, CHUNK_SIZE_Y);
    *outLocalZ = floorMod(wz, CHUNK_SIZE_Z);
}

void worldChunkLocalToBlock(chunkCoord_t coord, int localX, int localY, int localZ,
                            int32_t *outWx, int32_t *outWy, int32_t *outWz)
{
    *outWx = coord.x * CHUNK_SIZE_X + localX;
    *outWy = coord.y * CHUNK_SIZE_Y + localY;
    *outWz = coord.z * CHUNK_SIZE_Z + localZ;
}

bool worldGetBlock(const world_t *world, int32_t wx, int32_t wy, int32_t wz,
                   blockId_t *outBlock, blockLookupResult_t *outResult, owsg_err *err)
{
    if (world == NULL)
    {
        owsgErrSet(err, "World is NULL");
        return false;
    }

    if (outBlock == NULL)
    {
        owsgErrSet(err, "Output block pointer is NULL");
        return false;
    }

    if (outResult == NULL)
    {
        owsgErrSet(err, "Output result pointer is NULL");
        return false;
    }

    chunkCoord_t chunkCoord;
    int localX;
    int localY;
    int localZ;

    worldBlockToChunkLocal(wx, wy, wz, &chunkCoord, &localX, &localY, &localZ);

    /*
     * worldGetChunk() distinguishes "not found" from an actual
     * argument error through its err parameter.
     */
    chunk_t *chunk = NULL;

    if (!worldGetChunk(world, chunkCoord, &chunk, err))
    {
        /*
         * With valid arguments above, false means the chunk simply
         * isn't loaded. This is an expected lookup result.
         */
        if (err != NULL && ERR_IS_NONEMPTY(err))
            return false;

        *outResult = BLOCK_LOOKUP_CHUNK_UNLOADED;
        return true;
    }

    /*
     * The local coordinates came directly from floorMod(), so they
     * are guaranteed to be inside the chunk's bounds.
     *
     * Still propagate failure rather than silently ignoring it.
     */
    if (!chunkGetBlock(chunk, localX, localY, localZ, outBlock, err))
        return false;

    *outResult = BLOCK_LOOKUP_OK;
    return true;
}

void worldDestroy(world_t *world)
{
    /*
     * Free every chunk_t owned by the world.
     */
    for (ptrdiff_t i = 0; i < hmlen(world->chunks); ++i)
    {
        owsgFree(&world->chunks[i].value);
    }

    /*
     * hmfree() safely handles a NULL map, which is the normal empty
     * world representation.
     */
    hmfree(world->chunks);
    world->chunks = NULL;
}
