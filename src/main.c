#include "util/owsg_err.h"
#include "graphics/shader.h"
#include "graphics/camera.h"

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

static bool useColor = false;

#define ANSI_RED "\033[31m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_BOLD "\033[1m"
#define ANSI_NO_BOLD "\033[22m"
#define ANSI_RESET "\033[0m"

static void log_message(const char *label, const char *color, const char *fmt, va_list args)
{
    if (useColor)
        fprintf(stderr, "%s%s[%s]%s ", color, ANSI_BOLD, label, ANSI_NO_BOLD);

    vfprintf(stderr, fmt, args);

    if (useColor)
        fprintf(stderr, ANSI_RESET);

    fprintf(stderr, "\n");
}

static void error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_message("ERROR", ANSI_RED, fmt, args);
    va_end(args);
}

static void info(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_message("INFO", ANSI_YELLOW, fmt, args);
    va_end(args);
}

/*
 * GLFW error callback. GLFW does not print errors itself; it hands
 * them to a callback you register, so failures are silent unless you
 * do this.
 */
static void glfwErrorCallback(int errorCode, const char *description)
{
    error("%s (%d)", description, errorCode);
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

/*
 * Interleaved cube vertex data: position (x,y,z) followed by color
 * (r,g,b) per vertex, 6 floats each.
 *
 * 24 vertices = 4 per face * 6 faces. Vertices are NOT shared between
 * faces because each face has its own distinct color.
 *
 * Face colors:
 *   Front  = Red
 *   Back   = Green
 *   Right  = Blue
 *   Left   = Yellow
 *   Top    = Magenta
 *   Bottom = Cyan
 */

static const float cubeVertices[] = {

    // Front face (+Z) - Red
    // vertex 0
    -0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
    // vertex 1
    0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
    // vertex 2
    0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
    // vertex 3
    -0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,

    // Back face (-Z) - Green
    // vertex 4
    0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
    // vertex 5
    -0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
    // vertex 6
    -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
    // vertex 7
    0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,

    // Right face (+X) - Blue
    // vertex 8
    0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
    // vertex 9
    0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
    // vertex 10
    0.5f, 0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
    // vertex 11
    0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,

    // Left face (-X) - Yellow
    // vertex 12
    -0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.0f,
    // vertex 13
    -0.5f, -0.5f, 0.5f, 1.0f, 1.0f, 0.0f,
    // vertex 14
    -0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 0.0f,
    // vertex 15
    -0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 0.0f,

    // Top face (+Y) - Magenta
    // vertex 16
    -0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f,
    // vertex 17
    0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f,
    // vertex 18
    0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 1.0f,
    // vertex 19
    -0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 1.0f,

    // Bottom face (-Y) - Cyan
    // vertex 20
    -0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 1.0f,
    // vertex 21
    0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 1.0f,
    // vertex 22
    0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 1.0f,
    // vertex 23
    -0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 1.0f};

/*
 * Index buffer:
 * 6 faces * 2 triangles * 3 indices = 36 indices.
 *
 * Every triangle uses counter-clockwise winding when viewed
 * from outside the cube, so the face normals point outward.
 */

static const unsigned int cubeIndices[] = {

    // Front (+Z)
    0, 1, 2,
    0, 2, 3,

    // Back (-Z)
    4, 5, 6,
    4, 6, 7,

    // Right (+X)
    8, 9, 10,
    8, 10, 11,

    // Left (-X)
    12, 13, 14,
    12, 14, 15,

    // Top (+Y)
    16, 17, 18,
    16, 18, 19,

    // Bottom (-Y)
    20, 21, 22,
    20, 22, 23};

int main(void)
{
    if (isatty(STDERR_FILENO))
        useColor = true;

    info("Colored output: %s", useColor ? "enabled" : "disabled");

    /* --- GLFW init --- */
    glfwSetErrorCallback(glfwErrorCallback);
    info("GLFW error callback set successfully!");

    if (glfwInit() == GLFW_FALSE)
    {
        error("GLFW initialization failed");
        return EXIT_FAILURE;
    }

    info("GLFW initialized successfully!");

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

    info("GLFW version: 3.3");
    info("GLFW profile: Core");
    info("GLFW forward compatibility: enabled");

    /* --- create the window + context --- */
    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (window == NULL)
    {
        error("GLFW window and context creation failed");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    info("GLFW window and context created successfully!");

    /* --- make context current --- */
    glfwMakeContextCurrent(window);
    info("GLFW context set to current window successfully!");

    /* Register the resize callback now that we have a window. */
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    info("GLFW framebuffer size callback set successfully!");

    /* --- load GL function pointers via glad --- */
    int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0)
    {
        error("GL function pointers loading failed");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    info("OpenGL version: %s", glGetString(GL_VERSION));
    info("OpenGL renderer: %s", glGetString(GL_RENDERER));

    /* --- load the shader program --- */
    shader_t shader;
    owsg_err err = {0};
    const char *vertShader = "shaders/vert/shader.vert";
    const char *fragShader = "shaders/frag/shader.frag";
    if (!shaderCreate(vertShader, fragShader, &shader, &err))
    {
        error("Shader program loading failed: " ERR_FMT, ERR_ARG(err));
        glfwTerminate();
        return EXIT_FAILURE;
    }
    info("Loaded vertex shader: '%s' successfully!", vertShader);
    info("Loaded fragment shader: '%s' successfully!", fragShader);

    /* --- Set up VAO + VBO + EBO --- */
    unsigned int vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), NULL);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    /* --- Enable depth testing --- */
    glEnable(GL_DEPTH_TEST);
    info("Depth testing: enabled");

    /* Model matrix: local -> world space. */
    mat4 model;
    glm_mat4_identity(model);

    camera_t camera;
    cameraInit(&camera, (vec3){0.0f, 0.0f, 3.0f});

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

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, sizeof(cubeIndices) / sizeof(cubeIndices[0]), GL_UNSIGNED_INT, NULL);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    info("Exiting...");
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    shaderDestroy(&shader);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
