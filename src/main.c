#include "util/owsg_err.h"
#include "util/log.h"
#include "graphics/shader.h"
#include "graphics/camera.h"
#include "graphics/mesh.h"
#include "world/chunk.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdarg.h>

#include <glad/gl.h>
/* GLFW must be included AFTER glad, since GLFW's header will otherwise
 * try to pull in the system GL headers, which conflicts with glad's
 * own definitions. */
#include <GLFW/glfw3.h>

#include <cglm/cglm.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define WINDOW_TITLE "owsg"

/*
 * GLFW error callback. GLFW does not print errors itself; it hands
 * them to a callback you register, so failures are silent unless you
 * do this.
 */
static void glfwErrorCallback(int errorCode, const char *description)
{
    logError("%s (%d)", description, errorCode);
}

/*
 * Called by GLFW whenever the window is resized (including at
 * creation). Responsible for keeping the GL viewport in sync with the
 * actual framebuffer size, which is NOT automatic - GL has no idea the
 * window changed unless you tell it via glViewport.
 *
 * width, height: new framebuffer size in pixels (not screen
 * coordinates - these can differ on high-DPI displays, which is why
 * we use glfwSetFramebufferSizeCallback rather than the window size
 * callback).
 */
static void framebufferSizeCallback(GLFWwindow *window, int width, int height)
{
    (void)window;
    glViewport(0, 0, width, height);
}

static camera_t *g_camera = NULL;
static bool firstMouse = true;
static float lastX = 0.0f;
static float lastY = 0.0f;

/*
 * GLFW cursor position callback - fired whenever the mouse moves
 * while the window has focus.
 *
 * xpos, ypos: absolute cursor position in screen coordinates (NOT a
 *             delta - GLFW doesn't give you deltas directly, you
 *             compute them yourself against the last known position).
 */
static void cursorPosCallback(GLFWwindow *window, double xpos, double ypos)
{
    (void)window;

    /* Prevent a large jump when mouse input starts. */
    if (firstMouse)
    {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
        return;
    }

    /* Calculate mouse movement since the previous callback. */
    float xOffset = (float)xpos - lastX;

    /*
     * GLFW screen Y increases downward, but our camera expects
     * positive Y movement to mean "look up", so flip the sign.
     */
    float yOffset = lastY - (float)ypos;

    /* Store current position for the next callback. */
    lastX = (float)xpos;
    lastY = (float)ypos;

    /* Update camera orientation. */
    if (g_camera != NULL)
        cameraProcessMouseMovement(g_camera, xOffset, yOffset);
}

int main(void)
{
    bool useColor = isatty(STDERR_FILENO) != 0;
    logSetColorEnabled(useColor);

    logInfo("Colored output: %s", useColor ? "enabled" : "disabled");

    /* --- GLFW init --- */
    glfwSetErrorCallback(glfwErrorCallback);
    logInfo("GLFW error callback set successfully!");

    if (glfwInit() == GLFW_FALSE)
    {
        logError("GLFW initialization failed");
        return EXIT_FAILURE;
    }

    logInfo("GLFW initialized successfully!");

    /* --- window hints --- */
    /*
     *   GLFW_CONTEXT_VERSION_MAJOR = 3
     *   GLFW_CONTEXT_VERSION_MINOR = 3
     *   GLFW_OPENGL_PROFILE        = GLFW_OPENGL_CORE_PROFILE
     *   GLFW_OPENGL_FORWARD_COMPAT = GLFW_TRUE   (required on macOS,
     *       harmless elsewhere - forward-compat contexts drop
     *       deprecated functionality that core profile already
     *       excludes, so this mostly matters for macOS's stricter
     *       enforcement)
     */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    logInfo("GLFW version: 3.3");
    logInfo("GLFW profile: Core");
    logInfo("GLFW forward compatibility: enabled");

    /* --- create the window + context --- */
    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (window == NULL)
    {
        logError("GLFW window and context creation failed");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    logInfo("GLFW window and context created successfully!");

    /* --- make context current --- */
    glfwMakeContextCurrent(window);
    logInfo("GLFW context set to current window successfully!");

    /* Register the resize callback now that we have a window. */
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    logInfo("GLFW framebuffer size callback set successfully!");

    /* Register mouse callback */
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, cursorPosCallback);

    /* --- load GL function pointers via glad --- */
    int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0)
    {
        logError("GL function pointers loading failed");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    logInfo("OpenGL version: %s", glGetString(GL_VERSION));
    logInfo("OpenGL renderer: %s", glGetString(GL_RENDERER));

    /* --- load the shader program --- */
    shader_t shader;
    owsg_err err = {0};
    const char *vertShader = "shaders/vert/shader.vert";
    const char *fragShader = "shaders/frag/shader.frag";
    if (!shaderCreate(vertShader, fragShader, &shader, &err))
    {
        logError("Shader program loading failed: " ERR_FMT, ERR_ARG(err));
        glfwTerminate();
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

                glfwDestroyWindow(window);
                glfwTerminate();
                return EXIT_FAILURE;
            }
        }
    }

    mesh_t mesh;
    if (!meshGenerateFromChunk(&chunk, &mesh, &err))
    {
        logError("Mesh generation failed: " ERR_FMT, ERR_ARG(err));
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }
    logInfo("Mesh generated: %u indices", mesh.indexCount);

    /* --- Enable depth testing --- */
    glEnable(GL_DEPTH_TEST);
    logInfo("Depth testing: enabled");

    /* Model matrix: local -> world space. */
    mat4 model;
    glm_mat4_identity(model);

    camera_t camera;
    cameraInit(&camera, (vec3){0.0f, 0.0f, 3.0f});
    g_camera = &camera;

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    /* --- The render loop --- */
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            cameraProcessKeyboard(&camera, CAMERA_MOVE_FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            cameraProcessKeyboard(&camera, CAMERA_MOVE_BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            cameraProcessKeyboard(&camera, CAMERA_MOVE_LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            cameraProcessKeyboard(&camera, CAMERA_MOVE_RIGHT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            cameraProcessKeyboard(&camera, CAMERA_MOVE_UP, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            cameraProcessKeyboard(&camera, CAMERA_MOVE_DOWN, deltaTime);

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

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

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    logInfo("Exiting...");
    meshDestroy(&mesh);
    shaderDestroy(&shader);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
