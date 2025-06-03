#include <glad/glad.h>
#include <program.hpp>
#include "glutils.h"
#include <vector>

template <class T>
unsigned int generateAttribute(int id, int elementsPerEntry, std::vector<T> data, bool normalize) {
    unsigned int bufferID;
    glGenBuffers(1, &bufferID);
    glBindBuffer(GL_ARRAY_BUFFER, bufferID);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(T), data.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(id, elementsPerEntry, GL_FLOAT, normalize ? GL_TRUE : GL_FALSE, sizeof(T), 0);
    glEnableVertexAttribArray(id);
    return bufferID;
}

unsigned int generateBuffer(Mesh &mesh) {
    unsigned int vaoID;
    glGenVertexArrays(1, &vaoID);
    glBindVertexArray(vaoID);

    generateAttribute(0, 3, mesh.vertices, false);
    if (mesh.normals.size() > 0) {
        generateAttribute(1, 3, mesh.normals, true);
    }
    if (mesh.textureCoordinates.size() > 0) {
        generateAttribute(2, 2, mesh.textureCoordinates, false);
    }

        // Beregn tangenter og bitangenter hvis vi har teksturkoordinater
    if (mesh.textureCoordinates.size() > 0) {
        std::vector<glm::vec3> tangents(mesh.vertices.size(), glm::vec3(0.0f));
        std::vector<glm::vec3> bitangents(mesh.vertices.size(), glm::vec3(0.0f));

        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            // Hent trekantens tre hjørner
            unsigned int i0 = mesh.indices[i];
            unsigned int i1 = mesh.indices[i + 1];
            unsigned int i2 = mesh.indices[i + 2];

            glm::vec3 v0 = mesh.vertices[i0];
            glm::vec3 v1 = mesh.vertices[i1];
            glm::vec3 v2 = mesh.vertices[i2];

            glm::vec2 uv0 = mesh.textureCoordinates[i0];
            glm::vec2 uv1 = mesh.textureCoordinates[i1];
            glm::vec2 uv2 = mesh.textureCoordinates[i2];

            // Beregn vektorer
            glm::vec3 deltaPos1 = v1 - v0;
            glm::vec3 deltaPos2 = v2 - v0;
            glm::vec2 deltaUV1 = uv1 - uv0;
            glm::vec2 deltaUV2 = uv2 - uv0;

            // Løs ligningssystemet for å finne tangent og bitangent
            float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
            glm::vec3 tangent = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;
            glm::vec3 bitangent = (deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x) * r;

            // Legg til tangent og bitangent for hver topp
            tangents[i0] += tangent;
            tangents[i1] += tangent;
            tangents[i2] += tangent;

            bitangents[i0] += bitangent;
            bitangents[i1] += bitangent;
            bitangents[i2] += bitangent;
        }

        // Normaliser tangentene og bitangentene
        for (size_t i = 0; i < mesh.vertices.size(); i++) {
            tangents[i] = glm::normalize(tangents[i]);
            bitangents[i] = glm::normalize(bitangents[i]);
        }

        // Send tangentene til GPU-en
        generateAttribute(7, 3, tangents, false);
        generateAttribute(8, 3, bitangents, false);
    }
    unsigned int indexBufferID;
    glGenBuffers(1, &indexBufferID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(unsigned int), mesh.indices.data(), GL_STATIC_DRAW);

    return vaoID;
}
