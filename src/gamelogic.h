#pragma once

#include <utilities/window.hpp>
#include "sceneGraph.hpp"

void updateNodeTransformations(SceneNode* node, glm::mat4 currentModelMatrix);
void initGame(GLFWwindow* window);
void updateFrame(GLFWwindow* window);
void renderFrame(GLFWwindow* window);
struct Heightmap {
    int width, height;
    std::vector<float> heights;
};

// Funksjon for å laste inn heightmap
Heightmap loadHeightmap(const std::string& filename, float heightScale = 20.0f);
