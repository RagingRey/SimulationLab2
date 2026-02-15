#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in vec3 fragModelPos;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 checkerColorA;
    vec4 checkerColorB;
    vec4 checkerParams;
} pushConstants;

void main() {
    float scale = max(pushConstants.checkerParams.x, 0.001);
    vec2 coord = fragModelPos.xz * scale;
    float checker = mod(floor(coord.x) + floor(coord.y), 2.0);
    vec3 baseColor = mix(pushConstants.checkerColorA.rgb, pushConstants.checkerColorB.rgb, checker);

    vec3 lightDir = normalize(vec3(0.5, -1.0, 0.3));
    vec3 lightColor = vec3(1.0);

    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;

    float diff = max(dot(fragNormal, -lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    vec3 viewDir = normalize(vec3(0.0, 5.0, 10.0) - fragWorldPos);
    vec3 halfwayDir = normalize(-lightDir + viewDir);
    float spec = pow(max(dot(fragNormal, halfwayDir), 0.0), 32.0);
    vec3 specular = 0.5 * spec * lightColor;

    vec3 result = (ambient + diffuse + specular) * baseColor;
    outColor = vec4(result, 1.0);
}