#pragma once

//based on the implementation by Mikola Lysenko
//at http://0fps.net/2012/07/12/smooth-voxel-terrain-part-2/

#include "Mesh.hpp"

class SurfaceNet final
{
public:
    SurfaceNet();

    Mesh calculate(const std::vector<float>& volume, glm::ivec3 dimensions) const;

private:
    std::vector<std::int32_t> m_cubeEdges;
    std::vector<std::int32_t> m_edgeTable;
};