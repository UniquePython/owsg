#ifndef UTIL_NOISE_H_
#define UTIL_NOISE_H_

#include "util/owsg_err.h"

#include <open-simplex-noise/open-simplex-noise.h>

#include <stdint.h>
#include <stdbool.h>

/*
 * A single seeded noise field, wrapping one open-simplex-noise context.
 *
 * Each independent "layer" of the game's generation (terrain shape,
 * cave tunnels, cave caverns, temperature, humidity, ...) should own
 * its own noise_t, seeded DIFFERENTLY from the others (see
 * noiseInit()) - reusing one context for multiple purposes would make
 * those purposes perfectly correlated with each other, which defeats
 * the point of treating them as independent axes.
 *
 * ctx: opaque pointer to the underlying open-simplex-noise context.
 *      Owned by this noise_t; freed by noiseDestroy(). NULL only
 *      before a successful noiseInit() or after noiseDestroy().
 */
typedef struct
{
    struct osn_context *ctx;
} noise_t;

/*
 * Seeds and initializes a noise field.
 *
 * noise: non-NULL noise_t to initialize. Left unchanged on failure.
 *
 * seed: seed for this field. Reusing the same seed always reproduces
 *       the same field. Different fields should use DIFFERENT seeds
 *       (e.g. derived from one world seed by adding a small per-field
 *       offset) - TODO: decide the actual derivation scheme when we
 *       wire this into world generation.
 *
 * err: non-NULL error object to populate on failure.
 *
 * Returns true on success, false on failure (invalid arguments, or an
 * allocation failure inside open-simplex-noise).
 *
 * Ownership: on success, the caller is responsible for eventually
 * calling noiseDestroy() on noise.
 */
bool noiseInit(noise_t *noise, int64_t seed, owsg_err *err);

/*
 * Releases the resources owned by a noise_t. Must not be used again
 * afterward except to be re-initialized by noiseInit().
 *
 * noise: non-NULL noise_t to destroy. Safe to call even if noiseInit()
 *        was never called on it, as long as it was zero-initialized
 *        first (ctx == NULL is treated as "nothing to free").
 */
void noiseDestroy(noise_t *noise);

/*
 * Fractal Brownian motion (fBm): sums multiple octaves of the
 * underlying 2D noise at increasing frequency and decreasing
 * amplitude, to combine broad, large-scale variation with finer
 * detail in a single value.
 *
 * noise: non-NULL, already-initialized noise field to sample.
 * x, y: sample position.
 * octaves: number of noise layers to sum. Must be >= 1. More octaves
 *          = more fine detail, but more expensive (one full noise
 *          call per octave).
 * frequency: base frequency of the FIRST octave. Roughly "how
 *            zoomed-in" the noise is - smaller values = broader,
 *            slower-changing features.
 * persistence: amplitude multiplier applied each octave (e.g. 0.5).
 *              Values <1 make later, higher-frequency octaves
 *              contribute progressively less.
 * lacunarity: frequency multiplier applied each octave (e.g. 2.0).
 *             Values >1 make later octaves progressively "busier".
 * outValue: non-NULL output pointer, set to the summed value on
 *           success. NOT currently normalized to any fixed range -
 *           TODO: decide whether callers need normalization once we
 *           see real output ranges from actual terrain/climate use.
 * err: non-NULL error object to populate on failure.
 *
 * Returns true on success, false on failure (invalid arguments, e.g.
 * octaves < 1).
 */
bool noiseFbm2D(const noise_t *noise, double x, double y,
                int octaves, double frequency, double persistence, double lacunarity,
                double *outValue, owsg_err *err);

/*
 * 3D counterpart to noiseFbm2D() - see that function's doc comment
 * for parameter meanings, identical here with an added z coordinate.
 */
bool noiseFbm3D(const noise_t *noise, double x, double y, double z,
                int octaves, double frequency, double persistence, double lacunarity,
                double *outValue, owsg_err *err);

#endif /* UTIL_NOISE_H_ */
