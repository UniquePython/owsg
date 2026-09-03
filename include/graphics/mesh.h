#ifndef GRAPHICS_MESH_H_
#define GRAPHICS_MESH_H_

#include "world/chunk.h"
#include "world/world.h"
#include "util/owsg_err.h"

#include <stdbool.h>

/*
 * A GPU-resident mesh generated from a chunk's block data: one VAO
 * owning a vertex buffer (position + color, interleaved) and an index
 * buffer, containing only the CULLED, VISIBLE faces of the chunk's
 * solid blocks.
 *
 * A mesh_t is drawn with a single ordinary (non-instanced) glDrawElements
 * call - the mesh already represents the chunk's complete visible geometry
 * as one piece, since face culling happened at generation time rather than
 * at draw time.
 */
typedef struct
{
    unsigned int vao;
    unsigned int vbo;
    unsigned int ebo;

    /* Number of indices in the EBO - what glDrawElements needs to
     * know how much to draw. NOT the same as vertex count (each face
     * is 4 vertices but 6 indices, per the two-triangles-per-quad
     * pattern). */
    unsigned int indexCount;
} mesh_t;

/*
 * Generates a GPU mesh from a chunk's block data, using face culling:
 * only faces between a solid block and a non-solid neighbor are
 * included. Faces between two solid blocks are omitted entirely,
 * since they can never be seen.
 *
 * A neighbor may fall outside `chunk`'s own bounds - in that case it
 * belongs to a DIFFERENT chunk, resolved via `world`:
 *   - if the neighboring chunk is loaded, its actual block is used,
 *     same as an in-bounds neighbor would be.
 *   - if the neighboring chunk is NOT loaded (worldGetBlock() reports
 *     BLOCK_LOOKUP_CHUNK_UNLOADED), the neighbor is treated as SOLID
 *     (the face is NOT emitted). This is a deliberate, conservative
 *     policy: it avoids ever drawing a face that might turn out to be
 *     hidden once that neighbor chunk loads, at the cost of
 *     under-drawing (a temporary missing face) at the frontier of
 *     loaded terrain until neighbors are loaded and this chunk is
 *     remeshed. TODO: remeshing-on-neighbor-load doesn't exist yet -
 *     future milestone.
 *
 * Uses a two-pass approach: pass 1 counts exactly how many faces will
 * be emitted (by running the same culling logic with no output),
 * pass 2 allocates EXACTLY that much memory and emits the real vertex
 * /index data. This trades a small, chunk-scale amount of redundant
 * CPU work (the culling check runs twice per block) for zero wasted
 * memory in the final buffers.
 *
 * world: non-NULL world that `chunk` belongs to - used to resolve
 *        neighbor lookups that cross `chunk`'s boundary into
 *        adjacent chunks.
 * chunkCoord: `chunk`'s position on the chunk grid. Needed to convert
 *             a local out-of-bounds neighbor coordinate (see
 *             shouldEmitFace() in mesh.c) into the world-space
 *             coordinate worldGetBlock() expects.
 * chunk: non-NULL chunk to generate a mesh from. This should be the
 *        same chunk stored in `world` at `chunkCoord` - it is taken
 *        as an explicit parameter (rather than re-fetched from
 *        `world` internally) so callers that already hold the
 *        pointer don't pay for a redundant hashmap lookup.
 * outMesh: non-NULL mesh_t to populate on success. Left unchanged on
 *          failure.
 * err: non-NULL error object to populate on failure - this can
 *      surface either an allocation failure, or a propagated failure
 *      from the internal per-block neighbor lookup (either
 *      chunkGetBlock() for in-bounds neighbors, or worldGetBlock()
 *      for cross-chunk neighbors - shouldn't happen given correct
 *      traversal bounds and a correctly-constructed chunkCoord, but
 *      is not silently ignored if it somehow does).
 *
 * Returns true on success, false on failure.
 *
 * Ownership: on success, the caller is responsible for eventually
 * calling meshDestroy() on outMesh.
 */
bool meshGenerateFromChunk(const world_t *world, chunkCoord_t chunkCoord, const chunk_t *chunk,
                           mesh_t *outMesh, owsg_err *err);

/*
 * Issues a single draw call for this mesh's complete geometry.
 *
 * Does NOT bind a shader - the caller is responsible for having
 * called shaderUse() on whatever shader program should render this
 * mesh, same as the existing draw calls in main.c.
 *
 * mesh: non-NULL mesh previously created by meshGenerateFromChunk().
 *       Safe to call on a mesh with indexCount == 0 (empty mesh) -
 *       this is a no-op in that case.
 */
void meshDraw(const mesh_t *mesh);

/*
 * Deletes the underlying GPU buffers (VAO/VBO/EBO). The mesh_t must
 * not be used again afterward (except to be re-populated by another
 * meshGenerateFromChunk() call). Safe to call on an empty mesh
 * (indexCount == 0, vao/vbo/ebo == 0) - glDelete* calls are no-ops on
 * object id 0.
 *
 * mesh: non-NULL mesh to destroy.
 */
void meshDestroy(mesh_t *mesh);

#endif /* GRAPHICS_MESH_H_ */
