#version 430 core

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 textureCoordinates_out;
in vec3 TexCoords;
in vec4 fragPosLightSpace;

struct DirectionalLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    vec3 color;
};

uniform DirectionalLight dirLight;
uniform vec3 viewPos;
uniform samplerCube skybox;
uniform sampler2D Texture;
uniform sampler2D shadowMap;
uniform mat4 lightSpaceMatrix;

uniform bool isSkybox;
uniform bool isGeometry;
uniform bool isWater;
uniform bool isTree;
uniform float time;
uniform bool isBoat;
uniform vec3 boatWorldPosition;

out vec4 color;

//Skyggemapping for mer realistisk lys
float computeShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 1.0;

    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = max(0.01 * (1.0 - dot(normal, lightDir)), 0.005);

    return (currentDepth - bias > closestDepth) ? 0.3 : 1.0;
}

void main() {
    vec3 norm = normalize(fragNormal);  
    vec3 lightDir = normalize(-dirLight.direction);
    vec3 viewDir = normalize(viewPos - fragPosition);
    float diff = max(dot(norm, lightDir), 0.0);
    vec4 fragPosLightSpace = lightSpaceMatrix * vec4(fragPosition, 1.0);
    float shadow = computeShadow(fragPosLightSpace, norm, lightDir);

    if (isWater) {
        vec3 norm = normalize(fragNormal);

        // === Extra screen-space normal ripples ===
        float rippleScale = 0.1;
        float rippleFreq = 0.1;
        float rippleSpeed = 0.2;

        float rippleX = sin(fragPosition.z * rippleFreq + time * rippleSpeed);
        float rippleZ = cos(fragPosition.x * rippleFreq + time * rippleSpeed);
        vec3 rippleOffset = vec3(rippleX, 0.0, rippleZ) * rippleScale * 0.1;

        float boatRippleFreq = 0.5;
        float boatRippleSpeed = 0.5;
        float boatRippleStrength = 0.4;

        vec2 toBoat = fragPosition.xz - boatWorldPosition.xz + vec2(5.0, -7.0);
        float dist = length(toBoat);

        // Radial sine wave from boat center
        float boatRipple = sin(dist * boatRippleFreq - time * boatRippleSpeed);
        vec3 boatRippleOffset = vec3(
            normalize(vec3(toBoat.x, 0.0, toBoat.y)) * boatRipple * boatRippleStrength / (1.0 + dist)
        );

        // Combine all normal ripples
        vec3 rippleNormal = normalize(norm + rippleOffset + boatRippleOffset);

        vec3 lightDir = normalize(-dirLight.direction);
        vec3 viewDir = normalize(viewPos - fragPosition);

        vec3 reflectedDir = reflect(-viewDir, rippleNormal);
        vec3 reflection = texture(skybox, reflectedDir).rgb;
        float fresnel = clamp(1.0 - dot(viewDir, rippleNormal), 0.0, 1.0);
        reflection *= mix(0.2, 1.0, pow(fresnel, 3.0));

        float diff = max(dot(rippleNormal, lightDir), 0.0);
        vec3 deepWaterColor = vec3(0.0, 0.12, 0.25);    
        vec3 shallowWaterColor = vec3(0.0, 0.3, 0.45);  
        vec3 waterColor = mix(deepWaterColor, shallowWaterColor, clamp(fragPosition.y * 0.05 + 0.5, 0.0, 1.0));

        vec3 ambient = dirLight.ambient * 1.2 * waterColor;
        vec3 diffuse = dirLight.diffuse * diff * 1.3 * waterColor;

        vec3 halfwayDir = normalize(viewDir + lightDir);
        float spec = pow(max(dot(rippleNormal, halfwayDir), 0.0), 32.0);
        vec3 specular = dirLight.specular * spec * 0.1;

        vec4 fragPosLS = lightSpaceMatrix * vec4(fragPosition, 1.0);
        float shadow = computeShadow(fragPosLS, rippleNormal, lightDir)* 3.0;

        vec3 finalColor = ambient + (diffuse + specular) * shadow + reflection;
        color = vec4(finalColor, 0.75);
        return;
    }



    if (isGeometry) {
        vec3 objectColor = texture(Texture, textureCoordinates_out).rgb;
        if (isTree) {
            vec4 texColor = texture(Texture, textureCoordinates_out);

            // Forkast fragmenter som er for gjennomsiktige
            if (texColor.a < 0.2) {
                discard;
            }

            vec4 fragPosLightSpace = lightSpaceMatrix * vec4(fragPosition, 1.0);

            vec3 objectColor = texColor.rgb * 7.0;
            vec3 ambient = dirLight.ambient * objectColor;
            vec3 diffuse = dirLight.diffuse * diff * objectColor;

            
            color = vec4(ambient  + diffuse, 1.0);
            return;
        }
        vec3 ambient = dirLight.ambient * objectColor;
        vec3 diffuse = dirLight.diffuse * diff * objectColor * shadow;
    if (isBoat) {
        // Blinn-Phong specular
        vec3 halfwayDir = normalize(viewDir + lightDir);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), 64.0); 
        vec3 specular = dirLight.specular * spec * 2; 

        // Balanced ambient & diffuse
        vec3 ambient = dirLight.ambient * objectColor * 1.5;
        vec3 diffuse = dirLight.diffuse * diff * objectColor * 4.5 * shadow;

        vec3 finalColor = ambient + diffuse + specular;
        color = vec4(finalColor, 1.0);
        return;
    }


        color = vec4(ambient + diffuse, 1.0);
        return;
    }

    if (isSkybox) {
        color = texture(skybox, normalize(TexCoords));
        return;
    }
}
