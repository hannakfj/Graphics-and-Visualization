#include <chrono>
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <SFML/Audio/SoundBuffer.hpp>
#include <utilities/shader.hpp>
#include <glm/vec3.hpp>
#include <iostream>
#include <utilities/timeutils.h>
#include <utilities/mesh.h>
#include <utilities/shapes.h>
#include <utilities/glutils.h>
#include <SFML/Audio/Sound.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>
#include "gamelogic.h"
#include "sceneGraph.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#define STB_PERLIN_IMPLEMENTATION
#include <glm/gtx/transform.hpp>
#include "utilities/imageLoader.hpp"
#include "utilities/glfont.h"
#include "utilities/objectLoader.hpp"
#include <vector>
#include <glm/gtx/string_cast.hpp>
#include "stb_perlin.h"
#include <filesystem> 


enum KeyFrameAction {
    BOTTOM, TOP
};


#include <timestamps.h>
SceneNode* rootNode;
SceneNode* skyboxNode;
SceneNode* terrainNode;
SceneNode* lightNode;
std::vector<SceneNode*> treeNodes;
std::vector<SceneNode*> fishNodes;
SceneNode* boatNode;
SceneNode* dirLight;
SceneNode* waterNode;
SceneNode* tree1Node;
const unsigned int SHADOW_WIDTH = 20000;
const unsigned int SHADOW_HEIGHT = 20000;

unsigned int depthMapFBO;
unsigned int depthMap;
bool renderingShadowMap = false;

Gloom::Shader* shader;

struct TerrainMesh {
    unsigned int VAO, VBO, EBO;
    int indexCount;
};

struct WaterMesh {
    unsigned int VAO, VBO, EBO;
    int indexCount;
};

WaterMesh generateWaterMesh(int size, glm::vec2 lakeCenter, float lakeRadius, float waterLevel, float uvScale) {
    std::vector<float> waterVertices; // Stores vertex atrributes, position, UV and normal
    std::vector<unsigned int> waterIndices;// Triangle indices
    std::vector<int> validIndices(size * size, -1); //Keeps track of valid indices -> That is inside the lake

    int index = 0;

    //Precompute heights for normal calculation 
    std::vector<float> heights(size * size, waterLevel);

    // Generate vertices
    for (int z = 0; z < size; ++z) {
        for (int x = 0; x < size; ++x) {
            // Create an elliptical lake shape
            float ellipseFactorX = 1.3f;
            float ellipseFactorZ = 0.8f;
            float baseDistance = glm::distance(glm::vec2((x - lakeCenter.x) * ellipseFactorX,
                                                         (z - lakeCenter.y) * ellipseFactorZ), glm::vec2(0.0f));
            //If we are inside the lake
            if (baseDistance < lakeRadius) { 
                float height = waterLevel;

                waterVertices.push_back((float)x - size * 0.5f);  // X
                waterVertices.push_back(height);                 // Y 
                waterVertices.push_back((float)z - size * 0.5f); // Z

                heights[z * size + x] = height;

                // UV coordinates
                waterVertices.push_back((float)x / (size * uvScale));
                waterVertices.push_back((float)z / (size * uvScale));

                // Normal (initial: up)
                waterVertices.push_back(0.0f); // Normal X
                waterVertices.push_back(1.0f); // Normal Y
                waterVertices.push_back(0.0f); // Normal Z

                validIndices[z * size + x] = index++;
            }
        }
    }
    //Calculate normals based on surrounding height differences
    for (int z = 1; z < size - 1; ++z) {
        for (int x = 1; x < size - 1; ++x) {
            if (validIndices[z * size + x] == -1) continue;

            int index = validIndices[z * size + x] * 8; // 8 attributes per vertex

            float hL = heights[z * size + (x - 1)];
            float hR = heights[z * size + (x + 1)];
            float hD = heights[(z - 1) * size + x];
            float hU = heights[(z + 1) * size + x];

            glm::vec3 normal = glm::normalize(glm::vec3(hL - hR, 2.0f, hD - hU));

            // Update normal
            waterVertices[index + 5] = normal.x;
            waterVertices[index + 6] = normal.y;
            waterVertices[index + 7] = normal.z;
        }
    }
    //Genrerte indices for triangles. Two triangles per quad
    for (int z = 0; z < size - 1; ++z) {
        for (int x = 0; x < size - 1; ++x) {
            int topLeft = z * size + x;
            int topRight = topLeft + 1;
            int bottomLeft = (z + 1) * size + x;
            int bottomRight = bottomLeft + 1;

            if (validIndices[topLeft] != -1 && validIndices[bottomLeft] != -1 && validIndices[topRight] != -1) {
                waterIndices.push_back(validIndices[topLeft]);
                waterIndices.push_back(validIndices[bottomLeft]);
                waterIndices.push_back(validIndices[topRight]);
            }
            if (validIndices[topRight] != -1 && validIndices[bottomLeft] != -1 && validIndices[bottomRight] != -1) {
                waterIndices.push_back(validIndices[topRight]);
                waterIndices.push_back(validIndices[bottomLeft]);
                waterIndices.push_back(validIndices[bottomRight]);
            }
        }
    }
    //Create and return a WaterMesh struct
    WaterMesh waterMesh;
    waterMesh.indexCount = waterIndices.size();

    glGenVertexArrays(1, &waterMesh.VAO);
    glGenBuffers(1, &waterMesh.VBO);
    glGenBuffers(1, &waterMesh.EBO);

    glBindVertexArray(waterMesh.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, waterMesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, waterVertices.size() * sizeof(float), waterVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, waterMesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, waterIndices.size() * sizeof(unsigned int), waterIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);         // Posisjon
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); // UV
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float))); // Normal
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    return waterMesh;
}




