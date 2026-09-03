#include "graphics/mesh.h"
#include "util/alloc.h"

#include <glad/gl.h>
#include <stddef.h>

/*
 * One entry per cube face direction. Both passes (count and emit)
 * iterate this table identically, so the set of "6 possible faces" is
 * defined in exactly one place.
 *
 * neighborOffset: the (dx,dy,dz) to add to a solid block's coordinate
 *                 to find the neighbor that would occlude this face.
 *
 * corners: the 4 local corner positions of this face, relative to the
 *          block's local-space origin. A block spans [0,1] on each
 *          axis here - this convention change makes "block at grid position
 *          (x,y,z)" map directly to world-space [x,x+1] etc. without
 *          an extra offset. Listed in counter-clockwise winding order
 *          when viewed from outside the cube.
 */
typedef struct
{
    int neighborOffset[3];
    float corners[4][3];
} faceDirection_t;

static const faceDirection_t FACE_DIRECTIONS[6] = {
    /* +X (right) */
    {
        .neighborOffset = {1, 0, 0},
        .corners = {
            {1.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 0.0f},
        },
    },
    /* -X (left) */
    {
        .neighborOffset = {-1, 0, 0},
        .corners = {
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 1.0f, 1.0f},
        },
    },
    /* +Y (top) */
    {
        .neighborOffset = {0, 1, 0},
        .corners = {
            {0.0f, 1.0f, 0.0f},
            {1.0f, 1.0f, 0.0f},
            {1.0f, 1.0f, 1.0f},
            {0.0f, 1.0f, 1.0f},
        },
    },
    /* -Y (bottom) */
    {
        .neighborOffset = {0, -1, 0},
        .corners = {
            {0.0f, 0.0f, 1.0f},
            {1.0f, 0.0f, 1.0f},
            {1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.0f},
        },
    },
    /* +Z (front) */
    {
        .neighborOffset = {0, 0, 1},
        .corners = {
            {0.0f, 0.0f, 1.0f},
            {1.0f, 0.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},
            {0.0f, 1.0f, 1.0f},
        },
    },
    /* -Z (back) */
    {
        .neighborOffset = {0, 0, -1},
        .corners = {
            {1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {1.0f, 1.0f, 0.0f},
        },
    },
};

#define FACE_DIRECTION_COUNT 6

static void getBlockColor(blockId_t block, float outColor[3])
{
    switch (block)
    {
    case BLOCK_STONE:
        outColor[0] = 0.5f;
        outColor[1] = 0.5f;
        outColor[2] = 0.5f;
        break;
    default:
        outColor[0] = 1.0f;
        outColor[1] = 0.0f;
        outColor[2] = 1.0f;
        break;
    }
}

/*
 * Determines whether the face of the solid block at local coordinate
 * (x,y,z) WITHIN `chunk` in direction FACE_DIRECTIONS[dir] should be
 * emitted.
 *
 * If the neighbor position is still within `chunk`'s own bounds, this
 * is resolved locally via chunkGetBlock() (same as before this
 * function became world-aware). Otherwise the neighbor belongs to a
 * DIFFERENT chunk - see world/mesh.h's meshGenerateFromChunk() doc
 * comment for the exact policy applied when that neighboring chunk
 * isn't loaded.
 *
 * world: non-NULL world `chunk` belongs to.
 * chunkCoord: `chunk`'s position on the chunk grid - needed to
 *             convert an out-of-bounds local neighbor coordinate into
 *             a world-space coordinate.
 * chunk: non-NULL chunk being meshed.
 * x, y, z: LOCAL coordinates (within `chunk`) of the SOLID block
 *          whose face is being considered (not the neighbor).
 * dir: index into FACE_DIRECTIONS, selecting which face/direction to
 *      check.
 * outShouldEmit: non-NULL output pointer, set to the result on
 *                success.
 * err: non-NULL error object, populated on failure.
 *
 * Returns true on success (*outShouldEmit is valid), false on
 * failure.
 */
