#pragma once

//based on the implementation by Mikola Lysenko
//at http://0fps.net/2012/07/12/smooth-voxel-terrain-part-2/

#include <crogine/detail/glm/vec3.hpp>
#include <vector>

struct Mesh final
{
    std::vector<glm::vec3> vertices;
    std::vector<glm::uvec3> indices;
};

class MarchingCubes final
{
public:

    static Mesh calculate(const std::vector<float>& volume, glm::ivec3 dimensions);
};

class SurfaceNet final
{
public:
    static Mesh calculate(const std::vector<float>& volume, glm::ivec3 dimensions);

private:
    static std::vector<std::int32_t> m_cubeEdges;
    static std::vector<std::int32_t> m_edgeTable;
};