TerrainMesh generateUnevenTerrain(int size, float heightScale, float uvScale) {
    std::vector<float> vertices;
    std::vector<float> normals(size * size * 3, 0.0f);
    std::vector<unsigned int> indices;

    float noiseScale = 0.1f;
    float centerX = size / 2.0f;
    float centerZ = size / 2.0f;

    glm::vec2 lakeCenter = glm::vec2(size * 0.7f, size * 0.4f); //Move the lake to the side
    float lakeRadius = size * 0.08f; // Lake size

    // Generate vertex positions using Perlin noise
    for (int z = 0; z < size; ++z) {
        for (int x = 0; x < size; ++x) {

            float noiseFactor = stb_perlin_noise3(x * 0.2f, z * 0.2f, 0.0f, 0, 0, 0) * 8.0f;

            float ellipseFactorX = 1.3f;  // Stretch in x-direction
            float ellipseFactorZ = 0.8f;  // Compress in z-direction
            float baseDistance = glm::distance(glm::vec2((x - lakeCenter.x) * ellipseFactorX,
                                                         (z - lakeCenter.y) * ellipseFactorZ), glm::vec2(0.0f));

            float distortedDistance = baseDistance + noiseFactor;

            float height = stb_perlin_noise3(x * noiseScale, z * noiseScale, 0.0f, 0, 0, 0) * heightScale;

            if (distortedDistance < lakeRadius) {
                float blend = glm::smoothstep(lakeRadius - 10.0f, lakeRadius, distortedDistance);
                height = glm::mix(-20.0f, height, blend);
                
            }

            vertices.push_back((float)x - size * 0.5f); // X
            vertices.push_back(height);                 // Y
            vertices.push_back((float)z - size * 0.5f); // Z
            vertices.push_back((float)x / (size * uvScale));
            vertices.push_back((float)z / (size * uvScale));
        }
    }

    for (int z = 0; z < size - 1; ++z) {
        for (int x = 0; x < size - 1; ++x) {
            int topLeft = (z * size) + x;
            int topRight = topLeft + 1;
            int bottomLeft = ((z + 1) * size) + x;
            int bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    for (int z = 1; z < size - 1; ++z) {
        for (int x = 1; x < size - 1; ++x) {
            int idx = (z * size + x) * 5;

            float hL = vertices[idx + 1 - 5];
            float hR = vertices[idx + 1 + 5];
            float hD = vertices[idx + 1 - 5 * size];
            float hU = vertices[idx + 1 + 5 * size];

            glm::vec3 normal = glm::normalize(glm::vec3(hL - hR, 2.0f, hD - hU));

            if (glm::distance(glm::vec2(x, z), lakeCenter) < lakeRadius) {
                normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            int normalIdx = (z * size + x) * 3;
            normals[normalIdx] = normal.x;
            normals[normalIdx + 1] = normal.y;
            normals[normalIdx + 2] = normal.z;
        }
    }

    TerrainMesh terrain;
    terrain.indexCount = indices.size();

    glGenVertexArrays(1, &terrain.VAO);
    glGenBuffers(1, &terrain.VBO);
    glGenBuffers(1, &terrain.EBO);

    glBindVertexArray(terrain.VAO);

    std::vector<float> vertexData;
    for (int i = 0; i < size * size; ++i) {
        vertexData.push_back(vertices[i * 5]);
        vertexData.push_back(vertices[i * 5 + 1]);
        vertexData.push_back(vertices[i * 5 + 2]);
        vertexData.push_back(normals[i * 3]);
        vertexData.push_back(normals[i * 3 + 1]);
        vertexData.push_back(normals[i * 3 + 2]);
        vertexData.push_back(vertices[i * 5 + 3]);
        vertexData.push_back(vertices[i * 5 + 4]);
    }

    glBindBuffer(GL_ARRAY_BUFFER, terrain.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrain.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    return terrain;
}


SceneNode* createTerrainNode(const TerrainMesh& terrainMesh) {
    SceneNode* terrainNode = createSceneNode();
    terrainNode->nodeType = GEOMETRY;
    terrainNode->vertexArrayObjectID = terrainMesh.VAO;
    terrainNode->VAOIndexCount = terrainMesh.indexCount;
    return terrainNode;
}

SceneNode* createDirectionalLightNode(glm::vec3 direction, glm::vec3 color) {
    SceneNode* lightNode = createSceneNode();
    lightNode->nodeType = DIRECTIONAL_LIGHT;
    lightNode->lightDirection = glm::normalize(direction);
    lightNode->lightColor = color;
    return lightNode;
}

unsigned int createTextureFromImage(const PNGImage& image) {
    unsigned int textureID;

    //Generate an empty texture
    glGenTextures(1, &textureID);

    //Bind the textures
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Configure the textureparameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // S-akse
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // T-akse
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 

    float maxAniso = 0.2f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, maxAniso);

    glTexImage2D(
        GL_TEXTURE_2D,         // Target
        0,                     // Level of detail (mipmap level)
        GL_RGBA,               // Intern format (RGBA)
        image.width,
        image.height,
        0,
        GL_RGBA,               // Inputformat
        GL_UNSIGNED_BYTE,
        image.pixels.data()     //pointer to image data
    );
    // Generer mipmaps
    glGenerateMipmap(GL_TEXTURE_2D);


    // Returner tekstur-ID
    return textureID;
}

// Generate a VAO for the skybox
unsigned int generateSkyboxVAO() {
    // Create a VAO
    unsigned int VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Create a VBO
    unsigned int VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
        1.0f,  1.0f, -1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f,  1.0f
    };

    // Upload the vertices to the VBO
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);

    // Enable vertex attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // Unbind VAO and VBO
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Return the generated VAO
    return VAO;
}

unsigned int loadCubemap(const std::vector<std::string>& faces) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    for (size_t i = 0; i < faces.size(); i++) {
        PNGImage image = loadPNGFile(faces[i]);
        if (!image.pixels.empty()) {
            glTexImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0, GL_RGBA, image.width, image.height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, image.pixels.data()
            );
        } else {
            std::cerr << "Cubemap texture failed to load: " << faces[i] << std::endl;
        }
    }



    // Set texture parameters
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}


