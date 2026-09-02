#ifndef GRAPHICS_CAMERA_H_
#define GRAPHICS_CAMERA_H_

#include <cglm/cglm.h>

/*
 * Default values used when initializing a new camera_t.
 */
#define CAMERA_DEFAULT_YAW -90.0f /* -90 so the camera starts facing -Z  */
#define CAMERA_DEFAULT_PITCH 0.0f
#define CAMERA_DEFAULT_FOV 45.0f
#define CAMERA_DEFAULT_MOVE_SPEED 2.5f       /* world units per second */
#define CAMERA_DEFAULT_LOOK_SENSITIVITY 0.1f /* degrees per pixel of mouse movement */

/*
 * A free-fly camera: position + yaw/pitch orientation, with cached
 * derived direction vectors.
 *
 * Orientation is stored as yaw/pitch (degrees) rather than a rotation
 * matrix or quaternion - simpler to reason about and sufficient here,
 * since this camera never needs roll.
 *
 * front/right/up are DERIVED from yaw/pitch and worldUp, but are
 * cached rather than recomputed on every use. Whenever yaw, pitch, or
 * worldUp change, you MUST call cameraUpdateVectors() afterward or
 * these will go stale and rendering will use a stale orientation.
 */
typedef struct
{
    vec3 position; /* camera position in world space */

    float yaw;   /* degrees; rotation around the world up axis */
    float pitch; /* degrees; rotation up/down. */

    vec3 front; /* CACHED - unit vector the camera looks along */
    vec3 right; /* CACHED - unit vector to the camera's right */
    vec3 up;    /* CACHED - unit vector, camera-local up (tilts with
                 * pitch - distinct from worldUp) */

    vec3 worldUp; /* reference "global up", typically (0,1,0). Used
                   * alongside front to derive right/up via cross
                   * product - kept separate from `up` because world
                   * up never changes even as the camera tilts. */

    float fov; /* vertical field of view, in degrees. Used when building the projection matrix. */

    float moveSpeed;       /* world units per second */
    float lookSensitivity; /* degrees of yaw/pitch per pixel of mouse delta */
} camera_t;

/*
 * Initializes a camera at the given position with default orientation
 * (facing -Z), FOV, and speeds. Calls cameraUpdateVectors()
 * internally, so front/right/up are valid immediately after this
 * returns.
 *
 * camera: non-NULL camera_t to initialize.
 * position: initial world-space position.
 */
void cameraInit(camera_t *camera, vec3 position);

/*
 * Recomputes front/right/up from the camera's current yaw, pitch, and
 * worldUp.
 *
 * MUST be called after directly modifying yaw, pitch, or worldUp, or
 * the cached vectors (and therefore the view matrix) will be stale.
 *
 * camera: non-NULL camera_t whose yaw/pitch/worldUp have just changed.
 */
void cameraUpdateVectors(camera_t *camera);

/*
 * Builds this camera's view matrix (world space -> camera-relative
 * space) into `dest`.
 *
 * camera: non-NULL camera, with up-to-date cached vectors (see
 *         cameraUpdateVectors()).
 * dest: non-NULL mat4 to populate.
 */
void cameraGetViewMatrix(camera_t *camera, mat4 dest);

/*
 * Builds this camera's projection matrix (camera-relative space ->
 * clip space) into `dest`.
 *
 * camera: non-NULL camera.
 * aspect: viewport aspect ratio (width / height).
 * nearZ, farZ: near/far clipping planes.
 * dest: non-NULL mat4 to populate.
 */
void cameraGetProjectionMatrix(const camera_t *camera, float aspect, float nearZ, float farZ, mat4 dest);

#endif /* GRAPHICS_CAMERA_H_ */
