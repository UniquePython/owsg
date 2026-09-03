#include "util/owsg_err.h"
#include "util/log.h"
#include "graphics/window.h"
#include "graphics/shader.h"
#include "graphics/camera.h"
#include "graphics/mesh.h"
#include "world/chunk.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include <glad/gl.h>
#include <cglm/cglm.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define WINDOW_TITLE "owsg"

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

    chunk_t chunk = {0}; /* zero-initialized => all-air */
    err = (owsg_err){0};

    for (int x = 0; x < CHUNK_SIZE_X; x++)
    {
        for (int z = 0; z < CHUNK_SIZE_Z; z++)
        {
            if (!chunkSetBlock(&chunk, x, 0, z, BLOCK_STONE, &err))
            {
                logError("Failed to set block: " ERR_FMT, ERR_ARG(err));
                shaderDestroy(&shader);
                windowDestroy(&window);
                return EXIT_FAILURE;
            }
        }
    }

    mesh_t mesh;
    if (!meshGenerateFromChunk(&chunk, &mesh, &err))
    {
        logError("Mesh generation failed: " ERR_FMT, ERR_ARG(err));
        shaderDestroy(&shader);
        windowDestroy(&window);
        return EXIT_FAILURE;
    }
    logInfo("Mesh generated: %u indices", mesh.indexCount);

    /* Model matrix: local -> world space. */
    mat4 model;
    glm_mat4_identity(model);

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
        shaderSetMat4(&shader, "model", (const float *)model);
        shaderSetMat4(&shader, "view", (const float *)view);
        shaderSetMat4(&shader, "projection", (const float *)projection);

        meshDraw(&mesh);

        windowUpdate(&window);
    }

    logInfo("Exiting...");
    meshDestroy(&mesh);
    shaderDestroy(&shader);
    windowDestroy(&window);
    return EXIT_SUCCESS;
}