void initShadowMap() {
    glGenFramebuffers(1, &depthMapFBO);
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0, 1.0, 1.0, 1.0};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);

    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Error: Shadow framebuffer is not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


// Define faces of the cubemap
std::vector<std::string> faces = {
    "../res/textures/right.png",   // +X
    "../res/textures/left.png",    // -X
    "../res/textures/bottom.png",  // -Y
    "../res/textures/top.png",     // +Y
    "../res/textures/front.png",   // +Z
    "../res/textures/back.png"     // -Z
};



SceneNode* createSkyboxNode(GLuint cubemapTexture) {
    // Create a new SceneNode for the skybox
    skyboxNode = createSceneNode();

    // Set it as a SKYBOX type
    skyboxNode->nodeType = SKYBOX;

    // Assign the skybox's VAO 
    skyboxNode->vertexArrayObjectID = generateSkyboxVAO();

    // Assign the cubemap texture
    skyboxNode->textureID = cubemapTexture;

    // Large scale so the skybox surrounds the scene
    skyboxNode->scale = glm::vec3(1000.0f);

    return skyboxNode;
}


SceneNode* createTreeNode(Mesh treeMesh, unsigned int treeVAO, unsigned int treeTextureID) {
    SceneNode* treeNode = createSceneNode();
    treeNode->nodeType = GEOMETRY;
    treeNode->vertexArrayObjectID = treeVAO;          // VAO for texure
    treeNode->VAOIndexCount = treeMesh.indices.size(); 
    treeNode->textureID = treeTextureID;

    return treeNode;
}



