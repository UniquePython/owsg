#ifndef WORLD_WORLDGEN_H_
#define WORLD_WORLDGEN_H_

#include "world/chunk.h"
#include "world/world.h"
#include "util/noise.h"
#include "util/owsg_err.h"

#include <stdint.h>
#include <stdbool.h>

/*
 * Owns the noise field(s) and tunable parameters used to procedurally
 * generate terrain, and provides the functions that turn them into
 * actual block data.
 *
 * A single worldGen_t is meant to be created once per world (seeded
 * from that world's seed) and reused for every chunk generated in it -
 * NOT recreated per chunk, since recreating it would mean
 * re-initializing the noise context repeatedly for no benefit.
 *
 * terrainNoise: noise field used for terrain SHAPE (the 3D density
 *               field described in worldGenDensity()'s doc comment).
 *               TODO: additional noise_t fields will join this struct
 *               as cave/climate generation are added - kept to just
 *               terrain shape for now.
 *
 * terrainFrequency, terrainOctaves, terrainPersistence,
 * terrainLacunarity: fBm parameters forwarded directly to
 *               noiseFbm3D() - see that function's doc comment in
 *               noise.h for what each one controls.
 *
 * heightFalloffScale: controls how many blocks of vertical distance it
 *               takes for the height bias (see worldGenDensity()) to
 *               overpower a "typical" fBm value and force air (going
 *               up) or solid (going down). Larger values = a thicker
 *               transition zone = terrain can rise/fall further before
 *               the bias dominates. TODO: no principled default yet -
 *               this is meant to be tuned by eye against actual
 *               rendered terrain, not derived from a formula.
 */
typedef struct
{
    noise_t terrainNoise;

    double terrainFrequency;
    int terrainOctaves;
    double terrainPersistence;
    double terrainLacunarity;

    double heightFalloffScale;
} worldGen_t;

/*
 * Initializes a terrain generator, seeding its noise field(s) from a
 * single world seed.
 *
 * worldGen: non-NULL worldGen_t to initialize. Left unchanged on
 *           failure.
 * seed: the world's seed. TODO: once cave/climate noise fields exist
 *       alongside terrainNoise, this function will need to derive
 *       DISTINCT per-field seeds from this one value (e.g. seed+0,
 *       seed+1, ...) rather than reusing it directly - not needed yet
 *       with only one field.
 * terrainFrequency, terrainOctaves, terrainPersistence,
 * terrainLacunarity, heightFalloffScale: initial values for the
 *       corresponding worldGen_t fields - see the struct's doc comment.
 * err: non-NULL error object to populate on failure.
 *
 * Returns true on success, false on failure (invalid arguments, or a
 * noise field failing to initialize).
 *
 * Ownership: on success, the caller is responsible for eventually
 * calling worldGenDestroy() on worldGen.
 */
bool worldGenInit(worldGen_t *worldGen, int64_t seed,
                  double terrainFrequency, int terrainOctaves,
                  double terrainPersistence, double terrainLacunarity,
                  double heightFalloffScale,
                  owsg_err *err);

/*
 * Releases the resources owned by a worldGen_t. Must not be used again
 * afterward except to be re-initialized by worldGenInit().
 *
 * worldGen: non-NULL worldGen_t to destroy.
 */
void worldGenDestroy(worldGen_t *worldGen);

/*
 * Computes the terrain density at a single world-space block
 * coordinate.
 *
 * The density field determines solidity: positive density means
 * "solid" (there's more terrain-noise "mass" here than the height
 * bias can cancel out), non-positive means "air". This single field
 * naturally produces overhangs, cliffs, and (eventually, once a
 * separate cave noise field is layered in - not yet) caves, because
 * "solid" is not simply a function of being below some per-column
 * height, unlike a heightmap-based approach.
 *
 * Conceptually:
 *
 *     density(x, y, z) = fbm3D(x, y, z) - heightBias(y)
 *
 * where heightBias(y) is small/negative deep underground (so density
 * stays positive - solid, even if the fbm term dips low) and
 * large/positive high in the sky (so density goes negative - air,
 * even if the fbm term spikes high). Near y = 0 (this world's chosen
 * reference/"sea" level - see worldGen_t.heightFalloffScale), the bias
 * is weak enough that the fbm term dominates, which is where
 * interesting terrain features actually come from.
 *
 * worldGen: non-NULL, already-initialized terrain generator.
 * wx, wy, wz: world-space block coordinates. May be negative.
 * outDensity: non-NULL output pointer, set to the computed density on
 *             success.
 * err: non-NULL error object to populate on failure.
 *
 * Returns true on success, false on failure (invalid arguments, or a
 * propagated failure from the underlying noiseFbm3D() call).
 */
bool worldGenDensity(const worldGen_t *worldGen, int32_t wx, int32_t wy, int32_t wz, double *outDensity, owsg_err *err);

/*
 * Measures how many blocks of solid material lie directly above the
 * given world-space block position before open air (density <= 0.0)
 * is reached, by walking upward and re-sampling worldGenDensity() at
 * each step.
 *
 * Used to decide surface block type: a block touching air above it
 * (depth 0) becomes grass, a few blocks below that becomes dirt, and
 * anything deeper than maxDepth is treated as "not near the surface"
 * (stone) without bothering to search further - saves noise samples
 * for blocks buried deep underground where the answer doesn't matter.
 *
 * IMPORTANT: this samples worldGenDensity() directly (NOT chunk_t /
 * world_t block lookups). worldGenFillChunk() may be filling a chunk
 * whose neighbor-above isn't generated yet, so density is the only
 * source of truth available at generation time that works
 * correctly regardless of chunk generation order.
 *
 * worldGen: non-NULL world generator (see worldGenDensity()).
 * wx, wy, wz: world-space coordinates of the SOLID block being
 *             probed (not the neighbor above it).
 * maxDepth: maximum number of blocks to search upward before giving up.
 * outDepth: non-NULL output pointer. On success, set to the number
 *           of solid blocks strictly between (wx,wy,wz) and the
 *           first air block above it - 0 if (wx,wy+1,wz) is already
 *           air, up to maxDepth if no air was found within range.
 * err: non-NULL error object, populated on failure (propagated from
 *      worldGenDensity()).
 *
 * Returns true on success (*outDepth is valid), false on failure.
 */
bool worldGenSurfaceDepth(const worldGen_t *worldGen, int32_t wx, int32_t wy, int32_t wz,
                          int maxDepth, int *outDepth, owsg_err *err);

/*
 * Fills every block of a chunk using worldGenDensity(): a block is
 * BLOCK_STONE if density > 0, BLOCK_AIR otherwise.
 *
 * worldGen: non-NULL, already-initialized terrain generator.
 * chunkCoord: the chunk-grid coordinate `outChunk` will occupy once
 *             placed in a world - needed to convert each local block
 *             position into the world-space coordinate
 *             worldGenDensity() expects. (Mirrors how
 *             meshGenerateFromChunk() takes an explicit chunkCoord
 *             for the same reason - see mesh.h.)
 * outChunk: non-NULL, already zero-initialized by the caller (see
 *           chunk_t's zero-init convention in chunk.h). Every block is
 *           overwritten by this function, so the caller does not need
 *           to pre-fill anything beyond zero-initializing.
 * err: non-NULL error object to populate on failure.
 *
 * Returns true on success, false on failure (invalid arguments, or a
 * propagated failure from worldGenDensity() or chunkSetBlock()).
 */
bool worldGenFillChunk(const worldGen_t *worldGen, chunkCoord_t chunkCoord, chunk_t *outChunk, owsg_err *err);

#endif /* WORLD_WORLDGEN_H_ */
