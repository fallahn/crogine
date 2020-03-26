/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
http://trederia.blogspot.com

crogine - Zlib license.

This software is provided 'as-is', without any express or
implied warranty.In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.

-----------------------------------------------------------------------*/

#pragma once

#include <crogine/graphics/MeshBuilder.hpp>

namespace SubMeshID
{
    //order is important when it comes to blending!
    enum
    {
        Solid = 0,
        Foliage,
        Water,

        Count
    };
}

/*
custom mesh builder creates an empty mesh with the
correct attributes for a chunk mesh. The VBO itself
is updated by the ChunkRenderer system.
*/
class ChunkMeshBuilder final : public cro::MeshBuilder
{
public:

    //return 0 and each mesh gets an automatic ID
    //from the mesh resource, prevents returning the
    //same instance each time...

    //TODO could also use the chunk position hash here
    //but it's unlikely a chunk gets completely recreated
    //(chunk updates just replace the VBO/IBO data)
    std::size_t getUID() const override { return 0; }

    //this is a helper for when mesh data is updated
    static std::size_t getVertexComponentCount() { return m_componentCount; }

private:

    static std::size_t m_componentCount;

    cro::Mesh::Data build() const override;
};