unsigned int createTreeVAO(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
    unsigned int VAO, VBO, EBO;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);

    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);

    // UV
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    return VAO;
}


//Camera variables
glm::vec3 cameraPos = glm::vec3(0.0f, 20.0f, -30.0f);
glm::vec3 cameraFront = glm::normalize(glm::vec3(0.0f, -0.2f, 1.0f)); //View direction
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);    

// Rotation variables
float yaw   = -90.0f;  // Start pointing towards negative z-axis
float pitch =  0.0f; 
float lastX = windowWidth / 2.0; //Start in center of window
float lastY = windowHeight / 2.0;

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    static bool firstMouse = true;

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xOffset = xpos - lastX;  // Move side-to-side
    float yOffset = lastY - ypos;  // Move back and forth

    lastX = xpos;
    lastY = ypos;

    float mouseSensitivity = 0.1f;
    xOffset *= mouseSensitivity;
    yOffset *= mouseSensitivity;

    // Update yaw to rotate around y-axis (horizontal)
    yaw += xOffset;

    // Calculate the new camera front and camara position
    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw));
    direction.y = 0.0f; // No vertical movement
    direction.z = sin(glm::radians(yaw));
    cameraFront = glm::normalize(direction);

    float moveSpeed = 1.0f;
    cameraPos += cameraFront * (yOffset * moveSpeed);
}

