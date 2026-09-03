#include "util/owsg_err.h"
#include "util/log.h"
#include "util/alloc.h"
#include "graphics/window.h"
#include "graphics/shader.h"
#include "graphics/camera.h"
#include "graphics/mesh.h"
#include "world/chunk.h"
#include "world/worldgen.h"
#include "world/world.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include <glad/gl.h>
#include <cglm/cglm.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define WINDOW_TITLE "owsg"

/*
 * Bounds (inclusive) of the test generation grid, in CHUNK-GRID
 * coordinates (not blocks) - e.g. GRID_MIN_X=-4, GRID_MAX_X=3 covers
 * chunk x in [-4, 3], which is 8 chunks along that axis.
 *
 * TODO: this whole fixed, generate-everything-at-startup grid is a
 * deliberate stand-in for real chunk streaming (load near the player,
 * unload far away), which doesn't exist yet. Fine for now: it lets us
 * SEE generated terrain without first solving streaming.
 *
 * Y range is intentionally smaller and asymmetric around 0 (this
 * world's reference/"sea" level - see worldgen.h) - we want to see a
 * bit of solid ground below it and open sky above it, not the full
 * -35000..+45000 block world range.
 */
#define GRID_MIN_X (-4)
#define GRID_MAX_X 3
#define GRID_MIN_Y (-2)
#define GRID_MAX_Y 3
#define GRID_MIN_Z (-4)
#define GRID_MAX_Z 3

#define GRID_SIZE_X (GRID_MAX_X - GRID_MIN_X + 1)
#define GRID_SIZE_Y (GRID_MAX_Y - GRID_MIN_Y + 1)
#define GRID_SIZE_Z (GRID_MAX_Z - GRID_MIN_Z + 1)

#define GRID_CHUNK_COUNT (GRID_SIZE_X * GRID_SIZE_Y * GRID_SIZE_Z)

/*
 * Starting worldgen tuning values. See worldgen.h's worldGen_t doc
 * comment for what each one controls.
 *
 * TODO: these are a first guess meant to be tuned by eye once terrain
 * is actually on screen, not principled final values - expect to
 * change them.
 */
#define WORLDGEN_SEED 1234
#define WORLDGEN_TERRAIN_FREQUENCY 0.01
#define WORLDGEN_TERRAIN_OCTAVES 4
#define WORLDGEN_TERRAIN_PERSISTENCE 0.5
#define WORLDGEN_TERRAIN_LACUNARITY 2.0
#define WORLDGEN_HEIGHT_FALLOFF_SCALE 40.0

/*
 * Pairs a chunk's grid coordinate with the GPU mesh generated from it,
 * so the render loop knows what model-matrix translation to apply to
 * each mesh (mesh vertices are in that chunk's own LOCAL [0,16)
 * space - see mesh.c - so placing them in the world is a per-chunk
 * translation applied at draw time, not baked into the vertex data).
 *
 * Deliberately local to main.c, not part of world_t - world_t owns
 * block data only; pairing chunks with render state is a separate
 * concern we're intentionally not solving generally yet.
 */
typedef struct
{
    chunkCoord_t coord;
    mesh_t mesh;
} renderChunk_t;

