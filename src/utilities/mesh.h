#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>


struct Mesh {
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> textureCoordinates;
    std::vector<unsigned int> indices;
};