void initGame(GLFWwindow* window) {
    glfwSetCursorPosCallback(window, mouseCallback);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glm::vec2 lakeCenter = glm::vec2(700, 400);
    float lakeRadius = 80.0f;
    float waterLevel = -18.0f;
    float worldX = lakeCenter.x - 500.0f; // Sentrert rundt 0
    float worldZ = lakeCenter.y - 500.0f;

    //Load the cubemap texture
    unsigned int cubemapTexture = loadCubemap(faces);

    // Initialize the shadow map
    initShadowMap();

    //Load and create texture for terrain and trees
    PNGImage terrainImage = loadPNGFile("../res/textures/grass1.png");
    unsigned int terrainTexture = createTextureFromImage(terrainImage);

    PNGImage treeImage = loadPNGFile("../res/textures/treeTexture.png");
    unsigned int treeTexture = createTextureFromImage(treeImage);

    PNGImage boatImage = loadPNGFile("../res/textures/wood2.png");
    unsigned int boatTexture = createTextureFromImage(boatImage);

    PNGImage fishImage = loadPNGFile("../res/textures/fish.png");
    unsigned int fishTexture = createTextureFromImage(fishImage);

    std::vector<Vertex> boatVertices;
    std::vector<unsigned int> boatIndices;
    
    if (!loadOBJ("../res/obj/boat.obj", boatVertices, boatIndices)) {
        std::cerr << "Failed to load boat model!" << std::endl;
    } 
    std::vector<Vertex> treeVertices;
    std::vector<unsigned int> treeIndices;

    if (!loadOBJ("../res/obj/RedDeliciousApple.obj", treeVertices, treeIndices)) {
        std::cerr << "Failed to load tree model!" << std::endl;
    }

    std::vector<Vertex> fishVertices;
    std::vector<unsigned int> fishIndices;

    if (!loadOBJ("../res/obj/fish.obj", fishVertices, fishIndices)) {
        std::cerr << "Failed to load fish model!" << std::endl;
    }

    // Convert Vertex data to position-only (glm::vec3)
    std::vector<glm::vec3> treePositions;
    for (const Vertex& v : treeVertices) {
        treePositions.push_back(glm::vec3(v.position.x, v.position.y, v.position.z));
    }

    std::vector<glm::vec3> boatPositions;
    for (const Vertex& v : boatVertices) {
        boatPositions.push_back(glm::vec3(v.position.x, v.position.y, v.position.z));
    }
    std::vector<glm::vec3> fishPositions;
    for (const Vertex& v : fishVertices) {
        fishPositions.push_back(glm::vec3(v.position.x, v.position.y, v.position.z));
    }

    // Create Mesh structures for the tree and boat
    Mesh treemesh;
    treemesh.vertices = treePositions;
    treemesh.indices = treeIndices;
    Mesh boatmesh;
    boatmesh.vertices = boatPositions;
    boatmesh.indices = boatIndices;
    Mesh fishmesh;
    fishmesh.vertices = fishPositions;
    fishmesh.indices = fishIndices;


    // Create VAO for the tree and boat
    unsigned int treeVAO = createTreeVAO(treeVertices, treeIndices);
    unsigned int boatVAO = createTreeVAO(boatVertices, boatIndices);
    unsigned int fishVAO = createTreeVAO(fishVertices, fishIndices);

    //Set up the shader
    shader = new Gloom::Shader();
    shader->makeBasicShader("../res/shaders/simple.vert", "../res/shaders/simple.frag");
    shader->activate();

    //Construct the scene
    rootNode = createSceneNode();                                 
    skyboxNode = createSkyboxNode(cubemapTexture);               
    dirLight = createDirectionalLightNode(                       
        glm::vec3(-0.03f, -1.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f)
    );

    //Water setup
    WaterMesh waterMesh = generateWaterMesh(1000, lakeCenter, lakeRadius, waterLevel, 0.001f);

    waterNode = createSceneNode();
    waterNode->nodeType = GEOMETRY;
    waterNode->vertexArrayObjectID = waterMesh.VAO;
    waterNode->VAOIndexCount = waterMesh.indexCount;
    waterNode->position = glm::vec3(0, 15.0, 0); // Adjust water height

    tree1Node = createSceneNode();
    tree1Node->nodeType = GEOMETRY;
    tree1Node->vertexArrayObjectID = treeVAO;
    tree1Node->VAOIndexCount = treemesh.indices.size();
    tree1Node->position = glm::vec3(worldX + 60, 0.0f, worldZ);
    tree1Node->textureID = treeTexture;
    tree1Node->scale = glm::vec3(4.0f);

    //Boat setup
    boatNode = createSceneNode();
    boatNode->nodeType = GEOMETRY;
    boatNode->vertexArrayObjectID = boatVAO;
    boatNode->VAOIndexCount = boatmesh.indices.size();
    boatNode->position = glm::vec3(worldX-20, -10.0f, worldZ+20); 
    boatNode->textureID = boatTexture;
    boatNode->scale = glm::vec3(2.5f);


    //Terrain setup
    TerrainMesh terrainMesh = generateUnevenTerrain(1000, 4, 0.02f);
    terrainNode = createTerrainNode(terrainMesh);
    terrainNode->textureID = terrainTexture;

    // Add the nodes to the scene graph
    rootNode->children.push_back(waterNode);
    rootNode->children.push_back(dirLight);
    rootNode->children.push_back(terrainNode);
    rootNode->children.push_back(skyboxNode); 
    rootNode->children.push_back(boatNode);
    rootNode->children.push_back(tree1Node);

    //Generate trees and add them to the scene graph
    const int numTrees = 30;
    const int numFish = 10;
    int fishAdded = 0;
    while (fishAdded < numFish) {
        float x = (rand() % 1000) - 500;
        float z = (rand() % 1000) - 500;
    
        // Sjekk om posisjonen er innenfor innsjøen
        float ellipseFactorX = 1.3f;
        float ellipseFactorZ = 0.8f;
        glm::vec2 lakeCenterWorld = glm::vec2(worldX, worldZ); 
    
        float dx = (x - lakeCenterWorld.x) * ellipseFactorX;
        float dz = (z - lakeCenterWorld.y) * ellipseFactorZ;
        float distance = glm::length(glm::vec2(dx, dz));
    
        if (distance > lakeRadius) {
            continue; 
        }
    
        SceneNode* newFish = createSceneNode();
        newFish->nodeType = GEOMETRY;
        newFish->vertexArrayObjectID = fishVAO;
        newFish->VAOIndexCount = fishmesh.indices.size();
        newFish->textureID = fishTexture;
        newFish->position = glm::vec3(x, -7.0, z);
        newFish->scale = glm::vec3(0.3f);
        newFish->rotation.x = glm::radians(-90.0f);

    

        fishNodes.push_back(newFish);
        rootNode->children.push_back(newFish);
        fishAdded++;
    }
    

    for (int i = 0; i < numTrees; i++) {
        SceneNode* newTree = createTreeNode(treemesh, treeVAO, treeTexture);

        // Random position within the range -500 to 500 
        float x = (rand() % 1000) - 500;
        float z = (rand() % 1000) - 500;
        float y = 0.0f; 

        // Randomly scaling the trees between 1 and 5
        float scaleFactor = 1.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (5.0f - 1.0f)));

        newTree->position = glm::vec3(x, y, z);
        newTree->scale = glm::vec3(scaleFactor);

        treeNodes.push_back(newTree);
        rootNode->children.push_back(newTree);
    }
}

