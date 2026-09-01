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
    info("GL function pointers version %d loaded successfully!", version);

    /* --- The render loop --- */
    while (!glfwWindowShouldClose(window))
    {
        /* TODO: process input here later */

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black
        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    info("Exiting...");
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
