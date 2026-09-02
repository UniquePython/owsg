#include "graphics/camera.h"

#include <math.h>

void cameraInit(camera_t *camera, vec3 position)
{
    if (camera == NULL)
        return;

    glm_vec3_copy(position, camera->position);

    camera->yaw = CAMERA_DEFAULT_YAW;
    camera->pitch = CAMERA_DEFAULT_PITCH;

    camera->fov = CAMERA_DEFAULT_FOV;
    camera->moveSpeed = CAMERA_DEFAULT_MOVE_SPEED;
    camera->lookSensitivity = CAMERA_DEFAULT_LOOK_SENSITIVITY;

    glm_vec3_copy((vec3){0.0f, 1.0f, 0.0f}, camera->worldUp);

    cameraUpdateVectors(camera);
}

void cameraUpdateVectors(camera_t *camera)
{
    if (camera == NULL)
        return;

    float yaw = glm_rad(camera->yaw);
    float pitch = glm_rad(camera->pitch);

    /*
     * Convert yaw/pitch into a direction vector.
     *
     * yaw = -90°, pitch = 0° gives:
     * front = (0, 0, -1)
     */
    camera->front[0] = cosf(yaw) * cosf(pitch);
    camera->front[1] = sinf(pitch);
    camera->front[2] = sinf(yaw) * cosf(pitch);

    glm_vec3_normalize(camera->front);

    /*
     * Right = front × worldUp
     */
    glm_vec3_cross(camera->front, camera->worldUp, camera->right);
    glm_vec3_normalize(camera->right);

    /*
     * Up = right × front
     */
    glm_vec3_cross(camera->right, camera->front, camera->up);
    glm_vec3_normalize(camera->up);
}

void cameraGetViewMatrix(camera_t *camera, mat4 dest)
{
    if (camera == NULL)
        return;

    vec3 center;

    /*
     * Look from the camera position toward a point one unit
     * along the camera's front direction.
     */
    glm_vec3_add(camera->position, camera->front, center);

    glm_lookat(camera->position, center, camera->up, dest);
}

void cameraGetProjectionMatrix(const camera_t *camera, float aspect, float nearZ, float farZ, mat4 dest)
{
    if (camera == NULL)
        return;

    glm_perspective(glm_rad(camera->fov), aspect, nearZ, farZ, dest);
}

void cameraProcessKeyboard(camera_t *camera, camera_movement_t direction, float deltaTime)
{
    if (camera == NULL)
        return;

    float velocity = camera->moveSpeed * deltaTime;

    switch (direction)
    {
    case CAMERA_MOVE_FORWARD:
        glm_vec3_muladds(camera->front, velocity, camera->position);
        break;

    case CAMERA_MOVE_BACKWARD:
        glm_vec3_muladds(camera->front, -velocity, camera->position);
        break;

    case CAMERA_MOVE_LEFT:
        glm_vec3_muladds(camera->right, -velocity, camera->position);
        break;

    case CAMERA_MOVE_RIGHT:
        glm_vec3_muladds(camera->right, velocity, camera->position);
        break;

    case CAMERA_MOVE_UP:
        glm_vec3_muladds(camera->worldUp, velocity, camera->position);
        break;

    case CAMERA_MOVE_DOWN:
        glm_vec3_muladds(camera->worldUp, -velocity, camera->position);
        break;
    }
}