void updateFrame(GLFWwindow* window) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    updateNodeTransformations(rootNode, glm::mat4(1.0f));
}


void updateNodeTransformations(SceneNode* node, glm::mat4 currentModelMatrix) {
    static float time = 0.0f;
    time += getTimeDeltaSeconds(); // Time increases with each frame, used for animation
    if (std::find(fishNodes.begin(), fishNodes.end(), node) != fishNodes.end()) {
        float swimSpeed = 1.5f;
        float amplitude = 12.0f;
    
        float offset = node->position.x * 0.1f + node->position.z * 0.1f;
        node->position.x += sin(time * swimSpeed + offset) * 0.02f;
        node->position.z += cos(time * swimSpeed + offset) * 0.02f;
        node->rotation.y = sin(time * swimSpeed + offset) * glm::radians(30.0f); 
    }
    //Animate Tree Swaying
    if ((std::find(treeNodes.begin(), treeNodes.end(), node) != treeNodes.end())|| node == tree1Node) {
        float swayAmount = glm::radians(5.0f);  // Maximum rotation angle (5 degrees)
        float swaySpeed = 0.7f;                 // Speed of the swaying (wind-like)

        // Create a unique offset based on tree position so each tree moves slightly differently
        float offset = node->position.x * 0.1f + node->position.z * 0.1f;

        // Use both sine and cosine for more natural movement in multiple directions
        node->rotation.x = sin(time * swaySpeed + offset) * swayAmount;  // Forward/backward sway
        node->rotation.z = cos(time * swaySpeed + offset) * swayAmount;  // Side-to-side sway
    }
    if (node == boatNode && !renderingShadowMap) {
        float waveStrength = 0.6f;
        float waveSpeed = 2.0f;
        float waveFrequency = 0.2f;
    
        float time = glfwGetTime(); 
        float x = node->position.x;
        float z = node->position.z;
    
        float wave1 = sin(time * waveSpeed + x * waveFrequency) * waveStrength;
        float wave2 = cos(time * waveSpeed * 1.2f + z * waveFrequency * 1.5f) * (waveStrength * 0.7f);
        float wave3 = sin(time * waveSpeed * 0.9f + (x + z) * waveFrequency * 1.1f) * (waveStrength * 0.5f);
    
        float waveOffset = wave1 + wave2 + wave3;
    
        float baseY = -2.5f;
        node->position.y = baseY + waveOffset;
    
        node->rotation.x = sin(time * 1.5f) * glm::radians(2.0f);
        node->rotation.z = cos(time * 1.3f) * glm::radians(2.0f);
    }

    //Compute Transformation Matrix
    glm::mat4 transformationMatrix =
              glm::translate(node->position)                 // Position in world space
            * glm::translate(node->referencePoint)           // Move to reference point
            * glm::rotate(node->rotation.y, glm::vec3(0,1,0)) // Y-axis rotation
            * glm::rotate(node->rotation.x, glm::vec3(1,0,0)) // X-axis rotation
            * glm::rotate(node->rotation.z, glm::vec3(0,0,1)) // Z-axis rotation
            * glm::scale(node->scale)                         // Scaling
            * glm::translate(-node->referencePoint);          // Move back from reference point

    // Combine this node’s transform with its parent’s transform
    node->currentModelMatrix = currentModelMatrix * transformationMatrix;

    for (SceneNode* child : node->children) {
        updateNodeTransformations(child, node->currentModelMatrix);
    }
}


