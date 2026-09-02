#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aInstanceOffset; /* per-instance, not per-vertex - see glVertexAttribDivisor on the C side */

out vec3 vColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec3 worldPos = aPos + aInstanceOffset;
    gl_Position = projection * view * model * vec4(worldPos, 1.0);
    vColor = aColor;
}
