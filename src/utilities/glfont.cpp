#include <iostream>
#include "glfont.h"


Mesh generateTextGeometryBuffer(std::string text, float characterHeightOverWidth, float totalTextWidth) {
    float characterWidth = totalTextWidth / float(text.length());
    float characterHeight = characterHeightOverWidth * characterWidth;

    unsigned int vertexCount = 4 * text.length();
    unsigned int indexCount = 6 * text.length();

    Mesh mesh;

    mesh.vertices.resize(vertexCount);
    mesh.indices.resize(indexCount);
    mesh.textureCoordinates.resize(vertexCount);

    // Texture information
    const int cols = 128;    // Number of columns in the font texture
    const float cellWidth = 1.0f / cols; // Width of each character cell

    for(unsigned int i = 0; i < text.length(); i++)
    {
        float baseXCoordinate = float(i) * characterWidth;
        // Get the ASCII code and column offset
        int asciiIndex = static_cast<int>(text[i]);
        if (asciiIndex < 32 || asciiIndex > 126) continue;  // Skip unsupported characters
        int col = asciiIndex;

        // Calculate UV coordinates
        float u0 = col * cellWidth;
        float u1 = (col + 1) * cellWidth;
        float v0 = 0.0f;
        float v1 = 1.0f;

        mesh.vertices.at(4 * i + 0) = glm::vec3(baseXCoordinate, 0, 0);                     
        mesh.vertices.at(4 * i + 1) = glm::vec3(baseXCoordinate + characterWidth, 0, 0);    
        mesh.vertices.at(4 * i + 2) = glm::vec3(baseXCoordinate + characterWidth, characterHeight, 0); 
        mesh.vertices.at(4 * i + 3) = glm::vec3(baseXCoordinate, characterHeight, 0);       

        mesh.textureCoordinates.at(4 * i + 0) = glm::vec2(u0, v0);  
        mesh.textureCoordinates.at(4 * i + 1) = glm::vec2(u1, v0);  
        mesh.textureCoordinates.at(4 * i + 2) = glm::vec2(u1, v1);  
        mesh.textureCoordinates.at(4 * i + 3) = glm::vec2(u0, v1);  
        
        mesh.indices.at(6 * i + 0) = 4 * i + 0;
        mesh.indices.at(6 * i + 1) = 4 * i + 1;
        mesh.indices.at(6 * i + 2) = 4 * i + 2;
        mesh.indices.at(6 * i + 3) = 4 * i + 0;
        mesh.indices.at(6 * i + 4) = 4 * i + 2;
        mesh.indices.at(6 * i + 5) = 4 * i + 3;
    }

    return mesh;
}


