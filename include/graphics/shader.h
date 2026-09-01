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

#endif /* GRAPHICS_SHADER_H_ */
