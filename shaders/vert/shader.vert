#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;

out vec3 vColor;
out vec3 vNormal;
out vec3 vViewPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);

    /*
     * model currently only ever translates (see main.c's per-chunk
     * glm_translate), never rotates or non-uniformly scales, so the
     * upper-left 3x3 of model is still axis-aligned and transforming
     * the normal directly with it (rather than the inverse-transpose
     * a rotating/scaling model would require) is correct as-is.
     * TODO: switch to mat3(transpose(inverse(model))) if/when chunks
     * ever get rotated or non-uniformly scaled.
     */
    vNormal = mat3(model) * aNormal;

    vColor = aColor;

    vViewPos = (view * model * vec4(aPos, 1.0)).xyz;
}