void renderNode(SceneNode* node, glm::mat4 viewMatrix, glm::mat4 projection) {
    glm::mat4 currentMVPMatrix;
    currentMVPMatrix = projection * viewMatrix * node->currentModelMatrix;
    
    // Calculate normal matrix and set matrix uniforms
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(node->currentModelMatrix)));
    glUniformMatrix4fv(3, 1, GL_FALSE, glm::value_ptr(node->currentModelMatrix));
    glUniformMatrix4fv(4, 1, GL_FALSE, glm::value_ptr(currentMVPMatrix));
    glUniformMatrix3fv(5, 1, GL_FALSE, glm::value_ptr(normalMatrix));

    if (node->nodeType == SKYBOX) {
        glDepthFunc(GL_LEQUAL);  // Ensure skybox is drawn in the background
        glDepthMask(GL_FALSE);   // Disable depth writing
        glUniform1i(glGetUniformLocation(shader->get(), "isSkybox"), 1);

        glBindVertexArray(node->vertexArrayObjectID);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_CUBE_MAP, node->textureID);
        glUniform1i(glGetUniformLocation(shader->get(), "skybox"), 3);

        glDrawArrays(GL_TRIANGLES, 0, 36);

        glUniform1i(glGetUniformLocation(shader->get(), "isSkybox"), 0);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);

    }

    if (node->nodeType == GEOMETRY) {

        glUseProgram(shader->get());
        glUniform1i(glGetUniformLocation(shader->get(), "isSkybox"), 0);
        glUniform3fv(glGetUniformLocation(shader->get(), "boatWorldPosition"), 1, glm::value_ptr(boatNode->position));
        glUniform1i(glGetUniformLocation(shader->get(), "isGeometry"), 1);
        int isTree = (std::find(treeNodes.begin(), treeNodes.end(), node) != treeNodes.end()) ? 1 : 0;
        if(node == tree1Node) isTree = 1;
        glUniform1i(glGetUniformLocation(shader->get(), "isTree"), isTree);
        int isWater = (node == waterNode) ? 1 : 0;
        glUniform1i(glGetUniformLocation(shader->get(), "isWater"), isWater);
        int isBoat = (node == boatNode) ? 1 : 0;
        glUniform1i(glGetUniformLocation(shader->get(), "isBoat"), isBoat);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, node->textureID);
        glUniform1i(glGetUniformLocation(shader->get(), "Texture"), 0);
        
        glBindVertexArray(node->vertexArrayObjectID);
        glDrawElements(GL_TRIANGLES, node->VAOIndexCount, GL_UNSIGNED_INT, 0);
        glUniform1i(glGetUniformLocation(shader->get(), "isGeometry"), 0);
        glUniform1i(glGetUniformLocation(shader->get(), "isTree"), 0);
        glUniform1i(glGetUniformLocation(shader->get(), "isWater"), 0);
    }


    if (node->nodeType == DIRECTIONAL_LIGHT) {
        glUseProgram(shader->get());

        glUniform3fv(glGetUniformLocation(shader->get(), "dirLight.direction"), 1, glm::value_ptr(node->lightDirection));
        glUniform3fv(glGetUniformLocation(shader->get(), "dirLight.ambient"), 1, glm::value_ptr(node->lightColor * 0.2f));
        glUniform3fv(glGetUniformLocation(shader->get(), "dirLight.diffuse"), 1, glm::value_ptr(node->lightColor * 0.2f));
        glUniform3fv(glGetUniformLocation(shader->get(), "dirLight.specular"), 1, glm::value_ptr(glm::vec3(0.1f)));
        glUniform3fv(glGetUniformLocation(shader->get(), "dirLight.color"), 1, glm::value_ptr(node->lightColor * 0.2f));
    }

    for(SceneNode* child : node->children) {
        renderNode(child, viewMatrix, projection);
    }
}