static bool shouldEmitFace(const world_t *world, chunkCoord_t chunkCoord, const chunk_t *chunk,
                           int x, int y, int z, int dir, bool *outShouldEmit, owsg_err *err)
{
    int nx = x + FACE_DIRECTIONS[dir].neighborOffset[0];
    int ny = y + FACE_DIRECTIONS[dir].neighborOffset[1];
    int nz = z + FACE_DIRECTIONS[dir].neighborOffset[2];

    /* Still-local neighbor: resolve directly against this chunk. */
    if (nx >= 0 && nx < CHUNK_SIZE_X &&
        ny >= 0 && ny < CHUNK_SIZE_Y &&
        nz >= 0 && nz < CHUNK_SIZE_Z)
    {
        blockId_t neighborBlock;

        if (!chunkGetBlock(chunk, nx, ny, nz, &neighborBlock, err))
            return false;

        *outShouldEmit = !blockIsSolid(neighborBlock);
        return true;
    }

    /*
     * The neighbor lies outside this chunk. Convert its local
     * coordinate into a world-space block coordinate.
     *
     * For example, if chunkCoord.x == 2 and nx == -1:
     *
     *     worldX = 2 * CHUNK_SIZE_X + (-1)
     *            = 32 - 1
     *            = 31
     *
     * which is the final block of chunk x == 1.
     */
    int32_t worldX = chunkCoord.x * CHUNK_SIZE_X + nx;
    int32_t worldY = chunkCoord.y * CHUNK_SIZE_Y + ny;
    int32_t worldZ = chunkCoord.z * CHUNK_SIZE_Z + nz;

    blockId_t neighborBlock;
    blockLookupResult_t lookupResult;

    if (!worldGetBlock(
            world,
            worldX,
            worldY,
            worldZ,
            &neighborBlock,
            &lookupResult,
            err))
    {
        return false;
    }

    if (lookupResult == BLOCK_LOOKUP_OK)
    {
        /*
         * Neighboring chunk is loaded, so use its actual block.
         */
        *outShouldEmit = !blockIsSolid(neighborBlock);
        return true;
    }

    /*
     * Neighboring chunk isn't loaded. Per the new meshing policy,
     * treat it as solid so that we DON'T generate a boundary face
     * that may later turn out to be hidden by a neighboring chunk.
     */
    *outShouldEmit = false;
    return true;
}

