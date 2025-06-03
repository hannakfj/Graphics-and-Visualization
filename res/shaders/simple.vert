#version 430 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal_in;
layout(location = 2) in vec2 textureCoordinates_in;
layout(location = 7) in vec3 tangent_in;
layout(location = 8) in vec3 bitangent_in;

layout(location = 3) uniform mat4 modelMatrix;    // Model matrix
layout(location = 4) uniform mat4 MVP;            // Model-View-Projection matrix
layout(location = 5) uniform mat3 normalMatrix;   // Normal matrix for lighting
layout(location = 6) uniform bool isTextRendering;
layout(location = 9) uniform bool isSkybox;
layout(location = 10) uniform bool isGeometry;
layout(location = 11) uniform bool isShadowPass;  
layout(location = 12) uniform mat4 lightSpaceMatrix; 

uniform bool isWater;  
uniform float time;    
uniform vec3 boatWorldPosition;

// Output til fragmentshaderen
out vec3 fragPosition;
out vec3 fragNormal;
out vec2 textureCoordinates_out;
out mat3 TBN;
out vec3 TexCoords;
out vec4 fragPosLightSpace;

void main() {
    vec3 newPosition = position;

if (isWater) {
    float waveStrength = 0.7;
    float waveSpeed = 2.0;
    float waveFrequency = 0.2;

    float wave1 = sin(time * waveSpeed + position.x * waveFrequency) * waveStrength;
    float wave2 = cos(time * waveSpeed * 1.2 + position.z * waveFrequency * 1.5) * (waveStrength * 0.7);
    float wave3 = sin(time * waveSpeed * 0.9 + (position.x + position.z) * waveFrequency * 1.1) * (waveStrength * 0.5);

    float baseWave = wave1 + wave2 + wave3;
    newPosition.y += baseWave;
}

    if (isSkybox) {
        TexCoords = normalize(vec3(newPosition.x, -newPosition.y, newPosition.z));
        gl_Position = MVP * vec4(newPosition, 1.0);
        gl_Position = gl_Position.xyww;  
        return;
    } 

    if (isGeometry) {
        gl_Position = MVP * vec4(newPosition, 1.0);
        fragPosition = vec3(modelMatrix * vec4(newPosition, 1.0));
        fragNormal = normalize(normalMatrix * normal_in);
        textureCoordinates_out = textureCoordinates_in;

        vec3 T = normalize(vec3(modelMatrix * vec4(tangent_in, 0.0)));
        vec3 B = normalize(vec3(modelMatrix * vec4(bitangent_in, 0.0)));
        vec3 N = normalize(vec3(modelMatrix * vec4(normal_in, 0.0)));

        TBN = mat3(T, B, N);

        fragPosLightSpace = lightSpaceMatrix * vec4(fragPosition, 1.0);
    }

    if (isShadowPass) {
       fragPosLightSpace = lightSpaceMatrix * vec4(newPosition, 1.0);
       gl_Position = fragPosLightSpace;
       return;
    }

}
