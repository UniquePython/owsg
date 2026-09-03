#include "util/noise.h"

bool noiseInit(noise_t *noise, int64_t seed, owsg_err *err)
{
    if (noise == NULL)
    {
        owsgErrSet(err, "Noise field is NULL");
        return false;
    }

    struct osn_context *ctx;

    /*
     * open_simplex_noise() returns 0 on success, non-zero on failure
     * (an internal allocation failure) - unlike this codebase's own
     * bool-returning convention, so the result is translated here
     * rather than propagated directly.
     */
    if (open_simplex_noise(seed, &ctx) != 0)
    {
        owsgErrSet(err, "Failed to initialize noise context (seed=%lld)", (long long)seed);
        return false;
    }

    noise->ctx = ctx;

    return true;
}

void noiseDestroy(noise_t *noise)
{
    if (noise == NULL || noise->ctx == NULL)
        return;

    open_simplex_noise_free(noise->ctx);
    noise->ctx = NULL;
}

bool noiseFbm2D(const noise_t *noise, double x, double y,
                int octaves, double frequency, double persistence, double lacunarity,
                double *outValue, owsg_err *err)
{
    if (noise == NULL || noise->ctx == NULL)
    {
        owsgErrSet(err, "Noise field is NULL or uninitialized");
        return false;
    }

    if (outValue == NULL)
    {
        owsgErrSet(err, "Output value pointer is NULL");
        return false;
    }

    if (octaves < 1)
    {
        owsgErrSet(err, "octaves must be >= 1 (got %d)", octaves);
        return false;
    }

    double total = 0.0;
    double currentAmplitude = 1.0;
    double currentFrequency = frequency;

    for (int i = 0; i < octaves; ++i)
    {
        double sample = open_simplex_noise2(
            noise->ctx,
            x * currentFrequency,
            y * currentFrequency);

        total += sample * currentAmplitude;

        currentAmplitude *= persistence;
        currentFrequency *= lacunarity;
    }

    *outValue = total;

    return true;
}

bool noiseFbm3D(const noise_t *noise, double x, double y, double z,
                int octaves, double frequency, double persistence, double lacunarity,
                double *outValue, owsg_err *err)
{
    if (noise == NULL || noise->ctx == NULL)
    {
        owsgErrSet(err, "Noise field is NULL or uninitialized");
        return false;
    }

    if (outValue == NULL)
    {
        owsgErrSet(err, "Output value pointer is NULL");
        return false;
    }

    if (octaves < 1)
    {
        owsgErrSet(err, "octaves must be >= 1 (got %d)", octaves);
        return false;
    }

    double total = 0.0;
    double currentAmplitude = 1.0;
    double currentFrequency = frequency;

    for (int i = 0; i < octaves; ++i)
    {
        double sample = open_simplex_noise3(
            noise->ctx,
            x * currentFrequency,
            y * currentFrequency,
            z * currentFrequency);

        total += sample * currentAmplitude;

        currentAmplitude *= persistence;
        currentFrequency *= lacunarity;
    }

    *outValue = total;

    return true;
}
