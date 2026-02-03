#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D texture_diffuse1;

// Simple directional light from sun
void main() {
    vec3 lightPos = vec3(0.0, 0.0, 0.0); // Sun is at center
    vec3 lightColor = vec3(1.0, 1.0, 0.9);
    vec3 objectColor = vec3(0.7, 0.1, 0.1); // fallback color

    // Ambient
    float ambientStrength = 0.4;
    vec3 ambient = ambientStrength * lightColor;
  
    // Diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos); // Direction TO light
    // Actually lightPos is 0, so lightDir is normalize(-FragPos).
    // But if object is behind planet, it should be dark? 
    // For now simple lighting.
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Sample texture if available
    vec3 texColor = objectColor;
    // Note: binding texture 0 if none available will yield 0; we expect a white fallback texture or valid texture bound
    texColor = texture(texture_diffuse1, TexCoords).rgb * objectColor;
    vec3 result = (ambient + diffuse) * texColor;
    FragColor = vec4(result, 1.0);
}