#ifndef GRAPHICS_SHADER_H_
#define GRAPHICS_SHADER_H_

#include <stdbool.h>

#include "util/owsg_err.h"

/*
 * A linked GPU shader program (vertex + fragment stages combined).
 *
 * id: the underlying OpenGL program object handle, as returned by
 *     glCreateProgram(). Valid only between a successful shaderCreate()
 *     and the matching shaderDestroy().
 */
typedef struct
{
    unsigned int id;
} shader_t;

/*
 * Compiles and links a vertex+fragment shader program from GLSL source
 * files on disk.
 *
 * vertPath, fragPath: filesystem paths to the vertex and fragment
 *      shader source files. Read via readFile() internally.
 *
 * outShader: non-NULL pointer to the shader_t to populate on success.
 *      Left unchanged on failure.
 *
 * err: non-NULL owsg_err to populate with a human-readable message on
 *      failure (file read failure, GLSL compile error - including the
 *      driver's compiler log, or link error).
 *
 * Returns true on success, false on any failure.
 *
 * Ownership: on success, the caller is responsible for eventually
 * calling shaderDestroy() on outShader.
 */
bool shaderCreate(const char *vertPath, const char *fragPath, shader_t *outShader, owsg_err *err);

/*
 * Binds this shader program as the currently active one. All subsequent draw
 * calls use this program until a different one is bound.
 *
 * shader: non-NULL shader previously created by shaderCreate().
 */
void shaderUse(const shader_t *shader);

/*
 * Deletes the underlying GPU program object. The shader_t must not be
 * used again afterward (except to be re-created via shaderCreate()).
 *
 * shader: non-NULL shader to destroy. Safe to call even if the
 *         program failed to link, as long as shaderCreate() was
 *         attempted.
 */
void shaderDestroy(shader_t *shader);

/*
 * Sets a mat4 uniform on this shader by name.
 *
 * shader: non-NULL shader previously created by shaderCreate(). Does
 *         NOT need to be the currently active shader - this function
 *         binds it internally, so it's safe to call regardless of
 *         what shaderUse() was last called with. (Note: this does
 *         mean it changes the currently bound program as a side
 *         effect - if you call this between shaderUse() and a draw
 *         call for a DIFFERENT shader, you'll need to shaderUse() the
 *         one you actually want to draw with again afterward.)
 *
 * name: null-terminated uniform name as declared in the GLSL source
 *       (e.g. "model", "view", "projection"). If no uniform with this
 *       name exists in the program - e.g. a typo, or it was optimized
 *       out by the driver for being unused - this call is silently a
 *       no-op, per normal OpenGL behavior.
 *
 * value: pointer to a 4x4 float matrix in column-major order (cglm's
 *        mat4 layout matches this directly, so a cglm mat4 can be
 *        passed as-is).
 */
void shaderSetMat4(const shader_t *shader, const char *name, const float *value);

/*
 * Sets a vec3 uniform on this shader by name.
 *
 * shader: non-NULL shader previously created by shaderCreate(). Does
 *         NOT need to be the currently active shader - see the
 *         shaderSetMat4() doc comment for the same binding-as-a-side-
 *         effect note, which applies identically here.
 *
 * name: null-terminated uniform name as declared in the GLSL source
 *       (e.g. "fogColor"). If no uniform with this name exists in the
 *       program, this call is silently a no-op, per normal OpenGL
 *       behavior.
 *
 * value: a 3-component float vector (cglm's vec3 layout matches
 *        directly, so a cglm vec3 can be passed as-is).
 */
void shaderSetVec3(const shader_t *shader, const char *name, const float *value);

#endif /* GRAPHICS_SHADER_H_ */
