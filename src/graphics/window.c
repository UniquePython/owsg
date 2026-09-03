#include "graphics/window.h"
#include "util/log.h"

#include <glad/gl.h>
/* GLFW must be included AFTER glad, since GLFW's header will otherwise
 * try to pull in the system GL headers, which conflicts with glad's
 * own definitions. */
#include <GLFW/glfw3.h>

/*
 * GLFW error callback. GLFW does not print errors itself; it hands
 * them to a callback you register, so failures are silent unless you
 * do this.
 *
 * Logs directly rather than going through owsg_err, since GLFW invokes
 * this asynchronously from arbitrary GLFW calls, not from a
 * windowCreate()-style function this module could thread an err
 * pointer through.
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
static void framebufferSizeCallback(GLFWwindow *handle, int width, int height)
{
    (void)handle;
    glViewport(0, 0, width, height);
}

/*
 * GLFW cursor position callback - fired whenever the mouse moves while
 * the window has focus.
 *
 * The associated window_t is retrieved via glfwGetWindowUserPointer()
 * rather than a file-scope global, so multiple window_t instances
 * (were this ever to support more than one) wouldn't collide.
 *
 * xpos, ypos: absolute cursor position in screen coordinates (NOT a
 *             delta - GLFW doesn't give you deltas directly, you
 *             compute them yourself against the last known position).
 */
static void cursorPosCallback(GLFWwindow *handle, double xpos, double ypos)
{
    window_t *window = glfwGetWindowUserPointer(handle);

    if (window == NULL)
        return;

    /* Prevent a large jump when mouse input starts. */
    if (window->firstMouse)
    {
        window->lastX = (float)xpos;
        window->lastY = (float)ypos;
        window->firstMouse = false;
        return;
    }

    /* Calculate mouse movement since the previous callback. */
    float xOffset = (float)xpos - window->lastX;

    /*
     * GLFW screen Y increases downward, but our camera expects
     * positive Y movement to mean "look up", so flip the sign.
     */
    float yOffset = window->lastY - (float)ypos;

    /* Store current position for the next callback. */
    window->lastX = (float)xpos;
    window->lastY = (float)ypos;

    /* Update camera orientation. */
    if (window->camera != NULL)
        cameraProcessMouseMovement(window->camera, xOffset, yOffset);
}

bool windowCreate(int width, int height, const char *title, window_t *outWindow, owsg_err *err)
{
    if (title == NULL)
    {
        owsgErrSet(err, "Window title is NULL");
        return false;
    }

    if (outWindow == NULL)
    {
        owsgErrSet(err, "Output window pointer is NULL");
        return false;
    }

    /* --- GLFW init --- */
    glfwSetErrorCallback(glfwErrorCallback);
    logInfo("GLFW error callback set successfully!");

    if (glfwInit() == GLFW_FALSE)
    {
        owsgErrSet(err, "GLFW initialization failed");
        return false;
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
    GLFWwindow *handle = glfwCreateWindow(width, height, title, NULL, NULL);
    if (handle == NULL)
    {
        owsgErrSet(err, "GLFW window and context creation failed");
        glfwTerminate();
        return false;
    }
    logInfo("GLFW window and context created successfully!");

    /* --- make context current --- */
    glfwMakeContextCurrent(handle);
    logInfo("GLFW context set to current window successfully!");

    /* --- load GL function pointers via glad --- */
    int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0)
    {
        owsgErrSet(err, "GL function pointers loading failed");
        glfwDestroyWindow(handle);
        glfwTerminate();
        return false;
    }
    logInfo("OpenGL version: %s", glGetString(GL_VERSION));
    logInfo("OpenGL renderer: %s", glGetString(GL_RENDERER));

    /*
     * Zero-initialize before wiring up callbacks/user pointer, so the
     * callbacks never observe stale/garbage input-state fields if GLFW
     * (implausibly) fires one before this function returns.
     */
    *outWindow = (window_t){0};
    outWindow->handle = handle;
    outWindow->firstMouse = true;

    /* Let the C callbacks above recover this window_t from the raw
     * GLFWwindow* GLFW hands them. */
    glfwSetWindowUserPointer(handle, outWindow);

    /* Register the resize callback now that we have a window. */
    glfwSetFramebufferSizeCallback(handle, framebufferSizeCallback);
    logInfo("GLFW framebuffer size callback set successfully!");

    /* Register mouse callback */
    glfwSetInputMode(handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(handle, cursorPosCallback);

    /* --- Enable depth testing --- */
    glEnable(GL_DEPTH_TEST);
    logInfo("Depth testing: enabled");

    return true;
}

void windowSetCamera(window_t *window, camera_t *camera)
{
    if (window == NULL)
        return;

    window->camera = camera;
}

void windowProcessInput(window_t *window, float deltaTime)
{
    if (window == NULL || window->camera == NULL)
        return;

    if (glfwGetKey(window->handle, GLFW_KEY_W) == GLFW_PRESS)
        cameraProcessKeyboard(window->camera, CAMERA_MOVE_FORWARD, deltaTime);
    if (glfwGetKey(window->handle, GLFW_KEY_S) == GLFW_PRESS)
        cameraProcessKeyboard(window->camera, CAMERA_MOVE_BACKWARD, deltaTime);
    if (glfwGetKey(window->handle, GLFW_KEY_A) == GLFW_PRESS)
        cameraProcessKeyboard(window->camera, CAMERA_MOVE_LEFT, deltaTime);
    if (glfwGetKey(window->handle, GLFW_KEY_D) == GLFW_PRESS)
        cameraProcessKeyboard(window->camera, CAMERA_MOVE_RIGHT, deltaTime);
    if (glfwGetKey(window->handle, GLFW_KEY_Q) == GLFW_PRESS)
        cameraProcessKeyboard(window->camera, CAMERA_MOVE_UP, deltaTime);
    if (glfwGetKey(window->handle, GLFW_KEY_E) == GLFW_PRESS)
        cameraProcessKeyboard(window->camera, CAMERA_MOVE_DOWN, deltaTime);
}

bool windowShouldClose(const window_t *window)
{
    if (window == NULL)
        return true;

    return glfwWindowShouldClose(window->handle) != 0;
}

void windowGetFramebufferSize(const window_t *window, int *outWidth, int *outHeight)
{
    if (window == NULL || outWidth == NULL || outHeight == NULL)
        return;

    glfwGetFramebufferSize(window->handle, outWidth, outHeight);
}

void windowUpdate(window_t *window)
{
    if (window == NULL)
        return;

    glfwSwapBuffers(window->handle);
    glfwPollEvents();
}

double windowGetTime(void)
{
    return glfwGetTime();
}

void windowDestroy(window_t *window)
{
    if (window == NULL)
        return;

    glfwDestroyWindow(window->handle);
    glfwTerminate();

    *window = (window_t){0};
}
