#version 330 core

in vec3 vColor;
in vec3 vNormal;
in vec3 vViewPos;

uniform vec3 fogColor;

out vec4 FragColor;

/*
 * Fixed directional light, standing in for a sun/sky light source -
 * no actual light system exists yet. Points down and slightly to the
 * side so horizontal vs vertical faces read as visibly different
 * brightness rather than top-down faces being the only ones lit.
 */
const vec3 LIGHT_DIRECTION = normalize(vec3(-0.4, -1.0, -0.3));
const float AMBIENT_STRENGTH = 0.35;
const float FOG_DENSITY = 0.000136;

void main()
{
    vec3 normal = normalize(vNormal);

    /*
     * max(..., 0.0): a face angled away from the light contributes no
     * direct light (not negative light), same reasoning as standard
     * Lambertian diffuse shading.
     */
    float diffuseStrength = max(dot(normal, -LIGHT_DIRECTION), 0.0);

    /*
     * Ambient term ensures faces facing away from the light are dim,
     * not pure black - there is no bounce/indirect lighting yet, so
     * without this every backfacing surface would be fully unlit.
     */
    float lightAmount = AMBIENT_STRENGTH + (1.0 - AMBIENT_STRENGTH) * diffuseStrength;

    vec3 litColor = vColor * lightAmount;

    float dist = length(vViewPos);
    float fogFactor = clamp(1.0 - exp(-dist * FOG_DENSITY), 0.0, 1.0);

    vec3 finalColor = mix(litColor, fogColor, fogFactor);

    FragColor = vec4(finalColor, 1.0);
}
