#include "world/worldgen.h"

bool worldGenInit(worldGen_t *worldGen, int64_t seed,
                  double terrainFrequency, int terrainOctaves,
                  double terrainPersistence, double terrainLacunarity,
                  double heightFalloffScale,
                  owsg_err *err)
{
    if (worldGen == NULL)
    {
        owsgErrSet(err, "World generator is NULL");
        return false;
    }

    noise_t terrainNoise;

    if (!noiseInit(&terrainNoise, seed, err))
        return false;

    worldGen->terrainNoise = terrainNoise;
    worldGen->terrainFrequency = terrainFrequency;
    worldGen->terrainOctaves = terrainOctaves;
    worldGen->terrainPersistence = terrainPersistence;
    worldGen->terrainLacunarity = terrainLacunarity;
    worldGen->heightFalloffScale = heightFalloffScale;

    return true;
}

void worldGenDestroy(worldGen_t *worldGen)
{
    if (worldGen == NULL)
        return;

    noiseDestroy(&worldGen->terrainNoise);
}

bool worldGenDensity(const worldGen_t *worldGen, int32_t wx, int32_t wy, int32_t wz, double *outDensity, owsg_err *err)
{
    if (worldGen == NULL)
    {
        owsgErrSet(err, "World generator is NULL");
        return false;
    }

    if (outDensity == NULL)
    {
        owsgErrSet(err, "Output density pointer is NULL");
        return false;
    }

    double fbmValue;

    if (!noiseFbm3D(
            &worldGen->terrainNoise,
            (double)wx,
            (double)wy,
            (double)wz,
            worldGen->terrainOctaves,
            worldGen->terrainFrequency,
            worldGen->terrainPersistence,
            worldGen->terrainLacunarity,
            &fbmValue,
            err))
    {
        return false;
    }

    double heightBias = (double)wy / worldGen->heightFalloffScale;

    *outDensity = fbmValue - heightBias;

    return true;
}

bool worldGenFillChunk(const worldGen_t *worldGen, chunkCoord_t chunkCoord, chunk_t *outChunk, owsg_err *err)
{
    if (worldGen == NULL)
    {
        owsgErrSet(err, "World generator is NULL");
        return false;
    }

    if (outChunk == NULL)
    {
        owsgErrSet(err, "Output chunk pointer is NULL");
        return false;
    }

    for (int x = 0; x < CHUNK_SIZE_X; ++x)
    {
        for (int y = 0; y < CHUNK_SIZE_Y; ++y)
        {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z)
            {
                int32_t wx, wy, wz;
                worldChunkLocalToBlock(chunkCoord, x, y, z, &wx, &wy, &wz);

                double density;

                if (!worldGenDensity(worldGen, wx, wy, wz, &density, err))
                    return false;

                blockType_t block = density > 0.0 ? BLOCK_STONE : BLOCK_AIR;

                if (!chunkSetBlock(outChunk, x, y, z, block, err))
                    return false;
            }
        }
    }

    return true;
}
