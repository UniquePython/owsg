#ifndef GRAPHICS_WINDOW_H_
#define GRAPHICS_WINDOW_H_

#include "graphics/camera.h"
#include "util/owsg_err.h"

#include <stdbool.h>

/* Forward-declared rather than #include <GLFW/glfw3.h> here, so that
 * consumers of this header (e.g. main.c) don't need to worry about
 * glad/GLFW include ordering unless they also need raw GLFW calls
 * themselves. */
typedef struct GLFWwindow GLFWwindow;

/*
 * Owns the application's single GLFW window, its GL context, and the
 * input state (cursor tracking, the camera it drives) that the
 * window's callbacks need to update.
 *
 * A window_t does NOT own a camera_t - it only holds a pointer to one
 * (set via windowSetCamera()), so the camera's lifetime remains
 * main.c's responsibility, same as the shader and mesh.
 */
typedef struct
{
    GLFWwindow *handle;

    camera_t *camera; /* not owned - see windowSetCamera() */

    bool firstMouse;
    float lastX;
    float lastY;
} window_t;

/*
 * Initializes GLFW, creates a window + OpenGL 3.3 core context, makes
 * it current, loads GL function pointers via glad, and registers the
 * framebuffer-resize and cursor-position callbacks.
 *
 * Also enables raw cursor capture (GLFW_CURSOR_DISABLED) for
 * mouse-look, and depth testing.
 *
 * width, height: initial window size, in screen coordinates.
 * title: window title. Must remain valid only for the duration of this
 *        call - GLFW copies it internally.
 * outWindow: non-NULL window_t to populate on success. Left unchanged
 *            on failure.
 * err: non-NULL error object to populate on failure.
 *
 * Returns true on success, false on failure (GLFW init failure, window
 * context creation failure, or glad failing to load GL function
 * pointers).
 *
 * Ownership: on success, the caller is responsible for eventually
 * calling windowDestroy() on outWindow.
 */
bool windowCreate(int width, int height, const char *title, window_t *outWindow, owsg_err *err);

/*
 * Associates a camera with this window, so the window's input
 * callbacks and windowProcessInput() can drive it.
 *
 * window: non-NULL window previously created by windowCreate().
 * camera: camera to drive. Must outlive the window, or must be
 *         cleared (pass NULL) before it's destroyed. Not owned by the
 *         window - see the window_t.camera field comment.
 */
void windowSetCamera(window_t *window, camera_t *camera);

/*
 * Polls this window's current keyboard state and moves its associated
 * camera accordingly (WASD for horizontal movement, Q/E for
 * vertical) - a no-op if no camera has been set via windowSetCamera().
 *
 * Intended to be called once per frame, before rendering.
 *
 * window: non-NULL window to poll.
 * deltaTime: seconds elapsed since the last frame, forwarded to
 *            cameraProcessKeyboard() so movement speed stays
 *            framerate-independent.
 */
void windowProcessInput(window_t *window, float deltaTime);

/*
 * Returns whether the user has requested the window close (e.g. via
 * the close button or Alt+F4) - the render loop's continuation
 * condition.
 *
 * window: non-NULL window to check.
 */
bool windowShouldClose(const window_t *window);

/*
 * Retrieves the current framebuffer size, in pixels (not screen
 * coordinates - see the framebuffer-resize callback's own note on
 * this distinction in window.c).
 *
 * window: non-NULL window to query.
 * outWidth, outHeight: non-NULL output pointers.
 */
void windowGetFramebufferSize(const window_t *window, int *outWidth, int *outHeight);

/*
 * Swaps the front/back buffers and polls/dispatches pending GLFW
 * events (which is what actually invokes the resize/cursor
 * callbacks). Intended to be called once per frame, after rendering.
 *
 * window: non-NULL window to update.
 */
void windowUpdate(window_t *window);

/*
 * Returns the number of seconds since GLFW was initialized (i.e.
 * since windowCreate() was called) - exposed so main.c can compute
 * per-frame deltaTime without needing to include GLFW itself just for
 * glfwGetTime().
 *
 * Not tied to any particular window_t (GLFW's timer is process-global,
 * not per-window), but kept here rather than as a free function so all
 * window/GLFW-adjacent calls stay behind this one header.
 */
double windowGetTime(void);

/*
 * Destroys the underlying GLFW window and terminates GLFW entirely
 * (safe to do unconditionally here, since this application only ever
 * has one window).
 *
 * window: non-NULL window to destroy. Must not be used again
 *         afterward, except to be re-populated by another
 *         windowCreate() call.
 */
void windowDestroy(window_t *window);

#endif /* GRAPHICS_WINDOW_H_ */
