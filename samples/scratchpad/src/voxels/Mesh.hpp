#pragma once

#include <crogine/detail/glm/vec3.hpp>

#include <vector>

struct Mesh final
{
    std::vector<glm::vec3> vertices;
    std::vector<glm::uvec3> indices;
};