int main(void)
{
    bool useColor = isatty(STDERR_FILENO) != 0;
    logSetColorEnabled(useColor);

    logInfo("Colored output: %s", useColor ? "enabled" : "disabled");

    window_t window;
    owsg_err err = {0};

    if (!windowCreate(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, &window, &err))
    {
        logError("Window creation failed: " ERR_FMT, ERR_ARG(err));
        return EXIT_FAILURE;
    }

    /* --- load the shader program --- */
    shader_t shader;
    err = (owsg_err){0};

    const char *vertShader = "shaders/vert/shader.vert";
    const char *fragShader = "shaders/frag/shader.frag";

    if (!shaderCreate(vertShader, fragShader, &shader, &err))
    {
        logError("Shader program loading failed: " ERR_FMT, ERR_ARG(err));
        windowDestroy(&window);
        return EXIT_FAILURE;
    }

    logInfo("Loaded vertex shader: '%s' successfully!", vertShader);
    logInfo("Loaded fragment shader: '%s' successfully!", fragShader);

    /* --- set up the world and its terrain generator --- */
    world_t world;
    worldInit(&world);

    worldGen_t worldGen;
    err = (owsg_err){0};

    if (!worldGenInit(&worldGen,
                      WORLDGEN_SEED,
                      WORLDGEN_TERRAIN_FREQUENCY,
                      WORLDGEN_TERRAIN_OCTAVES,
                      WORLDGEN_TERRAIN_PERSISTENCE,
                      WORLDGEN_TERRAIN_LACUNARITY,
                      WORLDGEN_HEIGHT_FALLOFF_SCALE,
                      &err))
    {
        logError("World generator initialization failed: " ERR_FMT, ERR_ARG(err));
        worldDestroy(&world);
        shaderDestroy(&shader);
        windowDestroy(&window);
        return EXIT_FAILURE;
    }

    /*
     * Allocate render state for every chunk in the generation grid.
     *
     * owsGAlloc() returns bool and writes the allocated pointer through
     * its output parameter.
     */
    renderChunk_t *renderChunks = NULL;

    if (!owsgAlloc(sizeof(renderChunk_t) * GRID_CHUNK_COUNT, &renderChunks))
    {
        logError("Render chunk allocation failed");

        worldGenDestroy(&worldGen);
        worldDestroy(&world);
        shaderDestroy(&shader);
        windowDestroy(&window);
        return EXIT_FAILURE;
    }

    /*
     * Generate every chunk in the requested grid.
     */
    int chunkIndex = 0;

    for (int x = GRID_MIN_X; x <= GRID_MAX_X; ++x)
    {
        for (int y = GRID_MIN_Y; y <= GRID_MAX_Y; ++y)
        {
            for (int z = GRID_MIN_Z; z <= GRID_MAX_Z; ++z)
            {
                renderChunks[chunkIndex].coord = (chunkCoord_t){
                    .x = x,
                    .y = y,
                    .z = z};

                chunk_t *chunk = NULL;

                if (!owsgAlloc(sizeof(chunk_t), &chunk))
                {
                    logError("Chunk allocation failed");

                    owsgFree(&renderChunks);
                    worldGenDestroy(&worldGen);
                    worldDestroy(&world);
                    shaderDestroy(&shader);
                    windowDestroy(&window);
                    return EXIT_FAILURE;
                }

                /*
                 * chunk_t uses zero initialization as its established
                 * initialization convention.
                 */
                memset(chunk, 0, sizeof(*chunk));

                if (!worldGenFillChunk(&worldGen,
                                       renderChunks[chunkIndex].coord,
                                       chunk,
                                       &err))
                {
                    logError("Chunk generation failed: " ERR_FMT, ERR_ARG(err));

                    owsgFree(&chunk);
                    owsgFree(&renderChunks);
                    worldGenDestroy(&worldGen);
                    worldDestroy(&world);
                    shaderDestroy(&shader);
                    windowDestroy(&window);
                    return EXIT_FAILURE;
                }

                /*
                 * worldSetChunk() takes ownership of chunk on success.
                 * Therefore, only free chunk ourselves when this call
                 * fails.
                 */
                if (!worldSetChunk(&world,
                                   renderChunks[chunkIndex].coord,
                                   chunk,
                                   &err))
                {
                    logError("Chunk insertion failed: " ERR_FMT, ERR_ARG(err));

                    owsgFree(&chunk);
                    owsgFree(&renderChunks);
                    worldGenDestroy(&worldGen);
                    worldDestroy(&world);
                    shaderDestroy(&shader);
                    windowDestroy(&window);
                    return EXIT_FAILURE;
                }

                ++chunkIndex;
            }
        }
    }

    /*
     * The generator is no longer needed after all chunks have been
     * generated.
     */
    worldGenDestroy(&worldGen);

    /* --- generate meshes for every generated chunk --- */
    for (int i = 0; i < GRID_CHUNK_COUNT; ++i)
    {
        chunk_t *chunk = NULL;

        if (!worldGetChunk(&world, renderChunks[i].coord, &chunk, &err))
        {
            logError("Chunk lookup failed: " ERR_FMT, ERR_ARG(err));

            for (int j = 0; j < i; ++j)
                meshDestroy(&renderChunks[j].mesh);

            owsgFree(&renderChunks);
            worldDestroy(&world);
            shaderDestroy(&shader);
            windowDestroy(&window);
            return EXIT_FAILURE;
        }

        if (!meshGenerateFromChunk(&world,
                                   renderChunks[i].coord,
                                   chunk,
                                   &renderChunks[i].mesh,
                                   &err))
        {
            logError("Chunk mesh generation failed: " ERR_FMT, ERR_ARG(err));

            for (int j = 0; j < i; ++j)
                meshDestroy(&renderChunks[j].mesh);

            owsgFree(&renderChunks);
            worldDestroy(&world);
            shaderDestroy(&shader);
            windowDestroy(&window);
            return EXIT_FAILURE;
        }
    }

    /* --- camera --- */
    camera_t camera;
    cameraInit(&camera, (vec3){0.0f, 0.0f, 3.0f});
    windowSetCamera(&window, &camera);

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    /* --- The render loop --- */
    while (!windowShouldClose(&window))
    {
        float currentFrame = (float)windowGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        windowProcessInput(&window, deltaTime);

        int width, height;
        windowGetFramebufferSize(&window, &width, &height);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 view;
        cameraGetViewMatrix(&camera, view);

        mat4 projection;
        cameraGetProjectionMatrix(
            &camera,
            (float)width / (float)height,
            0.1f,
            100.0f,
            projection);

        shaderUse(&shader);
        shaderSetMat4(&shader, "view", (const float *)view);
        shaderSetMat4(&shader, "projection", (const float *)projection);

        for (int i = 0; i < GRID_CHUNK_COUNT; ++i)
        {
            mat4 model;
            glm_mat4_identity(model);

            glm_translate(
                model,
                (vec3){
                    (float)(renderChunks[i].coord.x * CHUNK_SIZE_X),
                    (float)(renderChunks[i].coord.y * CHUNK_SIZE_Y),
                    (float)(renderChunks[i].coord.z * CHUNK_SIZE_Z)});

            shaderSetMat4(&shader, "model", (const float *)model);
            meshDraw(&renderChunks[i].mesh);
        }

        windowUpdate(&window);
    }

    logInfo("Exiting...");

    for (int i = 0; i < GRID_CHUNK_COUNT; ++i)
        meshDestroy(&renderChunks[i].mesh);

    owsgFree(&renderChunks);

    /*
     * worldDestroy() frees every chunk_t owned by the world.
     */
    worldDestroy(&world);
    shaderDestroy(&shader);
    windowDestroy(&window);

    return EXIT_SUCCESS;
}
