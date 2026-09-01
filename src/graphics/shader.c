#include "graphics/shader.h"
#include "util/file.h"
#include "util/alloc.h"

#include <glad/gl.h>

/*
 * Compiles a single shader stage from source already loaded into
 * memory.
 *
 * type: GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
 * source: NUL-terminated GLSL source string.
 * outShaderId: non-NULL, populated with the compiled shader's handle
 *              on success. Caller must glDeleteShader() it eventually
 *              (shaderCreate() does this once the shaders are linked
 *              into a program - see note there).
 * err: populated on failure.
 *
 * Returns true on success, false if compilation fails.
 */
static bool compileStage(unsigned int type, const char *source, unsigned int *outShaderId, owsg_err *err)
{
    if (source == NULL)
    {
        owsgErrSet(err, "Shader source is NULL");
        return false;
    }

    if (outShaderId == NULL)
    {
        owsgErrSet(err, "Output shader ID pointer is NULL");
        return false;
    }

    unsigned int id = glCreateShader(type);
    if (id == 0)
    {
        owsgErrSet(err, "Failed to create shader object");
        return false;
    }

    glShaderSource(id, 1, &source, NULL);
    glCompileShader(id);

    int success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        int logLength;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &logLength);

        if (logLength <= 0)
        {
            glDeleteShader(id);
            owsgErrSet(err, "Shader compilation failed with no info log");
            return false;
        }

        char *log;

        if (!owsgAlloc((size_t)logLength, &log))
        {
            glDeleteShader(id);
            owsgErrSet(err, "Shader compilation failed and allocating "
                            "the compiler log (%d bytes) failed",
                       logLength);
            return false;
        }

        int actualLength;
        glGetShaderInfoLog(id, logLength, &actualLength, log);

        glDeleteShader(id);

        if (actualLength < 0)
        {
            owsgFree(&log);
            owsgErrSet(err, "Shader compilation failed and retrieving "
                            "the compiler log failed");
            return false;
        }

        log[actualLength] = '\0';

        owsgErrSet(err, "%s", log);
        owsgFree(&log);

        return false;
    }

    *outShaderId = id;
    return true;
}

bool shaderCreate(const char *vertPath, const char *fragPath, shader_t *outShader, owsg_err *err)
{
    if (vertPath == NULL)
    {
        owsgErrSet(err, "Vertex shader path is NULL");
        return false;
    }

    if (fragPath == NULL)
    {
        owsgErrSet(err, "Fragment shader path is NULL");
        return false;
    }

    if (outShader == NULL)
    {
        owsgErrSet(err, "Output shader pointer is NULL");
        return false;
    }

    char *vertSource;

    if (!readFile(vertPath, NULL, &vertSource, err))
        return false;

    char *fragSource;

    if (!readFile(fragPath, NULL, &fragSource, err))
    {
        owsgFree(&vertSource);
        return false;
    }

    unsigned int vertId;

    if (!compileStage(GL_VERTEX_SHADER, vertSource, &vertId, err))
    {
        owsgFree(&vertSource);
        owsgFree(&fragSource);
        return false;
    }

    unsigned int fragId;

    if (!compileStage(GL_FRAGMENT_SHADER, fragSource, &fragId, err))
    {
        owsgFree(&vertSource);
        owsgFree(&fragSource);
        glDeleteShader(vertId);
        return false;
    }

    owsgFree(&vertSource);
    owsgFree(&fragSource);

    unsigned int program = glCreateProgram();

    if (program == 0)
    {
        glDeleteShader(vertId);
        glDeleteShader(fragId);
        owsgErrSet(err, "Failed to create shader program");
        return false;
    }

    glAttachShader(program, vertId);
    glAttachShader(program, fragId);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success)
    {
        int logLength;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

        if (logLength <= 0)
        {
            glDeleteShader(vertId);
            glDeleteShader(fragId);
            glDeleteProgram(program);

            owsgErrSet(err, "Shader program linking failed with no info log");
            return false;
        }

        char *log;

        if (!owsgAlloc((size_t)logLength, &log))
        {
            glDeleteShader(vertId);
            glDeleteShader(fragId);
            glDeleteProgram(program);

            owsgErrSet(err,
                       "Shader program linking failed and allocating "
                       "the linker log (%d bytes) failed",
                       logLength);
            return false;
        }

        int actualLength;
        glGetProgramInfoLog(program, logLength, &actualLength, log);

        if (actualLength < 0)
        {
            owsgFree(&log);
            glDeleteShader(vertId);
            glDeleteShader(fragId);
            glDeleteProgram(program);

            owsgErrSet(err,
                       "Shader program linking failed and retrieving "
                       "the linker log failed");
            return false;
        }

        log[actualLength] = '\0';

        owsgErrSet(err, "%s", log);

        owsgFree(&log);
        glDeleteShader(vertId);
        glDeleteShader(fragId);
        glDeleteProgram(program);

        return false;
    }

    glDeleteShader(vertId);
    glDeleteShader(fragId);

    outShader->id = program;

    return true;
}

void shaderUse(const shader_t *shader)
{
    if (shader == NULL)
        return;

    glUseProgram(shader->id);
}

void shaderDestroy(shader_t *shader)
{
    if (shader == NULL)
        return;

    glDeleteProgram(shader->id);
    shader->id = 0;
}