bool meshGenerateFromChunk(const world_t *world, chunkCoord_t chunkCoord, const chunk_t *chunk,
                           mesh_t *outMesh, owsg_err *err)
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

    if (outMesh == NULL)
    {
        owsgErrSet(err, "Output mesh pointer is NULL");
        return false;
    }

    /*
     * --- Pass 1: count ---
     */
    unsigned int vertexCount = 0;
    unsigned int indexCount = 0;

    for (int x = 0; x < CHUNK_SIZE_X; ++x)
    {
        for (int y = 0; y < CHUNK_SIZE_Y; ++y)
        {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z)
            {
                blockId_t block;

                if (!chunkGetBlock(chunk, x, y, z, &block, err))
                    return false;

                if (!blockIsSolid(block))
                    continue;

                for (int dir = 0; dir < FACE_DIRECTION_COUNT; ++dir)
                {
                    bool shouldEmit;

                    if (!shouldEmitFace(world, chunkCoord, chunk, x, y, z, dir, &shouldEmit, err))
                        return false;

                    if (shouldEmit)
                    {
                        vertexCount += 4;
                        indexCount += 6;
                    }
                }
            }
        }
    }

    /*
     * Empty mesh: no visible faces.
     */
    if (vertexCount == 0)
    {
        outMesh->vao = 0;
        outMesh->vbo = 0;
        outMesh->ebo = 0;
        outMesh->indexCount = 0;

        return true;
    }

    /*
     * --- Allocate exactly enough memory for pass 2 ---
     */
    /*
     * 9 floats per vertex: position (3) + color (3) + normal (3). The
     * normal is constant across all 4 vertices of a given face (cube
     * faces are flat), but is still stored per-vertex rather than
     * per-face/uniform, since that's what lets a single shared vertex
     * shader attribute layout work for arbitrary (eventually
     * non-cube) mesh data later - flat shading via a per-vertex
     * attribute costs a few duplicated floats now in exchange for not
     * having to special-case cube faces in the shader.
     */
    float *vertices;
    if (!owsgAlloc((size_t)vertexCount * 9 * sizeof(float), &vertices))
    {
        owsgErrSet(err, "Failed to allocate %u vertices", vertexCount);
        return false;
    }

    unsigned int *indices;
    if (!owsgAlloc((size_t)indexCount * sizeof(unsigned int), &indices))
    {
        owsgErrSet(err, "Failed to allocate %u indices", indexCount);
        owsgFree(&vertices);
        return false;
    }

    /*
     * --- Pass 2: emit ---
     */
    unsigned int vertexWriteCount = 0;
    unsigned int indexWriteCount = 0;

    for (int x = 0; x < CHUNK_SIZE_X; ++x)
    {
        for (int y = 0; y < CHUNK_SIZE_Y; ++y)
        {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z)
            {
                blockId_t block;

                if (!chunkGetBlock(chunk, x, y, z, &block, err))
                {
                    owsgFree(&vertices);
                    owsgFree(&indices);
                    return false;
                }

                if (!blockIsSolid(block))
                    continue;

                float color[3];
                getBlockColor(block, color);

                for (int dir = 0; dir < FACE_DIRECTION_COUNT; ++dir)
                {
                    bool shouldEmit;

                    if (!shouldEmitFace(world, chunkCoord, chunk, x, y, z, dir, &shouldEmit, err))
                    {
                        owsgFree(&vertices);
                        owsgFree(&indices);
                        return false;
                    }

                    if (!shouldEmit)
                        continue;

                    /*
                     * The first vertex generated for this face becomes
                     * the base index used by the six face indices.
                     */
                    unsigned int baseVertex = vertexWriteCount;

                    /*
                     * Every vertex of this face shares the same
                     * outward-facing normal - neighborOffset is
                     * already a unit vector in that direction, so no
                     * separate normal table is needed.
                     */
                    float normal[3] = {
                        (float)FACE_DIRECTIONS[dir].neighborOffset[0],
                        (float)FACE_DIRECTIONS[dir].neighborOffset[1],
                        (float)FACE_DIRECTIONS[dir].neighborOffset[2]};

                    for (int corner = 0; corner < 4; ++corner)
                    {
                        const float *offset = FACE_DIRECTIONS[dir].corners[corner];

                        size_t vertexOffset = (size_t)vertexWriteCount * 9;

                        vertices[vertexOffset + 0] = (float)x + offset[0];
                        vertices[vertexOffset + 1] = (float)y + offset[1];
                        vertices[vertexOffset + 2] = (float)z + offset[2];

                        vertices[vertexOffset + 3] = color[0];
                        vertices[vertexOffset + 4] = color[1];
                        vertices[vertexOffset + 5] = color[2];

                        vertices[vertexOffset + 6] = normal[0];
                        vertices[vertexOffset + 7] = normal[1];
                        vertices[vertexOffset + 8] = normal[2];

                        ++vertexWriteCount;
                    }

                    /*
                     * Four vertices:
                     *
                     *   0 ---- 1
                     *   |    / |
                     *   |  /   |
                     *   3 ---- 2
                     *
                     * Two triangles: 0-1-2 and 0-2-3.
                     */
                    indices[indexWriteCount++] = baseVertex + 0;
                    indices[indexWriteCount++] = baseVertex + 1;
                    indices[indexWriteCount++] = baseVertex + 2;

                    indices[indexWriteCount++] = baseVertex + 0;
                    indices[indexWriteCount++] = baseVertex + 2;
                    indices[indexWriteCount++] = baseVertex + 3;
                }
            }
        }
    }

    /*
     * Optional sanity checks: pass 2 must produce exactly what pass 1
     * counted. These should always hold because both passes use the same
     * traversal and culling logic.
     */
    if (vertexWriteCount != vertexCount || indexWriteCount != indexCount)
    {
        owsgErrSet(err, "Mesh generation count mismatch");
        owsgFree(&vertices);
        owsgFree(&indices);
        return false;
    }

    /* --- Upload to GPU --- */
    unsigned int vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, ((size_t)vertexCount * 9 * sizeof(float)), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), NULL);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ((size_t)indexCount * sizeof(unsigned int)), indices, GL_STATIC_DRAW);

    /*
     * The GPU now has its own copy of this data.
     */
    owsgFree(&vertices);
    owsgFree(&indices);

    outMesh->vao = vao;
    outMesh->vbo = vbo;
    outMesh->ebo = ebo;
    outMesh->indexCount = indexCount;

    return true;
}

void meshDraw(const mesh_t *mesh)
{
    if (mesh == NULL || mesh->indexCount == 0)
        return;

    glBindVertexArray(mesh->vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)mesh->indexCount, GL_UNSIGNED_INT, NULL);
}

void meshDestroy(mesh_t *mesh)
{
    if (mesh == NULL)
        return;

    glDeleteVertexArrays(1, &mesh->vao);
    glDeleteBuffers(1, &mesh->vbo);
    glDeleteBuffers(1, &mesh->ebo);

    mesh->vao = 0;
    mesh->vbo = 0;
    mesh->ebo = 0;
    mesh->indexCount = 0;
}
