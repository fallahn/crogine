#pragma once

//based on the implementation by Mikola Lysenko
//at http://0fps.net/2012/07/12/smooth-voxel-terrain-part-2/

#include "Mesh.hpp"

class MarchingCubes final
{
public:

    static Mesh calculate(const std::vector<float>& volume, glm::ivec3 dimensions);
};
