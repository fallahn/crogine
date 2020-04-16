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

#include <crogine/graphics/DynamicMeshBuilder.hpp>

#include "../detail/glad.hpp"
#include "../detail/GLCheck.hpp"

using namespace cro;

DynamicMeshBuilder::DynamicMeshBuilder(std::uint32_t flags, std::uint8_t submeshCount, std::uint32_t primitiveType)
    : m_flags       (flags),
    m_submeshCount  (submeshCount),
    m_primitiveType (primitiveType)
{
    CRO_ASSERT(flags != 0, "must specify at least one atribute");
    CRO_ASSERT(submeshCount > 0, "must request at least one submesh");
}

//private
Mesh::Data DynamicMeshBuilder::build() const
{


    return {};
}