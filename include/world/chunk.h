#ifndef WORLD_CHUNK_H_
#define WORLD_CHUNK_H_

#include "util/owsg_err.h"

#include <stdint.h>
#include <stdbool.h>

/*
 * Chunk dimensions, in blocks, along each axis. Uniform on all three axes.
 */
#define CHUNK_SIZE_X 16
#define CHUNK_SIZE_Y 16
#define CHUNK_SIZE_Z 16

#define CHUNK_BLOCK_COUNT (CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z)

/*
 * Identifies a block's type within a chunk.
 *
 * 0 is reserved for BLOCK_AIR (see below) - this is a deliberate
 * convention, not arbitrary: it means a zero-initialized chunk
 * (calloc, or a zero-initialized struct/static array) is automatically
 * an all-air chunk with no extra setup.
 */
typedef uint8_t blockId_t;

typedef enum
{
    BLOCK_AIR = 0,
    BLOCK_STONE = 1,
    /* TODO: add more as needed */
} blockType_t;

/*
 * A single fixed-size chunk of block data.
 *
 * blocks: flattened 1D array of CHUNK_BLOCK_COUNT block IDs, indexed
 *         via chunkBlockIndex() rather than accessed directly - this
 *         keeps the flattening/indexing math in one place instead of
 *         duplicated at every call site.
 */
typedef struct
{
    blockId_t blocks[CHUNK_BLOCK_COUNT];

} chunk_t;

/*
 * Converts local chunk-space coordinates to a flat index into
 * chunk_t.blocks.
 *
 * x, y, z: local coordinates within the chunk, each expected in
 *          [0, CHUNK_SIZE_* - 1]. Behavior is undefined if out of
 *          range.
 *
 * Returns: the corresponding index into a CHUNK_BLOCK_COUNT-sized
 *          flat array.
 */
static inline int chunkBlockIndex(int x, int y, int z)
{
    return x + y * CHUNK_SIZE_X + z * CHUNK_SIZE_X * CHUNK_SIZE_Y;
}

/*
 * Gets the block type at the given local chunk coordinates.
 *
 * chunk: non-NULL chunk to read from.
 * x, y, z: local coordinates, same range constraints as chunkBlockIndex().
 * outBlock: non-NULL output pointer that receives the block ID.
 *
 * err: non-NULL error object to populate if the operation fails.
 *
 * Returns: true on success, false on failure. On success, the block
 * ID at the given position is written to *outBlock.
 *
 * The block data is accessed via chunkBlockIndex() so that the
 * flattening/indexing details remain encapsulated here.
 */
bool chunkGetBlock(const chunk_t *chunk, int x, int y, int z, blockId_t *outBlock, owsg_err *err);

/*
 * Sets the block type at the given local chunk coordinates.
 *
 * chunk: non-NULL chunk to modify.
 *
 * x, y, z: local coordinates, same range constraints as chunkBlockIndex().
 *
 * block: the block ID to place.
 *
 * err: non-NULL error object to populate if the operation fails.
 *
 * Returns: true on success, false on failure.
 *
 * The block data is accessed via chunkBlockIndex() so that the
 * flattening/indexing details remain encapsulated here.
 */
bool chunkSetBlock(chunk_t *chunk, int x, int y, int z, blockId_t block, owsg_err *err);

/*
 * Returns whether a block type occupies space / blocks visibility
 * through it - i.e. whether a face between this block and a
 * neighboring one should be considered "hidden" during mesh
 * generation.
 */
static inline bool blockIsSolid(blockId_t block)
{
    return block != BLOCK_AIR;
}

#endif /* WORLD_CHUNK_H_ */
