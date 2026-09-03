#include "util/owsg_err.h"
#include "util/log.h"
#include "util/alloc.h"
#include "graphics/window.h"
#include "graphics/shader.h"
#include "graphics/camera.h"
#include "graphics/mesh.h"
#include "world/chunk.h"
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

#define TEST_CHUNK_COUNT 2

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

/*
 * outChunk: non-NULL, already zero-initialized by the caller (see
 *           chunk_t's zero-init convention in chunk.h) - this
 *           function only needs to set the blocks that should be
 *           non-air.
 * err: non-NULL, populated on failure.
 *
 * Returns true on success, false on failure (propagated from
 * chunkSetBlock()).
 */
static bool fillTestChunk(chunk_t *outChunk, owsg_err *err)
{
    for (int x = 0; x < CHUNK_SIZE_X; ++x)
        for (int z = 0; z < CHUNK_SIZE_Z; ++z)
            if (!chunkSetBlock(outChunk, x, 0, z, BLOCK_STONE, err))
                return false;

    return true;
}

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

    /* --- set up the world and its test chunks --- */
    world_t world;
    worldInit(&world);

    chunkCoord_t testCoords[TEST_CHUNK_COUNT] = {
        {.x = 0, .y = 0, .z = 0},
        {.x = 1, .y = 0, .z = 0},
    };

    renderChunk_t renderChunks[TEST_CHUNK_COUNT];

    for (int i = 0; i < TEST_CHUNK_COUNT; ++i)
    {
        renderChunks[i].coord = testCoords[i];

        chunk_t *chunk = NULL;

        if (!owsgAlloc(sizeof(*chunk), (void **)&chunk))
        {
            logError("Chunk allocation failed");

            worldDestroy(&world);
            shaderDestroy(&shader);
            windowDestroy(&window);
            return EXIT_FAILURE;
        }

        memset(chunk, 0, sizeof(*chunk));

        if (!fillTestChunk(chunk, &err))
        {
            logError("Chunk initialization failed: " ERR_FMT, ERR_ARG(err));

            owsgFree((void **)&chunk);
            worldDestroy(&world);
            shaderDestroy(&shader);
            windowDestroy(&window);
            return EXIT_FAILURE;
        }

        if (!worldSetChunk(&world, testCoords[i], chunk, &err))
        {
            logError("Adding chunk to world failed: " ERR_FMT, ERR_ARG(err));

            owsgFree((void **)&chunk);
            worldDestroy(&world);
            shaderDestroy(&shader);
            windowDestroy(&window);
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < TEST_CHUNK_COUNT; ++i)
    {
        chunk_t *chunk = NULL;

        if (!worldGetChunk(&world, renderChunks[i].coord, &chunk, &err))
        {
            logError("Chunk lookup failed: " ERR_FMT, ERR_ARG(err));

            for (int j = 0; j < i; ++j)
                meshDestroy(&renderChunks[j].mesh);

            worldDestroy(&world);
            shaderDestroy(&shader);
            windowDestroy(&window);
            return EXIT_FAILURE;
        }

        if (!meshGenerateFromChunk(&world, renderChunks[i].coord, chunk, &renderChunks[i].mesh, &err))
        {
            logError("Chunk mesh generation failed: " ERR_FMT, ERR_ARG(err));

            for (int j = 0; j < i; ++j)
                meshDestroy(&renderChunks[j].mesh);

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

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 view;
        cameraGetViewMatrix(&camera, view);

        mat4 projection;
        cameraGetProjectionMatrix(&camera, (float)width / (float)height, 0.1f, 100.0f, projection);

        shaderUse(&shader);
        shaderSetMat4(&shader, "view", (const float *)view);
        shaderSetMat4(&shader, "projection", (const float *)projection);

        for (int i = 0; i < TEST_CHUNK_COUNT; ++i)
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

    for (int i = 0; i < TEST_CHUNK_COUNT; ++i)
        meshDestroy(&renderChunks[i].mesh);

    worldDestroy(&world); /* frees every chunk_t the world owns */
    shaderDestroy(&shader);
    windowDestroy(&window);
    return EXIT_SUCCESS;
}