void renderShadowMap() {
    // Set the viewport to the shadow map's resolution
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);

    // Bind the framebuffer used for rendering the depth map
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);

    // Clear the depth buffer so that old shadows are erased
    glClear(GL_DEPTH_BUFFER_BIT);

    // Set up the light's projection matrix
    // This defines how much of the scene the light "camera" can see.
    // If it's too small, trees outside this box won't cast shadows.
    glm::mat4 lightProjection = glm::ortho(
        -600.0f, 600.0f,     // left, right
        -600.0f, 600.0f,     // bottom, top
        1.0f, 1000.0f        // near, far
    );

    // too small projection will clip shadows:
    // glm::mat4 lightProjection = glm::ortho(-200.0f, 200.0f, -200.0f, 200.0f, 1.0f, 1000.0f);

    // Set up the light's view matrix
    // Positioning a camera at the light’s position,
    // looking in the direction of the light.
    glm::vec3 cameraPosition = glm::vec3(0.0f, 600.0f, 0.0f); // Light's "camera" position above the scene

    glm::mat4 lightView = glm::lookAt(
        cameraPosition,                              // Eye (camera) position
        cameraPosition + dirLight->lightDirection,   // Look-at target (where the light points)
        glm::vec3(0.0f, 1.0f, 0.0f)                   // Up direction
    );

    // Multiply projection and view to get the light-space matrix
    // This matrix transforms world-space positions into the light’s perspective
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;

    // Send the light-space matrix to the shader
    shader->activate();
    glUniformMatrix4fv(
        glGetUniformLocation(shader->get(), "lightSpaceMatrix"),
        1, GL_FALSE, glm::value_ptr(lightSpaceMatrix)
    );

    renderingShadowMap = true;

    // Midlertidig løft båten
    float originalY = boatNode->position.y;
    boatNode->position.y
     += 30.0f;
    
    // Oppdater transformasjoner
    updateNodeTransformations(rootNode, glm::mat4(1.0f));
    renderNode(tree1Node, lightView, lightProjection);
    for(SceneNode* fish : fishNodes) {
        renderNode(fish, lightView, lightProjection);
    }
    // Render alt med opphevet båt
    renderNode(boatNode, lightView, lightProjection);

    for (SceneNode* tree : treeNodes){
        renderNode(tree, lightView, lightProjection);
    }
    renderNode(terrainNode, lightView, lightProjection);
    
    // Tilbakestill båtens posisjon
    boatNode->position.y = originalY;
    renderingShadowMap = false;
    
    // Oppdater transformasjoner igjen etterpå
    updateNodeTransformations(rootNode, glm::mat4(1.0f));
    
    // Unbind the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Restore the viewport for normal screen rendering
    glViewport(0, 0, windowWidth, windowHeight);
}


void renderFrame(GLFWwindow* window) {
    //Get window size
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    //Pass elapsed time to shader for animations of water
    glUniform1f(glGetUniformLocation(shader->get(), "time"), glfwGetTime());

    //first render shadow map. This renders the scene from the light's perspective into a depth texture
    renderShadowMap();

    //Bind the shadow map texture
    shader->activate();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glUniform1i(glGetUniformLocation(shader->get(), "shadowMap"), 1);

    //Create camera projection and view matrix
    glm::mat4 projection = glm::perspective(
        glm::radians(60.0f),               // Field of view
        float(windowWidth) / float(windowHeight), // Aspect ratio
        0.5f,                              // Near clipping plane
        1000.f                             // Far clipping plane
    );    
    glm::mat4 viewMatrix = glm::lookAt(
        cameraPos,           // Camera position in world space
        cameraPos + cameraFront, // Target (what camera is looking at)
        cameraUp             // Up direction
    );
    //Sort scene nodes to render water last. Needed this to make the water transparent and see the terrain beneath
    std::sort(rootNode->children.begin(), rootNode->children.end(), 
        [](SceneNode* a, SceneNode* b) { 
            return a != waterNode && b == waterNode;
        }
    );

    //Render each node in the scene graph
    for (SceneNode* child : rootNode->children) {
            renderNode(child, viewMatrix, projection);
        
    }
}

