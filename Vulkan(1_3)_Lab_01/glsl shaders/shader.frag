#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

void main() {
    // Simple directional light (from above and slightly to the side)
    vec3 lightDir = normalize(vec3(0.5, -1.0, 0.3));
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    
    // Ambient lighting
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;
    
    // Diffuse lighting
    float diff = max(dot(fragNormal, -lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Specular lighting (simple Blinn-Phong)
    vec3 viewDir = normalize(vec3(0.0, 5.0, 10.0) - fragWorldPos); // Approximate camera position
    vec3 halfwayDir = normalize(-lightDir + viewDir);
    float spec = pow(max(dot(fragNormal, halfwayDir), 0.0), 32.0);
    vec3 specular = 0.5 * spec * lightColor;
    
    // Combine lighting with base color
    vec3 result = (ambient + diffuse + specular) * fragColor;
    outColor = vec4(result, 1.0);
}