#include "objectLoader.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
bool loadOBJ(const std::string& filePath, std::vector<Vertex>& vertices, std::vector<unsigned int>& indices) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open OBJ file: " << filePath << std::endl;
        return false;
    }

    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> texCoords;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string prefix;
        ss >> prefix;

        if (prefix == "v") {
            Vec3 pos;
            ss >> pos.x >> pos.y >> pos.z;
            positions.push_back(pos);
        } else if (prefix == "vt") {
            Vec2 uv;
            ss >> uv.u >> uv.v;
            texCoords.push_back(uv);
        } else if (prefix == "vn") {
            Vec3 norm;
            ss >> norm.x >> norm.y >> norm.z;
            normals.push_back(norm);
        } else if (prefix == "f") {
            std::vector<Vertex> faceVertices;
            std::string token;
            while (ss >> token) {
                Vertex vert{};
                std::istringstream vss(token);
                std::string vIdx, vtIdx, vnIdx;

                size_t firstSlash = token.find('/');
                size_t secondSlash = token.find('/', firstSlash + 1);

                try {
                    // Only position
                    if (firstSlash == std::string::npos) {
                        vIdx = token;
                    }
                    // v//vn format
                    else if (secondSlash == firstSlash + 1) {
                        vIdx = token.substr(0, firstSlash);
                        vnIdx = token.substr(secondSlash + 1);
                    }
                    // v/vt format
                    else if (secondSlash == std::string::npos) {
                        vIdx = token.substr(0, firstSlash);
                        vtIdx = token.substr(firstSlash + 1);
                    }
                    // v/vt/vn format
                    else {
                        vIdx = token.substr(0, firstSlash);
                        vtIdx = token.substr(firstSlash + 1, secondSlash - firstSlash - 1);
                        vnIdx = token.substr(secondSlash + 1);
                    }

                    vert.position = positions.at(std::stoi(vIdx) - 1);
                    if (!vtIdx.empty()) vert.texCoord = texCoords.at(std::stoi(vtIdx) - 1);
                    if (!vnIdx.empty()) vert.normal = normals.at(std::stoi(vnIdx) - 1);

                    faceVertices.push_back(vert);
                } catch (const std::exception& e) {
                    std::cerr << "Failed to parse face: " << token << " | " << e.what() << std::endl;
                    return false;
                }
            }

            // Triangulate polygon (fan method)
            for (size_t i = 1; i + 1 < faceVertices.size(); ++i) {
                vertices.push_back(faceVertices[0]);
                vertices.push_back(faceVertices[i]);
                vertices.push_back(faceVertices[i + 1]);

                indices.push_back(static_cast<unsigned int>(indices.size()));
                indices.push_back(static_cast<unsigned int>(indices.size()));
                indices.push_back(static_cast<unsigned int>(indices.size()));
            }
        }
    }

    file.close();
    return true;
}
