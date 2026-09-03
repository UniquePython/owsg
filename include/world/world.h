#ifndef WORLD_WORLD_H_
#define WORLD_WORLD_H_

#include "world/chunk.h"
#include "util/owsg_err.h"

#include <stdint.h>
#include <stdbool.h>

/*
 * Identifies a chunk's position on the chunk grid (NOT block
 * coordinates). Chunk (1,0,0) spans block x in [16,32), y in [0,16),
 * z in [0,16) - i.e. block coordinate = chunk coordinate * CHUNK_SIZE_*.
 *
 * Must remain free of padding bytes: stb_ds hashmaps compare keys
 * bit-for-bit, so any padding would be uninitialized garbage that
 * could make two logically-equal coordinates compare unequal. Three
 * consecutive int32_t fields are padding-free on any platform we
 * care about, but if this struct's shape ever changes, that
 * invariant needs to be re-checked.
 */
typedef struct
{
    int32_t x, y, z;
} chunkCoord_t;

/*
 * One entry in world_t's chunk hashmap (stb_ds hashmap-of-structs
 * convention: a struct with a `key` field and a `value` field).
 *
 * value is a POINTER to a heap-allocated chunk_t, not an inline
 * chunk_t - this keeps chunk data pointer-stable across hashmap
 * resizes (stb_ds may reallocate/rehash the entries array itself,
 * but never touches what a `value` pointer points to), and keeps
 * resize cost proportional to pointer size rather than chunk size.
 */
typedef struct
{
    chunkCoord_t key;
    chunk_t *value;
} chunkEntry_t;

/*
 * Owns every currently-loaded chunk, keyed by chunk-grid coordinate.
 *
 * chunks: stb_ds hashmap. NULL is the valid empty-map state (this is
 *         an stb_ds convention - do not owsgAlloc this yourself).
 *         Do not access this field directly outside world.c - go
 *         through worldGetChunk() / worldSetChunk() so the
 *         hashmap-vs-something-else choice stays swappable later.
 */
typedef struct
{
    chunkEntry_t *chunks;
} world_t;

/*
 * Initializes an empty world (no chunks loaded).
 *
 * world: non-NULL world_t to initialize.
 */
void worldInit(world_t *world);

/*
 * Looks up the chunk at the given chunk-grid coordinate.
 *
 * world: non-NULL world to query.
 * coord: chunk-grid coordinate to look up.
 * outChunk: non-NULL. Written only when a chunk exists at that
 *           coordinate (i.e. only when this returns true). The
 *           written pointer is owned by `world` and remains valid
 *           until that chunk is unloaded (no unloading mechanism
 *           exists yet) - callers must not free it.
 * err: may be NULL. Populated only on invalid arguments (world or
 *      outChunk was NULL) - NOT populated when the chunk simply
 *      isn't loaded, since that's an expected outcome, not a failure.
 *
 * Returns true if a chunk exists at that coordinate (in which case
 * *outChunk was written), false otherwise - either because no chunk
 * is loaded at that coordinate (the common case; err is left
 * untouched), or because of invalid arguments (err is populated).
 * Callers that need to tell these two false cases apart should check
 * err; meshing code, which only cares "do I have a block to read or
 * not", does not need to.
 */
bool worldGetChunk(const world_t *world, chunkCoord_t coord, chunk_t **outChunk, owsg_err *err);

/*
 * Inserts a heap-allocated chunk at the given coordinate, taking
 * ownership of it.
 *
 * If a chunk already exists at that coordinate, it is freed and
 * replaced by the new one - the caller does not need to check first
 * or free the old chunk themselves.
 *
 * world: non-NULL world to insert into.
 * coord: chunk-grid coordinate to insert at.
 * chunk: non-NULL, heap-allocated chunk. `world` takes ownership -
 *        caller must not free it or use it after a successful call.
 * err: may be NULL. Populated on failure.
 *
 * Returns true on success, false on failure (invalid arguments, or an
 * allocation failure inside the hashmap itself).
 */
bool worldSetChunk(world_t *world, chunkCoord_t coord, chunk_t *chunk, owsg_err *err);

/*
 * Converts a world-space block coordinate into the chunk-grid
 * coordinate of the chunk containing it, plus that block's local
 * coordinate within that chunk.
 *
 * wx, wy, wz: world-space block coordinates. May be negative.
 * outCoord: non-NULL, receives the chunk-grid coordinate.
 * outLocalX, outLocalY, outLocalZ: non-NULL, each receives a value in
 *          [0, CHUNK_SIZE_* - 1] - the block's position within
 *          *outCoord's chunk.
 *
 * This function has no failure mode worth reporting (it's pure
 * arithmetic on values already required to be non-NULL out-params by
 * its signature), so - unlike the rest of this header - it does not
 * take an owsg_err* or return bool. TODO: revisit if that stops being
 * true.
 */
void worldBlockToChunkLocal(int32_t wx, int32_t wy, int32_t wz,
                            chunkCoord_t *outCoord,
                            int *outLocalX, int *outLocalY, int *outLocalZ);

/*
 * Result of a world-space block lookup - distinguishes "this position
 * is definitely air/solid" from "we don't know yet" (the chunk
 * containing it isn't loaded), so callers (chiefly meshing) can react
 * to the two cases differently.
 */
typedef enum
{
    BLOCK_LOOKUP_OK,             /* chunk loaded; outBlock is valid */
    BLOCK_LOOKUP_CHUNK_UNLOADED, /* chunk not loaded; outBlock is NOT valid */
} blockLookupResult_t;

/*
 * Looks up the block at a world-space coordinate, which may belong to
 * any loaded chunk (not just one specific chunk) - this is the
 * function meshing will call for neighbor checks that cross a chunk
 * boundary.
 *
 * world: non-NULL world to query.
 * wx, wy, wz: world-space block coordinates. May be negative.
 * outBlock: non-NULL. Written only when *outResult ends up
 *           BLOCK_LOOKUP_OK.
 * outResult: non-NULL. Written whenever this function returns true -
 *            tells the caller whether the chunk was loaded
 *            (BLOCK_LOOKUP_OK, outBlock valid) or not
 *            (BLOCK_LOOKUP_CHUNK_UNLOADED, outBlock untouched). Kept
 *            separate from this function's true/false return so
 *            "chunk not loaded yet" (normal, expected) can never be
 *            confused with "you passed me a NULL pointer" (a bug).
 * err: may be NULL. Populated only when this function returns false.
 *
 * Returns true if the lookup was performed (regardless of whether the
 * block was actually found - check *outResult for that), false on
 * invalid arguments (world, outBlock, or outResult was NULL).
 */
bool worldGetBlock(const world_t *world, int32_t wx, int32_t wy, int32_t wz,
                   blockId_t *outBlock, blockLookupResult_t *outResult, owsg_err *err);

/*
 * Frees every loaded chunk and the hashmap itself.
 *
 * world: non-NULL world to destroy. Must not be used again afterward
 *        except to be re-populated by worldInit().
 */
void worldDestroy(world_t *world);

#endif /* WORLD_WORLD_H_ */
