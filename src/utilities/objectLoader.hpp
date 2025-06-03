#ifndef OBJECT_LOADER_HPP
#define OBJECT_LOADER_HPP

#include <string>
#include <vector>

// Definer strukturer slik at de er tilgjengelige i gamelogic.cpp
struct Vec3 {
    float x, y, z;
};

struct Vec2 {
    float u, v;
};

struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texCoord;
};

// Deklarasjon av loadOBJ()
bool loadOBJ(const std::string& filePath, std::vector<Vertex>& vertices, std::vector<unsigned int>& indices);

#endif 
