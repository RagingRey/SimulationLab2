#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 modelOverride;
} pushConstants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;

void main() {
    vec4 worldPos = pushConstants.modelOverride * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;
    
    fragColor = inColor;
    fragWorldPos = worldPos.xyz;
    
    // Transform normal to world space (assuming uniform scaling)
    mat3 normalMatrix = mat3(transpose(inverse(pushConstants.modelOverride)));
    fragNormal = normalize(normalMatrix * inNormal);
}